/* aether_h2.c — HTTP/2 server-side glue, libnghttp2 wrapper.
 *
 * Issue #260 Tier 2.
 *
 * Layout:
 *   - Stream state lives in AetherH2Stream nodes hanging off a
 *     per-session linked list; nghttp2 owns its own stream metadata
 *     internally, ours holds the "what we're going to dispatch" view
 *     (request bits assembled so far, per-stream DATA payload, etc.).
 *   - The nghttp2 callbacks are intentionally narrow shims that
 *     append to / consume from those Aether-side structs. The
 *     dispatch decision (call into the existing route table) only
 *     fires once nghttp2 reports END_STREAM on the request side.
 *   - Response submission goes back through nghttp2 via
 *     nghttp2_submit_response + a data provider that reads from the
 *     stream's response-body buffer.
 *
 * When the build doesn't link libnghttp2, every public function is
 * a stub that returns the "unavailable" sentinel. The compile gate
 * is AETHER_HAS_NGHTTP2.
 */

#include "aether_h2.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef AETHER_HAS_NGHTTP2

#include <nghttp2/nghttp2.h>
#include "../../../net/aether_http_server.h"

/* Per-stream state — what we've collected on the request side and
 * what we owe back on the response side. Streams live in a
 * singly-linked list off the session; lookup is O(N) which is fine
 * for the modest number of in-flight streams a typical h2 connection
 * holds (RFC default 100). */
typedef struct AetherH2Stream {
    int32_t stream_id;

    /* Request side — assembled from on_header callbacks until the
     * client end-stream signals "ready to dispatch." Everything is
     * heap-allocated since nghttp2 doesn't retain header buffers
     * past the callback that delivered them. */
    char*  method;       /* :method pseudo-header */
    char*  path;         /* :path pseudo-header */
    char*  scheme;       /* :scheme pseudo-header (informational) */
    char*  authority;    /* :authority pseudo-header (Host equivalent) */
    char*  header_block; /* HTTP/1.1-shaped header block we hand to
                          * the existing route handler so middleware
                          * that introspects raw headers (CORS,
                          * basic-auth, vhost) keeps working. */
    size_t header_block_len;
    size_t header_block_cap;
    char*  body;
    size_t body_len;
    size_t body_cap;
    int    request_done;   /* peer sent END_STREAM on the request */

    /* Response side — populated when the route handler returns,
     * consumed by the nghttp2_data_source_read_callback as the
     * library drains the response onto the wire. */
    int          response_status;
    char*        response_body;
    size_t       response_body_len;
    size_t       response_body_cursor;
    /* Header pairs ready to submit. Each entry is a malloc'd name+
     * value pair owned by the stream. */
    nghttp2_nv*  response_headers;
    size_t       response_header_count;
    int          response_submitted;

    struct AetherH2Stream* next;
} AetherH2Stream;

struct AetherH2Session {
    nghttp2_session* ng;
    HttpServer*      server;
    void*            conn_userdata;

    /* Pending bytes nghttp2 has already serialised but the wire
     * callback hasn't accepted yet. We pull from nghttp2's send
     * pump on every drain call; this buffer absorbs short-write
     * cases where the wire callback can only take part of a chunk. */
    uint8_t*         out_buf;
    size_t           out_buf_len;
    size_t           out_buf_cap;

    AetherH2Stream*  streams;
    int              fatal;   /* set when a callback flagged a hard error */
};

/* ------------------------------------------------------------------
 * Stream bookkeeping
 * ------------------------------------------------------------------ */

static AetherH2Stream* stream_lookup(AetherH2Session* s, int32_t id) {
    for (AetherH2Stream* str = s->streams; str; str = str->next) {
        if (str->stream_id == id) return str;
    }
    return NULL;
}

static AetherH2Stream* stream_get_or_create(AetherH2Session* s, int32_t id) {
    AetherH2Stream* str = stream_lookup(s, id);
    if (str) return str;
    str = calloc(1, sizeof(*str));
    if (!str) return NULL;
    str->stream_id = id;
    str->next = s->streams;
    s->streams = str;
    return str;
}

