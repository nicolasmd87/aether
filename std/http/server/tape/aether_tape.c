/* std.http.server.tape — replay-driven HTTP server for testing.
 *
 * v0.1 surface (matches std/http/server/tape/module.ae):
 *
 *   tape_load(path)  -> 1 on success, 0 on parse / I/O failure
 *   tape_load_err()  -> error string (when tape_load returned 0)
 *   tape_count()     -> number of entries the loaded tape contains
 *   tape_dispatch(req, res, ud) -> registered as the catch-all GET
 *                                   handler; matches the next tape
 *                                   entry against (method, path) and
 *                                   emits the recorded response.
 *
 * Tape format — one or more `--- exchange N ---`-headed blocks, each
 * with `> ` request lines and `< ` response lines. See module.ae for
 * the documented shape and the worked example.
 *
 * Storage: a single global tape (one per process — adequate for v0.1
 * since each test runs as its own process). Loading a second tape
 * frees the first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* --- These come from std.http.server's request/response surface. --- */
extern const char* http_request_method(void* req);
extern const char* http_request_path(void* req);
extern void        http_response_set_status(void* res, int code);
extern void        http_response_set_header(void* res, const char* name, const char* value);
extern void        http_response_set_body  (void* res, const char* body);

/* And the routing extern — used by tape_register_routes() below to
 * register each unique tape-path under its own handler. We forward
 * declare here so we don't need a header dependency on std/net. */
extern void http_server_get(void* server, const char* path, void* handler, void* user_data);

/* ---- Tape storage --------------------------------------------------- */

typedef struct TapeEntry {
    char* method;       /* e.g. "GET" — owned, NUL-terminated */
    char* path;         /* e.g. "/ok" — owned, NUL-terminated */
    int   status;       /* e.g. 200 */
    char* body;         /* response body — owned, NUL-terminated; "" if empty */
    char* content_type; /* may be NULL (then no Content-Type emitted) */
} TapeEntry;

static TapeEntry* g_tape       = NULL;
static int        g_tape_n     = 0;     /* number of entries */
static int        g_tape_cap   = 0;     /* allocated capacity  */
static int        g_tape_cursor = 0;    /* next entry to consume */
static char*      g_tape_err   = NULL;  /* "" on success, error text otherwise */

static void tape_free_storage(void) {
    if (g_tape) {
        for (int i = 0; i < g_tape_n; i++) {
            free(g_tape[i].method);
            free(g_tape[i].path);
            free(g_tape[i].body);
            free(g_tape[i].content_type);
        }
        free(g_tape);
        g_tape = NULL;
    }
    g_tape_n = 0;
    g_tape_cap = 0;
    g_tape_cursor = 0;
    free(g_tape_err);
    g_tape_err = NULL;
}

static void tape_set_err(const char* msg) {
    free(g_tape_err);
    g_tape_err = msg ? strdup(msg) : NULL;
}

/* Parse the request line ("GET /ok HTTP/1.1") into method + path.
 * Both stored as new strdup'd strings; caller frees. Returns 0 on
 * success, -1 on malformed line. */
static int parse_request_line(const char* line, char** out_method, char** out_path) {
    *out_method = NULL;
    *out_path = NULL;
    /* Find the first space — that's the end of the method. */
    const char* sp1 = strchr(line, ' ');
    if (!sp1) return -1;
    /* And the second — that's the end of the path. */
    const char* sp2 = strchr(sp1 + 1, ' ');
    if (!sp2) return -1;
    size_t mlen = (size_t)(sp1 - line);
    size_t plen = (size_t)(sp2 - (sp1 + 1));
    *out_method = (char*)malloc(mlen + 1);
    *out_path   = (char*)malloc(plen + 1);
    if (!*out_method || !*out_path) {
        free(*out_method); free(*out_path);
        *out_method = *out_path = NULL;
        return -1;
    }
    memcpy(*out_method, line, mlen); (*out_method)[mlen] = '\0';
    memcpy(*out_path, sp1 + 1, plen); (*out_path)[plen] = '\0';
    return 0;
}

/* Parse the response status line ("HTTP/1.1 200 OK") into the int
 * status code. Returns -1 on malformed input. */
static int parse_status_line(const char* line) {
    const char* sp = strchr(line, ' ');
    if (!sp) return -1;
    return atoi(sp + 1);
}

