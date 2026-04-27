#include "aether_http_server.h"
#include "../../runtime/config/aether_optimization_config.h"

#if !AETHER_HAS_NETWORKING
// Stubs when networking is unavailable
HttpServer* http_server_create(int p) { (void)p; return NULL; }
int http_server_bind_raw(HttpServer* s, const char* h, int p) { (void)s; (void)h; (void)p; return -1; }
int http_server_start_raw(HttpServer* s) { (void)s; return -1; }
void http_server_stop(HttpServer* s) { (void)s; }
void http_server_free(HttpServer* s) { (void)s; }
const char* http_server_set_tls_raw(HttpServer* s, const char* c, const char* k) {
    (void)s; (void)c; (void)k; return "TLS unavailable: networking not built in";
}
const char* http_server_set_keepalive_raw(HttpServer* s, int e, int m, int i) {
    (void)s; (void)e; (void)m; (void)i; return "keep-alive unavailable: networking not built in";
}
void http_server_drain_connection(HttpServer* s, int fd) { (void)s; (void)fd; }
const char* http_server_shutdown_graceful_raw(HttpServer* s, int t) { (void)s; (void)t; return ""; }
void http_server_set_on_start(HttpServer* s, HttpLifecycleHook h, void* u) { (void)s; (void)h; (void)u; }
void http_server_set_on_stop (HttpServer* s, HttpLifecycleHook h, void* u) { (void)s; (void)h; (void)u; }
const char* http_server_set_health_probes_raw(HttpServer* s, const char* lp, const char* rp,
                                              HttpReadyCheck rc, void* ud) {
    (void)s; (void)lp; (void)rp; (void)rc; (void)ud; return "";
}
void http_server_use_request_hook(HttpServer* s, HttpRequestHook h, void* u) {
    (void)s; (void)h; (void)u;
}
const char* http_server_set_access_log_raw(HttpServer* s, const char* f, const char* p) {
    (void)s; (void)f; (void)p; return "";
}
const char* http_server_set_metrics_raw(HttpServer* s, const char* e) { (void)s; (void)e; return ""; }
void http_server_sse(HttpServer* s, const char* p, HttpSseHandler h, void* u) {
    (void)s; (void)p; (void)h; (void)u;
}
int http_sse_send_event(HttpSseConn* c, const char* n, const char* d) {
    (void)c; (void)n; (void)d; return -1;
}
int http_sse_send_event_id(HttpSseConn* c, const char* n, const char* d, const char* i) {
    (void)c; (void)n; (void)d; (void)i; return -1;
}
void http_sse_close(HttpSseConn* c) { (void)c; }
void http_server_add_route(HttpServer* s, const char* m, const char* p, HttpHandler h, void* u) { (void)s; (void)m; (void)p; (void)h; (void)u; }
void http_server_get(HttpServer* s, const char* p, HttpHandler h, void* u) { (void)s; (void)p; (void)h; (void)u; }
void http_server_post(HttpServer* s, const char* p, HttpHandler h, void* u) { (void)s; (void)p; (void)h; (void)u; }
void http_server_put(HttpServer* s, const char* p, HttpHandler h, void* u) { (void)s; (void)p; (void)h; (void)u; }
void http_server_delete(HttpServer* s, const char* p, HttpHandler h, void* u) { (void)s; (void)p; (void)h; (void)u; }
void http_server_use_middleware(HttpServer* s, HttpMiddleware m, void* u) { (void)s; (void)m; (void)u; }
void http_server_use_response_transformer(HttpServer* s, HttpResponseTransformer x, void* u) {
    (void)s; (void)x; (void)u;
}
HttpRequest* http_parse_request(const char* r) { (void)r; return NULL; }
const char* http_get_header(HttpRequest* r, const char* k) { (void)r; (void)k; return NULL; }
const char* http_get_query_param(HttpRequest* r, const char* k) { (void)r; (void)k; return NULL; }
const char* http_get_path_param(HttpRequest* r, const char* k) { (void)r; (void)k; return NULL; }
void http_request_free(HttpRequest* r) { (void)r; }
HttpServerResponse* http_response_create() { return NULL; }
void http_response_set_status(HttpServerResponse* r, int c) { (void)r; (void)c; }
void http_response_set_header(HttpServerResponse* r, const char* k, const char* v) { (void)r; (void)k; (void)v; }
void http_response_set_body(HttpServerResponse* r, const char* b) { (void)r; (void)b; }
void http_response_json(HttpServerResponse* r, const char* j) { (void)r; (void)j; }
char* http_response_serialize(HttpServerResponse* r) { (void)r; return NULL; }
void http_server_response_free(HttpServerResponse* r) { (void)r; }
int http_route_matches(const char* p, const char* u, HttpRequest* r) { (void)p; (void)u; (void)r; return 0; }
const char* http_status_text(int c) { (void)c; return "Unknown"; }
const char* http_mime_type(const char* p) { (void)p; return "application/octet-stream"; }
void http_serve_file(HttpServerResponse* r, const char* f) { (void)r; (void)f; }
void http_serve_static(HttpRequest* r, HttpServerResponse* s, void* d) { (void)r; (void)s; (void)d; }
void http_server_set_actor_handler(HttpServer* s, void (*sf)(void*), void (*snf)(void*, void*, size_t), void* (*spf)(int, void (*)(void*), size_t), void (*rf)(void*)) { (void)s; (void)sf; (void)snf; (void)spf; (void)rf; }
const char* http_request_method(HttpRequest* r) { (void)r; return ""; }
const char* http_request_path(HttpRequest* r) { (void)r; return ""; }
const char* http_request_body(HttpRequest* r) { (void)r; return ""; }
const char* http_request_query(HttpRequest* r) { (void)r; return ""; }
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../runtime/utils/aether_thread.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
    #define close closesocket
    typedef int socklen_t;
    #ifndef strcasecmp
        #define strcasecmp _stricmp
    #endif
    #ifndef strdup
        #define strdup _strdup
    #endif
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <limits.h>
    #include <poll.h>
    #include <errno.h>
    // I/O polling is handled by aether_io_poller (included via multicore_scheduler.h)
#endif

// -----------------------------------------------------------------------------
// Server-side TLS (#260 Tier 0)
// -----------------------------------------------------------------------------
// Connection-level transport abstraction so the rest of the server doesn't
// need to know whether each fd is plain or TLS-wrapped. plain_recv/plain_send
// when ssl == NULL; SSL_read/SSL_write when ssl is non-NULL. SSL_accept is
// driven once per accepted connection at the top of handle_client_connection
// before the HTTP parse begins.
//
// Built only when the project links OpenSSL. AETHER_HAS_OPENSSL is defined
// by Makefile when pkg-config finds the library — same gate std.cryptography
// and the http client (TLS) already use.
#ifdef AETHER_HAS_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

/* Per-connection read buffer. Persists across requests on a
 * keep-alive connection so that pipelined bytes (the start of
 * request N+1 already received while reading request N) are not
 * lost between handle_one_request calls. Without this, the
 * single-request implementation's "read up to \r\n\r\n then free"
 * pattern silently drops anything past the first request's body
 * boundary in the same recv. */
#define HTTP_CONN_BUF_CAP (16 * 1024)
typedef struct {
    int fd;
#ifdef AETHER_HAS_OPENSSL
    SSL* ssl;     /* non-NULL when this connection is TLS-wrapped */
#else
    void* ssl;    /* layout-stable placeholder for the no-TLS build */
#endif
    /* Read-side ring: [read_pos, write_pos) holds bytes already
     * received but not yet consumed by a request parse. Bytes
     * before read_pos are spent and may be discarded by compaction;
     * bytes after write_pos are unallocated. */
    char* buf;
    int   buf_cap;
    int   read_pos;
    int   write_pos;
} HttpConn;

static int conn_recv(HttpConn* c, void* buf, int len) {
#ifdef AETHER_HAS_OPENSSL
    if (c->ssl) {
        int n = SSL_read(c->ssl, buf, len);
        if (n <= 0) return -1;
        return n;
    }
#endif
    return (int)recv(c->fd, buf, len, 0);
}

static int conn_send(HttpConn* c, const void* buf, int len) {
#ifdef AETHER_HAS_OPENSSL
    if (c->ssl) {
        int n = SSL_write(c->ssl, buf, len);
        if (n <= 0) return -1;
        return n;
    }
#endif
    return (int)send(c->fd, buf, len, 0);
}

static void conn_close(HttpConn* c) {
#ifdef AETHER_HAS_OPENSSL
    if (c->ssl) {
        /* Best-effort graceful shutdown — ignore the result; we're
         * about to close the fd anyway and a half-closed peer is
         * not a server-side problem. */
        SSL_shutdown(c->ssl);
        SSL_free(c->ssl);
        c->ssl = NULL;
    }
#endif
    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
    if (c->buf) {
        free(c->buf);
        c->buf = NULL;
        c->buf_cap = 0;
        c->read_pos = 0;
        c->write_pos = 0;
    }
}

/* Compact the read buffer: shift unconsumed bytes [read_pos,
 * write_pos) down to position 0 so the tail is available for a
 * fresh recv. Cheap when read_pos is large relative to write_pos. */
static void conn_buf_compact(HttpConn* c) {
    if (c->read_pos == 0) return;
    int unread = c->write_pos - c->read_pos;
    if (unread > 0) {
        memmove(c->buf, c->buf + c->read_pos, (size_t)unread);
    }
    c->write_pos = unread;
    c->read_pos = 0;
}

/* Grow the read buffer to at least the given capacity. */
static int conn_buf_ensure(HttpConn* c, int needed) {
    if (c->buf_cap >= needed) return 0;
    int new_cap = c->buf_cap > 0 ? c->buf_cap : HTTP_CONN_BUF_CAP;
    while (new_cap < needed) new_cap *= 2;
    char* nb = realloc(c->buf, (size_t)new_cap);
    if (!nb) return -1;
    c->buf = nb;
    c->buf_cap = new_cap;
    return 0;
}

#ifdef AETHER_HAS_OPENSSL
/* TLS-wrap an accepted fd. Returns 0 on success (conn->ssl set), -1 on
 * handshake failure (caller should close conn->fd and discard). */