static void stream_free(AetherH2Stream* str) {
    if (!str) return;
    free(str->method);
    free(str->path);
    free(str->scheme);
    free(str->authority);
    free(str->header_block);
    free(str->body);
    free(str->response_body);
    if (str->response_headers) {
        for (size_t i = 0; i < str->response_header_count; i++) {
            free(str->response_headers[i].name);
            free(str->response_headers[i].value);
        }
        free(str->response_headers);
    }
    free(str);
}

static void stream_unlink(AetherH2Session* s, int32_t id) {
    AetherH2Stream** cur = &s->streams;
    while (*cur) {
        if ((*cur)->stream_id == id) {
            AetherH2Stream* dead = *cur;
            *cur = dead->next;
            stream_free(dead);
            return;
        }
        cur = &(*cur)->next;
    }
}

/* Append `data` (of `len` bytes) to a heap buffer that grows by
 * doubling. Returns 0 on success, -1 on OOM. */
static int buf_append(char** buf, size_t* len, size_t* cap,
                      const char* data, size_t data_len) {
    if (*len + data_len + 1 > *cap) {
        size_t new_cap = (*cap == 0) ? 256 : *cap;
        while (new_cap < *len + data_len + 1) new_cap *= 2;
        char* nb = realloc(*buf, new_cap);
        if (!nb) return -1;
        *buf = nb;
        *cap = new_cap;
    }
    memcpy(*buf + *len, data, data_len);
    *len += data_len;
    (*buf)[*len] = '\0';
    return 0;
}

/* ------------------------------------------------------------------
 * nghttp2 callbacks
 * ------------------------------------------------------------------ */

static ssize_t ng_send_callback(nghttp2_session* ng,
                                const uint8_t* data, size_t len,
                                int flags, void* user_data) {
    (void)ng; (void)flags;
    AetherH2Session* s = (AetherH2Session*)user_data;
    /* Buffer everything; the drain function flushes on its own
     * schedule. Returning a short value here is interpreted as
     * NGHTTP2_ERR_WOULDBLOCK, which the library handles gracefully. */
    if (s->out_buf_len + len > s->out_buf_cap) {
        size_t new_cap = (s->out_buf_cap == 0) ? 4096 : s->out_buf_cap;
        while (new_cap < s->out_buf_len + len) new_cap *= 2;
        uint8_t* nb = realloc(s->out_buf, new_cap);
        if (!nb) return NGHTTP2_ERR_CALLBACK_FAILURE;
        s->out_buf = nb;
        s->out_buf_cap = new_cap;
    }
    memcpy(s->out_buf + s->out_buf_len, data, len);
    s->out_buf_len += len;
    return (ssize_t)len;
}

/* Called for every header name/value pair on a request stream.
 * We collect the four pseudo-headers separately (since they have
 * special meaning) and append everything else into a header block
 * that mimics the HTTP/1.1 wire format — that way middleware that
 * pattern-matches the raw header block (CORS, basic-auth, vhost)
 * keeps working without an h2-specific code path. */
