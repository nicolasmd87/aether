/* std.http.server.vcr — replay-driven HTTP server for testing.
 *
 * Tape format: Servirtium markdown (https://servirtium.dev). One
 * tape file holds N "interactions"; each interaction is a request +
 * response pair. The parser strategy is split-on-delimiter, the same
 * strategy other Servirtium implementations chose without it being
 * documented (the Java implementation's lock-step parsing is widely
 * regarded as a mistake — the markdown grammar splits cleanly).
 *
 * Three delimiters do all the work:
 *
 *   "\n## Interaction "   – between interactions
 *   "\n### "              – between sections within an interaction
 *   "\n```\n"             – fenced code block boundaries
 *
 * Within each section we look for the (single) fenced code block
 * and capture its contents verbatim. No state machine, no header
 * accumulator, no quote-counting.
 *
 * Section names in canonical Servirtium order:
 *
 *   ### Request headers recorded for playback:
 *   ### Request body recorded for playback (mime/type):
 *   ### Response headers recorded for playback:
 *   ### Response body recorded for playback (status: mime/type):
 *
 * Status code lives in the response-body section header (e.g.
 * "200: text/plain"), not in a status-line in the response-headers
 * block. v0.1 only reads the status from there; response headers
 * the tape lists are not echoed in the emitted response (they will
 * be in v0.2; the test matrix doesn't need them yet).
 *
 * v0.1 surface (matches std/http/server/vcr/module.ae's externs):
 *
 *   vcr_load_tape(path)        -> 1 on success, 0 on parse / I/O failure
 *   vcr_load_err()             -> error string (when vcr_load_tape returned 0)
 *   vcr_tape_length()          -> number of interactions the loaded tape has
 *   vcr_register_routes(srv)   -> walks tape and calls server_get for each
 *                                  unique path so the routing layer dispatches
 *                                  to vcr_dispatch.
 *   vcr_dispatch(req, res, ud) -> registered handler. Matches the next tape
 *                                  interaction against (method, path) and
 *                                  emits the recorded response. Mismatch →
 *                                  599 with diagnostic body.
 *
 * Storage: a single global tape (one per process — adequate for v0.1
 * since each test runs as its own process). Loading a second tape
 * frees the first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- These come from std.http.server's request/response surface. --- */
extern const char* http_request_method(void* req);
extern const char* http_request_path(void* req);
extern void        http_response_set_status(void* res, int code);
extern void        http_response_set_header(void* res, const char* name, const char* value);
extern void        http_response_set_body  (void* res, const char* body);

/* Step 10: header iteration + body access. We need to walk the
 * full set of request headers (not just look one up by name) to
 * detect "extra" headers the tape didn't expect. The HttpRequest
 * struct's prefix layout is stable across the v2 server — we
 * mirror just the fields we touch. Keep this in sync with
 * std/net/aether_http_server.h's HttpRequest definition. */
typedef struct VcrHttpRequestPrefix {
    char*  method;
    char*  path;
    char*  query_string;
    char*  http_version;
    char** header_keys;
    char** header_values;
    int    header_count;
    char*  body;
    /* (rest of struct ignored — body_length, params, ...) */
} VcrHttpRequestPrefix;

/* Step 11: static-content serving externs. The http_server module
 * already implements the heavy lifting (mime-type detection,
 * realpath canonicalization, traversal-prevention) via
 * http_serve_file + http_serve_static. We reuse http_serve_file
 * directly so we can prepend the mount-prefix-stripping ourselves
 * and avoid double-prefix path construction. */
extern void http_serve_file(void* res, const char* filepath);
extern const char* http_mime_type(const char* path);

/* And the routing extern — used by vcr_register_routes(). v0.1
 * used http_server_get exclusively (replay-of-GET-only); v0.2
 * uses add_route which takes the method as a string and so handles
 * GET / POST / PUT / DELETE / PATCH and any custom verb without
 * per-verb dispatch in C. */
extern void http_server_add_route(void* server, const char* method, const char* path, void* handler, void* user_data);

/* ---- Tape storage --------------------------------------------------- */

typedef struct Interaction {
    char* method;       /* "GET" — owned, NUL-terminated */
    char* path;         /* "/path/to/resource" — owned */
    int   status;       /* 200, 404, ... */
    char* body;         /* response body — owned, NUL-terminated; "" if empty */
    char* content_type; /* may be NULL */
    /* Optional [Note] block (Servirtium step 9). Record-only — playback
     * ignores it. NULL when no note was attached to this interaction. */
    char* note_title;
    char* note_body;
    /* Step 10: request match fields. The parser fills these from the
     * `### Request headers ...` / `### Request body ...` code blocks.
     * Empty string ("") means the tape didn't constrain that field
     * — dispatcher skips comparison and matches on (method, path) only.
     * Non-empty → dispatcher must match the incoming request against
     * this exactly (after normalization), or fail with a diagnostic
     * the test reads via vcr.last_error(). */
    char* req_headers;  /* canonical-form: "Name: Value\n..." sorted by name */
    char* req_body;     /* request body bytes, "" if absent / empty */
} Interaction;

