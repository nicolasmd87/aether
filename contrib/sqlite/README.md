# contrib.sqlite — SQLite bindings for Aether

Thin veneer over the system `libsqlite3`. Lives in `contrib/` rather
than `std/` because the API surface is opinionated enough that
anchoring one shape in `std/` would force future contributors to work
around it. See [docs/stdlib-vs-contrib.md](../../docs/stdlib-vs-contrib.md)
for the rubric.

## v1 API

```aether
import contrib.sqlite

main() {
    db, err = sqlite.open(":memory:")
    if err != "" { return }
    defer sqlite.close(db)

    // DDL / INSERT / UPDATE — no rows back.
    sqlite.exec(db, "CREATE TABLE users (id INTEGER, name TEXT)")
    sqlite.exec(db, "INSERT INTO users VALUES (1, 'Alice')")
    sqlite.exec(db, "INSERT INTO users VALUES (2, 'Bob')")

    // SELECT — rows materialised into a ResultSet.
    rs, err2 = sqlite.query(db, "SELECT id, name FROM users ORDER BY id")
    if err2 != "" { return }
    defer sqlite.free(rs)

    n = sqlite.row_count(rs)
    r = 0
    while r < n {
        id   = sqlite.cell(rs, r, 0)
        name = sqlite.cell(rs, r, 1)
        println("${id}: ${name}")
        r = r + 1
    }
}
```

## Scope

- `sqlite.open(path) -> (db, err)`
- `sqlite.close(db) -> err`
- `sqlite.exec(db, sql) -> err` — for DDL and no-row DML
- `sqlite.query(db, sql) -> (rs, err)` — for SELECT
- `sqlite.row_count(rs)`, `sqlite.col_count(rs)`, `sqlite.col_name(rs, i)`, `sqlite.cell(rs, row, col)`, `sqlite.free(rs)`

Every cell is returned as a `string`. Numeric columns are rendered
by SQLite's internal text conversion — callers parse them back with
`string.to_int` / `string.to_float` from `std.string` if they need
typed values.

## What's deliberately out of scope for v1

- **Parameter binding** (`?1`, `:name`, etc.). The API would have
  to commit to one shape (positional vs named vs Go-style
  `sqlx`-ish), and that's the decision `contrib/` exists to defer.
  Callers concatenating untrusted input should escape it themselves
  or wait for the next iteration.
- **Streaming results.** Rows are materialised up-front into a flat
  buffer with a hard cap of 100 000 rows per query. Large-result
  workloads should use the raw externs (`sqlite3_prepare_v2` /
  `sqlite3_step`) directly until a streaming API lands.
- **BLOB columns.** v1 exposes only text cells. BLOBs containing
  embedded NULs will be truncated by the `strdup` in the C shim.
  The shim is binary-safe for its own input path (the AetherString
  unwrap helper is wired up), but the output path uses NUL-terminated
  strings because Aether lacks a nullable-byte-buffer type for
  result cells right now.
- **Transactions as first-class operations.** Use `sqlite.exec(db,
  "BEGIN")` / `"COMMIT"` / `"ROLLBACK"` — this is idiomatic in the
  SQLite C API too.
- **Prepared-statement caching, pragmas, attach/detach, backup API.**
  All callable via the raw externs if you need them. Additive future
  work in the same module.

## Build

This module depends on the system `libsqlite3`. There is no
auto-detection in the Aether toolchain (unlike OpenSSL and zlib),
so projects that want SQLite opt in explicitly:

**`aether.toml`:**

```toml
[[bin]]
name = "myapp"
path = "src/main.ae"
extra_sources = ["contrib/sqlite/aether_sqlite.c"]

[build]
link_flags = "-lsqlite3"
```

**Or on the command line:**

```sh
ae build src/main.ae \
  --extra contrib/sqlite/aether_sqlite.c \
  -- -lsqlite3
```

(The trailing `-- -lsqlite3` is a placeholder — `ae build` doesn't
currently accept bare link-flag pass-through on the CLI. Use the
`aether.toml` form.)

## Installing libsqlite3

- **Debian / Ubuntu:** `apt install libsqlite3-dev`
- **Fedora / RHEL:** `dnf install sqlite-devel`
- **macOS (Homebrew):** `brew install sqlite` (already shipped; brew provides pkg-config metadata)
- **Windows (MSYS2):** `pacman -S mingw-w64-x86_64-sqlite3`
- **Alpine:** `apk add sqlite-dev`

When `libsqlite3` isn't installed the user's `gcc` link step fails
with "undefined reference to `sqlite3_open`" — the usual loud
diagnostic. There's no runtime fallback because there's no runtime
to fall back to; without the library the binary never built.

## Test

```sh
sh contrib/sqlite/test_sqlite_roundtrip.sh
```

Runs a 7-case matrix against an in-memory database: open, CREATE
TABLE, two INSERTs, SELECT, column names, cell values, close.