static int conn_tls_accept(HttpConn* conn, SSL_CTX* ctx) {
    SSL* ssl = SSL_new(ctx);
    if (!ssl) return -1;
    if (SSL_set_fd(ssl, conn->fd) != 1) {
        SSL_free(ssl);
        return -1;
    }
    int r = SSL_accept(ssl);
    if (r <= 0) {
        /* Handshake failed — peer probably spoke plain HTTP at a TLS
         * port, or sent an unsupported cipher. Drain OpenSSL's error
         * queue so the next handshake on this thread starts clean. */
        ERR_clear_error();
        SSL_free(ssl);
        return -1;
    }
    conn->ssl = ssl;
    return 0;
}

/* OpenSSL global init — called once on first http_server_set_tls. The
 * defaults (TLS 1.2 minimum, all known ciphers) match the existing
 * client-side context in std/net/aether_http.c. */
static void server_openssl_init_once(void) {
    static int done = 0;
    if (done) return;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    done = 1;
}
#endif

// Portable case-insensitive substring search (strcasestr is a GNU extension)
static const char* http_strcasestr(const char* haystack, const char* needle) {
    if (!needle || !*needle) return haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            size_t i;
            for (i = 1; i < nlen; i++) {
                if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
                    break;
            }
            if (i == nlen) return haystack;
        }
    }
    return NULL;
}

static int http_server_initialized = 0;

static void http_server_init() {
    if (http_server_initialized) return;
    #ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif
    http_server_initialized = 1;
}

HttpServer* http_server_create(int port) {
    http_server_init();

    HttpServer* server = (HttpServer*)calloc(1, sizeof(HttpServer));
    server->port = port;
    server->host = strdup("0.0.0.0");
    server->socket_fd = -1;
    server->is_running = 0;
    server->routes = NULL;
    server->middleware_chain = NULL;
    server->max_connections = 1000;
    server->keep_alive_timeout = 30;
    server->scheduler = NULL;
    server->handler_actor = NULL;
    server->send_fn = NULL;
    server->spawn_fn = NULL;
    server->release_fn = NULL;
    server->step_fn = NULL;
    server->accept_poller.fd = -1;
    server->accept_poller.backend_data = NULL;
    server->multi_accept = 0;
    server->accept_thread_count = 0;
    server->accept_threads = NULL;
    server->accept_listen_fds = NULL;
    server->accept_pollers = NULL;
    server->tls_enabled = 0;
    server->tls_ctx = NULL;
    server->keep_alive_enabled = 0;
    server->keep_alive_max = 0;
    server->keep_alive_idle_ms = 0;
    server->response_transformer_chain = NULL;
    server->on_start = NULL;
    server->on_start_user_data = NULL;
    server->on_stop = NULL;
    server->on_stop_user_data = NULL;
    server->ready_check = NULL;
    server->ready_check_user_data = NULL;
    atomic_init(&server->inflight_connections, 0);
    server->request_hook_chain = NULL;
    server->sse_routes = NULL;

    return server;
}

const char* http_server_set_keepalive_raw(HttpServer* server,
                                          int enabled,
                                          int max_requests,
                                          int idle_ms) {
    if (!server) return "server is null";
    server->keep_alive_enabled = enabled ? 1 : 0;
    server->keep_alive_max = max_requests < 0 ? 0 : max_requests;
    server->keep_alive_idle_ms = idle_ms < 0 ? 0 : idle_ms;
    return "";
}

// =================================================================
// #260 Tier 3: graceful shutdown + lifecycle hooks + health probes
// =================================================================

void http_server_set_on_start(HttpServer* server, HttpLifecycleHook hook, void* user_data) {
    if (!server) return;
    server->on_start = hook;
    server->on_start_user_data = user_data;
}

void http_server_set_on_stop(HttpServer* server, HttpLifecycleHook hook, void* user_data) {
    if (!server) return;
    server->on_stop = hook;
    server->on_stop_user_data = user_data;
}

const char* http_server_shutdown_graceful_raw(HttpServer* server, int timeout_ms) {
    if (!server) return "server is null";

    /* Stop accepting new connections — this unblocks the accept
     * loop's poll() and lets it exit. The thread-pool destructor
     * (or actor scheduler shutdown) handles in-flight connections;
     * we just wait for the inflight counter to drain. */
    http_server_stop(server);

    if (timeout_ms <= 0) timeout_ms = 5000;

    /* Spin-wait with an exponential-ish back-off, capped at 50ms.
     * The connection counter is updated atomically by every
     * http_server_drain_connection invocation (Tier 0 / Phase C3
     * helper); when it reaches zero, all in-flight responses have
     * completed naturally. */
    int waited = 0;
    int sleep_us = 1000;  /* 1 ms */
    while (waited < timeout_ms) {
        int n = atomic_load(&server->inflight_connections);
        if (n <= 0) return "";
#ifdef _WIN32
        Sleep(sleep_us / 1000 ? sleep_us / 1000 : 1);
#else
        struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)sleep_us * 1000L };
        nanosleep(&ts, NULL);
#endif
        waited += sleep_us / 1000;
        if (sleep_us < 50000) sleep_us *= 2;
    }
    return "timeout";
}

/* Shared stateless handler for /healthz — always 200. */
static void health_live_handler(HttpRequest* req, HttpServerResponse* res, void* ud) {
    (void)req; (void)ud;
    http_response_set_status(res, 200);
    http_response_set_header(res, "Content-Type", "text/plain");
    http_response_set_body(res, "ok");
}

/* Shared stateless handler for /readyz — calls server->ready_check
 * (passed via the route's user_data slot — we stash the
 * server pointer there). */
static void health_ready_handler(HttpRequest* req, HttpServerResponse* res, void* ud) {
    (void)req;
    HttpServer* server = (HttpServer*)ud;
    int ok = 1;
    if (server && server->ready_check) {
        ok = server->ready_check(server->ready_check_user_data);
    }
    if (ok) {
        http_response_set_status(res, 200);
        http_response_set_header(res, "Content-Type", "text/plain");
        http_response_set_body(res, "ready");
    } else {
        http_response_set_status(res, 503);
        http_response_set_header(res, "Content-Type", "text/plain");
        http_response_set_body(res, "not ready");
    }
}

const char* http_server_set_health_probes_raw(HttpServer* server,
                                              const char* live_path,
                                              const char* ready_path,
                                              HttpReadyCheck ready_check,
                                              void* user_data) {
    if (!server) return "server is null";
    server->ready_check = ready_check;
    server->ready_check_user_data = user_data;
    if (live_path && *live_path) {
        http_server_get(server, live_path, health_live_handler, NULL);
    }
    if (ready_path && *ready_path) {
        http_server_get(server, ready_path, health_ready_handler, server);
    }
    return "";
}

// =================================================================
// #260 Tier 3 / F1: access logger
// =================================================================

typedef struct {
    char* format;       /* "combined" or "json" */
    FILE* fp;           /* not closed by us — that's set up below */
    int   own_fp;       /* 1 if we opened it (need to fclose); 0 for stderr */
} AccessLogState;

static void access_log_hook(HttpRequest* req, HttpServerResponse* res,
                            long duration_us, void* user_data) {
    AccessLogState* st = (AccessLogState*)user_data;
    if (!st || !st->fp) return;

    /* Common fields. Avoid NULL-deref by substituting "-" the way
     * NCSA log files traditionally do. */
    const char* method = req && req->method ? req->method : "-";
    const char* path   = req && req->path   ? req->path   : "-";
    const char* version = req && req->http_version ? req->http_version : "HTTP/1.1";
    const char* user_agent = req ? http_get_header(req, "User-Agent") : NULL;
    const char* referer    = req ? http_get_header(req, "Referer")    : NULL;
    int  status = res ? res->status_code : 0;
    long body_len = res ? (long)res->body_length : 0;

    /* RFC 1123 date for combined; ISO-8601 for json. Both via the
     * same struct tm pull. */
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif

    if (strcmp(st->format, "json") == 0) {
        char ts[32];
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tmv);
        fprintf(st->fp,
                "{\"ts\":\"%s\",\"method\":\"%s\",\"path\":\"%s\","
                "\"status\":%d,\"bytes\":%ld,\"dur_us\":%ld,"
                "\"ua\":\"%s\",\"ref\":\"%s\"}\n",
                ts, method, path, status, body_len, duration_us,
                user_agent ? user_agent : "",
                referer    ? referer    : "");
    } else {
        /* Combined: <ip> - - [DD/MMM/YYYY:HH:MM:SS +0000] "METHOD PATH HTTP/X.Y" status bytes "REF" "UA" */
        char ts[64];
        strftime(ts, sizeof(ts), "%d/%b/%Y:%H:%M:%S +0000", &tmv);
        const char* ip = req ? http_get_header(req, "X-Forwarded-For") : NULL;
        if (!ip) ip = "-";
        fprintf(st->fp,
                "%s - - [%s] \"%s %s %s\" %d %ld \"%s\" \"%s\"\n",
                ip, ts, method, path, version, status, body_len,
                referer    ? referer    : "-",
                user_agent ? user_agent : "-");
    }
    fflush(st->fp);
}

const char* http_server_set_access_log_raw(HttpServer* server,
                                           const char* format,
                                           const char* output_path) {
    if (!server) return "server is null";
    if (!format || !*format) return "";  /* disabled */

    int fmt_ok = strcmp(format, "combined") == 0 ||
                 strcmp(format, "json")     == 0;
    if (!fmt_ok) return "format must be \"combined\" or \"json\"";

    AccessLogState* st = (AccessLogState*)calloc(1, sizeof(AccessLogState));
    if (!st) return "out of memory";
    st->format = strdup(format);

    if (!output_path || !*output_path || strcmp(output_path, "-") == 0) {
        st->fp = stderr;
        st->own_fp = 0;
    } else {
        st->fp = fopen(output_path, "ab");
        if (!st->fp) {
            free(st->format);
            free(st);
            return "cannot open access-log output_path for append";
        }
        st->own_fp = 1;
    }

    http_server_use_request_hook(server, access_log_hook, st);
    return "";
}

// =================================================================
// #260 Tier 3 / F2: per-route metrics + Prometheus exposition
// =================================================================