static Interaction* g_tape       = NULL;
static int          g_tape_n     = 0;
static int          g_tape_cap   = 0;
static int          g_tape_cursor = 0;
static char*        g_tape_err   = NULL;

/* Pending note staged via vcr_note() — attaches to the next
 * vcr_record_interaction() call, then clears. NULL when nothing
 * is staged. */
static char* g_pending_note_title = NULL;
static char* g_pending_note_body  = NULL;

/* Step 10: last playback mismatch diagnostic. The dispatcher
 * populates this when an incoming request doesn't match the
 * next-in-line tape entry (extra header, different body, etc).
 * The test's tearDown reads it via vcr.last_error(). NULL or ""
 * means "no error since last clear". */
static char* g_last_error = NULL;

static void tape_free_storage(void) {
    if (g_tape) {
        for (int i = 0; i < g_tape_n; i++) {
            free(g_tape[i].method);
            free(g_tape[i].path);
            free(g_tape[i].body);
            free(g_tape[i].content_type);
            free(g_tape[i].note_title);
            free(g_tape[i].note_body);
            free(g_tape[i].req_headers);
            free(g_tape[i].req_body);
        }
        free(g_tape);
        g_tape = NULL;
    }
    g_tape_n = 0;
    g_tape_cap = 0;
    g_tape_cursor = 0;
    free(g_tape_err);
    g_tape_err = NULL;
    free(g_pending_note_title); g_pending_note_title = NULL;
    free(g_pending_note_body);  g_pending_note_body  = NULL;
    free(g_last_error);         g_last_error         = NULL;
    /* Static mounts intentionally not freed here — mounts are
     * configured by the caller before vcr.load() and survive across
     * loads in the same process; vcr.clear_static_content() is the
     * explicit reset. */
}

static void last_err_set(const char* msg) {
    free(g_last_error);
    g_last_error = msg ? strdup(msg) : NULL;
}

static void tape_set_err(const char* msg) {
    free(g_tape_err);
    g_tape_err = msg ? strdup(msg) : NULL;
}

static int tape_append(const char* method, const char* path,
                       int status, const char* body, const char* content_type,
                       const char* req_headers, const char* req_body) {
    if (g_tape_n >= g_tape_cap) {
        int new_cap = g_tape_cap ? g_tape_cap * 2 : 8;
        Interaction* bigger = (Interaction*)realloc(g_tape, sizeof(Interaction) * (size_t)new_cap);
        if (!bigger) return -1;
        g_tape = bigger;
        g_tape_cap = new_cap;
    }
    Interaction* e = &g_tape[g_tape_n];
    e->method       = strdup(method);
    e->path         = strdup(path);
    e->status       = status;
    e->body         = strdup(body ? body : "");
    e->content_type = content_type ? strdup(content_type) : NULL;
    e->req_headers  = strdup(req_headers ? req_headers : "");
    e->req_body     = strdup(req_body ? req_body : "");
    /* Drain the pending-note slot — ownership transfers to this
     * interaction, so the note attaches to exactly one capture. */
    e->note_title   = g_pending_note_title;
    e->note_body    = g_pending_note_body;
    g_pending_note_title = NULL;
    g_pending_note_body  = NULL;
    if (!e->method || !e->path || !e->body || (content_type && !e->content_type)
        || !e->req_headers || !e->req_body) {
        free(e->method); free(e->path); free(e->body); free(e->content_type);
        free(e->req_headers); free(e->req_body);
        free(e->note_title); free(e->note_body);
        return -1;
    }
    g_tape_n++;
    return 0;
}

/* ---- Helpers --------------------------------------------------------- */

/* slurp / slice_dup / rtrim deleted in the C→Aether parser port —
 * the Aether parser uses fs.read + string.substring + an Aether
 * rtrim() helper, none of which need C. See module.ae's
 * parse_tape_file. */

/* Forward decl for the dispatcher — referenced by vcr_register_routes
 * below and by the static-mount route registration. Body lives further
 * down (after the request-normalization helpers). */
void vcr_dispatch(void* req, void* res, void* ud);

const char* vcr_load_err(void) {
    return g_tape_err ? g_tape_err : "";
}

int vcr_tape_length(void) {
    return g_tape_n;
}

/* Walk the tape and register the same dispatcher (vcr_dispatch)
 * against each unique (method, path) pair. The dispatcher itself
 * still matches in cursor order and serves whatever's next; routing
 * is just there so the framework knows to call us at all.
 *
 * Dedup is by (method, path), not just path — a tape that has both
 * GET /foo and POST /foo needs both routes registered so the
 * framework dispatches to us for either verb. (The dispatcher then
 * still strict-matches the actual incoming request against the
 * cursor's expected (method, path); the registration is purely
 * "wake me up for this combination.") */
/* Forward decl — body lives next to the static-mount storage. */
static void register_static_routes(void* server);

void vcr_register_routes(void* server) {
    if (!server) return;
    for (int i = 0; i < g_tape_n; i++) {
        int already = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(g_tape[i].method, g_tape[j].method) == 0
                && strcmp(g_tape[i].path, g_tape[j].path) == 0) {
                already = 1;
                break;
            }
        }
        if (!already) {
            http_server_add_route(server, g_tape[i].method, g_tape[i].path,
                                  (void*)vcr_dispatch, NULL);
        }
    }
    register_static_routes(server);
}

