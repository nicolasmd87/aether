# contrib/host TODO

## All host modules
- [x] Bridges self-contained: each host maintains its own permission stack and registers its own checker via the public `_aether_sandbox_checker`. No longer pokes at the compiler-emitted preamble's `static` `_aether_ctx_stack`. Applies to python, lua, perl, ruby, js, tcl; the aether host is separate-process and unaffected. Demonstrated end-to-end in `examples/host-tcl-demo.ae` (compiles, links, runs).
- [x] Import path drift fixed: all READMEs and module.ae headers now document the working form `import contrib.host.<lang>` (previously said `std.host.<lang>`, which the compiler's stdlib resolver doesn't handle since the modules live under `contrib/host/`, not `std/host/<lang>/`). `docs/containment-sandbox.md` already used the correct form.
- [x] IPv6-mapped addresses normalized in `pattern_match`: a grant for `10.0.0.1` now matches a TCP resource reported as `::ffff:10.0.0.1` (and vice versa). Applied to all 6 in-process bridges, the LD_PRELOAD preload checker (`runtime/libaether_sandbox_preload.c`), and the Java-side `AetherGrantChecker`.
- [x] CI + `make contrib-host-check` target added: syntax-checks every bridge in stub mode and runs per-language demos when the dev library is installed. Gracefully skips languages whose headers/libs aren't present. Wired into a Linux CI job (`ci-contrib-host`) that installs lua/python/ruby/perl/tcl/duktape/go dev packages.
- [ ] **Deferred to roadmap**: see [`docs/next-steps.md`](../../docs/next-steps.md#host-language-bridges-contribhost)
  - Capture stdout/stderr from hosted code (pipe + shared map `_stdout`/`_stderr` keys, or pass-through)
  - Shared map `aether_map_get`/`aether_map_put` bindings for Perl and Ruby currently use eval-injected hashes — outputs stay in the hosted language. Need XS (Perl) or C extension (Ruby) to write outputs back to the C map.
  - `string:bytes` mode for shared map — binary data without base64

## Python
- [x] Dedicated `examples/host-python-demo.ae` written against the current stdlib + sandbox API (the `lazy-evaluation` recovery path no longer exists, confirmed via pickaxe search across all refs).
- [x] `os.environ` is cached at CPython startup — sandbox `getenv` interception only works via `ctypes.CDLL(None).getenv`, not `os.environ.get()`. Covered in `docs/containment-sandbox.md` → host module matrix (Env cache issue row) and the "Shared-interpreter caveats" section below it. The `examples/host-python-demo.ae` demonstrates the `ctypes` workaround inline.

## Lua
- [x] Tested and working well. Cleanest host module.

## JS (Duktape)
- [x] Purest containment — no LD_PRELOAD needed; only explicitly-exposed functions are callable. Bindings: `print`, `env`, `readFile`, `fileExists`, `writeFile`, `exec`. All sandbox-checked via `check_sandbox` against the active grant list.

## Perl
- [x] Function names prefixed `aether_perl_` to avoid conflict with Perl's own `perl_run`/`perl_init`. Documented in `contrib/host/perl/README.md` Notes section.
- [x] `%ENV` scrubbed at sandbox entry. Shared interpreter means unsandboxed `run()` after sandboxed `run_sandboxed()` sees scrubbed ENV. Covered in `docs/containment-sandbox.md` → "Shared-interpreter caveats" section.
- [x] Stub-mode typo fixed: `perl_init()` → `aether_perl_init()` (the stub was calling the real libperl symbol which isn't linked when AETHER_HAS_PERL isn't defined).

## Ruby
- [x] Same `ENV` scrub issue as Perl. Covered in `docs/containment-sandbox.md` → "Shared-interpreter caveats".
- [x] `Fiddle.dlopen("libc.so.6")` succeeds but calls are still intercepted — not a real escape but looks alarming in tests. Covered in `docs/containment-sandbox.md` → "Shared-interpreter caveats".

## Java
- [x] Separate process via JVM — uses Panama FFI for shared memory, not in-process embedding. Documented in the README.
- [x] `grant_jvm_runtime()` convenience helper shipped in `contrib/host/java/module.ae`: bundles the ~29 grants the JVM needs before any application code runs (linker paths, trust stores, locale, `JAVA_*` env vars). Callers import `contrib.host.java` and invoke `java.grant_jvm_runtime()` inside a sandbox block.
- [x] IPv6-mapped address normalization — applied in `AetherGrantChecker.patternMatch` (Java side) and `libaether_sandbox_preload.c` `pattern_match` (LD_PRELOAD C side). A grant for `10.0.0.1` matches `::ffff:10.0.0.1` and vice versa.

## Go
- [x] Added as the eighth host. Separate subprocess under LD_PRELOAD sandbox interception (same pattern as `contrib/host/aether`). Two modes: `go.run_script_sandboxed(perms, path)` shells out to `go run`, `go.run_sandboxed(perms, binary)` runs a pre-built binary under tight grants. End-to-end demo: `examples/host-go-demo.ae`.

## Tcl
- [x] Added as the seventh host, mirroring the lua template. `::env` caveat (shared interpreter, same as Perl/Ruby) covered in `docs/containment-sandbox.md` → "Shared-interpreter caveats". End-to-end verified: `examples/host-tcl-demo.ae` compiles, links, and runs on macOS with system Tcl 8.5.
