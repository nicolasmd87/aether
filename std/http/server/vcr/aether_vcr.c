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

/* And the routing extern — used by vcr_register_routes(). */
extern void http_server_get(void* server, const char* path, void* handler, void* user_data);

/* ---- Tape storage --------------------------------------------------- */

typedef struct Interaction {
    char* method;       /* "GET" — owned, NUL-terminated */
    char* path;         /* "/path/to/resource" — owned */
    int   status;       /* 200, 404, ... */
    char* body;         /* response body — owned, NUL-terminated; "" if empty */
    char* content_type; /* may be NULL */
} Interaction;

static Interaction* g_tape       = NULL;
static int          g_tape_n     = 0;
static int          g_tape_cap   = 0;
static int          g_tape_cursor = 0;
static char*        g_tape_err   = NULL;

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

static int tape_append(const char* method, const char* path,
                       int status, const char* body, const char* content_type) {
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
    if (!e->method || !e->path || !e->body || (content_type && !e->content_type)) {
        free(e->method); free(e->path); free(e->body); free(e->content_type);
        return -1;
    }
    g_tape_n++;
    return 0;
}

/* ---- Helpers --------------------------------------------------------- */

/* Read a whole file into a heap buffer (NUL-terminated). Returns
 * NULL on I/O failure with g_tape_err populated. */
static char* slurp(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "cannot open tape file: %s", path);
        tape_set_err(msg);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n < 0) { fclose(fp); tape_set_err("ftell failed"); return NULL; }
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(fp); tape_set_err("OOM slurping tape"); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, fp);
    fclose(fp);
    buf[rd] = '\0';
    return buf;
}

/* Allocate a copy of [start, end) — used for slicing substrings out
 * of the slurped tape buffer. */
static char* slice_dup(const char* start, const char* end) {
    if (!start || !end || end < start) return NULL;
    size_t n = (size_t)(end - start);
    char* s = (char*)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, start, n);
    s[n] = '\0';
    return s;
}

/* Trim trailing whitespace (newlines, CRs, spaces, tabs) in place. */
static void rtrim(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t')) {
        s[--n] = '\0';
    }
}

/* Find the contents of the (first) fenced code block within [start, end).
 * Returns a freshly-allocated string containing the block's body
 * (without the surrounding ``` lines), or NULL if none found. Trailing
 * newline trimmed. */
static char* extract_code_block(const char* start, const char* end) {
    /* Look for "```\n" (open fence). */
    const char* open = start;
    while (open < end) {
        const char* nl = (const char*)memchr(open, '\n', (size_t)(end - open));
        if (!nl) break;
        /* Check whether the line starts with ``` (ignoring trailing
         * language tag like ```xml — we don't care about the tag). */
        if ((nl - open) >= 3 && open[0] == '`' && open[1] == '`' && open[2] == '`') {
            const char* body_start = nl + 1;
            /* Find the close fence: line beginning with ```. */
            const char* p = body_start;
            while (p < end) {
                if ((end - p) >= 3 && p[0] == '`' && p[1] == '`' && p[2] == '`') {
                    /* Body ends just before this line. The line
                     * itself starts at the previous \n + 1; back
                     * up to find that \n. */
                    const char* body_end = p;
                    /* Strip the newline immediately before the close fence. */
                    if (body_end > body_start && body_end[-1] == '\n') body_end--;
                    return slice_dup(body_start, body_end);
                }
                const char* next = (const char*)memchr(p, '\n', (size_t)(end - p));
                if (!next) break;
                p = next + 1;
            }
            return NULL;  /* unclosed fence */
        }
        open = nl + 1;
    }
    return NULL;
}

/* Parse the interaction header line ("GET /path") into method + path.
 * Either the first space terminates the path, or the line ends.
 * Returns 0 on success, -1 on malformed input. */
