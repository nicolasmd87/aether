/* contrib/sqlite — thin SQLite veneer for Aether.
 *
 * v1 exposes four functions:
 *   sqlite_open_raw(path)            -> sqlite3*
 *   sqlite_close_raw(db)             -> int (1 on success)
 *   sqlite_exec_raw(db, sql)         -> const char*  (NULL on success, error text on failure)
 *   sqlite_query_raw(db, sql)        -> ResultSet*   (NULL on failure)
 *   sqlite_rs_row_count(rs)          -> int
 *   sqlite_rs_col_count(rs)          -> int
 *   sqlite_rs_col_name(rs, i)        -> const char*
 *   sqlite_rs_cell(rs, row, col)     -> const char*  ("" if NULL in the DB)
 *   sqlite_rs_free(rs)               -> void
 *
 * This is deliberately a C-only dependency — user programs link
 * -lsqlite3 via aether.toml's `[build] link_flags`. Bundling the
 * 4 MiB amalgamation in contrib/ would defeat the point of having
 * moved sqlite to contrib/ in the first place (see
 * docs/stdlib-vs-contrib.md).
 *
 * Parameter binding is deliberately out of scope for v1 — users
 * who need it should either wait for a future API add, or use the
 * extern interface with sqlite3_prepare_v2 / sqlite3_bind_* / step
 * directly. The string-concat path is fine for DDL and for
 * trusted-input INSERT/SELECT.
 */

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

#include "../../std/string/aether_string.h"

/* -----------------------------------------------------------------
 * Helper: unwrap an Aether `string` param — may be an AetherString*
 * or a plain char*. Same dispatch the std/ modules added for
 * binary-safe I/O (v0.86.0 / v0.87.0 / v0.88.0).
 * ----------------------------------------------------------------- */
static inline const char* sq_str(const char* s) {
    if (!s) return NULL;
    if (is_aether_string(s)) return ((const AetherString*)s)->data;
    return s;
}

/* -----------------------------------------------------------------
 * Per-query result set. Materialised up-front: sqlite3_exec's
 * callback fills rows/cols into a flat row-major buffer, and the
 * Aether layer reads cells by (row, col). Rows are limited to
 * SQLITE_MAX_ROWS to keep a pathological SELECT from OOMing the
 * process — users who need more should switch to streaming (a
 * follow-on API).
 * ----------------------------------------------------------------- */
#define SQLITE_MAX_ROWS 100000

typedef struct ResultSet {
    int ncols;
    int nrows;
    int cap_rows;
    char** col_names;  /* [ncols] — NUL-terminated, owned */
    char** cells;      /* [nrows * ncols] — NUL-terminated, owned; may be empty "" */
    char*  err;        /* NULL on success; owned on failure path */
} ResultSet;

static int query_callback(void* user, int ncols, char** values, char** col_names) {
    ResultSet* rs = (ResultSet*)user;

    if (rs->ncols == 0) {
        rs->ncols = ncols;
        rs->col_names = (char**)calloc((size_t)ncols, sizeof(char*));
        if (!rs->col_names) return 1;
        for (int i = 0; i < ncols; i++) {
            const char* nm = col_names[i] ? col_names[i] : "";
            rs->col_names[i] = strdup(nm);
            if (!rs->col_names[i]) return 1;
        }
    } else if (rs->ncols != ncols) {
        /* Column count shifting between rows would be a sqlite bug,
         * but defensive. */
        return 1;
    }

    if (rs->nrows >= SQLITE_MAX_ROWS) return 1;

    /* Grow the cell buffer geometrically so N rows cost amortised O(N). */
    if (rs->nrows >= rs->cap_rows) {
        int new_cap = rs->cap_rows ? rs->cap_rows * 2 : 16;
        char** bigger = (char**)realloc(rs->cells, (size_t)new_cap * (size_t)ncols * sizeof(char*));
        if (!bigger) return 1;
        rs->cells = bigger;
        rs->cap_rows = new_cap;
    }

    char** row_base = rs->cells + (rs->nrows * ncols);
    for (int i = 0; i < ncols; i++) {
        const char* v = values[i] ? values[i] : "";
        row_base[i] = strdup(v);
        if (!row_base[i]) return 1;
    }
    rs->nrows++;
    return 0;
}