typedef struct MetricsRoute {
    char* method;
    char* path_pattern;
    _Atomic long total_requests;
    _Atomic long total_errors;     /* status >= 500 */
    _Atomic long total_4xx;
    _Atomic long sum_duration_us;
    _Atomic long max_duration_us;
    /* Histogram buckets in microseconds. */
    _Atomic long bucket_le_5ms;
    _Atomic long bucket_le_25ms;
    _Atomic long bucket_le_100ms;
    _Atomic long bucket_le_500ms;
    _Atomic long bucket_le_2s;
    _Atomic long bucket_le_10s;
    struct MetricsRoute* next;
} MetricsRoute;

typedef struct {
    pthread_mutex_t lock;
    MetricsRoute* head;
} MetricsState;

static MetricsRoute* metrics_route_for(MetricsState* st,
                                       const char* method,
                                       const char* pattern) {
    MetricsRoute* r = st->head;
    while (r) {
        if (strcmp(r->method, method) == 0 &&
            strcmp(r->path_pattern, pattern) == 0) return r;
        r = r->next;
    }
    return NULL;
}

static void metrics_hook(HttpRequest* req, HttpServerResponse* res,
                         long duration_us, void* user_data) {
    MetricsState* st = (MetricsState*)user_data;
    if (!st || !req || !res) return;

    /* Look up by exact method+path. Production deployments would
     * usually want path-pattern bucketing (e.g. /users/:id collapses
     * across IDs), but the existing route table doesn't expose the
     * matched pattern back to the dispatch path. v1 buckets by
     * literal request path; the user can collapse via labels in a
     * follow-up. */
    const char* method = req->method ? req->method : "-";
    const char* pattern = req->path ? req->path : "-";

    pthread_mutex_lock(&st->lock);
    MetricsRoute* r = metrics_route_for(st, method, pattern);
    if (!r) {
        r = (MetricsRoute*)calloc(1, sizeof(MetricsRoute));
        if (!r) { pthread_mutex_unlock(&st->lock); return; }
        r->method = strdup(method);
        r->path_pattern = strdup(pattern);
        r->next = st->head;
        st->head = r;
    }
    pthread_mutex_unlock(&st->lock);

    atomic_fetch_add(&r->total_requests, 1);
    if (res->status_code >= 500) atomic_fetch_add(&r->total_errors, 1);
    else if (res->status_code >= 400) atomic_fetch_add(&r->total_4xx, 1);
    atomic_fetch_add(&r->sum_duration_us, duration_us);

    /* Update max via CAS. */
    long prev = atomic_load(&r->max_duration_us);
    while (duration_us > prev &&
           !atomic_compare_exchange_weak(&r->max_duration_us, &prev, duration_us)) {
        /* prev is updated by CAS on failure; re-test loop condition. */
    }

    /* Cumulative histogram (Prometheus convention: each bucket
     * counts events <= upper bound). */
    if (duration_us <= 5000)    atomic_fetch_add(&r->bucket_le_5ms,    1);
    if (duration_us <= 25000)   atomic_fetch_add(&r->bucket_le_25ms,   1);
    if (duration_us <= 100000)  atomic_fetch_add(&r->bucket_le_100ms,  1);
    if (duration_us <= 500000)  atomic_fetch_add(&r->bucket_le_500ms,  1);
    if (duration_us <= 2000000) atomic_fetch_add(&r->bucket_le_2s,     1);
    if (duration_us <= 10000000)atomic_fetch_add(&r->bucket_le_10s,    1);
}

/* Escape a pattern for use as a Prometheus label value. */
static void metrics_escape(char* dst, size_t cap, const char* src) {
    size_t n = 0;
    for (; *src && n + 2 < cap; src++) {
        if (*src == '"' || *src == '\\') {
            if (n + 2 >= cap) break;
            dst[n++] = '\\';
            dst[n++] = *src;
        } else if (*src == '\n') {
            if (n + 2 >= cap) break;
            dst[n++] = '\\';
            dst[n++] = 'n';
        } else {
            dst[n++] = *src;
        }
    }
    dst[n] = '\0';
}

/* Handler for the configured /metrics endpoint. Walks the
 * per-route counters and emits Prometheus text format. */
static void metrics_handler(HttpRequest* req, HttpServerResponse* res, void* user_data) {
    (void)req;
    MetricsState* st = (MetricsState*)user_data;
    if (!st) {
        http_response_set_status(res, 500);
        http_response_set_body(res, "metrics: state missing");
        return;
    }

    /* Build into a heap buffer; size grows with route count. 64KB
     * suffices for hundreds of routes. */
    size_t cap = 64 * 1024;
    char* buf = (char*)malloc(cap);
    if (!buf) {
        http_response_set_status(res, 500);
        http_response_set_body(res, "metrics: oom");
        return;
    }
    size_t off = 0;
    off += snprintf(buf + off, cap - off,
        "# TYPE aether_http_requests_total counter\n"
        "# TYPE aether_http_errors_total counter\n"
        "# TYPE aether_http_4xx_total counter\n"
        "# TYPE aether_http_request_duration_seconds histogram\n");

    pthread_mutex_lock(&st->lock);
    for (MetricsRoute* r = st->head; r; r = r->next) {
        char m[64], p[256];
        metrics_escape(m, sizeof(m), r->method);
        metrics_escape(p, sizeof(p), r->path_pattern);
        long total = atomic_load(&r->total_requests);
        long errs  = atomic_load(&r->total_errors);
        long c4xx  = atomic_load(&r->total_4xx);
        long sum   = atomic_load(&r->sum_duration_us);
        long b5    = atomic_load(&r->bucket_le_5ms);
        long b25   = atomic_load(&r->bucket_le_25ms);
        long b100  = atomic_load(&r->bucket_le_100ms);
        long b500  = atomic_load(&r->bucket_le_500ms);
        long b2    = atomic_load(&r->bucket_le_2s);
        long b10   = atomic_load(&r->bucket_le_10s);
        int wrote = snprintf(buf + off, cap - off,
            "aether_http_requests_total{method=\"%s\",path=\"%s\"} %ld\n"
            "aether_http_errors_total{method=\"%s\",path=\"%s\"} %ld\n"
            "aether_http_4xx_total{method=\"%s\",path=\"%s\"} %ld\n"
            "aether_http_request_duration_seconds_bucket{method=\"%s\",path=\"%s\",le=\"0.005\"} %ld\n"
            "aether_http_request_duration_seconds_bucket{method=\"%s\",path=\"%s\",le=\"0.025\"} %ld\n"
            "aether_http_request_duration_seconds_bucket{method=\"%s\",path=\"%s\",le=\"0.1\"} %ld\n"
            "aether_http_request_duration_seconds_bucket{method=\"%s\",path=\"%s\",le=\"0.5\"} %ld\n"
            "aether_http_request_duration_seconds_bucket{method=\"%s\",path=\"%s\",le=\"2\"} %ld\n"
            "aether_http_request_duration_seconds_bucket{method=\"%s\",path=\"%s\",le=\"10\"} %ld\n"
            "aether_http_request_duration_seconds_bucket{method=\"%s\",path=\"%s\",le=\"+Inf\"} %ld\n"
            "aether_http_request_duration_seconds_sum{method=\"%s\",path=\"%s\"} %.6f\n"
            "aether_http_request_duration_seconds_count{method=\"%s\",path=\"%s\"} %ld\n",
            m, p, total,
            m, p, errs,
            m, p, c4xx,
            m, p, b5, m, p, b25, m, p, b100, m, p, b500,
            m, p, b2, m, p, b10, m, p, total,
            m, p, (double)sum / 1e6,
            m, p, total);
        if (wrote < 0 || (size_t)wrote >= cap - off) break;
        off += (size_t)wrote;
    }
    pthread_mutex_unlock(&st->lock);

    http_response_set_status(res, 200);
    http_response_set_header(res, "Content-Type", "text/plain; version=0.0.4");
    http_response_set_body(res, buf);
    free(buf);
}

const char* http_server_set_metrics_raw(HttpServer* server,
                                        const char* metrics_endpoint) {
    if (!server) return "server is null";
    MetricsState* st = (MetricsState*)calloc(1, sizeof(MetricsState));
    if (!st) return "out of memory";
    pthread_mutex_init(&st->lock, NULL);
    http_server_use_request_hook(server, metrics_hook, st);
    const char* endpoint = (metrics_endpoint && *metrics_endpoint)
        ? metrics_endpoint : "/metrics";
    http_server_get(server, endpoint, metrics_handler, st);
    return "";
}

const char* http_server_set_tls_raw(HttpServer* server,
                                    const char* cert_path,
                                    const char* key_path) {
    if (!server) return "server is null";
    if (!cert_path || !*cert_path) return "cert_path is empty";
    if (!key_path  || !*key_path)  return "key_path is empty";
#ifdef AETHER_HAS_OPENSSL
    server_openssl_init_once();

    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) return "SSL_CTX_new failed";
    /* Match the client side: TLS 1.2+ only. Older versions are
     * known-broken (POODLE, etc.) and there's no compat reason to
     * keep them around for an in-process server. */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    /* Disable legacy compression and renegotiation to remove the
     * remaining historical attack surface. */
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION |
                              SSL_OP_NO_RENEGOTIATION);

    if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(ctx);
        return "failed to load TLS certificate (check cert_path and PEM format)";
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(ctx);
        return "failed to load TLS private key (check key_path and PEM format)";
    }
    if (SSL_CTX_check_private_key(ctx) != 1) {
        SSL_CTX_free(ctx);
        return "TLS cert and private key do not match";
    }

    /* Replace any prior context (idempotent re-load). */
    if (server->tls_ctx) {
        SSL_CTX_free((SSL_CTX*)server->tls_ctx);
    }
    server->tls_ctx = ctx;
    server->tls_enabled = 1;
    return "";
#else
    (void)cert_path; (void)key_path;
    return "TLS unavailable: built without OpenSSL";
#endif
}

