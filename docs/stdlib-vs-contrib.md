# `std/` vs `contrib/` — placement decisions

When adding a new Aether module that wraps an external C library or
provides a capability not covered by the core language, the first
question is: **does it belong in `std/` or `contrib/`?**

This document captures the rubric used to make that call, and applies
it to the "Zero-C LOC" plan modules (crypto, zlib, sqlite, HTTP client
split). It complements [stdlib-module-pattern.md](stdlib-module-pattern.md),
which covers the *shape* of a module once you know *where* it lives.

## The rubric

Ask four questions. A `std/` module should answer "yes" to all four. If
any one is "no", it belongs in `contrib/`.

1. **Is this something a typical Aether project expects to find in the
   standard library?** The bar is what a developer coming from Go, Rust,
   Python, or Node would consider "batteries included". File I/O, HTTP,
   hashing, compression — yes. A database driver, a JSON schema
   validator, a WebSocket framework — no.

2. **Does it have a single obvious API shape?** `std/` modules don't
   have competing implementations. There is one way to hash with SHA-256;
   the Aether wrapper is a thin Go-style veneer over whatever library
   we picked. A database driver, by contrast, can reasonably expose a
   "prepared statement + bind + fetch" API, an "execute returning rows"
   API, or an ORM-ish builder. Not in `std/`.

3. **Are the dependencies minimal and well-scoped?** A `std/` module
   adds to the baseline cost of building Aether. `OpenSSL` is already a
   dependency (we link `-lssl -lcrypto`), so `std.cryptography` is free.
   `zlib` is similarly ambient on every POSIX box. `SQLite` is a
   4 MiB amalgamation — significant weight for projects that don't
   need it.

4. **Is the API surface stable and small?** `std/` modules have a
   stability commitment — changes ripple to every Aether user. If the
   API has many knobs, many optional parameters, or many places where a
   "just use the library directly" escape hatch makes sense, that
   friction belongs in `contrib/` where the module can evolve without
   the stability constraint.

## Applied: Zero-C LOC plan

### `std.cryptography.sha1` / `std.cryptography.sha256` — **std/**

1. Every serious stdlib has hashing. (yes)
2. One obvious shape: `crypto.sha256(bytes, length) -> string` returning
   a hex digest. No streaming API, no HMAC, no key derivation in v1. (yes)
3. OpenSSL is already linked; the wrapper is ~30 lines of C calling
   `SHA256_Init` / `SHA256_Update` / `SHA256_Final`. Zero new
   dependencies. (yes)
4. The API is a pure function: `bytes in, hex digest out`. No state,
   no configuration, no lifecycle. (yes)

Lands in `std/cryptography/module.ae` and `std/cryptography/aether_cryptography.c`.

### `std.zlib` — **std/**

1. Compression round-trip is a batteries-included baseline capability —
   it shows up in HTTP, in every file format, in network protocols. (yes)
2. One obvious shape: `zlib.deflate(bytes, length) -> (compressed, err)`
   and `zlib.inflate(bytes, length) -> (decompressed, err)`. Streaming
   is a separate future API under the same module. (yes)
3. `zlib` is ambient on every POSIX box and is already a transitive
   dependency of OpenSSL on most distributions. (yes)
4. The v1 API is two functions with the same signature. Adding
   `gzip_compress` / `gzip_decompress` variants later is additive. (yes)

Lands in `std/zlib/module.ae` and `std/zlib/aether_zlib.c`.

### `contrib/sqlite/` — **contrib/**

1. Database bindings are not a universal stdlib expectation. Go's stdlib
   has `database/sql` as an abstract interface, but the concrete driver
   (pq, mysql-driver, go-sqlite3) is a separate package. Rust has no
   database driver in std. Python has `sqlite3` in stdlib — that's the
   outlier, not the norm. (no)
2. SQLite itself has a large, opinionated C API (prepare / bind /
   step / finalize vs. exec vs. one-shot query). The Aether veneer
   will land on one shape, and that shape will have trade-offs that
   other projects might reasonably dispute. (no)
3. SQLite is a 4 MiB amalgamation. Linking it by default into every
   `--with=std` build is a significant weight tax on projects that
   don't use a database. (no)
4. Parameter binding, transactions, prepared-statement caching, blob
   handling, pragmas — the surface area grows naturally, and none of
   it is load-bearing for anyone who doesn't already need SQLite. (no)

Lands in `contrib/sqlite/` alongside `contrib/aeocha/` and
`contrib/tinyweb/`. Callers opt in explicitly.

### HTTP client split — **std/** (existing location, refactor only)

The existing `std.net` already has HTTP client support. The Zero-C LOC
plan proposal was to split the request side out into a more ergonomic
`std.http.client` surface (headers as a map, automatic JSON
(de)serialisation, follow-redirects flag). That's a refactor of an
existing `std/` module, not a new decision — the placement question
doesn't apply.

The one thing worth calling out: if the split lands as a proper
sub-module (`std.http` parallel to `std.net`), it keeps the same
stability bar as every other `std/` module. Anything that would require
a "pick your HTTP backend" choice (libcurl vs native vs mock) goes in
`contrib/` instead.

## Migration test

A module can always move later — `std/` → `contrib/` is painful
(breaks every caller), but `contrib/` → `std/` is low-cost (add an
alias import path, deprecate the `contrib/` one, remove after a
release cycle). When in doubt, **start in `contrib/`**. The only
things that should start in `std/` are the ones that clearly pass all
four rubric questions on day one.

## Cross-reference

- [stdlib-module-pattern.md](stdlib-module-pattern.md) — how to *shape*
  a module once you know where it lives.
- [CONTRIBUTING.md](../CONTRIBUTING.md) — PR process, including the
  `[current]` CHANGELOG convention.
- [contrib/aeocha/README.md](../contrib/aeocha/README.md) and
  [contrib/tinyweb/README.md](../contrib/tinyweb/README.md) — reference
  examples of `contrib/` modules that could have landed in `std/` but
  have too opinionated an API.
