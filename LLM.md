# Notes to self (LLM assisting on Aether)

Not a CLAUDE.md — short, opinionated, written for a future LLM picking up
mid-task. Re-read at start of every session.

## What Aether is, in one paragraph

Systems language. Compiles to C. Two emit modes: `--emit=exe` (default,
produces a binary with a `main()`) and `--emit=lib` (produces `.so`/`.dylib`
with ABI-stable `aether_<name>` exports for FFI from C / Python / Ruby / Java
via ctypes/SWIG/Panama). `--emit=lib` is capability-empty by default —
`std.fs` / `std.net` / `std.os` imports are rejected. The opt-in is
`--with=fs[,net,os]`, an explicit per-build flag for projects that ARE
the host and want full syscall access (e.g. implementing a systems tool
in Aether + a thin C driver).

## How to anchor Aether against languages you already know

Think of it as: **Go's ergonomics + Rust's capability discipline +
Erlang's actor syntax, compiling via C.**

- **NOT Rust** — no borrow checker, no ownership, no lifetimes. Strings
  are ref-counted or arena-owned; you release explicitly where it
  matters, drop-on-scope-exit elsewhere.
- **NOT Go** — no GC. Memory is manual at the runtime C level; Aether
  code mostly avoids the issue via RAII-via-codegen.
- **NOT C** — has closures, tuples, Go-style `(value, err)` returns,
  string interpolation, pattern matching.
- **Actor model is Erlang-ish** but message types are declared, not
  duck-typed. `receive` + `send`, not `!` / mailbox-matching.
- **Capability discipline at three levels, bidirectional host interop.**
  Module: `--emit=lib` rejects `std.fs|net|os` imports by default,
  opt-in via `--with=fs,net,os`. Scope: `hide`/`seal except` on any
  lexical block (closure, trailing-block DSL, actor handler) denies
  names from enclosing scopes at compile time. Runtime:
  `libaether_sandbox.so` (LD_PRELOAD) intercepts libc (`open*`,
  `connect`/`bind`/`accept`, `execve`/`fork`, `mmap`/`mprotect`,
  `dlopen`, `getenv`) against a builder-DSL grant list, inherited
  across `execve` — covers normal-libc code, adversaries using
  `openat2` / `io_uring` / raw `syscall()` bypass it. Aether can be
  the guest (host-lang apps embed via `--emit=lib` → Python ctypes /
  Java Panama / Ruby Fiddle SDKs generated from `aether_describe()`
  manifest; Go stubbed) OR the host (embeds Lua/Python/Perl/Ruby/Tcl/JS
  in-process via `contrib.host.<lang>.run_sandboxed(perms, code)`;
  Java/Go/aether-hosts-aether are separate-process). `hide`/`seal
  except` don't travel into hosted non-Aether interpreters — those
  are contained by grants + LD_PRELOAD only; `hide`/`seal except`
  shape the Aether-side grant-assembly block. Closest analogue: Pony
  object capabilities, but coarser (stdlib category / name / libc
  entry point). See `docs/emit-lib.md`, `docs/hide-and-seal.md`,
  `docs/containment-sandbox.md`, `docs/embedded-namespaces-and-host-bindings.md`.

  Current cross-cutting gaps (`contrib/host/TODO.md` /
  `docs/next-steps.md` → *Host Language Bridges*): capturing
  stdout/stderr from hosted scripts (pipe rewire vs. shared-map key
  vs. pass-through — undecided), native shared-map bindings for
  Perl and Ruby (currently tied-hash via `eval`, which swallows
  writes — Python/Lua/Tcl/JS have proper native bindings), and a
  `bytes` mode on the shared map so callers don't have to base64
  binary payloads.

  Worked examples: `examples/embedded-java/trading/` (direction 1),
  `examples/sandbox-spawn.ae` (direction 2). Integration tests:
  `tests/integration/namespace_{python,ruby,java}/` and
  `tests/integration/embedded_java_trading_e2e/` (direction 1).
  Canonical docs: `docs/embedded-namespaces-and-host-bindings.md`
  (direction 1 / typed-SDK story),
  `docs/aether-embedded-in-host-applications.md` (direction 1 /
  rationale — also the YAML/HCL/Pkl/Jsonnet/Starlark comparison),
  `docs/containment-sandbox.md` (direction 2 / how the in-process
  checker and LD_PRELOAD share a code path).

  What's novel is the combination: _most_ embeddable languages
  (Lua, Wren, Starlark, Hermes) give you the embedding but leave
  capability management to the host. _Most_ capability-secure
  languages (Pony, E) don't ship polyglot SDK generators or
  in-process interpreter bridges. Aether bundles both directions
  behind one permissions model.