static int ng_on_header_callback(nghttp2_session* ng,
                                 const nghttp2_frame* frame,
                                 const uint8_t* name, size_t namelen,
                                 const uint8_t* value, size_t valuelen,
                                 uint8_t flags, void* user_data) {
    (void)ng; (void)flags;
    if (frame->hd.type != NGHTTP2_HEADERS ||
        frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
        return 0;
    }
    AetherH2Session* s = (AetherH2Session*)user_data;
    AetherH2Stream* str = stream_get_or_create(s, frame->hd.stream_id);
    if (!str) return NGHTTP2_ERR_CALLBACK_FAILURE;

    /* Pseudo-headers begin with ':'. Normal headers are lowercase
     * per RFC 7540 §8.1.2. */
    if (namelen > 0 && name[0] == ':') {
        char** slot = NULL;
        if (namelen == 7 && memcmp(name, ":method", 7) == 0)        slot = &str->method;
        else if (namelen == 5 && memcmp(name, ":path", 5) == 0)     slot = &str->path;
        else if (namelen == 7 && memcmp(name, ":scheme", 7) == 0)   slot = &str->scheme;
        else if (namelen == 10 && memcmp(name, ":authority", 10) == 0) slot = &str->authority;
        if (slot) {
            free(*slot);
            *slot = malloc(valuelen + 1);
            if (!*slot) return NGHTTP2_ERR_CALLBACK_FAILURE;
            memcpy(*slot, value, valuelen);
            (*slot)[valuelen] = '\0';
        }
        return 0;
    }

    /* Regular header — append `Name: value\r\n` to the header block.
     * h2 names are lowercase by spec; we emit them as-is, which most
     * existing middleware handles via case-insensitive comparison. */
    if (buf_append(&str->header_block, &str->header_block_len,
                   &str->header_block_cap,
                   (const char*)name, namelen) != 0) return NGHTTP2_ERR_CALLBACK_FAILURE;
    if (buf_append(&str->header_block, &str->header_block_len,
                   &str->header_block_cap, ": ", 2) != 0) return NGHTTP2_ERR_CALLBACK_FAILURE;
    if (buf_append(&str->header_block, &str->header_block_len,
                   &str->header_block_cap,
                   (const char*)value, valuelen) != 0) return NGHTTP2_ERR_CALLBACK_FAILURE;
    if (buf_append(&str->header_block, &str->header_block_len,
                   &str->header_block_cap, "\r\n", 2) != 0) return NGHTTP2_ERR_CALLBACK_FAILURE;
    return 0;
}

/* DATA frame chunks land here; we just accumulate the body until
 * END_STREAM. The body buffer doubles, so amortised append cost is
 * constant; nghttp2 already enforces SETTINGS_MAX_FRAME_SIZE so a
 * single chunk can't blow the buffer arbitrarily. */