static int parse_interaction_header(const char* line_start, const char* line_end,
                                    char** out_method, char** out_path) {
    *out_method = NULL;
    *out_path = NULL;
    /* The interaction header is the chunk after "## Interaction N: "
     * and up to the newline. Format: "METHOD path". */
    const char* sp = (const char*)memchr(line_start, ' ', (size_t)(line_end - line_start));
    if (!sp) return -1;
    *out_method = slice_dup(line_start, sp);
    /* path runs from sp+1 to line_end (rtrim handled by caller via
     * rtrim() on the result). */
    *out_path = slice_dup(sp + 1, line_end);
    if (!*out_method || !*out_path) {
        free(*out_method); free(*out_path);
        *out_method = *out_path = NULL;
        return -1;
    }
    rtrim(*out_method);
    rtrim(*out_path);
    return 0;
}

/* Parse the response-body section header: "Response body recorded
 * for playback (200: text/plain):". Extracts status and content_type
 * by string-search. Returns 0 on success, -1 on malformed.
 * content_type is allocated; method is borrowed (NULL on absence). */
static int parse_response_body_header(const char* line, int* out_status, char** out_content_type) {
    *out_status = 0;
    *out_content_type = NULL;
    /* Find the (...): segment. */
    const char* open_paren = strchr(line, '(');
    const char* close_paren = open_paren ? strchr(open_paren, ')') : NULL;
    if (!open_paren || !close_paren) return -1;
    /* Inside parens: "STATUS: content/type" */
    const char* colon = (const char*)memchr(open_paren + 1, ':', (size_t)(close_paren - open_paren - 1));
    if (!colon) return -1;
    *out_status = atoi(open_paren + 1);
    /* Skip ": " then take up to close_paren. */
    const char* ct_start = colon + 1;
    while (ct_start < close_paren && (*ct_start == ' ' || *ct_start == '\t')) ct_start++;
    *out_content_type = slice_dup(ct_start, close_paren);
    return *out_content_type ? 0 : -1;
}

/* Parse one interaction chunk — text starting just after
 * "## Interaction N: " and ending at the start of the next
 * "## Interaction " (or end of file). */
static int parse_interaction(const char* chunk_start, const char* chunk_end) {
    /* The first line of the chunk is the rest of the interaction
     * header line: "0: GET /path\n...". Skip past the "N: " prefix. */
    const char* nl = (const char*)memchr(chunk_start, '\n', (size_t)(chunk_end - chunk_start));
    if (!nl) { tape_set_err("interaction missing header line"); return -1; }
    /* Skip the digits + colon + space. */
    const char* mp_start = chunk_start;
    while (mp_start < nl && *mp_start >= '0' && *mp_start <= '9') mp_start++;
    if (mp_start < nl && *mp_start == ':') mp_start++;
    while (mp_start < nl && *mp_start == ' ') mp_start++;

    char* method = NULL;
    char* path = NULL;
    if (parse_interaction_header(mp_start, nl, &method, &path) != 0) {
        tape_set_err("malformed interaction header");
        return -1;
    }

    /* Rest of the chunk is sections separated by "\n### ". The
     * response body section carries both the status (in its header
     * line) and the body (in its code block). v0.1 only needs those
     * two — request headers/body and response headers are parsed-and-
     * dropped because matching is method+path only and response
     * header echo is v0.2. */
    int status = 0;
    char* content_type = NULL;
    char* body = NULL;

    const char* p = nl + 1;
    while (p < chunk_end) {
        /* Find next "\n### " (or "### " right at the start of p). */
        const char* sect = NULL;
        if ((chunk_end - p) >= 4 && memcmp(p, "### ", 4) == 0) {
            sect = p;
        } else {
            sect = strstr(p, "\n### ");
            if (sect) sect++;  /* skip the leading \n */
        }
        if (!sect || sect >= chunk_end) break;

        /* Section name runs from sect (which starts at "### ...") to
         * the first \n. */
        const char* sect_nl = (const char*)memchr(sect, '\n', (size_t)(chunk_end - sect));
        if (!sect_nl) break;
        const char* sect_name_start = sect + 4;  /* skip "### " */
        size_t sect_name_len = (size_t)(sect_nl - sect_name_start);

        /* Determine the section's body extent — up to the next "\n### "
         * or chunk_end. */
        const char* next_sect = strstr(sect_nl, "\n### ");
        const char* sect_body_end = next_sect && next_sect < chunk_end ? next_sect : chunk_end;

        /* Only the response-body section is interesting in v0.1. */
        if (sect_name_len > 25
            && memcmp(sect_name_start, "Response body recorded for", 26) == 0) {
            /* The header line carries (status: content/type). */
            char* line = slice_dup(sect_name_start, sect_nl);
            if (!line) { tape_set_err("OOM"); goto fail; }
            if (parse_response_body_header(line, &status, &content_type) != 0) {
                free(line);
                tape_set_err("malformed response body header");
                goto fail;
            }
            free(line);
            /* Body is the contents of the fenced code block in the
             * remainder of this section. */
            body = extract_code_block(sect_nl + 1, sect_body_end);
            if (!body) {
                tape_set_err("response body section has no fenced code block");
                goto fail;
            }
        }

        p = sect_body_end;
    }

    if (status == 0) {
        tape_set_err("interaction has no Response body section");
        goto fail;
    }
    /* body could legitimately be empty string — that's fine. */
    if (!body) body = strdup("");

    if (tape_append(method, path, status, body, content_type) != 0) {
        tape_set_err("OOM appending interaction");
        goto fail;
    }

    free(method); free(path); free(body); free(content_type);
    return 0;

fail:
    free(method); free(path); free(body); free(content_type);
    return -1;
}