Syntax looks Go-ish at a glance (braces, `func_name(x: int) -> int`).
Don't overshoot — there's no `go` keyword, no channels (send-to-actor
plays that role), no interfaces.

## Files/dirs worth knowing

- `CHANGELOG.md` — reverse-chron, `[current]` section holds unreleased
  work. Everything since the last tagged release sits there. Read the
  top 40 lines at session start to know what just landed.
- `docs/emit-lib.md` — the capability-opt-in doc. Canonical reference
  for why `std.fs` is banned under `--emit=lib` by default.
- `docs/next-steps.md` — roadmap. P1/P2/P3/P4 are ordered; `fs.copy` /
  `fs.move` / `fs.chmod` / `fs.symlink` / `fs.realpath` are P4. Check
  here before speccing a new stdlib addition.
- `std/<module>/module.ae` — the Aether-facing surface. Raw externs
  end in `_raw`; Go-style wrappers return `(value, err)` tuples.
- `std/<module>/aether_<module>.c` — the C runtime behind the externs.
- `compiler/aetherc.c` — CLI entry. `--emit=lib`, `--with=`, import
  gate lives around line 590.
- `build/aetherc`, `build/ae` — the compiled binary. `make && make
  install` to rebuild. The binary is SHA-pinned per commit; if the
  CHANGELOG `[current]` mentions features not in `aetherc --help`,
  the binary is stale and needs rebuilding.
- `patch_json_plan.md`, `stdlib_wish.md` — spec documents written by
  downstream users (svn-aether port) requesting changes. Good model
  for incoming feature requests: exact API shape, call-site census,
  rationale. When a wish lands, the wish file gets a status banner
  at the top but stays in-tree as context.
- `tests/regression/` — one `.ae` file per feature; the CI gate.
  `tests/integration/` — integration test directories (e.g.
  `emit_lib_with_capability/` for the `--with=` opt-in).

## Idioms that keep biting

- **Go-style returns, not tuples-as-values.** `fs.write_atomic(path,
  data, len) -> string` — empty string = success, non-empty = error
  message. Don't "improve" the convention, it's consistent across all
  of `std`.
- **Split-accessor pattern for multi-return.** Where a wrapper needs
  more than one return value and the language can't yet unify tuples
  cleanly across FFI, the pattern is a try/get pair backed by TLS:
  `fs_try_read_binary(path) -> int` writes into a TLS buffer,
  `fs_get_read_binary()` / `fs_get_read_binary_length()` / `fs_release_read_binary()`
  drain it. Mirrors the paths_index / checksum pattern elsewhere.
- **Strings are length-aware internally**, but `string_concat("", raw)`
  treats `raw` as C-string (strlen-bounded) and will truncate at the
  first embedded NUL. Binary-safe reads must go through the raw
  `fs_get_read_binary()` + `_length()` pair and memcpy into caller
  storage, not through the `fs.read_binary` wrapper.
- **Reserved keywords that trip users up**: `state`, `match`,
  `message` (actor-model hangover). Fails in extern param names too.
  Rename to `st`, `is_match`, `msg`.
- **`export` functions can't call each other in the same file.**
  `export baz() { return bar(x) }` where `bar` is also `export`
  errors with `Undefined function 'bar'`. Workaround: non-export
  `foo_impl()` + `export foo() { return foo_impl() }` wrapper, and
  have other exports call `foo_impl`. Not the language's best day,
  but it's consistent.