int http_server_bind_raw(HttpServer* server, const char* host, int port) {
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (strcmp(host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host, &addr.sin_addr);
    }
    
    if (bind(server->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to bind socket to %s:%d\n", host, port);
        close(server->socket_fd);
        server->socket_fd = -1;
        return -1;
    }
    
    if (listen(server->socket_fd, server->max_connections) < 0) {
        fprintf(stderr, "Failed to listen on socket\n");
        close(server->socket_fd);
        server->socket_fd = -1;
        return -1;
    }
    
    // Update host and port (make copy of host string first)
    char* new_host = strdup(host);
    if (server->host) {
        free(server->host);
    }
    server->host = new_host;
    server->port = port;
    
    return 0;
}

// Request parsing
HttpRequest* http_parse_request(const char* raw_request) {
    HttpRequest* req = (HttpRequest*)calloc(1, sizeof(HttpRequest));
    
    // Parse request line: METHOD /path HTTP/1.1
    char* line_end = strstr(raw_request, "\r\n");
    if (!line_end) {
        free(req);
        return NULL;
    }
    
    char request_line[2048];
    int line_len = line_end - raw_request;
    strncpy(request_line, raw_request, line_len);
    request_line[line_len] = '\0';
    
    // Extract method
    char* space = strchr(request_line, ' ');
    if (!space) {
        free(req);
        return NULL;
    }
    
    int method_len = space - request_line;
    req->method = (char*)malloc(method_len + 1);
    if (!req->method) { free(req); return NULL; }
    memcpy(req->method, request_line, method_len);
    req->method[method_len] = '\0';

    // Extract path and query string
    char* path_start = space + 1;
    char* path_end = strchr(path_start, ' ');
    if (!path_end) {
        free(req->method);
        free(req);
        return NULL;
    }

    char* query = strchr(path_start, '?');
    if (query && query < path_end) {
        // Has query string
        int path_len = query - path_start;
        req->path = (char*)malloc(path_len + 1);
        if (!req->path) { free(req->method); free(req); return NULL; }
        memcpy(req->path, path_start, path_len);
        req->path[path_len] = '\0';

        int query_len = path_end - query - 1;
        req->query_string = (char*)malloc(query_len + 1);
        if (!req->query_string) {
            free(req->path); free(req->method); free(req); return NULL;
        }
        memcpy(req->query_string, query + 1, query_len);
        req->query_string[query_len] = '\0';
    } else {
        // No query string
        int path_len = path_end - path_start;
        req->path = (char*)malloc(path_len + 1);
        if (!req->path) { free(req->method); free(req); return NULL; }
        memcpy(req->path, path_start, path_len);
        req->path[path_len] = '\0';
        req->query_string = NULL;
    }
    
    // Extract HTTP version
    char* version_start = path_end + 1;
    req->http_version = strdup(version_start);
    
    // Parse headers
    req->header_keys = (char**)malloc(sizeof(char*) * 50);
    req->header_values = (char**)malloc(sizeof(char*) * 50);
    req->header_count = 0;
    
    const char* header_start = line_end + 2;
    while (1) {
        line_end = strstr(header_start, "\r\n");
        if (!line_end || line_end == header_start) {
            // End of headers
            if (line_end) {
                header_start = line_end + 2;
            }
            break;
        }
        
        char header_line[1024];
        line_len = line_end - header_start;
        strncpy(header_line, header_start, line_len);
        header_line[line_len] = '\0';
        
        char* colon = strchr(header_line, ':');
        if (colon && req->header_count < 50) {
            *colon = '\0';
            char* key = header_line;
            char* value = colon + 1;

            // Trim whitespace from value
            while (*value == ' ') value++;

            req->header_keys[req->header_count] = strdup(key);
            req->header_values[req->header_count] = strdup(value);
            req->header_count++;
        }
        
        header_start = line_end + 2;
    }
    
    // Parse body
    if (header_start && *header_start) {
        req->body = strdup(header_start);
        req->body_length = strlen(req->body);
    } else {
        req->body = NULL;
        req->body_length = 0;
    }
    
    req->param_keys = NULL;
    req->param_values = NULL;
    req->param_count = 0;
    
    return req;
}

const char* http_get_header(HttpRequest* req, const char* key) {
    if (!req || !key || !req->header_keys || !req->header_values) return NULL;
    for (int i = 0; i < req->header_count; i++) {
        if (!req->header_keys[i]) continue;
        if (strcasecmp(req->header_keys[i], key) == 0) {
            return req->header_values[i];
        }
    }
    return NULL;
}

const char* http_get_query_param(HttpRequest* req, const char* key) {
    if (!req || !key) return NULL;
    if (!req->query_string) return NULL;
    
    // Parse query params on demand
    char* found = strstr(req->query_string, key);
    if (!found) return NULL;
    
    // Check if it's actually the key (not part of another key)
    if (found != req->query_string && *(found - 1) != '&') {
        return NULL;
    }
    
    char* equals = strchr(found, '=');
    if (!equals) return NULL;
    
    char* value_start = equals + 1;
    char* value_end = strchr(value_start, '&');
    
    static char value_buf[256];
    size_t value_len = value_end ? (size_t)(value_end - value_start) : strlen(value_start);
    strncpy(value_buf, value_start, value_len);
    value_buf[value_len] = '\0';
    
    return value_buf;
}

const char* http_get_path_param(HttpRequest* req, const char* key) {
    for (int i = 0; i < req->param_count; i++) {
        if (strcmp(req->param_keys[i], key) == 0) {
            return req->param_values[i];
        }
    }
    return NULL;
}

void http_request_free(HttpRequest* req) {
    if (!req) return;
    
    free(req->method);
    free(req->path);
    free(req->query_string);
    free(req->http_version);
    free(req->body);
    
    for (int i = 0; i < req->header_count; i++) {
        free(req->header_keys[i]);
        free(req->header_values[i]);
    }
    free(req->header_keys);
    free(req->header_values);
    
    for (int i = 0; i < req->param_count; i++) {
        free(req->param_keys[i]);
        free(req->param_values[i]);
    }
    free(req->param_keys);
    free(req->param_values);
    
    free(req);
}

// Response building
HttpServerResponse* http_response_create() {
    HttpServerResponse* res = (HttpServerResponse*)calloc(1, sizeof(HttpServerResponse));
    res->status_code = 200;
    res->status_text = strdup("OK");
    res->header_keys = (char**)malloc(sizeof(char*) * 50);
    res->header_values = (char**)malloc(sizeof(char*) * 50);
    res->header_count = 0;
    res->body = NULL;
    res->body_length = 0;

    // Add default headers
    http_response_set_header(res, "Content-Type", "text/html; charset=utf-8");
    http_response_set_header(res, "Server", "Aether/1.0");

    return res;
}

void http_response_set_status(HttpServerResponse* res, int code) {
    if (!res) return;
    res->status_code = code;
    free(res->status_text);
    res->status_text = strdup(http_status_text(code));
}

void http_response_set_header(HttpServerResponse* res, const char* key, const char* value) {
    if (!res || !key || !value) return;

    // Lazy-allocate the header arrays. http_response_create() sets them
    // up eagerly, but external callers constructing the response struct
    // themselves (e.g. a C dispatch layer that wants to hand a response
    // into Aether handlers) only zero the struct. Without this, the
    // strdup-into-NULL below was a crash on the first header write.
    if (!res->header_keys || !res->header_values) {
        res->header_keys = (char**)calloc(50, sizeof(char*));
        res->header_values = (char**)calloc(50, sizeof(char*));
        res->header_count = 0;
        if (!res->header_keys || !res->header_values) return;
    }

    // Check if header exists, update it
    for (int i = 0; i < res->header_count; i++) {
        if (strcasecmp(res->header_keys[i], key) == 0) {
            free(res->header_values[i]);
            res->header_values[i] = strdup(value);
            return;
        }
    }

    // Add new header (max 50)
    if (res->header_count >= 50) return;
    res->header_keys[res->header_count] = strdup(key);
    res->header_values[res->header_count] = strdup(value);
    res->header_count++;
}

void http_response_set_body(HttpServerResponse* res, const char* body) {
    if (!res || !body) return;
    free(res->body);
    res->body = strdup(body);
    res->body_length = strlen(body);

    // Update Content-Length
    char len_str[32];
    snprintf(len_str, sizeof(len_str), "%zu", res->body_length);
    http_response_set_header(res, "Content-Length", len_str);
}

void http_response_json(HttpServerResponse* res, const char* json) {
    http_response_set_header(res, "Content-Type", "application/json");
    http_response_set_body(res, json);
}

// Length-aware serializer. The body may be binary (gzip-compressed,
// other application/octet-stream payloads); the returned buffer is
// NOT a C string — caller must use *out_len, never strlen.
char* http_response_serialize_len(HttpServerResponse* res, size_t* out_len) {
    if (!res) { if (out_len) *out_len = 0; return NULL; }
    // Compute required size: status line + headers + blank line + body
    size_t needed = 64;  // status line headroom
    for (int i = 0; i < res->header_count; i++)
        needed += strlen(res->header_keys[i]) + strlen(res->header_values[i]) + 4;
    needed += 2;  // blank line
    if (res->body) needed += res->body_length;

    char* buf = malloc(needed + 1);
    if (!buf) { if (out_len) *out_len = 0; return NULL; }

    int off = snprintf(buf, needed + 1, "HTTP/1.1 %d %s\r\n",
                       res->status_code, res->status_text);
    for (int i = 0; i < res->header_count; i++)
        off += snprintf(buf + off, needed + 1 - off, "%s: %s\r\n",
                        res->header_keys[i], res->header_values[i]);
    off += snprintf(buf + off, needed + 1 - off, "\r\n");
    if (res->body && res->body_length > 0) {
        memcpy(buf + off, res->body, res->body_length);
        off += (int)res->body_length;
    }
    if (out_len) *out_len = (size_t)off;
    return buf;
}

// String-shaped legacy serializer. Equivalent to the length-aware
// variant for text bodies; truncates at the first NUL in the body
// for binary responses (callers wanting binary support should use
// http_response_serialize_len directly). Kept for backward compat
// with downstream consumers.
char* http_response_serialize(HttpServerResponse* res) {
    size_t len = 0;
    return http_response_serialize_len(res, &len);
}

void http_server_response_free(HttpServerResponse* res) {
    if (!res) return;

    free(res->status_text);
    free(res->body);

    for (int i = 0; i < res->header_count; i++) {
        free(res->header_keys[i]);
        free(res->header_values[i]);
    }
    free(res->header_keys);
    free(res->header_values);

    free(res);
}

const char* http_status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

// Routing
void http_server_add_route(HttpServer* server, const char* method, const char* path, HttpHandler handler, void* user_data) {
    HttpRoute* route = (HttpRoute*)malloc(sizeof(HttpRoute));
    route->method = strdup(method);
    route->path_pattern = strdup(path);
    route->handler = handler;
    route->user_data = user_data;
    route->next = server->routes;
    server->routes = route;
}

void http_server_get(HttpServer* server, const char* path, HttpHandler handler, void* user_data) {
    http_server_add_route(server, "GET", path, handler, user_data);
}

void http_server_post(HttpServer* server, const char* path, HttpHandler handler, void* user_data) {
    http_server_add_route(server, "POST", path, handler, user_data);
}

void http_server_put(HttpServer* server, const char* path, HttpHandler handler, void* user_data) {
    http_server_add_route(server, "PUT", path, handler, user_data);
}

void http_server_delete(HttpServer* server, const char* path, HttpHandler handler, void* user_data) {
    http_server_add_route(server, "DELETE", path, handler, user_data);
}

void http_server_use_middleware(HttpServer* server, HttpMiddleware middleware, void* user_data) {
    HttpMiddlewareNode* node = (HttpMiddlewareNode*)malloc(sizeof(HttpMiddlewareNode));
    node->middleware = middleware;
    node->user_data = user_data;
    node->next = NULL;
    /* Append to the end of the chain so registration order matches
     * execution order — what every HTTP framework does, what the
     * std.http.middleware factories assume, and what the
     * documentation will say. (The earlier prepend-to-head shape
     * silently reversed the order, which only matters when more
     * than one middleware is installed; #260 Tier 1 is the first
     * caller that ever installs more than one.) */
    if (!server->middleware_chain) {
        server->middleware_chain = node;
    } else {
        HttpMiddlewareNode* tail = server->middleware_chain;
        while (tail->next) tail = tail->next;
        tail->next = node;
    }
}

/* Response-transformer chain node. Hidden in .c — header only
 * forward-declares the struct via the field's `struct
 * HttpResponseTransformerNode*` reference so the type is stable
 * across translation units. */
struct HttpResponseTransformerNode {
    HttpResponseTransformer xform;
    void* user_data;
    struct HttpResponseTransformerNode* next;
};

/* Per-request observation hook chain node (#260 Tier 3 F1/F2). */
struct HttpRequestHookNode {
    HttpRequestHook hook;
    void* user_data;
    struct HttpRequestHookNode* next;
};

/* SSE route node (#260 Tier 2). */
struct HttpSseRoute {
    char* path;
    HttpSseHandler handler;
    void* user_data;
    struct HttpSseRoute* next;
};

/* Public SSE-connection handle exposed to user handlers. Wraps the
 * underlying HttpConn so http_sse_send_event can write directly to
 * the wire (with TLS unwrap when applicable, via conn_send). */
struct HttpSseConn {
    HttpConn* conn;     /* not owned */
    int       closed;
};

void http_server_sse(HttpServer* server, const char* path,
                     HttpSseHandler handler, void* user_data) {
    if (!server || !path || !handler) return;
    struct HttpSseRoute* r = (struct HttpSseRoute*)calloc(1, sizeof(*r));
    if (!r) return;
    r->path = strdup(path);
    r->handler = handler;
    r->user_data = user_data;
    r->next = server->sse_routes;
    server->sse_routes = r;
}

int http_sse_send_event(HttpSseConn* sse,
                        const char* event_name,
                        const char* data) {
    return http_sse_send_event_id(sse, event_name, data, NULL);
}

int http_sse_send_event_id(HttpSseConn* sse,
                           const char* event_name,
                           const char* data,
                           const char* id) {
    if (!sse || !sse->conn || sse->closed) return -1;

    /* Build an SSE chunk:
     *     id: <id>\n           (optional)
     *     event: <name>\n      (optional)
     *     data: <line1>\n
     *     data: <line2>\n      (one line per \n in data)
     *     \n                   (terminator)
     */
    /* Worst case: data is N bytes, every byte is \n -> N "data: \n"
     * lines plus the constant overhead. Round generously to 4*N + 256. */
    size_t data_len = data ? strlen(data) : 0;
    size_t cap = data_len * 4 + 256
               + (event_name ? strlen(event_name) + 16 : 0)
               + (id ? strlen(id) + 16 : 0);
    char* buf = (char*)malloc(cap);
    if (!buf) return -1;
    size_t off = 0;

    if (id && *id) {
        off += (size_t)snprintf(buf + off, cap - off, "id: %s\n", id);
    }
    if (event_name && *event_name) {
        off += (size_t)snprintf(buf + off, cap - off, "event: %s\n", event_name);
    }
    if (data && *data) {
        const char* line_start = data;
        const char* p = data;
        for (;;) {
            if (*p == '\n' || *p == '\0') {
                size_t line_len = (size_t)(p - line_start);
                if (off + 7 + line_len + 1 >= cap) break;
                memcpy(buf + off, "data: ", 6); off += 6;
                memcpy(buf + off, line_start, line_len); off += line_len;
                buf[off++] = '\n';
                if (*p == '\0') break;
                line_start = p + 1;
            }
            p++;
        }
    } else {
        if (off + 8 < cap) {
            memcpy(buf + off, "data: \n", 7);
            off += 7;
        }
    }
    if (off + 2 < cap) {
        buf[off++] = '\n';   /* event terminator */
    }

    int n = conn_send(sse->conn, buf, (int)off);
    free(buf);
    if (n < (int)off) {
        sse->closed = 1;
        return -1;
    }
    return 0;
}

void http_sse_close(HttpSseConn* sse) {
    if (!sse) return;
    sse->closed = 1;
}

void http_server_use_request_hook(HttpServer* server,
                                  HttpRequestHook hook,
                                  void* user_data) {
    if (!server || !hook) return;
    struct HttpRequestHookNode* node =
        (struct HttpRequestHookNode*)malloc(sizeof(*node));
    node->hook = hook;
    node->user_data = user_data;
    node->next = NULL;
    if (!server->request_hook_chain) {
        server->request_hook_chain = node;
    } else {
        struct HttpRequestHookNode* tail = server->request_hook_chain;
        while (tail->next) tail = tail->next;
        tail->next = node;
    }
}

void http_server_use_response_transformer(HttpServer* server,
                                          HttpResponseTransformer xform,
                                          void* user_data) {
    if (!server || !xform) return;
    struct HttpResponseTransformerNode* node =
        (struct HttpResponseTransformerNode*)malloc(sizeof(*node));
    node->xform = xform;
    node->user_data = user_data;
    node->next = NULL;
    if (!server->response_transformer_chain) {
        server->response_transformer_chain = node;
    } else {
        struct HttpResponseTransformerNode* tail = server->response_transformer_chain;
        while (tail->next) tail = tail->next;
        tail->next = node;
    }
}

// Route matching with parameter extraction
int http_route_matches(const char* pattern, const char* path, HttpRequest* req) {
    // Exact match
    if (strcmp(pattern, path) == 0) {
        return 1;
    }
    
    // Pattern matching with parameters
    const char* p = pattern;
    const char* u = path;
    
    // Allocate space for params
    req->param_keys = (char**)malloc(sizeof(char*) * 10);
    req->param_values = (char**)malloc(sizeof(char*) * 10);
    req->param_count = 0;
    
    while (*p && *u) {
        if (*p == ':') {
            // Parameter segment
            p++; // Skip ':'
            
            // Extract parameter name
            const char* param_start = p;
            while (*p && *p != '/') p++;
            
            int param_name_len = p - param_start;
            char* param_name = (char*)malloc(param_name_len + 1);
            strncpy(param_name, param_start, param_name_len);
            param_name[param_name_len] = '\0';
            
            // Extract parameter value from URL
            const char* value_start = u;
            while (*u && *u != '/') u++;
            
            int value_len = u - value_start;
            char* value = (char*)malloc(value_len + 1);
            strncpy(value, value_start, value_len);
            value[value_len] = '\0';
            
            req->param_keys[req->param_count] = param_name;
            req->param_values[req->param_count] = value;
            req->param_count++;
            
        } else if (*p == '*') {
            // Wildcard - matches anything remaining
            return 1;
        } else if (*p == *u) {
            p++;
            u++;
        } else {
            // No match
            return 0;
        }
    }
    
    // Both should be at end for exact match
    return (*p == '\0' && *u == '\0');
}

// Handle a single client connection
#include <time.h>

/* Monotonic microsecond clock for per-request latency measurement. */
static long http_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}