/* -----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------- */

void* sqlite_open_raw(const char* path) {
    const char* p = sq_str(path);
    if (!p) return NULL;
    sqlite3* db = NULL;
    if (sqlite3_open(p, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    return (void*)db;
}

int sqlite_close_raw(void* db) {
    if (!db) return 0;
    return sqlite3_close((sqlite3*)db) == SQLITE_OK ? 1 : 0;
}

/* Returns NULL on success, a newly-allocated error string on
 * failure. Caller (the Aether wrapper) must free with free(). */
char* sqlite_exec_raw(void* db, const char* sql) {
    const char* s = sq_str(sql);
    if (!db || !s) return strdup("null db or sql");

    char* errmsg = NULL;
    int rc = sqlite3_exec((sqlite3*)db, s, NULL, NULL, &errmsg);
    if (rc == SQLITE_OK) {
        if (errmsg) sqlite3_free(errmsg);
        return NULL;
    }
    char* owned;
    if (errmsg) {
        owned = strdup(errmsg);
        sqlite3_free(errmsg);
    } else {
        owned = strdup("sqlite exec failed");
    }
    return owned;
}

void* sqlite_query_raw(void* db, const char* sql) {
    const char* s = sq_str(sql);
    if (!db || !s) return NULL;

    ResultSet* rs = (ResultSet*)calloc(1, sizeof(ResultSet));
    if (!rs) return NULL;

    char* errmsg = NULL;
    int rc = sqlite3_exec((sqlite3*)db, s, query_callback, rs, &errmsg);
    if (rc != SQLITE_OK) {
        /* Free whatever we accumulated before returning NULL so the
         * Aether wrapper can fall through to the error path. */
        if (rs->col_names) {
            for (int i = 0; i < rs->ncols; i++) free(rs->col_names[i]);
            free(rs->col_names);
        }
        if (rs->cells) {
            for (int i = 0; i < rs->nrows * rs->ncols; i++) free(rs->cells[i]);
            free(rs->cells);
        }
        free(rs);
        if (errmsg) sqlite3_free(errmsg);
        return NULL;
    }
    if (errmsg) sqlite3_free(errmsg);
    return (void*)rs;
}

int sqlite_rs_row_count(void* rs_opaque) {
    ResultSet* rs = (ResultSet*)rs_opaque;
    return rs ? rs->nrows : 0;
}

int sqlite_rs_col_count(void* rs_opaque) {
    ResultSet* rs = (ResultSet*)rs_opaque;
    return rs ? rs->ncols : 0;
}

const char* sqlite_rs_col_name(void* rs_opaque, int idx) {
    ResultSet* rs = (ResultSet*)rs_opaque;
    if (!rs || idx < 0 || idx >= rs->ncols) return "";
    return rs->col_names[idx] ? rs->col_names[idx] : "";
}

const char* sqlite_rs_cell(void* rs_opaque, int row, int col) {
    ResultSet* rs = (ResultSet*)rs_opaque;
    if (!rs || row < 0 || row >= rs->nrows) return "";
    if (col < 0 || col >= rs->ncols) return "";
    const char* v = rs->cells[row * rs->ncols + col];
    return v ? v : "";
}

void sqlite_rs_free(void* rs_opaque) {
    ResultSet* rs = (ResultSet*)rs_opaque;
    if (!rs) return;
    if (rs->col_names) {
        for (int i = 0; i < rs->ncols; i++) free(rs->col_names[i]);
        free(rs->col_names);
    }
    if (rs->cells) {
        int total = rs->nrows * rs->ncols;
        for (int i = 0; i < total; i++) free(rs->cells[i]);
        free(rs->cells);
    }
    free(rs);
}