/* ---- Record-mode externs ---------------------------------------------
 *
 * Record mode is driven from the Aether side. The recorder's HTTP
 * handler calls std.http.client to forward the request to upstream,
 * then calls vcr_record_interaction() with the method/path/status/
 * content_type/body it just observed. On vcr.eject(), the Aether
 * side calls vcr_flush_to_tape(path) which writes every captured
 * interaction to disk in canonical Servirtium markdown — the same
 * format the parser above reads, so a recorded tape is immediately
 * replayable on the next test run.
 *
 * The capture list reuses g_tape as its backing storage. A test
 * that loads a tape and then records into the same VCR instance
 * would mix them — fine for v0.1 since record/replay are mutually
 * exclusive per-run.
 * -------------------------------------------------------------------- */

int vcr_record_interaction_full(const char* method, const char* path,
                                int status, const char* content_type, const char* body,
                                const char* req_headers, const char* req_body);

int vcr_record_interaction(const char* method, const char* path,
                           int status, const char* content_type, const char* body) {
    /* Step-7-era recorder: only response bytes, no request capture.
     * Forwards to the step-10 entry point with empty req_headers /
     * req_body — empty means "no constraint" so on later replay the
     * dispatcher matches on (method, path) only, identical to the
     * pre-step-10 behavior. */
    return vcr_record_interaction_full(method, path, status, content_type, body, "", "");
}

/* Step 10 record entry point: like vcr_record_interaction but also
 * captures the request headers (canonical-form: "Name: Value\n..."
 * sorted by name, Host stripped) and request body. Pass "" for
 * either to opt that field out of mismatch detection. */
int vcr_record_interaction_full(const char* method, const char* path,
                                int status, const char* content_type, const char* body,
                                const char* req_headers, const char* req_body) {
    if (!method || !path) { tape_set_err("null method or path"); return 0; }
    if (tape_append(method, path, status, body, content_type,
                    req_headers ? req_headers : "",
                    req_body    ? req_body    : "") != 0) {
        tape_set_err("OOM appending captured interaction");
        return 0;
    }
    return 1;
}

/* Step 9: Notes — record-only annotations the test author can attach
 * to a specific interaction. Stages a (title, body) pair; the next
 * vcr_record_interaction call drains the stage onto the new
 * interaction. Calling vcr_add_note() twice in a row without an
 * intervening record() replaces the staged note (last writer wins).
 *
 * Playback ignores notes — they're parser-tolerated but never
 * surfaced. Notes survive across flush() but not across
 * vcr_clear_tape() / load(). */
int vcr_add_note(const char* title, const char* body) {
    if (!title) { tape_set_err("vcr_add_note: null title"); return 0; }
    char* t = strdup(title);
    char* b = strdup(body ? body : "");
    if (!t || !b) {
        free(t); free(b);
        tape_set_err("OOM staging note");
        return 0;
    }
    free(g_pending_note_title);
    free(g_pending_note_body);
    g_pending_note_title = t;
    g_pending_note_body  = b;
    return 1;
}

/* ---- Step 8: redactions (Servirtium "mutation operations") -----------
 *
 * Pattern → replacement substitutions applied at flush time, against
 * specific fields of each interaction. The in-memory tape stays
 * pristine (so the SUT sees real responses during recording);
 * redactions only affect what gets written to disk and what gets
 * compared in flush_or_check.
 *
 * v0.1: substring match only (no regex). Field selectors limited to
 * what the recorder actually captures today — `path` and
 * `response_body`. Adding `request_headers` etc. is straightforward
 * once those fields land in the recorder.
 *
 * Use case: scrub Authorization tokens, session cookies, API keys
 * embedded in URLs, server-issued ids that would otherwise leak
 * into a public repo. Test author calls vcr.redact(field, pattern,
 * replacement) once per pattern before flush; the flush walks each
 * interaction and applies every registered redaction.
 * -------------------------------------------------------------------- */

#define VCR_FIELD_PATH          1
#define VCR_FIELD_RESPONSE_BODY 2
/* Future: VCR_FIELD_REQUEST_HEADERS, REQUEST_BODY, RESPONSE_HEADERS,
 * once the recorder captures those. */

typedef struct Redaction {
    int   field;        /* VCR_FIELD_* */
    char* pattern;      /* substring to match, owned */
    char* replacement;  /* what to put in its place, owned */
} Redaction;

static Redaction* g_redactions     = NULL;
static int        g_redactions_n   = 0;
static int        g_redactions_cap = 0;

static void redactions_free_storage(void) {
    if (g_redactions) {
        for (int i = 0; i < g_redactions_n; i++) {
            free(g_redactions[i].pattern);
            free(g_redactions[i].replacement);
        }
        free(g_redactions);
        g_redactions = NULL;
    }
    g_redactions_n = 0;
    g_redactions_cap = 0;
}