/* ---- Forward decl ---------------------------------------------------- */

void vcr_dispatch(void* req, void* res, void* ud);

/* ---- Aether-side externs -------------------------------------------- */

int vcr_load_tape(const char* path) {
    if (!path) { tape_set_err("null tape path"); return 0; }
    tape_free_storage();
    g_tape_err = strdup("");

    char* buf = slurp(path);
    if (!buf) return 0;  /* g_tape_err already set */

    /* Split on "\n## Interaction " to get N chunks. The first chunk
     * (before any "## Interaction ") is the file's preamble (could be
     * a top-level title, a description, anything) — we discard it. */
    const char* p = buf;
    const char* DELIM = "\n## Interaction ";
    const size_t DELIM_LEN = strlen(DELIM);

    /* Special case: tape starts with "## Interaction " on the very
     * first line (no preceding newline). Look for that explicitly. */
    if (strncmp(buf, "## Interaction ", 15) == 0) {
        p = buf + 15;
    } else {
        const char* first = strstr(buf, DELIM);
        if (!first) {
            tape_set_err("tape contains no '## Interaction ' headers");
            free(buf);
            return 0;
        }
        p = first + DELIM_LEN;
    }

    while (p < buf + strlen(buf)) {
        const char* next = strstr(p, DELIM);
        const char* chunk_end = next ? next : (buf + strlen(buf));
        if (parse_interaction(p, chunk_end) != 0) {
            free(buf);
            return 0;  /* g_tape_err already set */
        }
        if (!next) break;
        p = next + DELIM_LEN;
    }

    free(buf);
    if (g_tape_n == 0) {
        tape_set_err("tape parsed but yielded zero interactions");
        return 0;
    }
    return 1;
}

const char* vcr_load_err(void) {
    return g_tape_err ? g_tape_err : "";
}

int vcr_tape_length(void) {
    return g_tape_n;
}

/* Walk the tape and call http_server_get for each unique path. Same
 * dispatcher (vcr_dispatch) handles every route; it matches against
 * the cursor and emits the recorded response. */
void vcr_register_routes(void* server) {
    if (!server) return;
    /* Dedup by path — multiple interactions can share a path
     * (dispatcher advances through them in cursor order). O(N²),
     * fine for any plausible tape size. */
    for (int i = 0; i < g_tape_n; i++) {
        int already = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(g_tape[i].path, g_tape[j].path) == 0) {
                already = 1;
                break;
            }
        }
        if (!already) {
            http_server_get(server, g_tape[i].path, (void*)vcr_dispatch, NULL);
        }
    }
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