static int ng_on_data_chunk_recv(nghttp2_session* ng,
                                 uint8_t flags,
                                 int32_t stream_id,
                                 const uint8_t* data, size_t len,
                                 void* user_data) {
    (void)ng; (void)flags;
    AetherH2Session* s = (AetherH2Session*)user_data;
    AetherH2Stream* str = stream_lookup(s, stream_id);
    if (!str) return 0;
    if (buf_append(&str->body, &str->body_len, &str->body_cap,
                   (const char*)data, len) != 0) {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    return 0;
}

/* nghttp2_data_source_read_callback for response body streaming.
 * We hand back chunks from the stream's response_body buffer; when
 * the cursor catches up to the buffer length we set END_STREAM. */
static ssize_t ng_response_data_read_callback(nghttp2_session* ng,
                                              int32_t stream_id,
                                              uint8_t* buf, size_t length,
                                              uint32_t* data_flags,
                                              nghttp2_data_source* source,
                                              void* user_data) {
    (void)ng; (void)source;
    AetherH2Session* s = (AetherH2Session*)user_data;
    AetherH2Stream* str = stream_lookup(s, stream_id);
    if (!str) {
        *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        return 0;
    }
    size_t remaining = (str->response_body_len > str->response_body_cursor)
                        ? str->response_body_len - str->response_body_cursor
                        : 0;
    size_t n = (remaining < length) ? remaining : length;
    if (n > 0 && str->response_body) {
        memcpy(buf, str->response_body + str->response_body_cursor, n);
        str->response_body_cursor += n;
    }
    if (str->response_body_cursor >= str->response_body_len) {
        *data_flags |= NGHTTP2_DATA_FLAG_EOF;
    }
    return (ssize_t)n;
}

/* Forward decl — defined further below so it can call into the
 * existing route table without exposing the dispatch shape via a
 * header circular include. */
static void dispatch_stream(AetherH2Session* s, AetherH2Stream* str);

/* on_frame_recv — called once per fully-received frame. The frames
 * we care about are HEADERS-with-END_STREAM (request fully buffered,
 * dispatch to handler) and DATA-with-END_STREAM (same, when the
 * request has a body). */
static int ng_on_frame_recv_callback(nghttp2_session* ng,
                                     const nghttp2_frame* frame,
                                     void* user_data) {
    (void)ng;
    AetherH2Session* s = (AetherH2Session*)user_data;
    int end = (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) ? 1 : 0;
    int is_request_done = 0;

    if (frame->hd.type == NGHTTP2_HEADERS &&
        frame->headers.cat == NGHTTP2_HCAT_REQUEST &&
        end) {
        is_request_done = 1;
    } else if (frame->hd.type == NGHTTP2_DATA && end) {
        is_request_done = 1;
    }

    if (is_request_done) {
        AetherH2Stream* str = stream_lookup(s, frame->hd.stream_id);
        if (str && !str->request_done) {
            str->request_done = 1;
            dispatch_stream(s, str);
        }
    }
    return 0;
}

static int ng_on_stream_close(nghttp2_session* ng,
                              int32_t stream_id, uint32_t error_code,
                              void* user_data) {
    (void)ng; (void)error_code;
    AetherH2Session* s = (AetherH2Session*)user_data;
    stream_unlink(s, stream_id);
    return 0;
}

/* ------------------------------------------------------------------
 * Stream dispatch — bridge to the existing route table.
 *
 * We synthesise the same shape an HTTP/1.1 connection would have
 * delivered: an HttpRequest with method/path/headers/body, and an
 * HttpServerResponse the route handler can populate. After the
 * handler returns, we extract its status + body + headers and submit
 * them back through nghttp2.
 *
 * The shared dispatch helper lives in the main server file so the
 * HTTP/1.1 and HTTP/2 paths exercise the same middleware chain,
 * route lookup, transformer chain, and request hooks.
 * ------------------------------------------------------------------ */

/* Shared dispatcher: runs middleware → route lookup → handler →
 * response transformers → request hooks, mutating `res` in place.
 * Defined in aether_http_server.c. */
extern void http_server_dispatch_for_h2(HttpServer* server,
                                        HttpRequest* req,
                                        HttpServerResponse* res);

static char* dup_str(const char* src) {
    if (!src) return NULL;
    size_t n = strlen(src) + 1;
    char* d = malloc(n);
    if (!d) return NULL;
    memcpy(d, src, n);
    return d;
}

/* Build an HttpRequest from the h2 stream's collected pieces. The
 * struct's public fields (method, path, header_keys/values, body)
 * mirror what HTTP/1.1 would have produced — no h2-specific code
 * path needed downstream. We parse the simple `Name: value\r\n`
 * header block we accumulated in on_header_callback. */
static HttpRequest* request_from_stream(AetherH2Stream* str) {
    HttpRequest* req = calloc(1, sizeof(HttpRequest));
    if (!req) return NULL;
    req->method        = dup_str(str->method ? str->method : "GET");
    req->path          = dup_str(str->path   ? str->path   : "/");
    req->query_string  = dup_str("");
    req->http_version  = dup_str("HTTP/2");

    /* Count headers in the block to size the parallel arrays. */
    int n = 0;
    if (str->header_block) {
        const char* p = str->header_block;
        while (*p) {
            const char* eol = strstr(p, "\r\n");
            if (!eol) break;
            if (eol != p) n++;
            p = eol + 2;
        }
    }
    if (n > 0) {
        req->header_keys   = calloc((size_t)n, sizeof(char*));
        req->header_values = calloc((size_t)n, sizeof(char*));
        if (!req->header_keys || !req->header_values) {
            free(req->header_keys); free(req->header_values);
            free(req->method); free(req->path);
            free(req->query_string); free(req->http_version);
            free(req);
            return NULL;
        }
        const char* p = str->header_block;
        int i = 0;
        while (*p && i < n) {
            const char* eol = strstr(p, "\r\n");
            if (!eol) break;
            const char* colon = memchr(p, ':', (size_t)(eol - p));
            if (colon && colon > p) {
                size_t klen = (size_t)(colon - p);
                const char* vstart = colon + 1;
                while (vstart < eol && (*vstart == ' ' || *vstart == '\t')) vstart++;
                size_t vlen = (size_t)(eol - vstart);
                req->header_keys[i]   = malloc(klen + 1);
                req->header_values[i] = malloc(vlen + 1);
                if (!req->header_keys[i] || !req->header_values[i]) {
                    p = eol + 2; continue;
                }
                memcpy(req->header_keys[i], p, klen);
                req->header_keys[i][klen] = '\0';
                memcpy(req->header_values[i], vstart, vlen);
                req->header_values[i][vlen] = '\0';
                i++;
            }
            p = eol + 2;
        }
        req->header_count = i;
    }

    if (str->body && str->body_len > 0) {
        req->body = malloc(str->body_len + 1);
        if (req->body) {
            memcpy(req->body, str->body, str->body_len);
            req->body[str->body_len] = '\0';
            req->body_length = str->body_len;
        }
    }
    return req;
}

/* Forward decl — defined in aether_http_server.c. */
extern void http_request_free(HttpRequest* req);

static void dispatch_stream(AetherH2Session* s, AetherH2Stream* str) {
    if (str->response_submitted) return;

    HttpRequest* req = request_from_stream(str);
    if (!req) {
        nghttp2_submit_rst_stream(s->ng, NGHTTP2_FLAG_NONE,
                                  str->stream_id, NGHTTP2_INTERNAL_ERROR);
        return;
    }

    HttpServerResponse* res = http_response_create();
    if (!res) {
        http_request_free(req);
        nghttp2_submit_rst_stream(s->ng, NGHTTP2_FLAG_NONE,
                                  str->stream_id, NGHTTP2_INTERNAL_ERROR);
        return;
    }

    http_server_dispatch_for_h2(s->server, req, res);

    /* Snapshot response into the stream so the data-read callback
     * can stream it out independently of `res`'s lifetime. */
    int status = res->status_code;
    if (status < 100 || status > 999) status = 200;
    str->response_status = status;

    if (res->body && res->body_length > 0) {
        str->response_body = malloc(res->body_length);
        if (str->response_body) {
            memcpy(str->response_body, res->body, res->body_length);
            str->response_body_len = res->body_length;
        }
    }

    /* Status pseudo-header first, then user headers. nghttp2 needs
     * the status as a string; we use a small stack buffer. */
    char status_str[8];
    snprintf(status_str, sizeof(status_str), "%d", str->response_status);

    int hdr_count = res->header_count;
    if (hdr_count < 0) hdr_count = 0;
    if (hdr_count > 256) hdr_count = 256;  /* sanity */

    str->response_headers = calloc((size_t)(hdr_count + 1), sizeof(nghttp2_nv));
    if (!str->response_headers) {
        http_request_free(req);
        http_server_response_free(res);
        nghttp2_submit_rst_stream(s->ng, NGHTTP2_FLAG_NONE,
                                  str->stream_id, NGHTTP2_INTERNAL_ERROR);
        return;
    }

    /* :status pseudo-header. nghttp2 takes name/value as malloc'd
     * buffers we own; sizes are byte-count, NOT including a NUL. */
    str->response_headers[0].name      = (uint8_t*)dup_str(":status");
    str->response_headers[0].namelen   = 7;
    str->response_headers[0].value     = (uint8_t*)dup_str(status_str);
    str->response_headers[0].valuelen  = strlen(status_str);
    str->response_headers[0].flags     = NGHTTP2_NV_FLAG_NONE;

    int real_count = 1;
    for (int i = 0; i < hdr_count; i++) {
        const char* hname  = res->header_keys   ? res->header_keys[i]   : NULL;
        const char* hvalue = res->header_values ? res->header_values[i] : NULL;
        if (!hname) continue;
        /* h2 disallows Connection / Transfer-Encoding / Upgrade /
         * Keep-Alive / Proxy-Connection (RFC 7540 §8.1.2.2). Filter
         * them so handlers that emit Connection: close on HTTP/1.1
         * don't accidentally trip the peer. */
        if (strcasecmp(hname, "Connection") == 0 ||
            strcasecmp(hname, "Transfer-Encoding") == 0 ||
            strcasecmp(hname, "Upgrade") == 0 ||
            strcasecmp(hname, "Keep-Alive") == 0 ||
            strcasecmp(hname, "Proxy-Connection") == 0) {
            continue;
        }
        nghttp2_nv* nv = &str->response_headers[real_count];
        nv->name     = (uint8_t*)dup_str(hname);
        nv->namelen  = strlen(hname);
        nv->value    = (uint8_t*)dup_str(hvalue ? hvalue : "");
        nv->valuelen = hvalue ? strlen(hvalue) : 0;
        nv->flags    = NGHTTP2_NV_FLAG_NONE;
        real_count++;
    }
    str->response_header_count = (size_t)real_count;

    nghttp2_data_provider data_prd;
    data_prd.source.ptr = NULL;
    data_prd.read_callback = ng_response_data_read_callback;

    int rv = nghttp2_submit_response(s->ng, str->stream_id,
                                     str->response_headers,
                                     str->response_header_count,
                                     &data_prd);
    str->response_submitted = 1;

    http_request_free(req);
    http_server_response_free(res);

    if (rv != 0) {
        nghttp2_submit_rst_stream(s->ng, NGHTTP2_FLAG_NONE,
                                  str->stream_id, NGHTTP2_INTERNAL_ERROR);
    }
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

AetherH2Session* aether_h2_session_new(HttpServer* server,
                                       void* conn_userdata) {
    if (!server) return NULL;

    AetherH2Session* s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->server = server;
    s->conn_userdata = conn_userdata;

    nghttp2_session_callbacks* cbs = NULL;
    if (nghttp2_session_callbacks_new(&cbs) != 0) {
        free(s);
        return NULL;
    }
    nghttp2_session_callbacks_set_send_callback(cbs, ng_send_callback);
    nghttp2_session_callbacks_set_on_header_callback(cbs, ng_on_header_callback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(cbs, ng_on_data_chunk_recv);
    nghttp2_session_callbacks_set_on_frame_recv_callback(cbs, ng_on_frame_recv_callback);
    nghttp2_session_callbacks_set_on_stream_close_callback(cbs, ng_on_stream_close);

    int rv = nghttp2_session_server_new(&s->ng, cbs, s);
    nghttp2_session_callbacks_del(cbs);
    if (rv != 0) {
        free(s);
        return NULL;
    }
    return s;
}

void aether_h2_session_free(AetherH2Session* sess) {
    if (!sess) return;
    if (sess->ng) nghttp2_session_del(sess->ng);
    AetherH2Stream* str = sess->streams;
    while (str) {
        AetherH2Stream* nxt = str->next;
        stream_free(str);
        str = nxt;
    }
    free(sess->out_buf);
    free(sess);
}

int aether_h2_session_send_initial_settings(AetherH2Session* sess) {
    if (!sess || !sess->ng) return -1;
    nghttp2_settings_entry iv[2];
    int n = 0;
    iv[n].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
    iv[n++].value     = 1 << 20;  /* 1 MiB initial flow-control window */

    if (sess->server->h2_max_concurrent_streams > 0) {
        iv[n].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
        iv[n++].value     = (uint32_t)sess->server->h2_max_concurrent_streams;
    }

    return nghttp2_submit_settings(sess->ng, NGHTTP2_FLAG_NONE, iv, (size_t)n);
}

int aether_h2_session_feed(AetherH2Session* sess,
                           const uint8_t* data, size_t len) {
    if (!sess || !sess->ng) return -1;
    ssize_t consumed = nghttp2_session_mem_recv(sess->ng, data, len);
    if (consumed < 0) {
        sess->fatal = 1;
        return -1;
    }
    /* nghttp2_session_mem_recv consumes the whole input buffer or
     * returns the count actually parsed; in non-error paths it
     * always equals len. */
    return (int)consumed;
}

int aether_h2_session_drain(AetherH2Session* sess,
                            AetherH2WriteFn write_fn) {
    if (!sess || !sess->ng || !write_fn) return -1;

    /* Pump nghttp2's send loop — this calls our ng_send_callback
     * to fill out_buf with serialised frames. */
    int rv = nghttp2_session_send(sess->ng);
    if (rv != 0) {
        sess->fatal = 1;
        return -1;
    }

    /* Flush whatever's queued. The wire callback may take a partial
     * write; we leave the rest for the next drain. */
    if (sess->out_buf_len > 0) {
        int written = write_fn(sess->conn_userdata,
                               sess->out_buf, sess->out_buf_len);
        if (written < 0) return -1;
        if ((size_t)written < sess->out_buf_len) {
            memmove(sess->out_buf, sess->out_buf + written,
                    sess->out_buf_len - written);
            sess->out_buf_len -= (size_t)written;
        } else {
            sess->out_buf_len = 0;
        }
    }

    if (sess->fatal) return -1;
    if (aether_h2_session_want_close(sess)) return 1;
    return 0;
}

int aether_h2_session_want_close(AetherH2Session* sess) {
    if (!sess || !sess->ng) return 1;
    if (sess->fatal) return 1;
    int want_read  = nghttp2_session_want_read(sess->ng);
    int want_write = nghttp2_session_want_write(sess->ng);
    return (!want_read && !want_write) ? 1 : 0;
}

/* ------------------------------------------------------------------
 * h2c (cleartext) upgrade — RFC 7540 §3.2.
 *
 * The HTTP/1.1 parser delivers the request that asked for the
 * upgrade plus the base64url-encoded HTTP2-Settings header value.
 * We:
 *   1. Decode the SETTINGS payload into the session's peer-side
 *      pre-loaded state (nghttp2_session_upgrade2 does this).
 *   2. Synthesise stream 1 from the original HTTP/1.1 request so
 *      the route handler sees a normal h2 stream.
 *   3. Write 101 Switching Protocols on the wire (caller responsible
 *      via the supplied callback before any h2 frames flow).
 *
 * The 101 emission is the caller's job — we just hand back the new
 * session pre-loaded with the synthesised stream. The caller writes
 * the 101 line before the first drain.
 * ------------------------------------------------------------------ */

/* base64url decode (no padding) into a freshly-malloc'd buffer.
 * Returns the buffer (NUL-terminated for safety) or NULL on bad
 * input. *out_len receives the decoded byte count. */
static uint8_t* b64url_decode(const char* in, size_t in_len, size_t* out_len) {
    static const int8_t T[256] = {
        ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
        ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
        ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
        ['Y']=24,['Z']=25,
        ['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,['g']=32,['h']=33,
        ['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,['o']=40,['p']=41,
        ['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,['w']=48,['x']=49,
        ['y']=50,['z']=51,
        ['0']=52,['1']=53,['2']=54,['3']=55,['4']=56,['5']=57,['6']=58,['7']=59,
        ['8']=60,['9']=61,
        ['-']=62,['_']=63,
    };
    /* Initialise the rest to -1. The static initialiser handles the
     * named slots; everything else is zero, which we'd misread as
     * 'A'. Walk once and explicitly mark non-base64url bytes. */
    static int initialised = 0;
    static int8_t lookup[256];
    if (!initialised) {
        for (int i = 0; i < 256; i++) lookup[i] = -1;
        for (int i = 0; i < 256; i++) if (T[i] != 0 || i == 'A') lookup[i] = T[i];
        lookup['A'] = 0;
        initialised = 1;
    }

    if (in_len == 0) {
        *out_len = 0;
        uint8_t* zero = malloc(1);
        if (zero) zero[0] = '\0';
        return zero;
    }
    /* Output upper bound: ceil(in_len * 3 / 4). */
    size_t cap = (in_len * 3) / 4 + 4;
    uint8_t* out = malloc(cap);
    if (!out) return NULL;
    size_t op = 0;
    int v = 0, bits = 0;
    for (size_t i = 0; i < in_len; i++) {
        int8_t c = lookup[(unsigned char)in[i]];
        if (c < 0) { free(out); return NULL; }
        v = (v << 6) | c;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[op++] = (uint8_t)((v >> bits) & 0xff);
        }
    }
    *out_len = op;
    return out;
}

AetherH2Session* aether_h2_session_from_h2c_upgrade(
    HttpServer* server,
    void* conn_userdata,
    const char* request_method,
    const char* request_path,
    const char* request_headers,
    const char* http2_settings_b64) {
    if (!server || !request_method || !request_path) return NULL;

    AetherH2Session* sess = aether_h2_session_new(server, conn_userdata);
    if (!sess) return NULL;

    /* Decode SETTINGS payload (base64url, no padding per RFC 7540). */
    size_t settings_len = 0;
    uint8_t* settings_buf = NULL;
    if (http2_settings_b64) {
        settings_buf = b64url_decode(http2_settings_b64,
                                     strlen(http2_settings_b64),
                                     &settings_len);
        if (!settings_buf) {
            aether_h2_session_free(sess);
            return NULL;
        }
    }

    /* Hand the peer SETTINGS to nghttp2 and synthesise stream 1. */
    int rv = nghttp2_session_upgrade2(sess->ng,
                                      settings_buf, settings_len,
                                      strcmp(request_method, "HEAD") == 0 ? 1 : 0,
                                      NULL);
    free(settings_buf);
    if (rv != 0) {
        aether_h2_session_free(sess);
        return NULL;
    }

    /* Pre-populate stream 1 with the original HTTP/1.1 request so
     * the dispatcher sees the same shape an h2 client would have
     * sent. */
    AetherH2Stream* str = stream_get_or_create(sess, 1);
    if (!str) {
        aether_h2_session_free(sess);
        return NULL;
    }
    str->method = dup_str(request_method);
    str->path   = dup_str(request_path);
    if (request_headers) {
        size_t hlen = strlen(request_headers);
        buf_append(&str->header_block, &str->header_block_len,
                   &str->header_block_cap, request_headers, hlen);
    }
    str->request_done = 1;
    dispatch_stream(sess, str);

    /* Send the SETTINGS frame the server must emit first. */
    aether_h2_session_send_initial_settings(sess);
    return sess;
}

#else  /* !AETHER_HAS_NGHTTP2 — no-op stubs */

AetherH2Session* aether_h2_session_new(struct HttpServer* server,
                                       void* conn_userdata) {
    (void)server; (void)conn_userdata; return NULL;
}
void aether_h2_session_free(AetherH2Session* sess) { (void)sess; }
int aether_h2_session_send_initial_settings(AetherH2Session* sess) {
    (void)sess; return -1;
}
int aether_h2_session_feed(AetherH2Session* sess,
                           const uint8_t* data, size_t len) {
    (void)sess; (void)data; (void)len; return -1;
}
int aether_h2_session_drain(AetherH2Session* sess,
                            AetherH2WriteFn write_fn) {
    (void)sess; (void)write_fn; return -1;
}
int aether_h2_session_want_close(AetherH2Session* sess) {
    (void)sess; return 1;
}
AetherH2Session* aether_h2_session_from_h2c_upgrade(
    struct HttpServer* server,
    void* conn_userdata,
    const char* request_method,
    const char* request_path,
    const char* request_headers,
    const char* http2_settings_b64) {
    (void)server; (void)conn_userdata; (void)request_method;
    (void)request_path; (void)request_headers; (void)http2_settings_b64;
    return NULL;
}

#endif  /* AETHER_HAS_NGHTTP2 */