int vcr_add_redaction(int field, const char* pattern, const char* replacement) {
    if (field != VCR_FIELD_PATH && field != VCR_FIELD_RESPONSE_BODY) {
        tape_set_err("vcr_add_redaction: unsupported field selector");
        return 0;
    }
    if (!pattern) { tape_set_err("vcr_add_redaction: null pattern"); return 0; }
    if (g_redactions_n >= g_redactions_cap) {
        int new_cap = g_redactions_cap ? g_redactions_cap * 2 : 4;
        Redaction* bigger = (Redaction*)realloc(g_redactions, sizeof(Redaction) * (size_t)new_cap);
        if (!bigger) { tape_set_err("OOM growing redactions"); return 0; }
        g_redactions = bigger;
        g_redactions_cap = new_cap;
    }
    Redaction* r = &g_redactions[g_redactions_n];
    r->field = field;
    r->pattern = strdup(pattern);
    r->replacement = strdup(replacement ? replacement : "");
    if (!r->pattern || !r->replacement) {
        free(r->pattern); free(r->replacement);
        tape_set_err("OOM appending redaction");
        return 0;
    }
    g_redactions_n++;
    return 1;
}

/* Step 12: optional markdown formatting toggles for the recorder.
 * Read by the Aether-side emitter via vcr_get_indent_code_blocks /
 * vcr_get_emphasize_http_verbs. Defaults off so existing tapes
 * round-trip byte-identical when no toggle is set. */
static int g_indent_code_blocks  = 0;
static int g_emphasize_http_verbs = 0;

/* Emitter ported to Aether — see std/http/server/vcr/module.ae's
 * emit_tape() / emit_one_interaction() / emit_code_block(). The
 * Aether emitter walks the C-side tape via vcr_get_* accessors,
 * builds the markdown blob, and writes it via fs.write. The C
 * emitter that lived here previously was deleted in the same
 * commit that introduced the Aether one. */

/* Drop all registered redactions. Useful to call between tests in
 * the same process so a redaction set doesn't leak across them. */
void vcr_clear_redactions(void) {
    redactions_free_storage();
}

/* Step 12 setters — opt the recorder into the alternative markdown
 * forms. Default off → fenced blocks + bare verbs (current
 * behavior, byte-identical re-flush of existing tapes). Playback
 * tolerates either form regardless of these toggles. */
void vcr_set_indent_code_blocks(int on)   { g_indent_code_blocks  = on ? 1 : 0; }
void vcr_set_emphasize_http_verbs(int on) { g_emphasize_http_verbs = on ? 1 : 0; }

/* ---- Step 11: static-content serving --------------------------------
 *
 * Mark an on-disk directory as bypass-the-tape territory. Every
 * incoming GET whose path starts with `mount_path` is served from
 * disk (with the existing http_serve_file's mime-type detection
 * and traversal-prevention) and never consulted against the tape.
 *
 * Use case: Selenium/Cypress/Playwright UI tests where the page
 * is the SUT — letting the tape capture every CSS/JS/image asset
 * is noise. Point the static-content config at the local build
 * artifacts directory; the tape stays focused on the actual
 * domain-API exchanges the test cares about.
 *
 * Lookup model: dispatcher's wildcard route `<mount>` followed by
 * /-star is bound
 * to vcr_static_dispatch with a heap StaticMount* as user_data.
 * The handler strips the mount prefix from req->path, joins
 * against fs_dir, and forwards to http_serve_file (whose realpath
 * + mime-type machinery we get for free).
 *
 * v0.1: GET only (matches the spec). Multiple non-overlapping
 * mounts are fine. Overlapping mounts (e.g. /a and /a/b) work in
 * route-registration order — first registered wins.
 * -------------------------------------------------------------------- */

typedef struct StaticMount {
    char* mount_path;    /* request prefix, e.g. "/static" — owned, no trailing slash */
    char* fs_dir;        /* on-disk dir, e.g. "/tmp/static_root" — owned, no trailing slash */
} StaticMount;

static StaticMount* g_mounts     = NULL;
static int          g_mounts_n   = 0;
static int          g_mounts_cap = 0;

static void mounts_free_storage(void) {
    if (g_mounts) {
        for (int i = 0; i < g_mounts_n; i++) {
            free(g_mounts[i].mount_path);
            free(g_mounts[i].fs_dir);
        }
        free(g_mounts);
        g_mounts = NULL;
    }
    g_mounts_n = 0;
    g_mounts_cap = 0;
}

/* Strip a single trailing '/' (other than a lone-root path) so
 * concatenation doesn't double-slash. mutates `s` in place. */
static void strip_trailing_slash(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    if (n > 1 && s[n - 1] == '/') s[n - 1] = '\0';
}

/* Dispatcher for a static-mount route. user_data is the
 * StaticMount* registered alongside it. */