/* HTTP/1.1 Connection-header inspection. Returns 1 if the client
 * asked to keep the connection open after this request, 0 if it asked
 * for close (explicitly or by HTTP/1.0 default). Used by the
 * keep-alive loop in handle_client_connection. */
static int http_request_wants_keepalive(HttpRequest* req) {
    if (!req) return 0;
    const char* conn_hdr = http_get_header(req, "Connection");
    int is_http_1_0 = req->http_version &&
                      strstr(req->http_version, "HTTP/1.0") != NULL;
    if (conn_hdr) {
        if (http_strcasestr(conn_hdr, "close") != NULL) return 0;
        if (http_strcasestr(conn_hdr, "keep-alive") != NULL) return 1;
    }
    /* No header: HTTP/1.1 defaults to keep-alive, HTTP/1.0 to close. */
    return is_http_1_0 ? 0 : 1;
}

/* Per-request slice of the connection lifecycle. Returns:
 *    1 — request was processed; caller may loop for another request
 *        on the same connection (subject to keep-alive policy).
 *    0 — connection should close (parse failure, EOF, client asked
 *        for close, or the response status mandates close).
 *
 * Uses the conn's persistent read buffer so that bytes the previous
 * request's recv loop pulled past its own boundary (HTTP pipelining,
 * or just two requests in one TCP packet) are not dropped. */