int vcr_record_interaction(const char* method, const char* path,
                           int status, const char* content_type, const char* body) {
    if (!method || !path) { tape_set_err("null method or path"); return 0; }
    if (tape_append(method, path, status, body, content_type) != 0) {
        tape_set_err("OOM appending captured interaction");
        return 0;
    }
    return 1;
}

/* Helper: emit a fenced code block. The body is written verbatim;
 * if the body itself contains a "```" line the output won't round-trip
 * cleanly, but that's a Servirtium-spec-wide problem (every
 * implementation has the same constraint). v0.1 doesn't try to
 * escape — production Servirtium tapes don't carry ``` in bodies
 * in any of the climate-API or similar fixtures. */
static void emit_code_block(FILE* fp, const char* body) {
    fputs("```\n", fp);
    if (body && *body) {
        fputs(body, fp);
        /* Ensure the body ends with a newline before the close fence. */
        size_t n = strlen(body);
        if (n > 0 && body[n - 1] != '\n') fputc('\n', fp);
    }
    fputs("```\n", fp);
}

/* Emit the in-memory tape (g_tape, g_tape_n) to an open FILE*.
 * Both vcr_flush_to_tape and vcr_flush_or_check_raw use this. */
static void emit_tape_to_fp(FILE* fp) {
    for (int i = 0; i < g_tape_n; i++) {
        Interaction* e = &g_tape[i];
        fprintf(fp, "## Interaction %d: %s %s\n\n", i, e->method, e->path);

        /* v0.1 doesn't capture request headers/body during recording —
         * the request shape is path-only and matching is method+path
         * only on replay. We still emit the section markers so the
         * tape stays a valid Servirtium document and is parseable by
         * other implementations. Empty code blocks are legal. */
        fputs("### Request headers recorded for playback:\n\n", fp);
        emit_code_block(fp, "");

        fputs("### Request body recorded for playback ():\n\n", fp);
        emit_code_block(fp, "");

        fputs("### Response headers recorded for playback:\n\n", fp);
        if (e->content_type) {
            char hdr[512];
            snprintf(hdr, sizeof(hdr), "Content-Type: %s", e->content_type);
            emit_code_block(fp, hdr);
        } else {
            emit_code_block(fp, "");
        }

        /* Response body section — its header line carries the status
         * and content-type, mirroring what the parser reads. */
        fprintf(fp, "### Response body recorded for playback (%d: %s):\n\n",
                e->status, e->content_type ? e->content_type : "text/plain");
        emit_code_block(fp, e->body);

        fputc('\n', fp);
    }
}

int vcr_flush_to_tape(const char* path) {
    if (!path) { tape_set_err("null tape output path"); return 0; }
    FILE* fp = fopen(path, "wb");
    if (!fp) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "cannot open tape file for writing: %s", path);
        tape_set_err(msg);
        return 0;
    }
    emit_tape_to_fp(fp);
    if (fclose(fp) != 0) {
        tape_set_err("close failed while writing tape");
        return 0;
    }
    return 1;
}

/* Step 4 of the Servirtium roadmap — re-record check.
 *
 * If `path` doesn't exist on disk, behaves identically to
 * vcr_flush_to_tape (writes the tape, returns 1). If it does
 * exist, writes the in-memory tape to a sibling temp file then
 * compares byte-for-byte against the on-disk version. Equal →
 * delete the temp, return 1. Different → keep the temp at
 * `<path>.actual` for inspection, populate g_tape_err with a
 * diagnostic, return 0.
 *
 * The "keep the differing temp around" choice matches what
 * Servirtium's Java implementation does: the developer can diff
 * the expected (committed) tape against the actual (just-recorded)
 * tape and decide whether the upstream changed (update the tape)
 * or the client drifted (fix the bug). Either decision is informed
 * by having both files on disk. */