static void vcr_static_dispatch(void* req, void* res, void* ud) {
    StaticMount* m = (StaticMount*)ud;
    if (!m) {
        http_response_set_status(res, 500);
        http_response_set_body(res, "static dispatch: null mount");
        return;
    }
    const char* path = http_request_path(req);
    if (!path) {
        http_response_set_status(res, 400);
        http_response_set_body(res, "static dispatch: null request path");
        return;
    }

    /* Strip the mount prefix. Both `mount_path` and `path` start
     * with '/'; after strlen(mount_path) chars, what remains is
     * the asset's relative path under fs_dir (with a leading
     * '/' if the request had it). */
    size_t mlen = strlen(m->mount_path);
    if (strncmp(path, m->mount_path, mlen) != 0) {
        http_response_set_status(res, 500);
        http_response_set_body(res, "static dispatch: prefix mismatch (router bug)");
        return;
    }
    const char* rel = path + mlen;
    if (*rel == '/') rel++;
    if (*rel == '\0') rel = "index.html";

    /* Belt-and-braces traversal guard. http_serve_file's
     * realpath check below also catches escapes, but the
     * percent-encoded variants need this string-level rejection
     * up front. Mirrors http_serve_static's checks. */
    if (strstr(rel, "..") != NULL
        || strstr(rel, "%2e") != NULL || strstr(rel, "%2E") != NULL
        || strstr(rel, "%2f") != NULL || strstr(rel, "%2F") != NULL
        || strstr(rel, "%5c") != NULL || strstr(rel, "%5C") != NULL
        || strstr(rel, "\\") != NULL) {
        http_response_set_status(res, 403);
        http_response_set_body(res, "403 - Forbidden");
        return;
    }

    char filepath[1024];
    int n = snprintf(filepath, sizeof(filepath), "%s/%s", m->fs_dir, rel);
    if (n < 0 || (size_t)n >= sizeof(filepath)) {
        http_response_set_status(res, 414);
        http_response_set_body(res, "414 - URI Too Long");
        return;
    }
    http_serve_file(res, filepath);
}

/* Register a static-content mount. Append to g_mounts; the
 * actual route registration happens in vcr_register_routes
 * (which is called by vcr.load — load order is: register tape
 * routes, then mounts; routes overlap is fine since
 * mount-rooted requests don't appear in tapes by definition). */
int vcr_add_static_content(const char* mount_path, const char* fs_dir) {
    if (!mount_path || !fs_dir) {
        tape_set_err("vcr_add_static_content: null mount_path or fs_dir");
        return 0;
    }
    if (mount_path[0] != '/') {
        tape_set_err("vcr_add_static_content: mount_path must start with '/'");
        return 0;
    }
    if (g_mounts_n >= g_mounts_cap) {
        int new_cap = g_mounts_cap ? g_mounts_cap * 2 : 4;
        StaticMount* bigger = (StaticMount*)realloc(g_mounts,
                                  sizeof(StaticMount) * (size_t)new_cap);
        if (!bigger) { tape_set_err("OOM growing mounts"); return 0; }
        g_mounts = bigger;
        g_mounts_cap = new_cap;
    }
    StaticMount* m = &g_mounts[g_mounts_n];
    m->mount_path = strdup(mount_path);
    m->fs_dir     = strdup(fs_dir);
    if (!m->mount_path || !m->fs_dir) {
        free(m->mount_path); free(m->fs_dir);
        tape_set_err("OOM appending mount");
        return 0;
    }
    strip_trailing_slash(m->mount_path);
    strip_trailing_slash(m->fs_dir);
    g_mounts_n++;
    return 1;
}

/* Drop all static-content mounts. The route registrations against
 * the http server stay live until the server is torn down — calling
 * this between tests prevents stale mount config bleeding into
 * the next test, but the recommended pattern is one VCR per test. */
void vcr_clear_static_content(void) {
    mounts_free_storage();
}

/* Register the wildcard routes for every mount with the http
 * server. Called from vcr_register_routes() after the tape
 * routes go in. */
static void register_static_routes(void* server) {
    if (!server) return;
    for (int i = 0; i < g_mounts_n; i++) {
        char pattern[1024];
        int n = snprintf(pattern, sizeof(pattern), "%s/*", g_mounts[i].mount_path);
        if (n < 0 || (size_t)n >= sizeof(pattern)) continue;
        http_server_add_route(server, "GET", pattern,
                              (void*)vcr_static_dispatch, &g_mounts[i]);
    }
}

/* vcr_flush_to_tape and vcr_flush_or_check_raw deleted in the
 * C→Aether emitter port. Their replacements live in module.ae as
 * vcr.flush() and vcr.flush_or_check(), built on top of the Aether
 * emit_tape() and fs.read / fs.write. The dispatcher and the
 * tape-storage globals stayed in C since the http_server hands us
 * function pointers, but everything that's pure string assembly
 * is Aether now. */

/* ---- Step 10: request normalization + match ----
 *
 * Goal: detect when an incoming request differs from what the
 * tape was recorded against (extra header, missing header, wrong
 * value, different body), and fail loudly with a diagnostic the
 * test reads via vcr.last_error().
 *
 * Both sides — the recorded blob in `e->req_headers` and the
 * incoming request's headers — are reduced to a common canonical
 * form before comparison: each header rendered as "Name: Value"
 * on its own line, sorted ascending by lowercased name, with
 * Host stripped (Host is always test-host-controlled and would
 * cause false positives on every record-vs-replay diff).
 *
 * The recording side is responsible for emitting the canonical
 * form into the tape; the dispatcher only normalizes the live
 * incoming side. Tapes hand-edited by humans aren't guaranteed
 * canonical — the spec's broken_recordings/ examples have one
 * header so order doesn't matter, but adding multi-header tests
 * would need the loaded blob normalized too. v0.1 leaves that
 * for future work; today's hand-edited tapes are short enough
 * to keep canonical by inspection.
 */