static int handle_one_request(HttpServer* server, HttpConn* conn,
                              int requests_served, int max_requests) {
    long t_start = http_now_us();
    /* Lazy-allocate the read buffer on first use. */
    if (!conn->buf) {
        if (conn_buf_ensure(conn, HTTP_CONN_BUF_CAP) != 0) return 0;
        conn->read_pos = 0;
        conn->write_pos = 0;
    }
    /* Reclaim space at the head if the previous request consumed it. */
    conn_buf_compact(conn);

    /* Read until \r\n\r\n appears in the unconsumed portion. The
     * scan starts from read_pos so already-buffered pipelined bytes
     * count toward the header boundary. */
    char* hdr_end = NULL;
    while (1) {
        if (conn->write_pos > conn->read_pos) {
            /* NUL-terminate the unconsumed slice in place — we have
             * one byte of slack reserved at the end of the buffer. */
            conn->buf[conn->write_pos] = '\0';
            hdr_end = strstr(conn->buf + conn->read_pos, "\r\n\r\n");
            if (hdr_end) break;
        }
        if (conn->write_pos + 1 >= conn->buf_cap) {
            /* Header section exceeded buffer capacity; bail. */
            return 0;
        }
        int n = conn_recv(conn, conn->buf + conn->write_pos,
                          conn->buf_cap - conn->write_pos - 1);
        if (n <= 0) return 0;  /* EOF / timeout / error */
        conn->write_pos += n;
    }

    int header_size = (int)(hdr_end - conn->buf) + 4 - conn->read_pos;
    int request_total = header_size;

    /* Resolve Content-Length and ensure the full body is buffered. */
    const char* cl_hdr = http_strcasestr(conn->buf + conn->read_pos,
                                         "Content-Length:");
    long content_length = 0;
    if (cl_hdr && cl_hdr < hdr_end) {
        content_length = strtol(cl_hdr + 15, NULL, 10);
        if (content_length < 0) content_length = 0;
    }
    if (content_length > 0) {
        int needed_total = header_size + (int)content_length;
        /* Make sure the buffer can hold (read_pos + needed_total)
         * bytes plus a NUL. */
        if (conn_buf_ensure(conn, conn->read_pos + needed_total + 1) != 0) {
            return 0;
        }
        while (conn->write_pos - conn->read_pos < needed_total) {
            int want = needed_total - (conn->write_pos - conn->read_pos);
            int avail = conn->buf_cap - conn->write_pos - 1;
            int chunk = want < avail ? want : avail;
            if (chunk <= 0) return 0;
            int n = conn_recv(conn, conn->buf + conn->write_pos, chunk);
            if (n <= 0) return 0;
            conn->write_pos += n;
        }
        request_total = needed_total;
    }

    /* Carve out the request slice. http_parse_request expects a
     * NUL-terminated C string; restore the byte we overwrite after
     * parsing so the next request's bytes (already in the buffer
     * from a pipelined recv) survive. */
    char* req_start = conn->buf + conn->read_pos;
    char saved = req_start[request_total];
    req_start[request_total] = '\0';
    HttpRequest* req = http_parse_request(req_start);
    req_start[request_total] = saved;

    /* Advance past the parsed bytes regardless of parse outcome —
     * a malformed request shouldn't make the same bytes parse again
     * on the next iteration. */
    conn->read_pos += request_total;
    if (!req) return 0;

    // Create response
    HttpServerResponse* res = http_response_create();

    // Execute middleware chain
    HttpMiddlewareNode* middleware = server->middleware_chain;
    int should_continue = 1;

    while (middleware && should_continue) {
        should_continue = middleware->middleware(req, res, middleware->user_data);
        middleware = middleware->next;
    }

    // If middleware blocked, send response and close.
    if (!should_continue) {
        size_t resp_len = 0;
        char* response_str = http_response_serialize_len(res, &resp_len);
        if (response_str) { conn_send(conn, response_str, (int)resp_len); free(response_str); }
        http_request_free(req);
        http_server_response_free(res);
        return 0;
    }

    /* SSE dispatch (#260 Tier 2). SSE routes own the connection
     * lifetime — the handler emits events directly to the wire and
     * the response struct is not used. Match before normal routes
     * so an /events SSE route takes precedence. */
    {
        struct HttpSseRoute* sr = server->sse_routes;
        while (sr) {
            if (req->path && sr->path && strcmp(sr->path, req->path) == 0) {
                /* Emit the SSE response head directly to the wire
                 * — bypassing http_response_set_body / serialize
                 * because we don't have a Content-Length and the
                 * body is open-ended. */
                const char* head =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                conn_send(conn, head, (int)strlen(head));

                HttpSseConn sse_handle = { .conn = conn, .closed = 0 };
                sr->handler(req, &sse_handle, sr->user_data);

                /* After the handler returns, flush whatever's
                 * pending in the response struct (it's empty —
                 * we never wrote to it) and tear down. SSE always
                 * forces close. */
                http_request_free(req);
                http_server_response_free(res);
                return 0;
            }
            sr = sr->next;
        }
    }

    // Find matching route
    HttpRoute* route = server->routes;
    HttpRoute* matched_route = NULL;

    while (route) {
        if (strcmp(route->method, req->method) == 0) {
            if (http_route_matches(route->path_pattern, req->path, req)) {
                matched_route = route;
                break;
            }
        }
        route = route->next;
    }

    // Execute route handler or return 404
    if (matched_route) {
        matched_route->handler(req, res, matched_route->user_data);
    } else {
        http_response_set_status(res, 404);
        http_response_set_body(res, "404 Not Found");
    }

    /* Run response-transformer chain (#260 Tier 1). Each transformer
     * may mutate the response — typical uses include gzip
     * compression and error-page substitution. They run in
     * registration order. */
    {
        struct HttpResponseTransformerNode* xform = server->response_transformer_chain;
        while (xform) {
            xform->xform(req, res, xform->user_data);
            xform = xform->next;
        }
    }

    /* Per-request observation hooks (#260 Tier 3). Fire after
     * transformers (so hooks see the final response state, e.g.
     * the gzip Content-Encoding) but before the wire send so the
     * latency measurement includes everything except network
     * write — the hooks are about server-side cost, not client-
     * perceived round trip. Hooks may not mutate; they observe. */
    if (server->request_hook_chain) {
        long t_end = http_now_us();
        long duration_us = t_end - t_start;
        struct HttpRequestHookNode* h = server->request_hook_chain;
        while (h) {
            h->hook(req, res, duration_us, h->user_data);
            h = h->next;
        }
    }

    /* Decide whether this response keeps the connection open.
     * Three things can force close:
     *   - keep-alive disabled on the server,
     *   - client requested close (or HTTP/1.0 default),
     *   - max_requests reached (caller passes the post-increment).
     * We also force close on response statuses that semantically
     * mandate it (408 Request Timeout, 426 Upgrade Required). */
    int will_keep_alive = server->keep_alive_enabled
                       && http_request_wants_keepalive(req)
                       && (max_requests == 0 ||
                           requests_served + 1 < max_requests)
                       && res->status_code != 408
                       && res->status_code != 426;

    /* Emit the right Connection / Keep-Alive headers so the client
     * knows what we're going to do. */
    if (will_keep_alive) {
        http_response_set_header(res, "Connection", "keep-alive");
        if (max_requests > 0) {
            char ka[64];
            int remaining = max_requests - requests_served - 1;
            int idle_sec = server->keep_alive_idle_ms > 0
                ? server->keep_alive_idle_ms / 1000 : 30;
            snprintf(ka, sizeof(ka), "timeout=%d, max=%d", idle_sec, remaining);
            http_response_set_header(res, "Keep-Alive", ka);
        }
    } else {
        http_response_set_header(res, "Connection", "close");
    }

    // Send response — length-aware so binary bodies (e.g. gzip-
    // compressed) survive intact past their first NUL byte.
    {
        size_t resp_len = 0;
        char* response_str = http_response_serialize_len(res, &resp_len);
        if (response_str) { conn_send(conn, response_str, (int)resp_len); free(response_str); }
    }

    // Cleanup
    http_request_free(req);
    http_server_response_free(res);
    return will_keep_alive;
}

/* Public reusable helper. Owns the full per-connection lifecycle:
 * optional TLS handshake, request-parsing loop with keep-alive,
 * route dispatch, response emission, socket close. Used by both the
 * thread-pool worker path inside this file AND user actor step
 * functions registered via http_server_set_actor_handler — both
 * call here to get identical TLS / keep-alive / route-dispatch
 * behaviour. (#260 Tier 0 / Phase C3.)
 *
 * The inflight_connections counter (#260 Tier 3 graceful shutdown)
 * tracks active drains so http_server_shutdown_graceful can wait
 * for them to complete naturally before forcing close. */
void http_server_drain_connection(HttpServer* server, int client_fd) {
    if (!server || client_fd < 0) return;
    atomic_fetch_add(&server->inflight_connections, 1);

    /* Connection transport — plain by default; SSL-wrapped when the
     * server has TLS enabled. The conn_recv/conn_send/conn_close
     * helpers polymorphise over both shapes so handle_one_request
     * reads the same regardless. */
    HttpConn conn = {
        .fd = client_fd,
        .ssl = NULL,
        .buf = NULL,
        .buf_cap = 0,
        .read_pos = 0,
        .write_pos = 0,
    };
#ifdef AETHER_HAS_OPENSSL
    if (server->tls_enabled && server->tls_ctx) {
        if (conn_tls_accept(&conn, (SSL_CTX*)server->tls_ctx) != 0) {
            close(client_fd);
            return;
        }
    }
#endif

    /* Per-recv socket timeout. When keep-alive is off this is the
     * existing 30-second guard that bounds slow-loris attacks; when
     * keep-alive is on this doubles as the idle-timeout (the loop
     * exits when conn_recv returns -1 from EAGAIN/timeout). */
    int idle_ms = server->keep_alive_idle_ms > 0
        ? server->keep_alive_idle_ms : 30000;
#ifdef _WIN32
    DWORD rcv_timeout = (DWORD)idle_ms;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&rcv_timeout, sizeof(rcv_timeout));
#else
    struct timeval rcv_tv = {
        .tv_sec  = idle_ms / 1000,
        .tv_usec = (idle_ms % 1000) * 1000
    };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &rcv_tv, sizeof(rcv_tv));
#endif

    /* Drain loop. handle_one_request returns 1 to keep the connection
     * open for another request, 0 to close. With keep-alive disabled
     * (server->keep_alive_enabled == 0) the function always returns 0
     * after the first request — no behavioural change vs. pre-#260. */
    int requests_served = 0;
    int max_requests = server->keep_alive_max;
    while (handle_one_request(server, &conn, requests_served, max_requests)) {
        requests_served++;
        if (max_requests > 0 && requests_served >= max_requests) break;
    }

    conn_close(&conn);
    atomic_fetch_sub(&server->inflight_connections, 1);
}

static void handle_client_connection(HttpServer* server, int client_fd) {
    /* Thread-pool path — defers to the public drain helper so the
     * keep-alive / TLS / route-dispatch logic lives in one place. */
    http_server_drain_connection(server, client_fd);
}