/* Append a parsed entry to the tape. Returns 0 on success, -1 OOM. */
static int tape_append(const char* method, const char* path,
                       int status, const char* body, const char* content_type) {
    if (g_tape_n >= g_tape_cap) {
        int new_cap = g_tape_cap ? g_tape_cap * 2 : 8;
        TapeEntry* bigger = (TapeEntry*)realloc(g_tape, sizeof(TapeEntry) * (size_t)new_cap);
        if (!bigger) return -1;
        g_tape = bigger;
        g_tape_cap = new_cap;
    }
    TapeEntry* e = &g_tape[g_tape_n];
    e->method       = strdup(method);
    e->path         = strdup(path);
    e->status       = status;
    e->body         = strdup(body ? body : "");
    e->content_type = content_type ? strdup(content_type) : NULL;
    if (!e->method || !e->path || !e->body || (content_type && !e->content_type)) {
        free(e->method); free(e->path); free(e->body); free(e->content_type);
        return -1;
    }
    g_tape_n++;
    return 0;
}

/* Strip a trailing \r if present (handles tape files saved with
 * Windows line endings). Modifies in place. */
static void rstrip_cr(char* s) {
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\r') s[n - 1] = '\0';
}

/* Parse one request/response pair from the file, starting at the
 * line *after* an `--- exchange N ---` header. Stops on EOF or on
 * the next `--- exchange ` line (which is pushed back via fseek so
 * the caller's loop sees it). Returns 0 on success, -1 on parse
 * failure (with g_tape_err populated). */
static int parse_one_exchange(FILE* fp) {
    char line[8192];
    char* method = NULL;
    char* path   = NULL;
    int   status = 0;
    /* Response body is accumulated into a growing buffer because it
     * can span many lines. Header lines `< X: y` are ignored except
     * for Content-Type, which becomes part of the entry. */
    size_t body_cap = 256, body_len = 0;
    char*  body = (char*)malloc(body_cap);
    char*  content_type = NULL;
    if (!body) { tape_set_err("OOM allocating body buffer"); return -1; }
    body[0] = '\0';

    enum { PHASE_REQ_HEADERS, PHASE_RESP_HEADERS, PHASE_RESP_BODY } phase = PHASE_REQ_HEADERS;

    long pos_before;
    while ((pos_before = ftell(fp)), fgets(line, sizeof(line), fp) != NULL) {
        rstrip_cr(line);
        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] == '\n') line[--llen] = '\0';

        /* Hit the next exchange marker — rewind so the outer loop sees it. */
        if (strncmp(line, "--- exchange", 12) == 0) {
            fseek(fp, pos_before, SEEK_SET);
            break;
        }

        if (phase == PHASE_REQ_HEADERS) {
            /* `> METHOD path HTTP/1.1` — the request line, first `> ` */
            if (strncmp(line, "> ", 2) == 0 && method == NULL) {
                if (parse_request_line(line + 2, &method, &path) != 0) {
                    tape_set_err("malformed request line");
                    goto fail;
                }
            } else if (strcmp(line, ">") == 0) {
                /* End of request headers — switch to response phase. */
                phase = PHASE_RESP_HEADERS;
            }
            /* Other `> Header: value` lines are ignored in v0.1 (no
             * header-aware matching). */
        } else if (phase == PHASE_RESP_HEADERS) {
            if (strncmp(line, "< ", 2) == 0 && status == 0) {
                /* `< HTTP/1.1 200 OK` — status line. */
                status = parse_status_line(line + 2);
                if (status < 0) {
                    tape_set_err("malformed status line");
                    goto fail;
                }
            } else if (strncmp(line, "< Content-Type:", 15) == 0) {
                /* Capture Content-Type so the replayer can emit it. */
                const char* v = line + 15;
                while (*v == ' ' || *v == '\t') v++;
                free(content_type);
                content_type = strdup(v);
                if (!content_type) { tape_set_err("OOM"); goto fail; }
            } else if (strcmp(line, "<") == 0) {
                /* End of response headers — body follows. */
                phase = PHASE_RESP_BODY;
            }
            /* Other `< Header: value` lines are ignored on emission
             * for v0.1 (the test matrix only needs Content-Type
             * and status). Header preservation is a v0.2 candidate. */
        } else { /* PHASE_RESP_BODY */
            const char* body_line = NULL;
            if (strncmp(line, "< ", 2) == 0) {
                body_line = line + 2;
            } else if (strcmp(line, "<") == 0) {
                body_line = "";
            } else {
                /* Unrecognised line in body section — treat as
                 * end-of-exchange (probably leading whitespace
                 * before next `--- exchange`). */
                fseek(fp, pos_before, SEEK_SET);
                break;
            }
            size_t add = strlen(body_line) + 1; /* +1 for newline */
            if (body_len + add + 1 > body_cap) {
                while (body_cap < body_len + add + 1) body_cap *= 2;
                char* bigger = (char*)realloc(body, body_cap);
                if (!bigger) { tape_set_err("OOM growing body"); goto fail; }
                body = bigger;
            }
            if (body_len > 0) {
                body[body_len++] = '\n';
            }
            memcpy(body + body_len, body_line, strlen(body_line));
            body_len += strlen(body_line);
            body[body_len] = '\0';
        }
    }

    if (!method || !path || status == 0) {
        tape_set_err("incomplete exchange (missing request line, response status, or both)");
        goto fail;
    }

    if (tape_append(method, path, status, body, content_type) != 0) {
        tape_set_err("OOM appending entry");
        goto fail;
    }

    free(method); free(path); free(body); free(content_type);
    return 0;