int vcr_flush_or_check_raw(const char* path) {
    if (!path) { tape_set_err("null tape output path"); return 0; }

    /* If the on-disk tape doesn't exist, this is the first record —
     * just write it. */
    FILE* existing = fopen(path, "rb");
    if (!existing) {
        return vcr_flush_to_tape(path);
    }

    /* Write the in-memory tape to a sibling .actual file. */
    char actual_path[1024];
    int n_written = snprintf(actual_path, sizeof(actual_path), "%s.actual", path);
    if (n_written < 0 || (size_t)n_written >= sizeof(actual_path)) {
        fclose(existing);
        tape_set_err("path too long for .actual sibling");
        return 0;
    }
    FILE* actual = fopen(actual_path, "wb");
    if (!actual) {
        fclose(existing);
        char msg[1100];
        snprintf(msg, sizeof(msg), "cannot open %s for writing", actual_path);
        tape_set_err(msg);
        return 0;
    }
    emit_tape_to_fp(actual);
    if (fclose(actual) != 0) {
        fclose(existing);
        tape_set_err("close failed writing .actual sibling");
        return 0;
    }

    /* Byte-compare the two files. Both are open-and-close in this
     * scope; rewind `existing` to the start since we may have
     * touched it via fopen.
     *
     * We re-open `actual` for read because we just closed the write
     * handle above. */
    FILE* actual_r = fopen(actual_path, "rb");
    if (!actual_r) {
        fclose(existing);
        tape_set_err("cannot reopen .actual for compare");
        return 0;
    }

    int diff_offset = -1;       /* -1 = no difference seen yet */
    long off = 0;
    int diff_byte_existing = 0;
    int diff_byte_actual = 0;
    for (;;) {
        int ce = fgetc(existing);
        int ca = fgetc(actual_r);
        if (ce == EOF && ca == EOF) break;       /* equal, both ended */
        if (ce != ca) {
            diff_offset = (int)off;
            diff_byte_existing = ce;
            diff_byte_actual = ca;
            break;
        }
        off++;
    }
    fclose(existing);
    fclose(actual_r);

    if (diff_offset < 0) {
        /* Equal — clean up the .actual since nobody needs to look at it. */
        remove(actual_path);
        return 1;
    }

    /* Differed — leave `.actual` on disk for inspection, surface
     * the diff location in the error string. */
    char msg[2048];
    snprintf(msg, sizeof(msg),
        "tape mismatch at byte %d: expected 0x%02x ('%c'), got 0x%02x ('%c'). "
        "Wrote new capture to %s; diff against %s to see what changed.",
        diff_offset,
        (unsigned)(diff_byte_existing & 0xff),
        (diff_byte_existing >= 32 && diff_byte_existing < 127) ? (char)diff_byte_existing : '.',
        (unsigned)(diff_byte_actual & 0xff),
        (diff_byte_actual >= 32 && diff_byte_actual < 127) ? (char)diff_byte_actual : '.',
        actual_path, path);
    tape_set_err(msg);
    return 0;
}

void vcr_dispatch(void* req, void* res, void* ud) {
    (void)ud;
    if (g_tape_cursor >= g_tape_n) {
        http_response_set_status(res, 599);
        http_response_set_body(res,
            "tape exhausted — SUT made more requests than the tape contains");
        return;
    }
    Interaction* e = &g_tape[g_tape_cursor];

    /* Strict (method, path) match. Mismatch → 599 with diagnostic
     * body so the failure surfaces at the test's assertion layer
     * rather than as a silent miss. Cursor doesn't advance on
     * mismatch — repeat calls produce the same diagnostic. */
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
        http_response_set_status(res, 599);
        http_response_set_body(res, msg);
        return;
    }

    http_response_set_status(res, e->status);
    if (e->content_type) {
        http_response_set_header(res, "Content-Type", e->content_type);
    }
    http_response_set_body(res, e->body);

    g_tape_cursor++;
}