// ============================================================================
// Bounded thread pool for connection handling
// ============================================================================
// Replaces thread-per-connection with a fixed pool of worker threads.
// Connections beyond pool capacity wait in the kernel accept backlog.
// This prevents unbounded thread creation under load.

#if AETHER_HAS_THREADS && !defined(_WIN32)

#define HTTP_POOL_WORKERS   8
#define HTTP_POOL_QUEUE_CAP 256

typedef struct {
    HttpServer* server;
    int queue[HTTP_POOL_QUEUE_CAP];   // Ring buffer of pending client fds
    int head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    int shutdown;
    pthread_t workers[HTTP_POOL_WORKERS];
} HttpConnectionPool;

static void* http_pool_worker(void* arg) {
    HttpConnectionPool* pool = (HttpConnectionPool*)arg;
    while (1) {
        pthread_mutex_lock(&pool->lock);
        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->lock);
        }
        if (pool->shutdown && pool->count == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }
        int client_fd = pool->queue[pool->head];
        pool->head = (pool->head + 1) % HTTP_POOL_QUEUE_CAP;
        pool->count--;
        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->lock);

        handle_client_connection(pool->server, client_fd);
    }
    return NULL;
}

static HttpConnectionPool* http_pool_create(HttpServer* server) {
    HttpConnectionPool* pool = calloc(1, sizeof(HttpConnectionPool));
    if (!pool) return NULL;
    pool->server = server;
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    pthread_cond_init(&pool->not_full, NULL);
    for (int i = 0; i < HTTP_POOL_WORKERS; i++) {
        pthread_create(&pool->workers[i], NULL, http_pool_worker, pool);
    }
    return pool;
}

static void http_pool_submit(HttpConnectionPool* pool, int client_fd) {
    pthread_mutex_lock(&pool->lock);
    while (pool->count >= HTTP_POOL_QUEUE_CAP && !pool->shutdown) {
        pthread_cond_wait(&pool->not_full, &pool->lock);
    }
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        close(client_fd);
        return;
    }
    pool->queue[pool->tail] = client_fd;
    pool->tail = (pool->tail + 1) % HTTP_POOL_QUEUE_CAP;
    pool->count++;
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->lock);
}

static void http_pool_destroy(HttpConnectionPool* pool) {
    if (!pool) return;
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->not_empty);
    pthread_cond_broadcast(&pool->not_full);
    pthread_mutex_unlock(&pool->lock);
    for (int i = 0; i < HTTP_POOL_WORKERS; i++) {
        pthread_join(pool->workers[i], NULL);
    }
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);
    free(pool);
}

#endif // AETHER_HAS_THREADS && !_WIN32

// ---------------------------------------------------------------------------
// Accept thread context (one per core in multi-accept mode)
// ---------------------------------------------------------------------------
#if !defined(_WIN32)
typedef struct {
    HttpServer* server;
    int listen_fd;          // This thread's SO_REUSEPORT listen socket
    AetherIoPoller* poller; // This thread's I/O poller
    int thread_index;       // Which core's workers to prefer
} AcceptThreadCtx;

// Create a SO_REUSEPORT listen socket bound to the same port
static int create_reuseport_socket(const char* host, int port, int backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (strcmp(host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host, &addr.sin_addr);
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, backlog) < 0) {
        close(fd);
        return -1;
    }

    // Non-blocking for epoll
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    return fd;
}

// Dispatch a data-ready fd to a worker actor
static inline void dispatch_to_worker(HttpServer* server, int fd) {
    void* worker = server->spawn_fn(-1, server->step_fn, 0);
    if (worker) {
        HttpConnectionMessage conn_msg;
        conn_msg.type = MSG_HTTP_CONNECTION;
        conn_msg.client_fd = fd;
        server->send_fn(worker, &conn_msg, sizeof(conn_msg));
    } else {
        const char* err = "HTTP/1.1 503 Service Unavailable\r\n"
                          "Content-Length: 19\r\n\r\nService Unavailable";
        send(fd, err, strlen(err), MSG_NOSIGNAL);
        close(fd);
    }
}

// Per-core accept + I/O poller loop with optimistic recv
// Strategy: try MSG_PEEK on accept() — if data is already there (common for
// short-lived HTTP), dispatch immediately without touching the poller (saves syscalls).
// Only register with the poller if the client hasn't sent data yet.
static void accept_poller_loop(HttpServer* server, int listen_fd, AetherIoPoller* poller) {
    AetherIoEvent events[256];

    while (server->is_running) {
        int n = aether_io_poller_poll(poller, events, 256, 100);

        for (int i = 0; i < n; i++) {
            int fd = events[i].fd;

            if (fd == listen_fd) {
                // Re-register listen fd (one-shot semantics auto-removed it)
                aether_io_poller_add(poller, listen_fd, NULL, AETHER_IO_READ);

                // Accept all pending connections
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(listen_fd,
                        (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd < 0) break;

                    // Optimistic path: check if data already arrived (very common
                    // for HTTP — request follows TCP handshake immediately).
                    // This skips syscalls per connection.
                    char peek;
                    int peeked = recv(client_fd, &peek, 1, MSG_PEEK | MSG_DONTWAIT);
                    if (peeked > 0) {
                        // Data ready — dispatch directly, no poller needed
                        dispatch_to_worker(server, client_fd);
                        continue;
                    }

                    // Slow path: no data yet, register with poller
                    int cflags = fcntl(client_fd, F_GETFL, 0);
                    if (cflags >= 0) fcntl(client_fd, F_SETFL, cflags | O_NONBLOCK);

                    if (aether_io_poller_add(poller, client_fd, NULL, AETHER_IO_READ) != 0) {
                        close(client_fd);
                    }
                }
            } else {
                // Data ready (from poller, one-shot already removed) — dispatch
                dispatch_to_worker(server, fd);
            }
        }
    }
}

static void* accept_thread_fn(void* arg) {
    AcceptThreadCtx* ctx = (AcceptThreadCtx*)arg;
    accept_poller_loop(ctx->server, ctx->listen_fd, ctx->poller);
    free(ctx);
    return NULL;
}
#endif

int http_server_start_raw(HttpServer* server) {
    server->is_running = 1;

#if !defined(_WIN32)
    int use_actor_mode = (server->spawn_fn && server->send_fn && server->step_fn);
    if (use_actor_mode && server->multi_accept) {
        // Multi-accept mode (opt-in): one accept thread per core with SO_REUSEPORT.
        // Best for very high connection rates where accept() is the bottleneck.
        int n_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
        if (n_threads <= 0) n_threads = 4;
        if (n_threads > 16) n_threads = 16;

        server->accept_listen_fds = calloc(n_threads, sizeof(int));
        server->accept_pollers = calloc(n_threads, sizeof(AetherIoPoller));
        server->accept_threads = calloc(n_threads, sizeof(pthread_t));
        if (!server->accept_listen_fds || !server->accept_pollers || !server->accept_threads) {
            fprintf(stderr, "Failed to allocate accept thread state\n");
            return -1;
        }

        for (int i = 0; i < n_threads; i++) {
            server->accept_listen_fds[i] = -1;
        }

        for (int i = 0; i < n_threads; i++) {
            server->accept_listen_fds[i] = create_reuseport_socket(
                server->host, server->port, server->max_connections);
            if (server->accept_listen_fds[i] < 0) {
                fprintf(stderr, "Failed to create SO_REUSEPORT socket for thread %d\n", i);
                return -1;
            }

            if (aether_io_poller_init(&server->accept_pollers[i]) < 0) {
                fprintf(stderr, "I/O poller init failed for thread %d\n", i);
                return -1;
            }

            aether_io_poller_add(&server->accept_pollers[i],
                                 server->accept_listen_fds[i], NULL, AETHER_IO_READ);
        }

        server->accept_thread_count = n_threads;

        printf("Server running at http://%s:%d (%d accept threads, SO_REUSEPORT)\n",
               server->host, server->port, n_threads);
        printf("Press Ctrl+C to stop\n\n");
        fflush(stdout);

        for (int i = 1; i < n_threads; i++) {
            AcceptThreadCtx* ctx = malloc(sizeof(AcceptThreadCtx));
            ctx->server = server;
            ctx->listen_fd = server->accept_listen_fds[i];
            ctx->poller = &server->accept_pollers[i];
            ctx->thread_index = i;
            pthread_create(&server->accept_threads[i], NULL, accept_thread_fn, ctx);
        }

        accept_poller_loop(server, server->accept_listen_fds[0],
                           &server->accept_pollers[0]);

        for (int i = 1; i < n_threads; i++) {
            pthread_join(server->accept_threads[i], NULL);
        }

        for (int i = 0; i < n_threads; i++) {
            aether_io_poller_destroy(&server->accept_pollers[i]);
            if (server->accept_listen_fds[i] >= 0) close(server->accept_listen_fds[i]);
        }
        free(server->accept_listen_fds);
        free(server->accept_pollers);
        free(server->accept_threads);
        server->accept_listen_fds = NULL;
        server->accept_pollers = NULL;
        server->accept_threads = NULL;
        server->accept_thread_count = 0;

    } else if (use_actor_mode) {
        // Single-accept with I/O poller (default): one accept thread waits for data
        // before dispatching to worker actors. Best for most workloads.
        if (http_server_bind_raw(server, server->host, server->port) < 0) {
            return -1;
        }

        if (aether_io_poller_init(&server->accept_poller) < 0) {
            fprintf(stderr, "I/O poller init failed\n");
            return -1;
        }

        aether_io_poller_add(&server->accept_poller, server->socket_fd, NULL, AETHER_IO_READ);

        int flags = fcntl(server->socket_fd, F_GETFL, 0);
        if (flags >= 0) fcntl(server->socket_fd, F_SETFL, flags | O_NONBLOCK);

        printf("Server running at http://%s:%d\n", server->host, server->port);
        printf("Press Ctrl+C to stop\n\n");
        fflush(stdout);

        accept_poller_loop(server, server->socket_fd, &server->accept_poller);

        aether_io_poller_destroy(&server->accept_poller);

    } else
#endif
    {
        if (http_server_bind_raw(server, server->host, server->port) < 0) {
            return -1;
        }

        printf("Server running at http://%s:%d\n", server->host, server->port);
        printf("Press Ctrl+C to stop\n\n");
        fflush(stdout);

        /* on_start lifecycle hook (#260 Tier 3). Fires once after
         * the listen socket is bound, before the accept loop runs.
         * Typical use: log "ready", flip a readiness probe to 200. */
        if (server->on_start) {
            server->on_start(server, server->on_start_user_data);
        }

#if AETHER_HAS_THREADS && !defined(_WIN32)
        HttpConnectionPool* pool = http_pool_create(server);
#endif

        // Fallback: poll + thread pool (non-Linux or no actor handler)
        while (server->is_running) {
#if !defined(_WIN32)
            struct pollfd pfd = { .fd = server->socket_fd, .events = POLLIN };
            int ready = poll(&pfd, 1, 1000);
            if (ready <= 0) continue;
#endif

            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server->socket_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                if (!server->is_running) break;
                continue;
            }

#if !AETHER_HAS_THREADS
            handle_client_connection(server, client_fd);
#elif defined(_WIN32)
            handle_client_connection(server, client_fd);
#else
            http_pool_submit(pool, client_fd);
#endif
        }

#if AETHER_HAS_THREADS && !defined(_WIN32)
        http_pool_destroy(pool);
#endif

        /* on_stop lifecycle hook fires after the accept loop exits
         * but BEFORE socket cleanup (so the hook still sees a live
         * server struct). Typical use: flush logs, snapshot
         * metrics, flip readiness probe to 503. */
        if (server->on_stop) {
            server->on_stop(server, server->on_stop_user_data);
        }
    }

    return 0;
}