fail:
    free(method); free(path); free(body); free(content_type);
    return -1;
}

/* ---- Aether-side externs ------------------------------------------- */

int tape_load(const char* path) {
    if (!path) { tape_set_err("null tape path"); return 0; }
    tape_free_storage();
    /* tape_set_err sets g_tape_err = NULL via its NULL guard, so we
     * need to start fresh after free above. */
    g_tape_err = strdup("");

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "cannot open tape file: %s", path);
        tape_set_err(msg);
        return 0;
    }

    char line[8192];
    while (fgets(line, sizeof(line), fp) != NULL) {
        rstrip_cr(line);
        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] == '\n') line[--llen] = '\0';
        if (strncmp(line, "--- exchange", 12) == 0) {
            if (parse_one_exchange(fp) != 0) {
                fclose(fp);
                return 0;  /* g_tape_err already set */
            }
        }
        /* Lines outside an exchange block are ignored — could be
         * comments, blank lines, etc. */
    }
    fclose(fp);

    if (g_tape_n == 0) {
        tape_set_err("tape file contained no '--- exchange' blocks");
        return 0;
    }
    return 1;
}

const char* tape_load_err(void) {
    return g_tape_err ? g_tape_err : "";
}

int tape_count(void) {
    return g_tape_n;
}

/* Forward decl — defined in the section below for the Aether-facing
 * surface. */
void tape_dispatch(void* req, void* res, void* ud);

/* Register each unique tape path against tape_dispatch. Called by
 * the Aether-side bind() after server_create + tape_load.
 *
 * Why per-path registration rather than a `/(.*)` catch-all: the
 * catch-all regex isn't reliably matching in std.http.server v0.90
 * (returns 404 for /ok against the pattern `/(.*)`), and the
 * framework's "no route → 404" behaviour is itself useful test
 * signal — a SUT that hits a path the tape doesn't anticipate
 * gets a real 404 from the server, separate from the dispatcher's
 * 599 "tape mismatch" diagnostic. */
void tape_register_routes(void* server) {
    if (!server) return;
    /* Dedup by path — multiple entries can share a path (the dispatcher
     * advances through them in order). Each unique path needs only one
     * registration with the routing layer.
     *
     * O(N²) over tape entries; fine for any plausible tape size. */
    for (int i = 0; i < g_tape_n; i++) {
        int already = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(g_tape[i].path, g_tape[j].path) == 0) {
                already = 1;
                break;
            }
        }
        if (!already) {
            http_server_get(server, g_tape[i].path, (void*)tape_dispatch, NULL);
        }
    }
}

void tape_dispatch(void* req, void* res, void* ud) {
    (void)ud;
    /* Pull the next entry. If we've exhausted the tape, fail loudly
     * with 599 (an unassigned-by-IANA status, used here to signal
     * "the test fixture itself failed" rather than a server error). */
    if (g_tape_cursor >= g_tape_n) {
        http_response_set_status(res, 599);
        http_response_set_body(res,
            "tape exhausted — SUT made more requests than the tape contains");
        return;
    }
    TapeEntry* e = &g_tape[g_tape_cursor];

    /* Strict (method, path) match. Any drift fails the test loudly
     * via a 599 response with a diagnostic body, rather than serving
     * the wrong entry and letting the assertion misfire downstream. */
    const char* got_method = http_request_method(req);
    const char* got_path   = http_request_path(req);
    if (!got_method || strcmp(got_method, e->method) != 0
        || !got_path || strcmp(got_path, e->path) != 0) {
        char msg[2048];
        snprintf(msg, sizeof(msg),
            "tape mismatch at entry %d: expected %s %s, got %s %s",
            g_tape_cursor + 1,
            e->method, e->path,
            got_method ? got_method : "(null)",
            got_path   ? got_path   : "(null)");
        http_response_set_status(res, 599);
        http_response_set_body(res, msg);
        /* Don't advance the cursor on mismatch — repeated calls
         * surface the same diagnostic. */
        return;
    }

    /* Emit the recorded response. */
    http_response_set_status(res, e->status);
    if (e->content_type) {
        http_response_set_header(res, "Content-Type", e->content_type);
    }
    http_response_set_body(res, e->body);

    g_tape_cursor++;
}