static int icmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* qsort comparator over an array of "Name: Value" lines (no
 * trailing newline). Sort key is the name (everything before
 * the first ':'), case-insensitive. */
static int line_cmp(const void* lhs, const void* rhs) {
    const char* a = *(const char* const*)lhs;
    const char* b = *(const char* const*)rhs;
    return icmp(a, b);
}


/* Build the canonical "Name: Value\n..." blob from an
 * HttpRequest's parallel keys/values arrays. Drops Host.
 * Returns malloc'd string the caller frees, or NULL on OOM. */
static char* normalize_live_headers(const VcrHttpRequestPrefix* r) {
    if (r->header_count <= 0) return strdup("");

    /* Collect non-Host lines into a heap array. */
    char** lines = (char**)calloc((size_t)r->header_count, sizeof(char*));
    if (!lines) return NULL;
    int n_lines = 0;
    for (int i = 0; i < r->header_count; i++) {
        const char* key = r->header_keys[i];
        const char* val = r->header_values[i] ? r->header_values[i] : "";
        if (!key) continue;
        if (icmp(key, "Host") == 0) continue;
        size_t len = strlen(key) + 2 + strlen(val) + 1;
        char* line = (char*)malloc(len);
        if (!line) {
            for (int j = 0; j < n_lines; j++) free(lines[j]);
            free(lines);
            return NULL;
        }
        snprintf(line, len, "%s: %s", key, val);
        lines[n_lines++] = line;
    }

    qsort(lines, (size_t)n_lines, sizeof(char*), line_cmp);

    /* Concatenate. */
    size_t total = 0;
    for (int i = 0; i < n_lines; i++) total += strlen(lines[i]) + 1; /* line + \n */
    char* out = (char*)malloc(total + 1);
    if (!out) {
        for (int i = 0; i < n_lines; i++) free(lines[i]);
        free(lines);
        return NULL;
    }
    char* dst = out;
    for (int i = 0; i < n_lines; i++) {
        size_t l = strlen(lines[i]);
        memcpy(dst, lines[i], l);
        dst[l] = '\n';
        dst += l + 1;
        free(lines[i]);
    }
    *dst = '\0';
    free(lines);
    return out;
}

/* True if `s` is empty or contains only whitespace. Used to
 * decide whether a captured field constrains matching at all
 * — the spec encodes "no constraint" as an empty code block. */
static int is_blank(const char* s) {
    if (!s) return 1;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return 0;
        s++;
    }
    return 1;
}

/* Pull the first header name out of `recorded` ("Name: ...\n..."),
 * up to MAX-1 chars, NUL-terminate. Used in diagnostics so we
 * can name a specific offending header in the error string. */
static void first_header_name(const char* recorded, char* out, size_t max) {
    size_t i = 0;
    while (recorded[i] && recorded[i] != ':' && recorded[i] != '\n' && i + 1 < max) {
        out[i] = recorded[i];
        i++;
    }
    out[i] = '\0';
}