void http_server_stop(HttpServer* server) {
    if (!server) return;

    server->is_running = 0;

#if !defined(_WIN32)
    // Destroy pollers to unblock poll/epoll_wait/kevent in accept threads
    for (int i = 0; i < server->accept_thread_count; i++) {
        if (server->accept_pollers) {
            aether_io_poller_destroy(&server->accept_pollers[i]);
        }
        if (server->accept_listen_fds && server->accept_listen_fds[i] >= 0) {
            close(server->accept_listen_fds[i]);
            server->accept_listen_fds[i] = -1;
        }
    }

    aether_io_poller_destroy(&server->accept_poller);
#endif

    if (server->socket_fd >= 0) {
#ifdef _WIN32
        closesocket(server->socket_fd);
        WSACleanup();
#else
        close(server->socket_fd);
#endif
        server->socket_fd = -1;
    }
}

void http_server_free(HttpServer* server) {
    if (!server) return;

    http_server_stop(server);
    
    free(server->host);
    
    // Free routes
    HttpRoute* route = server->routes;
    while (route) {
        HttpRoute* next = route->next;
        free(route->method);
        free(route->path_pattern);
        free(route);
        route = next;
    }
    
    // Free middleware
    HttpMiddlewareNode* middleware = server->middleware_chain;
    while (middleware) {
        HttpMiddlewareNode* next = middleware->next;
        free(middleware);
        middleware = next;
    }

    // Free response transformers
    {
        struct HttpResponseTransformerNode* xform = server->response_transformer_chain;
        while (xform) {
            struct HttpResponseTransformerNode* next = xform->next;
            free(xform);
            xform = next;
        }
    }

    // Free per-request observation hooks (#260 Tier 3 F1/F2). Note:
    // we deliberately don't free the hook's user_data here — that's
    // owned by the registering subsystem (access logger / metrics
    // collector). Those leak across server free, which is fine for
    // process-lifetime servers; a follow-up could add a destructor
    // hook to clean them up too.
    {
        struct HttpRequestHookNode* h = server->request_hook_chain;
        while (h) {
            struct HttpRequestHookNode* next = h->next;
            free(h);
            h = next;
        }
    }

    // Free SSE routes (#260 Tier 2)
    {
        struct HttpSseRoute* r = server->sse_routes;
        while (r) {
            struct HttpSseRoute* next = r->next;
            free(r->path);
            free(r);
            r = next;
        }
    }

    /* Free TLS context if one was loaded via http_server_set_tls_raw. */
#ifdef AETHER_HAS_OPENSSL
    if (server->tls_ctx) {
        SSL_CTX_free((SSL_CTX*)server->tls_ctx);
        server->tls_ctx = NULL;
    }
#endif

    free(server);
}

// MIME type detection based on file extension
const char* http_mime_type(const char* path) {
    if (!path) return "application/octet-stream";

    const char* ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";

    ext++; // Skip the dot

    // Common web MIME types
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(ext, "css") == 0) return "text/css; charset=utf-8";
    if (strcasecmp(ext, "js") == 0) return "application/javascript; charset=utf-8";
    if (strcasecmp(ext, "json") == 0) return "application/json; charset=utf-8";
    if (strcasecmp(ext, "xml") == 0) return "application/xml; charset=utf-8";
    if (strcasecmp(ext, "txt") == 0) return "text/plain; charset=utf-8";

    // Images
    if (strcasecmp(ext, "png") == 0) return "image/png";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "gif") == 0) return "image/gif";
    if (strcasecmp(ext, "svg") == 0) return "image/svg+xml";
    if (strcasecmp(ext, "ico") == 0) return "image/x-icon";
    if (strcasecmp(ext, "webp") == 0) return "image/webp";

    // Fonts
    if (strcasecmp(ext, "woff") == 0) return "font/woff";
    if (strcasecmp(ext, "woff2") == 0) return "font/woff2";
    if (strcasecmp(ext, "ttf") == 0) return "font/ttf";
    if (strcasecmp(ext, "otf") == 0) return "font/otf";

    // Other
    if (strcasecmp(ext, "pdf") == 0) return "application/pdf";
    if (strcasecmp(ext, "zip") == 0) return "application/zip";
    if (strcasecmp(ext, "wasm") == 0) return "application/wasm";

    return "application/octet-stream";
}

// Serve a single file
void http_serve_file(HttpServerResponse* res, const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        http_response_set_status(res, 404);
        http_response_set_body(res, "404 - File Not Found");
        return;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read file content
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        http_response_set_status(res, 500);
        http_response_set_body(res, "500 - Server Error");
        return;
    }

    size_t bytes_read = fread(content, 1, size, f);
    fclose(f);
    if (bytes_read == 0 && size > 0) {
        free(content);
        http_response_set_status(res, 500);
        http_response_set_body(res, "500 - Server Error");
        return;
    }
    content[bytes_read] = '\0';

    // Set response
    http_response_set_status(res, 200);
    http_response_set_header(res, "Content-Type", http_mime_type(filepath));
    http_response_set_header(res, "Access-Control-Allow-Origin", "*");
    http_response_set_body(res, content);
    free(content);
}

// Static file serving handler (for use with wildcard routes)
void http_serve_static(HttpRequest* req, HttpServerResponse* res, void* base_dir) {
    const char* dir = (const char*)base_dir;
    if (!dir) dir = ".";

    // Build filepath from request path
    const char* req_path = req->path;
    if (!req_path || req_path[0] == '\0') req_path = "/";

    // Handle root path
    if (strcmp(req_path, "/") == 0) {
        req_path = "/index.html";
    }

    // Skip leading slash
    if (req_path[0] == '/') req_path++;

    // Security: reject encoded traversal sequences (%2e, %2f, %5c)
    if (strstr(req_path, "..") != NULL ||
        strstr(req_path, "%2e") != NULL || strstr(req_path, "%2E") != NULL ||
        strstr(req_path, "%2f") != NULL || strstr(req_path, "%2F") != NULL ||
        strstr(req_path, "%5c") != NULL || strstr(req_path, "%5C") != NULL ||
        strstr(req_path, "\\") != NULL) {
        http_response_set_status(res, 403);
        http_response_set_body(res, "403 - Forbidden");
        return;
    }

    // Build full path
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir, req_path);

    // Security: resolve to canonical path and verify it's within the root dir
#ifndef _WIN32
    char resolved[PATH_MAX];
    char resolved_dir[PATH_MAX];
    if (!realpath(filepath, resolved) || !realpath(dir, resolved_dir)) {
        http_response_set_status(res, 404);
        http_response_set_body(res, "404 - Not Found");
        return;
    }
    if (strncmp(resolved, resolved_dir, strlen(resolved_dir)) != 0) {
        http_response_set_status(res, 403);
        http_response_set_body(res, "403 - Forbidden");
        return;
    }
    // Serve the resolved, validated path
    http_serve_file(res, resolved);
#else
    // On Windows, use _fullpath for canonicalization
    char resolved[1024];
    char resolved_dir[1024];
    if (!_fullpath(resolved, filepath, sizeof(resolved)) ||
        !_fullpath(resolved_dir, dir, sizeof(resolved_dir))) {
        http_response_set_status(res, 404);
        http_response_set_body(res, "404 - Not Found");
        return;
    }
    if (_strnicmp(resolved, resolved_dir, strlen(resolved_dir)) != 0) {
        http_response_set_status(res, 403);
        http_response_set_body(res, "403 - Forbidden");
        return;
    }
    http_serve_file(res, resolved);
#endif
}

// ============================================================================
// Actor dispatch mode
// ============================================================================

void http_server_set_actor_handler(HttpServer* server, void (*step_fn)(void*),
                                    void (*send_fn)(void*, void*, size_t),
                                    void* (*spawn_fn)(int, void (*)(void*), size_t),
                                    void (*release_fn)(void*)) {
    if (!server || !step_fn || !send_fn || !spawn_fn) return;
    server->step_fn = step_fn;
    server->send_fn = send_fn;
    server->spawn_fn = spawn_fn;
    server->release_fn = release_fn;
}

// Request accessors (for Aether .ae code via opaque ptr)
// Request field accessors. Each guards against NULL at both the struct
// and the field level so a partially-populated HttpRequest (e.g. one
// built by a C dispatch shim that didn't touch every field) doesn't
// crash downstream string ops. Empty string is the "absent" sentinel
// — consistent with Aether's Go-style string conventions.
const char* http_request_method(HttpRequest* req) {
    return (req && req->method) ? req->method : "";
}

const char* http_request_path(HttpRequest* req) {
    return (req && req->path) ? req->path : "";
}

const char* http_request_body(HttpRequest* req) {
    return (req && req->body) ? req->body : "";
}

const char* http_request_query(HttpRequest* req) {
    return (req && req->query_string) ? req->query_string : "";
}

#endif // AETHER_HAS_NETWORKING