- **Ownership of `ptr`-typed returns.** Strings returned by builtins
  like `string.from_long` / `string.concat` are ref-counted
  (`AetherString*` with a magic sentinel). Safe to pass to other
  string ops without explicit release; they get dropped when the
  last ref goes. Strings returned by `extern fs_foo_raw() -> string`
  are borrowed from a TLS buffer or arena — valid only until the
  next same-kind call or explicit release. If it smells like `ptr`,
  assume borrowed; if it smells like `string` from a builtin, assume
  ref-counted.
- **`extern f() -> ptr` type-erases length.** Aether strings are
  length-aware internally, but once they cross an extern boundary
  as `ptr`, Aether sees only the leading bytes up to the first
  NUL. This is why `fs.read_binary` has a paired `_length()`
  accessor, not a single-return.

## Working with downstream users

- **svn-aether port** (`~/scm/subversion/subversion/`) is the biggest
  real-world user. Paul Hammant. Port is methodical C → Aether,
  one-leaf-per-commit. They find the gaps before anyone else.
- **Feature request flow that works**: downstream writes a spec
  (`stdlib_wish.md` is the current example), Aether implements,
  downstream adopts within the same day. The specs are extremely
  concrete: API names, signatures, rationale, call-site census.
  Match that level when responding.
- **Don't gate on things that aren't real threats.** `--emit=lib`
  capability-empty is right for the embedded-DSL case (host accepts
  untrusted Aether). The svn-aether port is the opposite case —
  they are the host. The `--with=fs` opt-in covers both cleanly.
  Similar dualities will come up; watch for them.

## Build / test commands

- Full build: `make` (from repo root). Rebuilds `build/aetherc` and
  `build/ae`. Incremental builds sometimes miss `aetherc` reshapes;
  `make clean && make -j$(nproc)` when in doubt.
- All regression tests: `make test` or `make check`. Hot inner loop
  during feature work: `make test-ae` (parallel runner for
  `tests/regression/*.ae`).
- JSON conformance: `make test-json-conformance` — must pass 95/95
  `y_*` + 188/188 `n_*` for JSONTestSuite.
- Sanitizers: `make test-json-asan` / `make test-json-valgrind`.
- **Version drift after fetch.** Makefile derives `AETHER_VERSION`
  from the highest `v*.*.*` git tag, falling back to `VERSION` file.
  If `aetherc --version` disagrees with CHANGELOG `[current]` or
  with `cat VERSION`, the local tags are behind: `git fetch --tags`,
  then `make clean && make`.

## Branch / PR conventions

- Prefix with `feat/` for features, `fix/` for bug fixes, `docs/`
  for doc-only. Examples in merged history: `feat/json-object-iteration`,
  `fix/parser-ergonomics-and-cflags`, `fix/block-scope-restoration`.
- CHANGELOG entries go under `[current]` as part of the PR. The
  release workflow renames `[current]` → `[X.Y.Z]` at tag time.
- Never commit to `main` directly. Push the feature branch, open a
  PR, wait for green CI (includes Windows MSYS2 + MINGW-w64
  cross-builds that catch `#include` gaps Linux doesn't).

## Invariants to not break

- `--emit=lib` stays capability-empty by default. `--with=` is the
  only escape hatch; don't add backdoors (per-function allowlists,
  `--with=all`, etc.).
- The `aether_<name>()` ABI mangling is stable across releases. If
  you rename an export, the old name stays as an alias.
- `std.string.from_int(int)` and `string.from_long(long)` are
  separate — don't merge. `long` is 64-bit on every target including
  MSVC (where C `long` is 32 bit) because Aether's `long` type is
  defined as `long long` at the C level.
- Iteration order in `std.json` is insertion order across both parse
  and builder paths. Documented contract, several downstream users
  (including svn-aether's server) rely on it.

## When stuck

- `git log --oneline -30` to see what just shipped.
- Search tests: `grep -rn "capability\|emit.lib\|with=" tests/`.
- Check `docs/` first, then `compiler/` for language-level questions,
  then `std/<module>/` for runtime.
- The CHANGELOG has working-memory for what-landed-when. Treat it as
  authoritative for "is feature X available yet."