void vcr_dispatch(void* req, void* res, void* ud) {
    (void)ud;
    if (g_tape_cursor >= g_tape_n) {
        last_err_set("tape exhausted — SUT made more requests than the tape contains");
        http_response_set_status(res, 599);
        http_response_set_body(res,
            "tape exhausted — SUT made more requests than the tape contains");
        return;
    }
    Interaction* e = &g_tape[g_tape_cursor];

    /* (method, path) match — same as before step 10, just now
     * recorded into g_last_error too. Mismatch returns without
     * advancing cursor. */
    const char* got_method = http_request_method(req);
    const char* got_path   = http_request_path(req);
    if (!got_method || strcmp(got_method, e->method) != 0
        || !got_path || strcmp(got_path, e->path) != 0) {
        char msg[2048];
        snprintf(msg, sizeof(msg),
            "tape mismatch at interaction %d: expected %s %s, got %s %s",
            g_tape_cursor,
            e->method, e->path,
            got_method ? got_method : "(null)",
            got_path   ? got_path   : "(null)");
        last_err_set(msg);
        http_response_set_status(res, 599);
        http_response_set_body(res, msg);
        return;
    }

    /* Step 10: request-headers comparison. Only enforced when the
     * tape captured a non-blank request_headers block — empty means
     * the tape doesn't constrain headers (today's recorder default,
     * pre-step-10 tapes). */
    const VcrHttpRequestPrefix* live_req = (const VcrHttpRequestPrefix*)req;
    if (!is_blank(e->req_headers)) {
        char* live = normalize_live_headers(live_req);
        if (!live) {
            last_err_set("OOM normalizing live request headers");
            http_response_set_status(res, 599);
            http_response_set_body(res, "OOM normalizing live request headers");
            return;
        }
        if (strcmp(live, e->req_headers) != 0) {
            /* Identify the offending header. Walk the recorded blob
             * line by line: if a recorded line isn't in `live`, the
             * recording expected a header the SUT didn't send →
             * "<name> request header was expected but not encountered".
             * If we find no missing-from-live recorded line, walk live
             * for an unexpected one → "<name> request header
             * encountered but not expected". For same-name different-
             * value, → "<name> request header value differed". */
            char hdr_name[128];
            char msg[2048];
            int found_diag = 0;

            /* First sweep: recorded lines not present verbatim in live. */
            const char* p = e->req_headers;
            while (*p && !found_diag) {
                const char* eol = strchr(p, '\n');
                if (!eol) eol = p + strlen(p);
                size_t llen = (size_t)(eol - p);
                if (llen > 0) {
                    /* Reconstruct "<line>\n" so we match a full recorded line
                     * inside the live blob (live ends every line with \n). */
                    char one[512];
                    if (llen < sizeof(one) - 2) {
                        memcpy(one, p, llen);
                        one[llen] = '\n';
                        one[llen + 1] = '\0';
                        if (!strstr(live, one)) {
                            /* This recorded header is missing from live.
                             * Check whether the same NAME appears with a
                             * different value (value-differed) vs missing
                             * outright. */
                            const char* colon = (const char*)memchr(p, ':', llen);
                            if (colon) {
                                size_t nlen = (size_t)(colon - p);
                                if (nlen + 2 < sizeof(one)) {
                                    memcpy(one, p, nlen);
                                    one[nlen] = ':';
                                    one[nlen + 1] = '\0';
                                    /* Live blob always renders "Name: ..." */
                                    char* maybe = strstr(live, one);
                                    if (maybe && (maybe == live || maybe[-1] == '\n')) {
                                        first_header_name(p, hdr_name, sizeof(hdr_name));
                                        snprintf(msg, sizeof(msg),
                                            "interaction %d: '%s' request header value differed",
                                            g_tape_cursor, hdr_name);
                                        found_diag = 1;
                                        break;
                                    }
                                }
                            }
                            first_header_name(p, hdr_name, sizeof(hdr_name));
                            snprintf(msg, sizeof(msg),
                                "interaction %d: '%s' request header was expected but not encountered",
                                g_tape_cursor, hdr_name);
                            found_diag = 1;
                        }
                    }
                }
                if (!*eol) break;
                p = eol + 1;
            }

            /* Second sweep: live lines not in recorded → unexpected. */
            if (!found_diag) {
                const char* lp = live;
                while (*lp && !found_diag) {
                    const char* eol = strchr(lp, '\n');
                    if (!eol) eol = lp + strlen(lp);
                    size_t llen = (size_t)(eol - lp);
                    if (llen > 0 && llen < 510) {
                        char one[512];
                        memcpy(one, lp, llen);
                        one[llen] = '\n';
                        one[llen + 1] = '\0';
                        if (!strstr(e->req_headers, one)) {
                            first_header_name(lp, hdr_name, sizeof(hdr_name));
                            snprintf(msg, sizeof(msg),
                                "interaction %d: '%s' request header encountered but not expected",
                                g_tape_cursor, hdr_name);
                            found_diag = 1;
                        }
                    }
                    if (!*eol) break;
                    lp = eol + 1;
                }
            }

            if (!found_diag) {
                snprintf(msg, sizeof(msg),
                    "interaction %d: request headers differ — recorded:\n%s\nlive:\n%s",
                    g_tape_cursor, e->req_headers, live);
            }
            last_err_set(msg);
            free(live);
            http_response_set_status(res, 599);
            http_response_set_body(res, msg);
            return;
        }
        free(live);
    }

    /* Step 10: request-body comparison. Same opt-in semantics. */
    if (!is_blank(e->req_body)) {
        const char* live_body = live_req->body ? live_req->body : "";
        if (strcmp(live_body, e->req_body) != 0) {
            char msg[2048];
            snprintf(msg, sizeof(msg),
                "interaction %d: request body different to expectation — "
                "recorded=%zu bytes, live=%zu bytes",
                g_tape_cursor,
                strlen(e->req_body),
                strlen(live_body));
            last_err_set(msg);
            http_response_set_status(res, 599);
            http_response_set_body(res, msg);
            return;
        }
    }

    http_response_set_status(res, e->status);
    if (e->content_type) {
        http_response_set_header(res, "Content-Type", e->content_type);
    }
    http_response_set_body(res, e->body);

    g_tape_cursor++;
}

/* Step 10: surface the most recent dispatch mismatch. The test's
 * tearDown hook calls vcr.last_error() to confirm the right thing
 * happened (or empty, if no mismatch occurred this run).
 * Returns "" when nothing has been flagged. */
const char* vcr_last_error(void) {
    return g_last_error ? g_last_error : "";
}

/* Clear the last-error slot. Call between subtests in the same
 * process so a flagged mismatch doesn't bleed across them. */
void vcr_clear_last_error(void) {
    free(g_last_error);
    g_last_error = NULL;
}

/* ---- Tape-iteration accessors (Aether-side emitter consumes these) ----
 *
 * The emitter is being ported to Aether. These accessors let the
 * Aether code walk the in-memory tape one Interaction at a time
 * without crossing a struct boundary. Each returns "" (not NULL)
 * for absent / empty fields so the Aether side can do flat string
 * concatenation without null guards. Notes return NULL-equivalent
 * empty strings; vcr_get_note_present() distinguishes "no note"
 * from "note with empty body". Same for content_type. */

int vcr_get_tape_count(void) { return g_tape_n; }

static const char* safe(const char* s) { return s ? s : ""; }

const char* vcr_get_method(int i) {
    if (i < 0 || i >= g_tape_n) return "";
    return safe(g_tape[i].method);
}
const char* vcr_get_path(int i) {
    if (i < 0 || i >= g_tape_n) return "";
    return safe(g_tape[i].path);
}
int vcr_get_status(int i) {
    if (i < 0 || i >= g_tape_n) return 0;
    return g_tape[i].status;
}
const char* vcr_get_content_type(int i) {
    if (i < 0 || i >= g_tape_n) return "";
    return safe(g_tape[i].content_type);
}
const char* vcr_get_body(int i) {
    if (i < 0 || i >= g_tape_n) return "";
    return safe(g_tape[i].body);
}
const char* vcr_get_req_headers(int i) {
    if (i < 0 || i >= g_tape_n) return "";
    return safe(g_tape[i].req_headers);
}
const char* vcr_get_req_body(int i) {
    if (i < 0 || i >= g_tape_n) return "";
    return safe(g_tape[i].req_body);
}
int vcr_get_note_present(int i) {
    if (i < 0 || i >= g_tape_n) return 0;
    return g_tape[i].note_title ? 1 : 0;
}
const char* vcr_get_note_title(int i) {
    if (i < 0 || i >= g_tape_n) return "";
    return safe(g_tape[i].note_title);
}
const char* vcr_get_note_body(int i) {
    if (i < 0 || i >= g_tape_n) return "";
    return safe(g_tape[i].note_body);
}

/* Toggle accessors — the Aether emitter reads these when deciding
 * which markdown form to emit. */
int vcr_get_indent_code_blocks(void)   { return g_indent_code_blocks; }
int vcr_get_emphasize_http_verbs(void) { return g_emphasize_http_verbs; }

/* ---- Tape setters (Aether-side parser populates the tape via these) ----
 *
 * The parser is being ported to Aether. These setters let the Aether
 * code build up the in-memory tape without crossing the struct
 * boundary. Single all-fields setter avoids per-field setters
 * (which would need a "current interaction" cursor). Empty strings
 * for absent fields — the existing tape_append takes "" the same
 * way and the dispatcher skips matching when fields are empty.
 *
 * vcr_aether_clear_tape resets storage so the parser can rebuild
 * cleanly; it's the equivalent of what vcr_load_tape does at the
 * top of its body before parsing. */
void vcr_aether_clear_tape(void) {
    tape_free_storage();
    /* vcr_load_tape sets g_tape_err to "" so vcr_load_err returns
     * an empty string when called between a successful load and
     * the next parse failure. Mirror that behavior here. */
    g_tape_err = strdup("");
}

int vcr_aether_append_interaction(const char* method, const char* path,
                                  int status, const char* content_type,
                                  const char* body,
                                  const char* req_headers, const char* req_body,
                                  const char* note_title, const char* note_body) {
    /* The pending-note slot (used by record-mode) must NOT be
     * drained by the parser — loaded tapes carry their own per-
     * interaction notes. We stage the note pair into the slot
     * directly so tape_append's drain logic places it on the
     * about-to-be-appended interaction. */
    if (note_title && *note_title) {
        free(g_pending_note_title);
        free(g_pending_note_body);
        g_pending_note_title = strdup(note_title);
        g_pending_note_body  = strdup(note_body ? note_body : "");
    }
    /* Empty content_type → store NULL so the dispatcher's existing
     * "if (e->content_type)" branch behaves correctly. */
    const char* ct = (content_type && *content_type) ? content_type : NULL;
    if (tape_append(method, path, status, body, ct,
                    req_headers ? req_headers : "",
                    req_body    ? req_body    : "") != 0) {
        tape_set_err("OOM appending parsed interaction");
        return 0;
    }
    return 1;
}

/* Set the load error string. Aether parser calls this when it
 * detects a malformed tape so vcr.load_err() surfaces a useful
 * diagnostic. */
void vcr_aether_set_load_err(const char* msg) {
    tape_set_err(msg ? msg : "");
}

/* Redaction iteration — the Aether emitter applies redactions
 * before writing path / response-body fields. */
int vcr_get_redaction_count(void) { return g_redactions_n; }
int vcr_get_redaction_field(int i) {
    if (i < 0 || i >= g_redactions_n) return 0;
    return g_redactions[i].field;
}
const char* vcr_get_redaction_pattern(int i) {
    if (i < 0 || i >= g_redactions_n) return "";
    return safe(g_redactions[i].pattern);
}
const char* vcr_get_redaction_replacement(int i) {
    if (i < 0 || i >= g_redactions_n) return "";
    return safe(g_redactions[i].replacement);
}
