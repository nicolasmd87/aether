# Changelog

All notable changes to Aether are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

**Workflow**: New changes go under `## [0.73.0]`. When a PR merges to
`main`, the release pipeline automatically replaces `[current]` with the
next version number before tagging the release.

## [0.74.0]

### Changed

- **Docs audit: "Limitations" / "Caveats" / "Restrictions" sections replaced across the tree.** Sixteen such sections across nine docs files (`c-embedding.md`, `closures-and-lifetimes.md`, `containment-sandbox.md`, `emit-lib.md`, `module-system-design.md`, `named-args-and-select.md`, `type-annotation-style-guide.md`, `type-inference-guide.md`) were rewritten rather than listed. Design boundaries are now stated positively as part of the main narrative ("Host-process boundary", "Scope boundaries", "Shared-interpreter behavior", "Nesting rules", "When explicit types are required", "Type contract", "Closure patterns and workarounds", "Interception surface"). Items that represent pending engineering work (unintercepted syscalls, transitive package deps + lockfile + `ae publish`, polymorphism/type-classes/higher-rank types, `select()` type-inference propagation) moved into new sections in `docs/next-steps.md`. `docs/closures-and-lifetimes.md` was reframed from a limitations doc into a patterns+workarounds doc with per-item "proper fix" notes pointing at the roadmap. Verification: `grep -rniE "^## .*Limitations|^### .*Limitations|^## .*Caveats|^### .*Caveats|^## .*Restrictions|^### .*Restrictions|^## .*Gotchas|^### .*Gotchas" --include="*.md" .` returns empty.

- **Docs audit: specific benchmark numbers scrubbed.** Hardcoded comparison figures, cycle-count sample outputs, percentage-improvement claims, and concrete request-per-second numbers removed from `README.md`, `CONTRIBUTING.md`, `docs/language-reference.md`, `docs/profiling-guide.md`, `docs/build-system.md`, `docs/containment-sandbox.md`, `docs/stdlib-api.md`, `docs/stdlib-reference.md`, `docs/next-steps.md`, and historical entries in this file under `[0.31.0]` and `[0.55.0]`. Replacements use unit-ful descriptions of *what* is measured and how to reproduce it: profiling-guide sample output uses placeholder tokens with an explanatory note; the regression-check script in that doc reads a user-tunable `$TOLERANCE` against a saved baseline rather than a hardcoded threshold; cost-of-a-run descriptions in `docs/build-system.md` describe the dominant components (aetherc front-end, gcc compile+link, OS Gatekeeper first-run pause on macOS) without timings. A grep sweep for `M msg/sec` / `cycles/op` / `ns/op` / `Nx faster` / `req/s` / `Nx throughput` / `N% faster/slower/improvement` patterns across all markdown files returns empty.

- **`docs/next-steps.md` extended** with items relocated from the deleted "Limitations" sections: a new top-level `## Sandbox` section containing *Interception surface expansion* (catalogue of syscalls LD_PRELOAD doesn't see; points to the containment-sandbox doc's "Interception surface" table), a new top-level `## Type system` section containing *Type inference propagation through `select()`* (describes the string-typing gap that string interpolation currently works around) and *Polymorphism, higher-rank types, type classes* (surface-level description of monomorphic-only inference, not a scheduled item). The existing "Package Registry" bullet was extended to include transitive dependency resolution, lock file integrity, `ae update`, and `ae publish`.

- **Inline prose cleanup.** `docs/containment-sandbox.md` reshaped: the `## Limitations` section became `## Scope boundaries` with its sub-section `### Other limitations` merged in as `### Other boundaries`; `### Nestable restriction` became `### Nesting rules`; `### Known unintercepted libc functions and syscalls` became `### Interception surface — what LD_PRELOAD sees and what it doesn't` with a forward reference to the next-steps item; `### Shared-interpreter caveats` became `### Shared-interpreter behavior`; the "Nested maps: Not supported" framing was rewritten to state the flat-by-design decision up front. `docs/module-system-design.md` "Not yet supported" bullet list renamed to "On the module-system roadmap (language-level)".

- **Cross-reference cleanup.** The `[0.72.0]` entry that describes the new `docs/containment-sandbox.md` section was updated to reference the renamed heading ("Shared-interpreter behavior"). `contrib/host/TODO.md` references to the old heading were updated. `docs/closures-and-lifetimes.md` introduction paragraph that referred to the old "Known limitations" heading was rewritten to point at the new "Closure patterns and workarounds" section.

- **Doc-file rename.** `docs/closure-lifetime-bugs.md` → `docs/closures-and-lifetimes.md`. File title updated from "Closure Environment Lifetime Bugs" to "Closures and Environment Lifetimes"; historical bug-list framing reworked to read as mechanism documentation with a per-pattern workaround section. All five cross-references in this file were updated to the new path.

- **Docs audit: code-comment layer swept too.** The constraint pair (no limitations-style sections, no specific benchmark numbers) now applies to C/header comments and runtime printf output, not only markdown. Scope of this pass: four "Known limitation"-style comments in C files (`runtime/aether_config.c`, `runtime/libaether_sandbox_preload.c`, `contrib/host/perl/aether_host_perl.c`, `tests/syntax/test_closure_reassign_leaks_env.ae`) reworded to state the technical content positively; runtime printf blocks that emitted specific throughput tiers (`runtime/aether_runtime.c` "Expected Performance" block, `runtime/utils/aether_cpu_detect.c` SIMD recommendation block, `runtime/config/aether_optimization_config.c` Tier-1 listing, plus two example-program summary blocks and one bench-program prediction line) replaced with tier-name / component descriptions that point at the actual benchmark programs for measurement; top-of-file design comments with hardcoded speedup ratios across the actors / memory / scheduler / config headers and the `Makefile` PGO and build-help sections rewritten to describe what each optimisation does without committing to specific ratios; `benchmarks/baseline.json` string descriptions in its `optimizations` and `notes` keys reworded (its structured numeric measurement fields are unchanged — those are data, not prose). Full-tree verification: grep across `*.md`, `*.ae`, `*.c`, `*.h`, `Makefile`, `*.json` for quantitative performance-claim patterns returns empty except for `tests/runtime/test_scheduler.c:372` (factual statement about Valgrind's overhead, not an Aether claim) and `benchmarks/baseline.json` structured measurement fields.

- **Minor drift cleanup.** `docs/architecture.md` module-resolution example: the stale `import std.collections.HashMap` (no such module; the actual stdlib file is `aether_hashmap.c` behind `std.collections` / `std.map` / `import std.collections`) was replaced with the real form and the resolver steps updated to match the merger's current behaviour. `contrib/aether_ui/README.md` dead cross-reference to `aether_ui_conversion_plan.md` (file doesn't exist) was removed. Snippet fixes for renamed stdlib entry points: `docs/type-inference-guide.md` `tcp_connect(...)` / `tcp_listen(80)` → `_raw` forms; `docs/language-reference.md` `tcp_send(conn, msg)` → `tcp_send_raw(conn, msg)`; `docs/stdlib-reference.md` `net.http_get(url)` → raw + Go-style pair; `docs/next-steps.md` `tcp_connect()` → `tcp.connect()` for consistency with the neighbouring `http.get()` / `file.read()` / `sleep()` list items. `docs/api/*.html` (auto-generated API reference) refreshed by `make ci` to include entries for `http_response_status_code` / `http_response_body_str` / `http_response_headers_str` / `os_execv` / `os_which` that were missing from the committed HTML.

- **Tutorial prose reframed.** `CONTRIBUTING.md` `## Common Pitfalls` → `## Code review heuristics` with an intro explaining the BAD/GOOD/BETTER checklist form; `docs/getting-started.md` `### Common Pitfalls` → `### Quick tips when you hit trouble`, each bullet rewritten from warning-form ("Forgetting to rebuild…") to prescriptive-form ("Rebuild from scratch if anything looks stale…"). `docs/module-system-design.md` `## Future` section's intro line "Features not yet implemented:" → "Work planned on top of the current module system:". `docs/performance-benchmarks.md` and `benchmarks/cross-language/README.md` CV% interpretation notes gained an explicit line clarifying that the <5% / 5–15% / >15% bands are general statistical stability thresholds, not Aether-specific targets. `docs/next-steps.md` "33% size overhead" for base64 rephrased as "4:3 expansion ratio as a matter of the encoding's arithmetic" so it's clear the number is math, not a benchmark.

## [0.73.0]

### Added

- **`contrib/host/go/` — eighth language host module.** Separate-subprocess host (same containment model as `contrib/host/aether/`: `fork` + `execvp` under `LD_PRELOAD=libaether_sandbox.so`, grants serialized to `shm_open` segments named `/aether_host_go_<pid>`). Two usage modes: `go.run_script_sandboxed(perms, path)` shells out to `go run script.go` (wider grant surface because the `go` toolchain itself runs under the sandbox), and `go.run_sandboxed(perms, binary)` runs a pre-built binary under tight grants. Also ships `_with_map` variants that share a `uint64_t` map token with the child for bidirectional key/value passing. Six externs in `module.ae`; C-side in `aether_host_go.{h,c}` with Windows stub fallbacks. README documents the `$GO_BIN` override, the Linux-vs-macOS SIP caveat, and when to prefer `run_sandboxed` over `run_script_sandboxed`. End-to-end demo in `examples/host-go-demo.ae` (two grant profiles: wide-open vs narrow env+tmpfs, with the narrow profile expected to fail the `go run` toolchain's requirements on Linux).

- **`make contrib-host-check` target + `ci-contrib-host` workflow job.** Closes the "no CI coverage for contrib/host" gap. The target runs in two phases: phase one syntax-checks every bridge (`js`, `lua`, `perl`, `python`, `ruby`, `tcl`, `go`) in stub mode (no dev libs required) so missing headers never break the tree; phase two delegates to `tests/scripts/contrib_host_demos.sh`, which probes for each language's headers/libraries, emits `-I/-L/-l` fragments when available, and runs the per-language demo if present. Absent dev libs exit with a distinguishable "skip" code (3) rather than failure. The `ci-contrib-host` GitHub Actions job installs `liblua5.3-dev python3-dev ruby-dev libperl-dev duktape-dev tcl-dev golang-go` on `ubuntu-22.04` and invokes the target after `make compiler ae stdlib`.

- **`contrib/host/js/` — JS bindings for `writeFile` and `exec`.** Duktape doesn't go through libc for its own I/O (it has no built-in filesystem or process APIs), so the bridge explicitly exposes each capability as a native binding that routes through the in-process sandbox checker. Two new bindings join the existing `print`, `env`, `readFile`, `fileExists` set: `writeFile(path, content)` checks `"fs_write"`/path and returns a boolean success flag (creates or truncates); `exec(cmd)` checks `"exec"`/cmd, shells via `system()`, and returns the exit code unwrapped through `WEXITSTATUS` (or `-1` when denied).

- **`contrib/host/java/module.ae` + `grant_jvm_runtime()` helper.** The JVM needs ~29 grants before any application code runs (kernel/procfs probes for container detection, `/usr/lib/jvm/*` + `/lib*/` + `/etc/java-*` + `/etc/alternatives/*` for libs, `/etc/ssl/*` + `/etc/pki/*` + `/etc/ca-certificates/*` for trust stores, `/etc/localtime` + `/usr/share/zoneinfo/*` + `/usr/share/locale/*` for locale/TZ, and the `JAVA_HOME` / `JAVA_TOOL_OPTIONS` / `CLASSPATH` / `LANG` / `LC_*` / `TZ` env vars). Grant paths captured empirically via `strace java -version` on Corretto 24 (Debian) and Temurin 21 (Ubuntu). Callers `import contrib.host.java` and invoke `java.grant_jvm_runtime()` inside a `sandbox() { … }` block alongside application-specific grants. README extended with a usage snippet.

- **`contrib/host/tcl/` — seventh embedded-language host module.** Tcl 8.5+ joins the existing six hosts (aether, js, lua, perl, python, ruby). Four files under the new directory follow the lua template: `module.ae` declares `tcl_run_sandboxed` / `tcl_run` / `tcl_init` / `tcl_finalize` externs; `aether_host_tcl.{h,c}` implements the bridge against `libtcl` (single lazy-initialized `Tcl_Interp*`, `Tcl_Eval` for script execution, self-contained permission stack, native `aether_map_get`/`aether_map_put` Tcl commands registered via `Tcl_CreateObjCommand` for the `run_sandboxed_with_map` variant, stub-mode fallback under `#else` when `AETHER_HAS_TCL` isn't set). `README.md` documents prerequisites (`apt install tcl-dev` / `brew install tcl-tk`) and the canonical `aether.toml` cflags/link_flags recipe. Tcl is now listed in the "Available modules" table and the host-module matrix in `docs/containment-sandbox.md`.

- **`examples/host-tcl-demo.ae`** — end-to-end demo exercising the tcl host under two different grant profiles (worker with env/fs_read/exec grants, and a minimal sandbox with only env). Verified: compiles to C, links with libtcl + libaether, runs on macOS with system Tcl 8.5.

- **`examples/host-python-demo.ae`** — replaces the TODO reference to an untrackable `lazy-evaluation` branch with a fresh demo matching the current stdlib + sandbox API. Uses heredoc strings for multi-line Python snippets. Demonstrates the `ctypes.CDLL(None).getenv` workaround for CPython's cached `os.environ` (flagged in the host-module matrix in `docs/containment-sandbox.md`). Verified: compiles to C, links with libpython3.12 + libaether, runs on macOS.

### Fixed

- **All six contrib/host bridges are now self-contained.** Previously, `aether_host_{python,lua,perl,ruby,js,tcl}.c` declared `extern void* _aether_ctx_stack[]` and `extern int _aether_ctx_depth`, referencing symbols that the compiler emits as `static` in its preamble (`compiler/codegen/codegen.c:1213-1214`). The extern references had no visible definition at link time; host bridges could not link end-to-end for any language. Each bridge now owns a private `<lang>_perms_stack[64]` + `<lang>_perms_depth` pair, pushes its perms locally before installing its checker via the already-public `_aether_sandbox_checker`, and pops on exit. The only cross-translation-unit symbol required is `_aether_sandbox_checker` from `runtime/aether_sandbox.c`. Verified end-to-end: `examples/host-tcl-demo.ae` builds against libaether + libtcl and runs on macOS with system Tcl 8.5. Same pattern applied to the existing lua/python/perl/ruby/js hosts (aether host unaffected — it uses separate-process `spawn_sandboxed` rather than in-process embedding).

- **Import path drift across all six host modules.** Every host README and the `module.ae` header comments said `import std.host.<lang>`, but Aether's stdlib resolver (`compiler/aether_module.c:435`) only probes under `std/` while these modules live under `contrib/host/`. The working form — `import contrib.host.<lang>` — is now documented consistently across all `contrib/host/*/module.ae` headers and READMEs, matching what `docs/containment-sandbox.md` already got right. No compiler change needed; this was pure documentation drift.

- **Pre-existing Perl stub typo.** In `AETHER_HAS_PERL`-undefined mode, `aether_host_perl.c`'s fallback stubs called `perl_init()` — the real libperl symbol, unavailable in stub mode. Corrected to call the stub's own `aether_perl_init()`. Makes the perl host syntax-check clean in both modes.

- **IPv4-mapped IPv6 addresses now match across grant/resource sides.** A grant for `10.0.0.1` matches a resource reported as `::ffff:10.0.0.1` (and vice versa). The normalization strips the `::ffff:` prefix from both sides of `pattern_match` before any glob comparison. Applied in eight places: all six in-process bridges (`aether_host_{js,lua,perl,python,ruby,tcl}.c`), the `LD_PRELOAD` checker (`runtime/libaether_sandbox_preload.c`), and the Java-side `AetherGrantChecker.patternMatch`. The prefix literal is safe for non-TCP categories because it doesn't appear in filesystem paths, env var names, or exec command strings.

- **`clock_ns()` codegen no longer emits preprocessor directives inside an expression.** The builtin previously generated `loop_start = \n#if AETHER_GCC_COMPAT\n({ ... })\n#else\n_aether_clock_ns()\n#endif\n;` at every call site. That is only valid C when the `#` column is forced to 0 of its own line, so any surrounding context that didn't flush a newline first (macro expansion, some nested codegen paths) collapsed the assignment to `loop_start =` followed by empty lines, which the C compiler then reported as an undeclared identifier on the LHS with nothing on the RHS. The statement-expression branch was a micro-optimization that saved one function call on GCC/Clang; `_aether_clock_ns()` is already defined in the preamble with the correct per-platform `clock_gettime` / `QueryPerformanceCounter` / freestanding variants, and the helper is now marked `static inline` so the optimizer has the same freedom. Every `clock_ns()` call site now emits a plain `_aether_clock_ns()`, which is robust in every expression position. Surfaced by a report where a `while (cond) { t = clock_ns() ... }` inside an actor receive arm failed to compile the generated C, even though the same pattern inside a `for` body worked.

- **`ActorBase.dead` field plumbed through codegen.** The runtime struct gained `atomic_int dead` alongside `timeout_ns` and `last_activity_ns` in the scheduler panic/recovery work. The codegen actor struct in `compiler/codegen/codegen_actor.c` emitted the two timeout fields but not `dead`, leaving a layout mismatch so that scheduler reads of `actor->dead` read garbage (typically a user state field) and silently skipped message processing as if the actor had crashed. Codegen now emits `atomic_int dead;` after `last_activity_ns` and initializes it to 0 alongside the other atomic fields in the spawn path. Eight `.ae` integration tests that exercise multi-actor messaging (`test_actor_communication`, `test_msg_basic`, `test_coop_chain`, etc.) were failing as a result; all pass after the fix.

- **Scheduler test stub structs updated to mirror current `ActorBase`.** `tests/runtime/test_scheduler.c` and `test_scheduler_stress.c` manually duplicate the `ActorBase` layout field-by-field (five struct definitions total) with a `MUST match ActorBase layout exactly` comment. The duplicates missed the new `timeout_ns` / `last_activity_ns` / `dead` fields so the scheduler's reads past `step_lock` clobbered the tests' own counter fields, producing `expected 100, got 0` in `Scheduler message ordering`. Added the three fields to all five structs; `make test` now reports 191/191.

- **Windows portability of the panic/recover runtime.** `runtime/actors/aether_panic.h` used `sigjmp_buf`, `sigsetjmp`, and `siglongjmp` unconditionally, which MinGW and the `make ci-windows` cross-compile don't provide. Added a portability shim in the header: `aether_sigjmp_buf` and the macros `AETHER_SIGSETJMP` / `AETHER_SIGLONGJMP` expand to the POSIX `sigjmp_buf` family on Linux and macOS and to plain `jmp_buf` / `setjmp` / `longjmp` on Windows, where the signal-mask semantics of the POSIX variants aren't meaningful. The signal-handler installer (`aether_panic_install_signal_handlers`) guards its `sigaction` / `SA_SIGINFO` block under `#ifndef _WIN32` with a no-op stub for Win32 (SEH is a separate design). Callers updated: `runtime/scheduler/multicore_scheduler.c`, `compiler/codegen/codegen_stmt.c` (try/catch codegen), `runtime/actors/aether_panic.c`. `make ci-windows` passes 70/70 examples after the fix (previously 0/70).

- **`make examples` excludes `host-*-demo.ae` files.** The host language bridge demos (`host-go-demo.ae`, `host-python-demo.ae`, `host-tcl-demo.ae`) need bridge-specific C sources and `-l<lang>` link flags that the plain `examples` target doesn't know about. They are built and run by `make contrib-host-check` instead, which probes for dev libs and emits the right flags. The `examples` target filter now skips `host-.*-demo.ae` the same way it already skips `/lib/`, `/packages/`, and `/embedded-java/`.

- **`tests/integration/closure_nested_return` gcc invocation gains `-Iruntime/actors`.** The codegen preamble now emits `#include "aether_panic.h"`, which lives under `runtime/actors/`. The standalone gcc command the shell test uses to validate `-Werror=return-type` on the generated C previously passed only `-Iruntime -Istd -Istd/io`. The header wasn't found and the test failed for reasons unrelated to the return-type regression it guards against. Added `-Iruntime/actors` to match what `ae build` already passes.

- **`tests/runtime/test_worksteal_race.c` `WStealActor` struct updated to mirror current `ActorBase`.** The three work-steal race tests (`Work-steal no message loss`, `Work-steal re-route under migration`, `Work-steal atomic assigned_core read`) manually duplicate the `ActorBase` layout field-by-field. The duplicate was still missing the `timeout_ns` / `last_activity_ns` / `dead` fields the scheduler added, so `scheduler_send_remote`'s `atomic_load(&actor->dead)` read 16 bytes past the end of the 1616-byte allocation. AddressSanitizer reported it as a `heap-buffer-overflow` (seen in the `memory-checks` CI job); without ASAN the reads silently returned garbage, which produced intermittent `Work-steal no message loss` / `Work-steal re-route under migration` failures on CI runners due to messages being dropped when the garbage happened to be non-zero. Added the three fields; all four work-steal tests pass under ASAN with zero errors.

- **Emscripten/wasm32 cannot encode label-address dispatch tables; route wasm through the switch-case path.** The actor message dispatch in `compiler/codegen/codegen_actor.c` emits two implementations gated on `#if AETHER_GCC_COMPAT`: a computed-goto dispatch using GCC's "labels as values" (`&&handle_FOO` stored in a `void* dispatch_table[256]`) and a `switch`-case fallback that targets MSVC. Emscripten's clang identifies as GCC-compatible, so it took the computed-goto path, which wasm cannot represent: storing a function-local label address in a data-section array requires a code-section relocation, and wasm reports `fatal error: error in backend: relocations for function or section offsets are only supported in metadata sections`. Threshold dependent: `counter.ae` (2 label addresses) compiled; `test_platform_caps.ae` / `test_coop_chain.ae` (5 label addresses each) failed. The guard is now `#if AETHER_GCC_COMPAT && !defined(__EMSCRIPTEN__)`, so wasm builds take the switch-case path the same way MSVC does. No runtime impact on native GCC/Clang targets; they still get computed-goto dispatch.

- **`aether_panic.h` sigjmp_buf shim extended to Emscripten and freestanding targets.** The shim that maps `aether_sigjmp_buf` / `AETHER_SIGSETJMP` / `AETHER_SIGLONGJMP` to the signal-mask-unaware `jmp_buf` family was guarded by `#ifdef _WIN32` only, which missed two more targets: Emscripten's wasm32 clang supports `setjmp` / `longjmp` via `-mllvm -enable-emscripten-sjlj` but not `sigsetjmp` / `siglongjmp`, and freestanding ARM bare-metal (`arm-none-eabi-gcc -ffreestanding` against newlib-nano, as used by `make ci-embedded`) has no `sigjmp_buf` declaration at all because POSIX signals aren't part of the freestanding C surface. The guard is now `#if defined(_WIN32) || defined(__EMSCRIPTEN__) || (defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0)`. The matching `aether_panic_install_signal_handlers()` implementation is gated with the negation: the POSIX `sigaction` path compiles only on hosted non-Windows non-wasm targets; the other three get a no-op stub. The rest of the panic / try / catch path (built on `setjmp` / `longjmp`) works unchanged on all of them.

### Changed

- **`docs/containment-sandbox.md` — host-module matrix extended with a Tcl column and a new "Shared-interpreter behavior" section.** That section is the proper home for the long-standing design details Perl, Ruby, and Tcl exhibit (shared interpreter retains scrubbed env across run/run_sandboxed boundaries) plus Ruby's `Fiddle.dlopen` interaction. Keeps this information out of user-facing per-module READMEs (which should stay minimal) and puts it in the one design-level reference doc where sandbox users already look for behavior detail.

- **`contrib/host/TODO.md`** — closed every doc-only item that now has a canonical home in `docs/containment-sandbox.md` (Python os.environ, Perl prefix + %ENV, Ruby ENV + Fiddle.dlopen, Tcl ::env) and the two new-universal items that the bridge-self-containment + import-path fixes resolved. Also struck through the unachievable `git show lazy-evaluation:...` recovery instruction (the branch no longer exists on GitHub or in the local reflog) now that a fresh Python demo ships in this branch.

## [0.72.0]

### Fixed

- **`contrib/tinyweb` compiles on the current stdlib.** `filter_all` now inlines its body instead of calling `filter(_ctx, …)` — the DSL `_ctx` auto-injector was treating that as a 4-arg call against a 3-effective-arg function. Renamed the module's `server_start` / `server_stop` to `tw_start` / `tw_stop` to avoid colliding with `std.http`'s Go-style `server_start` wrapper. Examples now prefix every top-level tinyweb name with `tinyweb.` (non-selective `import contrib.tinyweb` does not expose names as bare) and add `import std.http` / `.string` / `.list` / `.map` / `.tcp` so the inlined module body's `http.xxx` / `string.xxx` / etc. qualified calls resolve in the importer's scope. `test_integration.ae` swapped `http.server_start` / `http.get` / `http.post` for the raw externs (`http_server_start_raw`, `http_get_raw`, `http_post_raw`): the Go-style wrappers now return `(body, err)` tuples that the test's ptr-based `_chk_resp` / `_chk_code` / `_chk_body_contains` assertions can't consume. `test_integration.ae` runs green end-to-end (8/8).

## [0.71.0]

### Fixed

- **Closure inside actor handler writing a state field is now rejected at compile time** (`compiler/codegen/codegen_expr.c`). Closures inside actor receive arms have no access to `self`, so a write like `inc = || { count = count + 1 }` (where `count` is an actor `state` field) compiled to a stale local read — a silent wrong answer. Codegen now walks every closure body inside every receive arm and, for each write to a state field, emits a compile-time error pointing at the offending line with a suggestion to use the arm-local workaround. New helper `aether_error_full()` in `compiler/aether_error.c` for line-numbered errors with suggestion + context + code. `aetherc` now checks `aether_error_count()` after codegen (compared against pre-codegen count, so legacy parser-noise tests don't regress) and aborts the build instead of leaving a half-written `.c` behind. Regression test: `tests/integration/closure_actor_state_reject/`. L4 in `docs/closures-and-lifetimes.md`.

### Changed

- **`docs/closures-and-lifetimes.md`**: enumerated all five currently-tracked closure limitations (L1–L5), split the previous lumped "Known limitations" paragraph into per-limit sections with minimal reproducer, workaround, and proper-fix shape for each. Added a note on L1 explaining why a trivial `intptr_t` widening in the generic `call()` dispatch doesn't fix the truncation (the destination variable's C type is pinned via the symbol table, not just the AST, so a real fix needs typechecker work).

## [0.69.0]

### Fixed

- **Closure body references a later-numbered closure** (`compiler/codegen/codegen_expr.c`). A closure's body can construct inline closure literals and pass them as arguments to other functions. Each lambda gets its own `_closure_fn_N` in the emitted C. When the outer closure is numbered before its inline lambdas, its body referenced `_closure_fn_N` symbols that hadn't been declared yet at that point in the file (`'_closure_fn_N' undeclared` error). `emit_closure_definitions` now runs in two passes: pass 1 emits every env typedef and every function prototype, pass 2 emits bodies and constructors. A closure body can reference any `_closure_fn_N` by name regardless of numbering. Three helpers extracted for readability: `resolve_closure_return_type`, `emit_closure_signature`, `emit_closure_env_typedef`. Regression test: `tests/syntax/test_closure_forward_references.ae`.

- **Nested lambda's `return` mis-typed the enclosing closure** (`compiler/codegen/codegen_func.c`). `has_return_value` walked an AST subtree looking for return statements with values. A nested lambda's `return` bubbled up through the recursive walk and mis-typed the enclosing closure as `int` — producing `static int _closure_fn_N(...) { ...; }` with no return statement, undefined behavior caught by `-Wreturn-type`. One-line fix: `has_return_value` now stops at `AST_CLOSURE` boundaries so a nested closure's return belongs to that closure, not to any enclosing scope. Regression test: `tests/integration/closure_nested_return/` (compiles the generated C with `-Werror=return-type` and requires a clean build).

- **Captures across nested trailing blocks** (`compiler/codegen/codegen_expr.c`). A variable declared inside a trailing block (e.g. `root = grid() { c = 42; ... }`) lives in the enclosing function's scope because trailing blocks are inlined at the call site, not hoisted. Previously a closure inside a sibling or nested trailing block could not capture such variables — capture discovery stopped at ANY `AST_CLOSURE`, including trailing-block closures (value == `"trailing"`), so names declared inside one trailing block were invisible to inner closures (`'c' undeclared` in generated C). Two scope-analysis helpers updated: `subtree_declares` now recurses through trailing-block closures while stopping at real closures; a new `scope_declares_at_top_level` helper is used by `is_top_level_decl_in_function` to walk trailing blocks but NOT nested if/for/while blocks — preserving the Python-style rule that `v = ref_get(num)` inside `if key == EQUAL { ... }` stays a block-local (examples/calculator-tui.ae still builds cleanly). Regression test: `tests/integration/closure_trailing_block_capture/`.

  Design note worth flagging: the nested-trailing-block fix was subtler than it looked. A naive "recurse everywhere" version broke calculator-tui. The final version threads the needle — trailing blocks are transparent to scope lookup (they inline at the call site), but nested if/for/while blocks still aren't. That's the right answer mechanically (it matches how trailing blocks work at codegen time) but it's the kind of detail that could surface a different corner case later. Tracked in `docs/closures-and-lifetimes.md` bugs 6-8.

## [0.67.0]

### Added

- **Embedded namespaces — Aether scripts as typed Java / Python libraries via `ae build --namespace`**: a directory containing a `manifest.ae` plus sibling `.ae` scripts is now compilable into a single `.so` / `.dylib` *plus* per-language SDKs (Java + Python in v1) that wrap the native library behind an idiomatic class API. The host application — Java, Python, or any FFI-capable language — `import`s the generated SDK, gets typed setters for inputs, typed event handlers for the script's `notify()` calls, and typed methods for every exported script function. No SWIG to invoke, no JNI to write, no Panama boilerplate visible at the user's level.

    Composing pieces:

    - `notify(event: string, id: long) -> int` extern in a new `std.host` Aether module (`runtime/aether_host.{h,c}` + `std/host/module.ae`): claim-check primitive following the EAI / Hohpe pattern. Aether scripts emit thin notifications carrying just an event name and an int64 id; the host registers handlers via `aether_event_register()` and looks up details (if any) through normal typed downcalls. Single-threaded synchronous; cap of 64 handlers (raisable). Tests: `tests/integration/notify/` covers register, missing-handler-returns-0, replace-in-place re-registration, unregister, clear, NULL-safety.

    - Manifest builder DSL in `std.host`: `describe(name)`, `input(name, type)`, `event(name, carries)`, `bindings()`, `java(package, class)`, `python(module)`, `go(package)`. A `manifest.ae` is conventionally an `abi() { describe("name") { input(...) event(...) bindings() { java(...) } } }` block — same trailing-block + `_ctx`-injection idiom as `contrib/tinyweb`'s `web_server() { path() { end_point(...) } }` or `examples/calculator-tui.ae`'s `grid() { btn(...) callback { ... } }`. Each builder declares `_ctx: ptr` first so the codegen auto-injects context inside the trailing block; the C side ignores `_ctx` (the manifest registry is global state). `tests/integration/manifest/` exercises every builder via dlopen + manifest_get(). Two compiler enhancements made the trailing-block grammar land cleanly: (a) the `_ctx`-first builder pre-pass in `codegen.c` now also recognizes externs from imported modules (previously only walked locally-defined functions, so std.host's externs were missed); (b) auto-injection at call sites fires whenever the user's arg count is exactly one less than the function's declared param count (previously gated on `gen->in_trailing_block > 0`, which broke the outermost call in a manifest's body — `_aether_ctx_get()` returns NULL at the top of the stack, which builders that ignore `_ctx` handle correctly).

    - `aetherc --emit-namespace-manifest <manifest.ae>` writes the manifest as JSON to stdout (declaration order preserved, every binding sub-object scoped). `aetherc --emit-namespace-describe <manifest.ae> <out.c>` writes a self-contained `.c` stub with a static const `AetherNamespaceManifest` and the `aether_describe()` discovery entry point. `aetherc --list-functions <file.ae>` prints `name|return|p1:t1,...` lines for every top-level function. Both extractors walk the parsed AST structurally — we deliberately don't execute the manifest at compile time, matching how `protoc-gen-*` and `openapi-generator` work.

    - `ae build --namespace <dir>` orchestrates the pipeline: discovers sibling `.ae` files, deduplicates `import` lines and at-most-one `main()`, concatenates them into one synthetic translation unit, runs `--emit=lib` with the describe stub appended via `--extra`, embeds the manifest as the static struct, and dispatches to per-language SDK generators. Default output: `<dir>/lib<namespace>.so` / `.dylib`, with the namespace name coming from the manifest (not the directory). Artifacts land **inside** `<dir>` so the namespace is shippable as a self-contained unit.

    - **Python SDK generator**: when a manifest declares `python("module_name")`, emits `<dir>/<module_name>.py` — a self-contained ctypes-based class. `set_<input>(value)` per input (v1 stores on instance), `on_<event>(handler)` per event with proper trampoline keepalive (callbacks held in `self._callbacks` so the GC doesn't reclaim the `CFUNCTYPE` while C still has a pointer), per-function methods with auto string ↔ bytes marshalling, `describe()` returning a typed `Manifest` populated from the embedded `_NamespaceManifest` struct via `ctypes.Structure`. Tests: `tests/integration/namespace_python/` covers two inputs, two events, three function signatures end-to-end. Skips cleanly if `python3` isn't installed.

    - **Java SDK generator**: when a manifest declares `java("com.example.foo", "Class")`, emits `<dir>/com/example/foo/Class.java` using Panama Foreign Function & Memory API (stable in JDK 22+). Cached `MethodHandle` per function via `Linker.downcallHandle`, `setX(value)` per input, `on<EventName>(LongConsumer)` per event using `MethodHandles.publicLookup().findVirtual(LongConsumer.class, "accept", ...)` (avoids the lambda nestmate-private lookup error), camelCased methods per function with `arena.allocateFrom` ↔ `MemorySegment.getString(0)` string marshalling, typed `Manifest describe()` walking the static struct with explicit byte offsets. Implements `AutoCloseable` so try-with-resources releases the `Arena`. Tests: `tests/integration/namespace_java/` mirrors the Python tests. Skips cleanly if `javac`/`java` aren't installed or the JDK is older than 22.

    - **Ruby SDK generator**: when a manifest declares `ruby("module_name")`, emits `<dir>/<module_name>.rb` using Ruby's stdlib `Fiddle` (no extra gem required, ships with MRI Ruby 1.9.2+). The generated module wraps the `.so` via `Fiddle.dlopen`; the SDK class exposes `set_<input>(value)` accessors, `on_<event_snake_case> { |id| ... }` block-based event handlers with proper `Fiddle::Closure::BlockCaller` trampoline keepalive (callbacks held in `@callbacks` so Ruby's GC doesn't reclaim them while C holds the function pointer), per-function methods with auto `String` ↔ `Fiddle::Pointer` marshalling, and `describe()` returning a typed `Manifest` populated by walking the embedded `AetherNamespaceManifest` struct with explicit pointer offsets (mirrors the Python and Java approaches). Event names are converted from PascalCase (manifest convention) to snake_case (Ruby convention): `event("OrderPlaced", "int64")` → `on_order_placed`. Tests: `tests/integration/namespace_ruby/` mirrors the Python and Java tests. Skips cleanly if `ruby` isn't installed.

    - **Worked example**: `examples/embedded-java/trading/` ships a `trading` namespace with `placeTrade.ae`, `killTrade.ae`, `getTicker.ae`, a `manifest.ae`, a `TradingDemo.java`, and a `build.sh` that runs the whole pipeline and prints a trade book built up from the events. `tests/integration/embedded_java_trading_e2e/` runs the worked example end to end and asserts every expected line of output appears.

    Out of scope for v1 (each tracked in `docs/embedded-namespaces-and-host-bindings.md`):

    - Live host-supplied callbacks (`host_call`) — Aether scripts can't yet call back into host functions. The v1 ABI is host → script for data and script → host for events; the bidirectional case is the "Shape B" milestone.
    - Escape-hatch `import trading.manifest` to add a non-sibling script to a namespace.
    - `@private_to_file` annotation to exclude a sibling script.
    - Wall-clock timeout / allocation budget for scripts running inside a host.
    - Go SDK generator (parser captures the `go(package)` binding but the emitter is a stub).

    The v2 work folds in the `--emit=lib` transport layer (compiler flag, capability-empty default, `runtime/aether_config.{h,c,i}`, opaque `AetherValue*`, eight `tests/integration/emit_lib*/` directories) that was originally planned as a standalone v1 PR; that layer was never shipped on its own — it's the foundation v2 builds on. The full design lives at `docs/embedded-namespaces-and-host-bindings.md`; the speculative `docs/aether-embedded-in-host-applications.md` and `docs/aether-dsl-as-a-rules-engine.md` are annotated with what's now real and what's still future.

- **`notify()` flushes stdout before invoking the host event handler**: when an Aether namespace is loaded as a `.so` by a Java / Python / Ruby host via `dlopen`, libc's `stdout` is fully buffered (the `.so` doesn't see a TTY). Script-side `println()` calls would accumulate in the C-side buffer and only flush at process exit, so demo console output appeared scrambled — host event-handler `println`s landed in order, but the Aether script's preceding lines all came out at the very end. Adding `fflush(NULL)` to `notify()` in `runtime/aether_host.c` (just before the registered handler runs) ensures that anything the script printed leading up to the event is on stdout in the right order. Verified by `tests/integration/embedded_java_trading_e2e/`, which now sees `[ae] place_trade order_id=100` immediately before `[event] OrderPlaced id=100`. Cosmetic-only — does not affect correctness of the values returned by the script, only the on-screen ordering of pre-event log output.

- **Embedded namespace tests + worked example tag script-side prints with `[ae]`**: every `println()` inside `tests/integration/namespace_{python,ruby,java}/calc.ae` and `examples/embedded-java/trading/aether/{placeTrade,killTrade,getTicker}.ae` now starts with the literal `[ae] ` so a human reading the test output (or the worked example's stdout) can tell at a glance which lines came from inside the embedded Aether script versus which came from the Java / Python / Ruby host. The four test scripts grep for at least one `[ae]` line in the captured output and report `[PASS] Aether [ae] script-side output visible to host` if it landed; this is the regression test that pairs with the `notify()`-flush fix above.

- **Embedded namespace test SDKs renamed to `*_generated_sdk` to make the generated nature obvious**: the Python / Ruby modules emitted by `ae build --namespace` for `tests/integration/namespace_{python,ruby}/manifest.ae` are now `calc_generated_sdk.py` / `calc_generated_sdk.rb` instead of `calc_sdk.py` / `calc_sdk.rb`; the Java class is `CalcGeneratedSdk` instead of `Calc`. No change to the SDK generators themselves — the manifest's `python("calc_generated_sdk")` / `ruby("calc_generated_sdk")` / `java("com.example.calc", "CalcGeneratedSdk")` declarations decide the output filename, and we just chose more obvious names. Reduces the chance a future contributor accidentally edits a file in a temp directory thinking it's hand-written source.

## [0.55.0]

### Changed (breaking)

- **Stdlib migrated to Go-style `(value, err)` result types**: every stdlib function that can fail and where the failure reason matters now returns a tuple. The raw ptr/int/NULL-returning externs are preserved under `*_raw` names as an escape hatch for advanced callers. Migrated modules and the new idiomatic calls:
  - `std.http`: `body, err = http.get(url)`, `http.post(url, body, ct)`, `http.put(...)`, `http.delete(url)`. Auto-free the underlying response, return transport errors and non-2xx status as strings. Server lifecycle: `http.server_bind(server, host, port)` and `http.server_start(server)` now return an error string. Raw externs renamed: `http_get_raw`, `http_post_raw`, `http_put_raw`, `http_delete_raw`, `http_server_bind_raw`, `http_server_start_raw`.
  - `std.file` / `std.fs`: `content, err = file.read(path)` (opens, reads, closes), `handle, err = file.open(path, mode)`, `err = file.write(path, content)`, `err = file.delete(path)`, `size, err = file.size(path)`. Raw externs renamed: `file_open_raw`, `file_read_all_raw`, `file_write_raw`, `file_delete_raw`, `file_size_raw`.
  - `std.io`: `content, err = io.read_file(path)`, `err = io.write_file(path, content)`, `err = io.append_file(path, content)`, `err = io.delete_file(path)`, `info, err = io.file_info(path)`, `err = io.setenv(name, value)`, `err = io.unsetenv(name)`. Raw externs renamed: `io_read_file_raw`, `io_write_file_raw`, `io_append_file_raw`, `io_delete_file_raw`, `io_file_info_raw`, `io_setenv_raw`, `io_unsetenv_raw`.
  - `std.string`: `value, err = string.to_int(s)` and the same for `to_long`, `to_float`, `to_double`. Replaces the out-parameter anti-pattern (`string_to_int(str, &out)`). Raw externs renamed: `string_to_int_raw`, `string_to_long_raw`, `string_to_float_raw`, `string_to_double_raw`. Supporting C shims `string_try_int`/`string_get_int` (and the four variants) split the parse into a validity check plus a value extractor so the wrapper can use tuple returns.
  - `std.json`: `value, err = json.parse(json_str)`. Raw extern renamed: `json_parse_raw`.
  - `std.os`: `output, err = os.exec(cmd)`. Raw extern renamed: `os_exec_raw`. `os_system` still returns an exit code by the POSIX convention.
  - `std.tcp` / `std.net`: `sock, err = tcp.connect(host, port)`, `n, err = tcp.write(sock, data)`, `data, err = tcp.read(sock, max)`, `server, err = tcp.listen(port)`, `sock, err = tcp.accept(server)`. Raw externs renamed: `tcp_connect_raw`, `tcp_send_raw`, `tcp_receive_raw`, `tcp_listen_raw`, `tcp_accept_raw`. (Byte-transfer wrappers are named `write`/`read` because `send` and `receive` are reserved actor keywords in Aether.)
  - `std.dir` / `std.fs`: `err = dir.create(path)`, `err = dir.delete(path)`, `list, err = dir.list(path)`. Raw externs renamed: `dir_create_raw`, `dir_delete_raw`, `dir_list_raw`.
  - `std.fs`: `list, err = fs.glob(pattern)`, `list, err = fs.glob_multi(patterns)`. Raw externs renamed: `fs_glob_raw`, `fs_glob_multi_raw`.
  - `std.list` / `std.collections`: `err = list.add(list, item)`. Previously silent void; now returns an error string on resize/OOM failure. Raw extern renamed: `list_add_raw` (returns `int` 1/0 for success/failure).
  - `std.map` / `std.collections`: `err = map.put(map, key, value)`. Previously silent void; now returns an error string on resize/OOM failure. Raw extern renamed: `map_put_raw` (returns `int` 1/0).
  - `std.log`: `err = log.init(filename, level)`. Previously silent void; now returns an error string if the log file can't be opened (logging still works via stderr as a fallback). Raw extern renamed: `log_init_raw` (returns `int` 1/0).

  **User migration**: every call site that uses a migrated function needs updating. The old ptr/int-returning externs are still available via the `_raw` suffix for code that needs direct struct access. Existing tests and examples have been updated in the same change.

  **Not migrated** (infallible or already-correct): all of `std.math` (pure computation), all of `std.path` (pure string manipulation), predicate functions (`file_exists`, `map_has`, etc.), defensive lookups (`map_get`, `list_get`, `json_object_get`, `io_getenv`, `os_getenv`), HTTP response accessors, print functions, setters, and `*_free` functions. `os_system` retains its POSIX exit-code convention.

### Added

- **Aether-native stdlib wrappers**: stdlib modules can now define Aether functions alongside their C externs. Previously the module merger in `compiler/aether_module.c:894` explicitly skipped `std.*` imports on the assumption that stdlib was extern-only, which blocked Go-style result-type wrappers from being written in the module files themselves. The skip is removed; the typechecker now always registers every extern from a stdlib module (regardless of selective-import filter) so merged wrapper bodies can resolve their dependencies. Selective-import visibility for user code is still enforced at qualified-call sites via a new `is_selective_import_blocked` check in `lookup_qualified_symbol`, so `import std.math (sqrt)` still rejects `math.pow` at the call site even though `math_pow` is now in the symbol table for internal use. Codegen's stdlib-import extern emission at `codegen.c:1320` correspondingly drops its selective filter so every extern from an imported stdlib module is declared in the generated C. The int→ptr call-site cast at `codegen_expr.c:1140` now matches both the original call-site name and the dot-normalized C name, so merged stdlib wrappers with ptr parameters also get their arguments auto-cast, and now also handles `string → ptr` conversion to silence C's discards-qualifiers warning when a string is passed into a ptr-typed stdlib wrapper. Supporting changes include a new syntax test `tests/syntax/test_string_parse_tuple.ae` covering the new `string.to_int/to_long/to_float/to_double` wrappers.

- **HTTP client response accessors**: `http.response_status`, `http.response_body`, `http.response_headers`, `http.response_error`, and `http.response_ok` let Aether code read fields out of the struct returned by `http.get_raw`/etc. Previously the client was a smoke test: you could fire a request and free the response but had no way to read status code, body, headers, or transport error from Aether. All accessors are NULL-safe. `http.response_ok` returns 1 only when there's no transport error AND the status is 2xx, which is the idiomatic "did it work?" check. Added in `std/net/aether_http.{c,h}`, exported in `std/http/module.ae` and `std/net/module.ae`, with C runtime tests in `tests/runtime/test_runtime_http.c`, a syntax test in `tests/syntax/test_http_client_accessors.ae`, and a runnable example in `examples/stdlib/http-client.ae`.

- **`net.await_io(fd)` — reactor-pattern async I/O for actors**: exposes the runtime's per-core I/O reactor (`scheduler_io_register` / `MSG_IO_READY`) to Aether code. An actor calls `net.await_io(fd)` to suspend until `fd` becomes readable; when the scheduler's poller observes the event, it delivers an `IoReady { fd, events }` message to the actor's mailbox and resumes it on any available core — without blocking any scheduler thread. The infrastructure already existed in `runtime/scheduler/` (PR #140 demonstrated it at the C level delivering substantially higher HTTP throughput than the blocking keep-alive worker it replaced); this change makes it reachable from `.ae` sources. Implementation: (1) `MSG_IO_READY` renumbered from 300 to 255 to fit inside the 256-slot generated actor dispatch table; (2) `register_message_type` in `runtime/actors/aether_message_registry.c` reserves ID 255 exclusively for the name `IoReady` and skips it when handing out user IDs, so any Aether program that defines `message IoReady { fd: int, events: int }` lands on the slot the scheduler delivers to; (3) a new TLS `void* g_current_step_actor` declared in `runtime/scheduler/multicore_scheduler.h` is set by every generated `*_step()` function at entry (`compiler/codegen/codegen_actor.c:347`), making the currently-running actor identifiable from the bridge regardless of scheduling mode (main-thread sync, scheduler dispatch, work-stealing); (4) new `std/net/aether_actor_bridge.c` reads `g_current_step_actor` + `current_core_id` from TLS and calls `scheduler_io_register`. The Aether surface lives in `std/net/module.ae` and `std/http/module.ae`: `net.await_io(fd) -> string` returns `""` on success or an error string, plus minimal pipe/fd helpers (`net.pipe_open`, `net.fd_write`, `net.fd_close`) that let user code produce a testable readable fd without going through TCP. The bridge is linked into `libaether.a` / tests / user programs but excluded from the `aetherc` compiler binary via a new `STD_REACTOR_SRC` split in the Makefile (the compiler doesn't link the scheduler runtime). End-to-end test in `tests/syntax/test_await_io.ae` creates a pipe, spawns a Reader actor that calls `await_io` on the read end, then writes "hello" to the write end from main; the scheduler's I/O poller observes the fd is ready, delivers `IoReady` to the actor, and the handler prints success. Proposed by Ariel Mirra.

- **macOS Gatekeeper handling for `ae version install` / `ae version use`**: new `macos_prepare_binary` / `macos_prepare_bin_dir` helpers in `tools/ae.c` re-sign freshly installed/copied binaries in place with `codesign --force --sign -` and clear resource-fork/xattr state. Without this, the first run of any binary from a fresh install hung for several minutes under `syspolicyd` or died with `Killed: 9`, because released Aether binaries inherit an adhoc signature but no local Gatekeeper assessment. Called from both `cmd_version_install` (after extraction) and `cmd_version_use` (after copy to `~/.aether/bin/`). Also removed the self-invoke `--sync-from` call in `cmd_version_use` which triggered the same Gatekeeper hang on the just-copied binary — the existing in-process sync of `lib/`, `include/`, `share/` replaces it. Replaced the silent-failure `cp -f "src"/* "dest"/ 2>/dev/null; true` with an explicit `sh -c 'set -e; for f in ...; do cp ...; done'` plus a post-copy check that the destination `ae` binary exists. Verified end-to-end on macOS arm64: `ae version use v0.30.0` and `ae version use v0.45.0` both complete in under a second with no hang and no SIGKILL. Fixes #88.

### Added

- **`hide` and `seal except` scope directives** — declare at the top of any block that one or more identifiers from outer scopes are unreachable from this block (and every nested block within it). `hide name1, name2` blocks specific outer bindings. `seal except name1, name2` is the inverse — blocks ALL outer bindings except the listed whitelist. Both directives are scope-level (position within the block doesn't matter), apply to reads and writes, propagate to all nested blocks, and don't reach through call boundaries (a visible function defined in an outer scope can still use the names via its own lexical chain). Forbids redeclaring a hidden name in the same scope. New error code `E0304` ("`X` is hidden in this scope by `hide` or `seal except`"). See `docs/hide-and-seal.md` for the full design rationale and edge cases. Tests: `tests/syntax/test_hide_basic.ae`, `tests/syntax/test_seal_except.ae`, `tests/integration/hide_seal_directives/test_hide_reject.sh`.
- **Filesystem stdlib bundle in `std.fs`**: five small POSIX wrappers that let Aether programs stop shelling out for routine filesystem operations, each following the 0.55.0 `_raw` + Go-style wrapper convention:
  - `fs_mkdir_p_raw(path)` / `err = fs.mkdir_p(path)` — `mkdir -p` semantics: creates the path and any missing parent directories, treats already-existing directories as success.
  - `fs_symlink_raw(target, link_path)` / `err = fs.symlink(target, link_path)` — create a symbolic link. The target string is recorded verbatim, so relative targets stay relative.
  - `fs_readlink_raw(path)` / `target, err = fs.readlink(path)` — read a symlink's target. Returns `("", "not a symlink")` when `path` isn't a symlink.
  - `fs_is_symlink(path)` — pure boolean query: returns 1 if `path` is itself a symlink, 0 otherwise. Does NOT follow the link. No Go-style wrapper (matches `file_exists` / `dir_exists` shape).
  - `fs_unlink_raw(path)` / `err = fs.unlink(path)` — remove a file or symlink. Refuses to remove directories (use `dir.delete` for that).
  Windows symlink ops (`fs_symlink_raw` / `fs_readlink_raw` / `fs_is_symlink`) are stubbed pending `CreateSymbolicLinkW` + a junction fallback for directories; `fs_mkdir_p_raw` and `fs_unlink_raw` work on Windows via `_mkdir` / `_unlink`. Tests: `tests/syntax/test_fs_stdlib_bundle.ae` (14 sub-cases including nested-dir creation, idempotent `mkdir_p`, symlink/readlink/is_symlink round-trip, refuse-to-unlink-a-directory, refuse-to-unlink-missing-path).

- **`os_which` in `std.os`**: search `$PATH` for an executable. Returns the absolute path to the first hit, or `""` if not found. Kept as a plain extern (not `_raw` + wrapper) because "not found" is a valid answer, not a failure — matches how `file_exists` / `dir_exists` are modelled. If `name` already contains `/`, it's returned as-is when it's executable (matches POSIX `command -v`). Empty `PATH` entries match the current directory (POSIX). Windows is stubbed pending PATHEXT-aware lookup. Tests: `tests/syntax/test_os_which.ae` (5 sub-cases).

### Fixed

- **Composite types in message fields (`string[]`, `int[]`) silently corrupted data**: Parser accepted `sites: string[]` in message definitions, but the runtime message registry only stored a single `type_kind` per field, dropping element-type information. Receive-pattern destructuring generated `int* sites = _pattern->sites;` when the struct field was actually `const char**`, and the send-side emitted `.sites = {"a", "b"}` inside designated initializers where C interprets the braces as a scalar initializer list rather than a pointer. Reading any element in the handler produced garbage (e.g. `1819043176` = ASCII bytes of "hell" read as int). Four-part fix: (1) `MessageFieldDef` now carries resolved `c_type` and `element_c_type` strings populated at registration time via `get_c_type`; (2) receive-pattern destructuring in `codegen_actor.c` uses the stored `c_type` instead of rebuilding a stub `Type` from `type_kind`; (3) send-side codegen pre-walks message field initializers and hoists each array literal to a `static const T _aether_arr_N[] = {...}` local before the `{ Msg _msg = ...; send; }` block, so the array's storage has program lifetime instead of expiring when the send-expression block exits. Hoisting is essential for fire-and-forget sends because the receiver processes the message asynchronously, after the sender's compound literal would have gone out of scope; without the hoist the receiver reads freed memory (observed on Linux gcc and Windows, worked by coincidence on macOS clang); (4) typechecker now propagates element type onto `AST_ARRAY_ACCESS` nodes so print format specifiers and string interpolation pick the correct specifier. Added `tests/syntax/test_message_array_fields.ae` covering `string[]`, `int[]`, and mixed scalar+array messages, and `examples/actors/message-with-arrays.ae` demonstrating the pattern. Reported by Teuvo Eloranta.
- **HTTP client example taught the wrong error-handling pattern**: `docs/stdlib-api.md` and `docs/stdlib-reference.md` showed `if response != 0 { println("Got response") }` as a success check, but `http.get_raw` always returns a non-null response (unless out of memory); on DNS/connect/send failure it returns a struct with `error` set. The documented pattern ran the success branch for failed requests. Both docs now use the new `http.response_ok` accessor and explicitly warn that `response != 0` is not a success check. Reported by Teuvo Eloranta.
- **Array type notation in style guide did not match parser**: `docs/type-annotation-style-guide.md` showed the prefix form `[int]` / `[string]` for array types, but the parser has only ever accepted the C-style suffix form `int[]` / `string[]` (consistent with `docs/type-inference-guide.md`). Updated the style guide to the correct form and added a note flagging the prefix form as historical. Reported by Teuvo Eloranta.
- **String memory leak in loops**: `string_concat`, `string_substring`, `string_to_upper`, `string_to_lower`, `string_trim`, and string interpolation in loops no longer leak the previous value on reassignment. The codegen now emits a heap-ownership flag per string variable and frees the old value before each reassignment, preventing O(n²) memory growth that caused OOM on tight loops.
- **`_heap_<var>` undeclared in late string reassignment**: When a string variable was first introduced inside a nested block (e.g. the else branch of an `if`/`else`) via a heap-string expression, the codegen emitted a reassignment wrapper referencing `_heap_<var>` without ever declaring the flag, producing "undeclared identifier" errors in the generated C. The reassignment path now lazy-declares the heap-ownership flag if it isn't already tracked, and the initial-declaration path marks the variable so the lazy path doesn't double-declare. Regression test: `tests/syntax/test_string_late_heap_reassign.ae`.
- **`fs_glob` recursive walker hid dot-prefixed files**: `walk_recursive` previously skipped every directory entry whose name started with `.`, so patterns like `**/.build.ae` or `**/.*.ae` always returned zero results — both `.git`/`.aeb` directories AND legitimate dot-files like `.build.ae` were filtered out at the dir-entry level. The walker now skips only `.` and `..` unconditionally, skips dot-prefixed *directories* from recursion (preserving the original intent), and passes `FNM_PERIOD` to `fnmatch` so dot-prefixed *files* match when the pattern explicitly includes a leading dot, matching POSIX shell-glob semantics. Regression test: `tests/syntax/test_glob_dotfiles.ae`.

- **`make test-ae` swallowed runtime-failure diagnostic output**: the parallel test runner captured compile-phase stderr to a per-test file but piped the run-phase stdout and stderr directly to `/dev/null`, so any test that built cleanly and then failed at runtime produced only `[FAIL] name (runtime error)` followed by an empty `--- name ---` block in FAILURE DETAILS. A Windows CI run on an unrelated PR made this obvious: `syntax_test_glob_dotfiles` failed with no recoverable error output, no exit code, no stdout, no stderr — nothing actionable. The inline runner built by the `test-ae` target now redirects the run-phase streams to per-test files under the shared tempdir (`build_<name>.{out,err}`, `run_<name>.{out,err}`, `rc_<name>.txt`, `phase_<name>.txt`), and FAILURE DETAILS emits a structured block per failed test with a horizontal rule, the failing phase (`compile` / `runtime` / `shell`), the runtime exit code, and the captured stdout/stderr indented for readability. Verified by injecting a deliberate runtime failure (`exit(42)` after three `println` calls) and a deliberate compile error (undefined variable + undefined function): both now surface their full diagnostic output in the CI log.

## [0.48.0]

### Added

- **`callback` keyword for trailing blocks**: Third trailing-block mode — `func(args) callback { body }` creates a real closure (hoisted, captures variables from scope) rather than an inline DSL block. The block runs later when invoked, not at construction time. Also supports explicit params (`callback |x: int| { ... }`) and arrow bodies (`callback |x| -> x * 2`).

## [0.47.0]

### Changed

- **Module resolver state moved from globals to registry**: `source_dir` and `lib_dir` are now fields on `ModuleRegistry` instead of file-level statics, so state is scoped to the registry lifecycle. Prepares the compiler for safe reuse as a library (e.g. from the LSP).
- **`AETHER_LIB_DIR` environment variable**: Set `AETHER_LIB_DIR` to configure the module library directory without passing `--lib` on every invocation. `--lib` takes precedence if both are set.

## [0.48.0]

### Added

- **`callback` keyword for trailing blocks**: Third trailing-block mode — `func(args) callback { body }` creates a real closure (hoisted, captures variables from scope) rather than an inline DSL block. The block runs later when invoked, not at construction time. Also supports explicit params (`callback |x: int| { ... }`) and arrow bodies (`callback |x| -> x * 2`).

## [0.47.0]

### Added

- **Builder functions** (renamed from `defer`): `builder` keyword for function definitions enables "configure then execute" DSL pattern. The trailing block runs first to fill a config object, then the function executes with it via implicit `_builder` parameter. Renamed to avoid overloading `defer` (which remains for scope cleanup). Complements the existing regular trailing-block pattern ("function first, block decorates").
- **Configurable builder factory with `with` clause**: `builder func(...) with factory_fn { ... }` lets SDK authors specify what config object the trailing block operates on. Defaults to `map_new`; alternatives include `list_new` or any user-defined zero-arg factory.
- **`--lib` flag for custom module library directory**: `aetherc --lib DIR` resolves imports from a custom directory instead of the default `lib/`. Threaded through `ae run` and `ae check`. Enables build tools like aetherBuild to use `.aeb/lib/` without polluting the project's `lib/`.
- **Unqualified selective imports**: `import mymodule (foo, bar)` now registers short names so `foo()` can be called without the `mymodule.` prefix. Qualified calls (`mymodule.foo()`) continue to work alongside unqualified ones. Enables clean DSL blocks where setter functions don't need module qualification:
  ```aether
  import build
  import build (release, lint, werror)

  main() {
      b = build.start()
      build.javac(b) {
          release("21")    // no build. prefix needed
          lint("all")
          werror()
      }
  }
  ```

### Fixed

- **Builder functions not merged from modules**: `module_merge_into_program` only handled `AST_FUNCTION_DEFINITION`, silently skipping `AST_BUILDER_FUNCTION`. Module-defined builder functions are now merged correctly.
- **Builder context `_ctx` injection failed for module-qualified calls**: The builder function registry compared names with underscores (`build_release`) against dotted call names (`build.release`), so `_ctx` was never injected. Fixed: normalize dots to underscores before comparison. Same fix applied to `is_builder_func_reg` and `get_builder_factory`.
- **Duplicate function definitions from repeated module imports**: `import build` followed by `import build (foo)` merged `foo` twice, with the second copy using generic `_argN` parameter names. Fixed: `module_merge_into_program` now skips already-merged functions.
- Fixed `-Wunused-result` warnings in `tools/ae.c` for `fread` and `system` calls.
- `install.sh` now prints a message before `git fetch --tags` so users know why SSH credentials may be requested.

## [0.45.0]

### Added

- **Core placement hint**: `spawn(Actor(), core: N)` distributes actors across scheduler cores. Actors on different cores run on different OS threads, enabling parallel I/O-heavy workloads. Use with `num_cores` builtin: `spawn(Worker(), core: i % num_cores)`.
- **Platform I/O poller**: `runtime/io/aether_poller.h` provides a unified event notification API with epoll (Linux), kqueue (macOS/BSD), and poll() (portable) backends. Foundation for non-blocking I/O and [PR #71](https://github.com/nicolasmd87/aether/pull/71) actor-integrated HTTP.
- **Socket timeouts on all stdlib TCP operations**: `tcp_connect`, `tcp_accept`, and HTTP server connections now set 30-second `SO_RCVTIMEO`/`SO_SNDTIMEO`. A dead or slow peer returns an error instead of hanging the thread forever.
- **HTTP server bounded thread pool**: Fixed pool of 8 worker threads replaces unbounded thread-per-connection. Prevents resource exhaustion under load.
- **HTTP server graceful shutdown**: `poll()`-based accept loop with 1-second timeout. Server checks `is_running` between connections and shuts down cleanly without waiting for the next client.

### Changed

- **Inline message path extended to int64/ptr/bool/actor_ref**: Single-field messages with `long`, `ptr`, `bool`, or actor ref types now skip heap allocation entirely — value stored directly in `Message.payload_int`. Previously only `int` (32-bit) qualified. Eliminates malloc+free for single-field messages, the most common pattern in tree-spawn and request-response workloads.
- **TLS caching in scheduler hot path**: `current_core_id` cached in a local variable at function entry in `scheduler_send_local` and `aether_send_message`, reducing TLS accessor overhead on macOS.
- **Partial batch enqueue**: `queue_enqueue_batch` now returns how many messages fit instead of failing the entire batch when the queue is near full. Eliminates the all-or-nothing behavior that forced remaining messages through the slower per-message fallback path.
- **Batch flush uses partial retry instead of per-message fallback**: `scheduler_send_batch_flush` retries `queue_enqueue_batch` with remaining messages instead of falling back to individual `scheduler_send_remote` calls, reducing kernel yield overhead in high-throughput fan-out patterns.
- **Missing benchmark implementations**: Added Pony, Java, and Scala skynet benchmarks for complete cross-language coverage (11 languages × 5 patterns = 55 benchmarks, zero skips).
- **Benchmark fairness**: Standardized skynet throughput to count total tree nodes across all 11 languages. Fixed Java detection on macOS (GNU sed `\U` → portable case statement). Fixed Scala package namespacing for sbt compilation.
- **Benchmark statistical rigor**: 5 runs per benchmark (median reported). JVM/BEAM languages get warmup runs before measurement. JSON results include min/max, coefficient of variation (CV%), and individual run values.
- **Benchmark visualization rewrite**: Sortable table columns, CV% color coding (green/orange/red), relative performance bars, min-max range, efficiency metric (throughput/MB), methodology explanation, Savina paper citation. Skynet tab added.
- **Benchmark runner rewritten in Aether**: `run_benchmarks.ae` replaces the bash script — compiles all 11 languages, runs benchmarks, parses output, computes statistics, writes JSON results. Dogfoods `std.os`, `std.string`, `std.io`.
- **Benchmark visualization server rewritten to pure stdlib**: `server.ae` replaced 26 `extern` FFI declarations and 200 lines of hand-written C (`server_ffi.c`) with stdlib imports (`std.net`, `std.io`, `std.string`, `std.os`). Zero C code required.
- **Inner benchmark Makefile removed**: All benchmark orchestration moved to root Makefile. `make benchmark` directly builds the Aether runner, runs benchmarks, builds the server, and launches the UI.
- **`string_array_get` bug fix**: Returned `AetherString*` (struct pointer) instead of `const char*` (raw string data). Any code using `string_split` + `string_array_get` with `print`, `string_contains`, etc. got garbled output. Fixed to return `s->data`.
## [0.44.0]

### Fixed

- **Module function return types not inferred across module boundaries**: When calling a module function that returns `string` or `ptr` (e.g., `result = mymod.greet("world")`), the type inferrer defaulted to `int` because `infer_type()` used `lookup_symbol()` which couldn't resolve dotted names like `mymod.greet`. Fixed: use `lookup_qualified_symbol()` instead, with guards to skip `void`/`unknown` types (which occur for pure-Aether functions whose return types haven't been inferred yet). Regression test added: `tests/integration/module_return_types/`.

### Changed

- **`MAX_MODULE_TOKENS`** increased from 2,000 to 20,000 — modules with many functions (e.g., a build system SDK) silently truncated at the old limit with no error message.
- **`MAX_TOKENS`** increased from 10,000 to 50,000 — same issue for large source files.

## [0.43.0]

### Changed

- **Sandbox preamble only emitted when needed**: Generated C no longer includes ~40 lines of sandbox bridge code (permission checker, `list_size`/`list_get` externs, `spawn_sandboxed` declaration) for programs that don't use sandboxing. Follows the same AST-scanning pattern as the existing actor detection.
## [0.42.0]

### Added

- **`fs_glob_multi(patterns)` in `std.fs`**: Multi-pattern glob that takes a list of patterns and returns merged results. Enables Starlark-style `glob(["**/*.c", "**/*.h"])` for build DSLs.

## [0.41.0]

### Fixed

- **`ae test` included library modules** (#109): `ae test` discovered `lib/*/module.ae` files and tried to build them as standalone programs, failing because they have no `main()`. Fixed: convention-based test discovery — only files named `test_*.ae` or `*_test.ae` are recognized as tests (like pytest's `test_*.py` or Go's `*_test.go`).
- **Module resolution didn't search relative to source file** (#99): `aetherc` resolved `lib/` and `src/` imports relative to CWD only, so integration tests with local `lib/` directories failed when run from the repo root. Fixed: `module_resolve_local_path()` now also searches relative to the source file's directory.
- **Windows: stdlib path functions used backslashes** (#99): `path_join()` used `\` on Windows, breaking cross-platform tests. `path_is_absolute()` didn't recognize `/` on Windows. Fixed: `path_join()` uses `/` universally; `path_is_absolute()` accepts both `/` and drive letters.
- **Windows: regression tests hardcoded `/tmp/`** (#99): File I/O tests used `/tmp/` which doesn't exist on native Windows. Fixed: tests use `TEMP`/`TMPDIR` env vars with `/tmp` fallback.
- **`ae version` showed stale version after install** (#88): `ae version list` checked the `current` symlink (stale from previous `ae version use`) before the `active_version` file. Fixed: `active_version` file is now the authoritative source. Also fixed `install.sh` reading the old version from the `ae` binary instead of the `VERSION` file.
- **Non-existent version install gave misleading message** (#95): `ae version install v0.99.0` downloaded a 404 HTML page, extracted 0 files, and suggested `--force`. Fixed: validates downloaded archive magic bytes (gzip/zip/xz) and fails immediately with a clear error if the version doesn't exist.
- **CI now runs `ae test`**: Added step [8/9] to `make ci` that runs the user-facing `ae test` command, catching divergence between the Makefile test runner and the CLI test runner.
## [0.40.0]

### Added

- **Runtime containment sandbox**: Deny-by-default grant system for filesystem, network, process, and environment access. Stdlib functions (`tcp_connect`, `file_open`, `os_exec`, `os_getenv`) check grants transparently — contained code has no idea it's sandboxed.
- **Sandbox DSL**: `sandbox("name") { grant_tcp("host"); grant_fs_read("/path/*"); ... }` with builder-style trailing blocks and invisible `_ctx` injection.
- **Grant pattern matching**: Prefix (`/etc/*`), suffix (`*.example.com`), exact, and wildcard (`*`) patterns for all grant types.
- **Nested sandbox restriction**: Inner sandboxes can only narrow, never escalate past outer sandbox grants.
- **`spawn_sandboxed(perms, program, arg)`**: Cross-process sandbox enforcement via POSIX shared memory and LD_PRELOAD. No temp files.
- **`libaether_sandbox.so`**: LD_PRELOAD library intercepting `open`, `connect`, `getenv`, `execve`, `dlopen`, `mmap(PROT_EXEC)`, `mprotect(PROT_EXEC)`, `fork`, `vfork`, `clone3`, `bind`, `listen`, `accept`. Built as part of `make stdlib`.
- **Denial logging**: File (default, `./aether-sandbox.log`), stderr (`AETHER_DENIED:` prefix), or silent. Controlled via `AETHER_SANDBOX_LOG` env var.
- **Six language host modules** (`contrib/host/`): Python, Lua, JS (Duktape), Perl, Ruby, Java. Each runs hosted code inside an Aether sandbox.
- **Token-guarded shared map**: `string:string` data exchange between Aether and hosted languages with frozen inputs, one-time token, and revoke-on-return. Native bindings (`aether_map_get`/`aether_map_put`) for all six languages.
- **Escape prevention**: `dlopen("libc.so.6")` blocked, `syscall()` blocked, `mmap(PROT_EXEC)` anonymous blocked, `mprotect(PROT_EXEC)` blocked, `fork`/`vfork`/`clone3` blocked by default (grant with `fork:*`).
## [0.39.0]

### Added

- **Named arguments**: `func(name: "alice", count: 3)` syntax in function calls. Names are documentation at the call site — consistent with Aether's `param: type` definition syntax. Positional and named can be mixed.
- **List literal tests**: Confirmed `[1, 2, 3]` and `["a", "b", "c"]` array literal syntax works (already existed in parser/codegen, now tested).
- **`select()` platform conditional**: Compile-time platform selection via named args. `select(linux: 8080, windows: 80, macos: 8080)` emits `#ifdef` chain in generated C. Supports `other:` fallback. Integer values work; string values require interpolation workaround pending type inference improvement.

## [0.35.0]

### Added

- **Heredoc strings**: `<<MARKER ... MARKER` syntax for multiline string literals. Preserves newlines, indentation, and special characters. Literal only (no `${expr}` interpolation — use regular strings for that). Left-shift operator `<<` is unaffected (heredoc only triggers when followed by an identifier). Dynamic buffer (no 64KB limit). Windows CRLF line endings handled.
- **`fs_glob(pattern)`**: Match files by pattern with `*`, `?`, and `**/` (recursive). Returns `DirList*` iterable via `dir_list_count`/`dir_list_get`/`dir_list_free`. Uses POSIX `glob()` for simple patterns and recursive directory walk for `**` patterns.
- **`dir_list_count(list)`** and **`dir_list_get(list, index)`**: Iterate `DirList` results from `dir_list()` and `fs_glob()`.
- **`aether_args_count()`** and **`aether_args_get(index)`**: Access command-line arguments via `std.os`. Exposes the runtime's existing `argc`/`argv` to Aether code. Returns `NULL` for out-of-bounds or negative indices.
- **`file_mtime(path)`**: File modification time as Unix timestamp (int). Returns 0 for nonexistent files. For incremental build support.
- **Lazy evaluation**: `lazy(closure)`, `force(thunk)`, `thunk_free(thunk)` builtins for deferred computation with memoization. Explicit forcing, eager by default.

## [0.32.0]

### Fixed

- **macOS Gatekeeper quarantine**: `install.sh` and `ae version use` now run `xattr -cr` to remove quarantine on macOS, fixing `Killed: 9` errors when switching versions
- **`ae version use` preserves initial install**: On both POSIX and Windows, the currently active version is backed up to `~/.aether/versions/` before switching, so it can be switched back to later
- **Uninitialized `tuple_types` in Type struct**: `FLUSH_LIT` macro in string interpolation parser used raw `malloc` without initializing `tuple_types`/`tuple_count`, causing `free_type()` to follow garbage pointers on Windows (ACCESS_VIOLATION `0xC0000005`)
- **Uninitialized `interp_as_printf` in CodeGenerator**: String interpolation in value assignments emitted `printf()` (returns int) instead of `_aether_interp()` (returns void*) on Windows due to uninitialized flag
- **Windows `_spawnvp` for command execution**: Replaced `system()` + `cmd /c` with `_spawnvp` on Windows to avoid shell quoting issues that caused random test failures
- **Unused variable checker false positive**: Match list patterns implicitly reference `<array>_len` variables via codegen — checker now marks them as used
- **Test runner failure details**: Failure summary now prints at the end of test-ae showing captured stderr for each failing test

## [0.31.0]

### Added

- **`ae check` command**: Type-check without compiling. Runs lexer → parser → typechecker → type inference, then exits. No C code generated, no gcc invoked, so iteration is much faster than `ae build`. Also available as `aetherc --check`
- **Unused variable warnings `[W1001]`**: Warns on declared-but-never-referenced local variables. Prefix with `_` to suppress (e.g., `_unused = 42`). Excludes function parameters and pattern bindings
- **Unreachable code warnings `[W1002]`**: Detects code after `return`, `exit()`, or exhaustive `if`/`else` blocks where all branches terminate. Recurses into nested blocks
- **Match expressions**: `msg = match x { 0 -> "ok", 1 -> "err", _ -> "unknown" }` — match can now be used as an expression on the right side of an assignment. Type is inferred from the first arm's result
- **For-in loop and match tests**: `test_for_in.ae` (range loops, variable bounds, nested), `test_match.ae` (statement match, expression match, wildcard, string/int patterns)
- **`ae build --target wasm`**: Compiles Aether to WebAssembly via Emscripten. Detects `emcc` on PATH, uses cooperative scheduler, sets `AETHER_NO_*` flags automatically. Produces `.js` + `.wasm` pair. Also reads `[build].target` from `aether.toml`
- **Actor timeouts**: `receive { Pattern -> body } after N -> { timeout_body }` fires timeout handler if no message arrives within N milliseconds. One-shot: cancelled when a message is received. `TOKEN_AFTER` keyword, `AST_TIMEOUT_ARM` node, `timeout_ns`/`last_activity_ns` fields in `ActorBase`, scheduler awareness for timeout polling
- **Cooperative preemption (opt-in)**: Two levels, both zero-cost when disabled. Scheduler-side: `AETHER_PREEMPT=1` env var enables time-based drain loop break after 1ms (configurable via `AETHER_PREEMPT_MS`). Codegen-side: `aetherc --preempt` inserts `sched_yield()` at loop back-edges with reduction counter (10000 iterations per yield). Prevents tight loops from starving other actors
- **`_aether_clock_ns` always available**: Moved nanosecond clock helper out of `#if !AETHER_GCC_COMPAT` guard so it's available on all platforms (needed by timeout checks)
- **Result types (multiple return values)**: Go-style `a, err = func()` tuple destructuring. Functions return multiple values with `return val, err`. `_` discards unwanted values. TYPE_TUPLE in type system, AST_TUPLE_DESTRUCTURE in parser, tuple struct generation in codegen. Chained error propagation works correctly across function boundaries
- **Package registry v1**: `ae add host/user/repo[@version]` downloads packages from any git host (GitHub, GitLab, Bitbucket, Codeberg, self-hosted) with optional version tags. Module resolver searches `~/.aether/packages/` for installed packages. Version stored in `aether.toml` `[dependencies]`

### Breaking Changes

- **Stdlib I/O functions** will migrate to `(value, error)` return types in a future release. Current `int` returns still work. The `error-handling.ae` example demonstrates the new pattern.

### Fixed

- **Selective imports**: `import std.math (sqrt, abs_int)` now works correctly. The prefix-stripping comparison was comparing user-facing names (`sqrt`) against C-level names (`math_sqrt`). Fixed in typechecker, codegen, and module merge. Non-selected symbols are properly rejected at both type-check and code generation time
- **Match-as-expression codegen**: Parser now attaches match as child of variable declaration. Codegen declares variable, then generates match with `var = arm_result;` in each arm. Type inference propagates arm result type to variable

## [0.30.0]

### Added

- **Platform portability layer**: Compile-time `AETHER_HAS_*` flags in `runtime/config/aether_optimization_config.h` (Tier 0) auto-detect platform capabilities and degrade gracefully. 9 feature flags: `AETHER_HAS_THREADS`, `AETHER_HAS_ATOMICS`, `AETHER_HAS_FILESYSTEM`, `AETHER_HAS_NETWORKING`, `AETHER_HAS_NUMA`, `AETHER_HAS_SIMD`, `AETHER_HAS_AFFINITY`, `AETHER_HAS_GETENV`, `AETHER_HAS_MALLOC`. Override any flag with `-DAETHER_NO_<FEATURE>` (e.g. `-DAETHER_NO_THREADING`)
- **Cooperative scheduler** (`runtime/scheduler/aether_scheduler_coop.c`): Single-threaded scheduler backend implementing the same API as the multi-core scheduler. Enables Aether programs to run on platforms without pthreads — WebAssembly (Emscripten), embedded systems, bare-metal. All actors run on core 0 via `aether_scheduler_poll()`. Multi-actor programs work cooperatively including ask/reply patterns
- **Makefile `PLATFORM` variable**: `PLATFORM=native` (default), `PLATFORM=wasm`, `PLATFORM=embedded`. Selects scheduler backend and sets appropriate `-DAETHER_NO_*` flags automatically. Also auto-detects `AETHER_NO_THREADING` in `EXTRA_CFLAGS` and switches to cooperative scheduler
- **Stdlib platform stubs**: `std/fs/`, `std/io/`, `std/os/`, `std/net/` modules return errors gracefully when `AETHER_HAS_FILESYSTEM` or `AETHER_HAS_NETWORKING` is 0. Console I/O (`print`, `println`) always works
- **C11 atomics fallback**: When `AETHER_HAS_ATOMICS == 0`, `atomic_int` → `volatile int`, `atomic_load` → identity, `atomic_store` → assignment. Safe for single-threaded builds
- **Emscripten timing fallback**: Generated code uses `emscripten_get_now()` instead of `rdtsc`/`clock_gettime` when compiled with Emscripten
- **Docker CI for cross-platform verification**: `docker/Dockerfile.wasm` (Emscripten SDK), `docker/Dockerfile.embedded` (ARM Cortex-M4 via arm-none-eabi-gcc)
- **Makefile CI targets**: `make ci-coop` (cooperative scheduler on native), `make ci-wasm` (Emscripten cross-compile + Node.js execution), `make ci-embedded` (ARM syntax-check), `make ci-portability` (all three)
- **Cooperative scheduler tests**: `test_platform_caps.ae` (multi-actor state), `test_coop_chain.ae` (4-actor message chain), `test_coop_many_actors.ae` (10 actors), `test_coop_ask_reply.ae` (synchronous ? operator with queued messages), `test_coop_self_send.ae` (self-send countdown, tick loop, concurrent self-senders), `test_coop_stubs.ae` (platform-independent operations), `test_stub_behavior.ae` (stub error handling)
- **Cooperative demo**: `examples/actors/cooperative-demo.ae` — supervisor distributing tasks to 3 workers, works identically in threaded and cooperative modes

### Fixed

- **Use-after-free in cooperative sync path**: `aether_send_message_sync()` passed a stack pointer as `msg.payload_ptr` and relied on `g_skip_free` to prevent freeing. When other messages were queued ahead in the mailbox, the immediate `step()` consumed a different message (FIFO order), and the stack-allocated message was freed later by `scheduler_wait()` after the stack frame was gone. Cooperative mode now heap-allocates message data via `malloc()`
- **`pthread_attr_t` typedef conflict on macOS/Linux**: `aether_thread.h` no-thread stubs redefined `pthread_attr_t` as `int`, conflicting with system headers. Now includes `<pthread.h>` for types on hosted platforms and uses macro redirects for no-op function stubs
- **Duplicate `g_sync_step_actor` symbol**: Defined in both `aether_scheduler_coop.c` and `aether_send_message.c`, causing linker errors. Changed to `extern` in cooperative scheduler
- **`AETHER_HAS_GETENV` operator precedence bug**: Missing parentheses around `defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 0)` caused incorrect evaluation
- **`AETHER_HAS_NUMA` incomplete**: Was only enabled on Linux, but Windows has NUMA APIs (`VirtualAllocExNuma`). Now enabled on both Linux and Windows
- **WASM computed goto crash**: Generated `static void* dispatch_table[256]` with label addresses caused LLVM WASM backend crash (`relocations for function or section offsets`). `AETHER_GCC_COMPAT` now excludes `__EMSCRIPTEN__`, using switch-case fallback for WASM
- **Missing `#include <emscripten.h>`**: Was buried inside `#if !AETHER_GCC_COMPAT` block, not reaching `rdtsc()` function. Moved to top-level includes conditional on `__EMSCRIPTEN__`
- **Dead `#include <pthread.h>` in codegen.c**: Included but never used — removed
- **`system()` return value warnings in ae.c**: 5 calls to `system()` with unchecked return values now check results and warn on failure
- **Duplicate `-lm` linker flag**: `ae` build used `-lm $(LDFLAGS)` where `LDFLAGS` already contained `-lm`

## [0.29.0]

### Added

- **`free()` builtin**: `free(ptr)` is now a language builtin for releasing heap-allocated memory. Use `defer free(ptr)` after calling stdlib functions that return malloc'd strings (`io.getenv()`, `io.read_file()`, `os.exec()`, `os.getenv()`, `file.read_all()`, `json.stringify()`, `json.get_string()`, `fs.path_join()`, etc.). Generates a clean `free((void*)ptr)` cast in the C output — no wrapper functions needed
- **Memory-safe stdlib examples**: All stdlib examples (`file-io.ae`, `io-demo.ae`, `os-demo.ae`, `json-demo.ae`) now use `defer free()` to release heap-allocated strings returned by stdlib functions

### Fixed

- **Release pipeline computed wrong next version**: `actions/checkout@v4` with `fetch-depth: 0` doesn't guarantee all tags are fetched. The prepare job's `git tag -l --sort=-version:refname` got a partial tag list, computing the wrong next version (e.g., 0.22.0 instead of 0.28.0). Added `fetch-tags: true` to prepare, bump, and tag jobs

## [0.28.0]

### Added

- **Pure Aether modules**: Write reusable `.ae` libraries without C backing files. `import mymath` loads `lib/mymath/module.ae`, and `mymath.func()` calls functions defined in pure Aether. Supports functions with type inference, constants (`const PI = 3`), and intra-module calls (module functions calling each other). Implemented via AST merge with namespace renaming — cloned module functions are inserted into the main program as regular top-level definitions, so the entire downstream pipeline (type inference, typechecking, codegen) works unchanged
- **Export visibility enforcement**: `export` keyword controls which functions and constants are part of a module's public API. `export double_it(x) { ... }` and `export const PI = 3` mark symbols as public; non-exported symbols are private — used internally by exported functions but not accessible via `module.name()` from importers. If a module has no `export` declarations, all symbols remain public (backwards compatible). Works with Aether-style functions (no `fn` keyword), `fn`-keyword functions, C-style typed functions, and constants
- **`clone_ast_node()`**: Deep-copy utility for AST nodes (used internally by the module merge system)
- **Pure module test suite**: 15 tests across `test_pure_module.ae` covering basic functions, intra-module calls, constants, constant-to-constant references, type inference through module boundaries, nested expressions, parameter/local variable shadowing, multi-module imports (mymath + strutil), and cross-module expressions
- **Export visibility test suite**: `test_export_visibility.ae` — 5 tests covering exported functions, exported constants, internal helpers, and mixed expressions. `test_export_reject.sh` — 2 compile-failure tests verifying non-exported functions and constants are rejected
- **Backwards compatibility test**: `test_noexport.ae` — 4 tests verifying modules with no `export` declarations keep all symbols public
- **Mixed import test**: `test_mixed.ae` — 5 tests verifying pure module imports work alongside `std.string` and `std.math` stdlib imports in the same program
- **Updated `examples/packages/myapp/`**: Now uses actual `import utils` with `export` visibility — `utils.double_value()`, `utils.MULTIPLIER` are public; `multiply()` is private
- **REPL rewrite**: `ae repl` rebuilt from scratch with session persistence — assignments and constants survive across evaluations, variable reassignment replaces previous value in history, multi-line blocks auto-continue until braces close. Single-line auto-execute: complete statements run immediately without needing a blank line. Retro box-drawing banner with dynamic width. Three-state prompt: `ae>` (normal), `...` (inside braces), `..` (multi-line accumulation). Commands: `:help` (with usage examples), `:quit`, `:reset`, `:show`
- **REPL integration tests**: `tests/integration/repl/test_repl.sh` — 40 tests covering basic output (integers, strings, arithmetic, negatives), variable persistence (int, string, const, derived expressions), reassignment (single and triple), string interpolation, multi-line blocks (if, if-else, nested if, while, while with accumulator), all commands (`:help`, `:h`, `:show`, `:reset`), all exit variants (`:quit`, `:q`, `exit`, `quit`), error recovery (compile errors, session continues after error, failed evals not persisted), single-line auto-execute (4 tests), and banner/goodbye messages
- **Shell test discovery in `make test-ae`**: Integration tests using `.sh` files (e.g., REPL tests, export rejection tests) are now auto-discovered and run alongside `.ae` tests

### Removed

- **Standalone REPL binary (`tools/aether_repl.c`)**: Deleted 657-line standalone REPL with readline dependency — redundant with the integrated `ae repl` which uses correct toolchain infrastructure, has fewer bugs, and requires no external dependencies

### Fixed

- **Module constants not renamed inside module functions**: When a pure module function referenced a module constant (e.g., `return x * SCALE`), the constant name was not namespace-prefixed during AST merge — generated C used the bare name `SCALE` instead of `mymath_SCALE`, causing "undeclared identifier" errors. Added `collect_module_const_names()` and extended `rename_intra_module_refs()` to rename `AST_IDENTIFIER` references to module constants alongside function calls
- **Parameter/local shadowing of module constants**: A function parameter or local variable with the same name as a module constant (e.g., `check(SCALE) { return SCALE }` where `const SCALE = 10`) had the body reference incorrectly renamed to `mymath_SCALE`, returning the constant instead of the parameter. `rename_intra_module_refs()` now collects all locally-bound names (parameters + variable declarations) per function scope via `collect_local_names()` and skips renaming identifiers that match a local name
- **Constant value expressions not renamed**: `const DOUBLE_SCALE = SCALE * 2` — the `SCALE` reference in the value expression was not namespace-prefixed during merge, causing "undeclared identifier" in generated C. Constant clones now run through `rename_intra_module_refs()` like function clones
- **Non-exported function calls produced misleading "Undefined function" error**: Calling a private function like `mathlib.internal_multiply()` reported "Undefined function 'mathlib.internal_multiply'" with help text suggesting a typo. Now reports "'internal_multiply' is not exported from module 'mathlib'" with error code E0303 and actionable help text
- **`export` keyword didn't work with Aether-style functions**: `export double_it(x) { ... }` (identifier followed by parentheses) was parsed as exporting a bare identifier `double_it`, leaving `(x) { ... }` as unparsed junk. The export parser now detects identifier-then-`(` as a function definition, and also handles `export const`, C-style return types (`export int func(...)`), and `export fn func(...)`
- **`export const` produced `VARIABLE_DECLARATION` instead of `CONST_DECLARATION`**: The export parser consumed the `const` token before calling `parse_statement`, which then saw the identifier and created a variable declaration. Fixed by letting `parse_statement` handle the `const` token directly
- **Unused function `is_type_token` compiler warning**: Removed dead code in `parser.c` — eliminated the only `-Wunused-function` warning in the compiler
- **`ae examples` and `make examples` failed on package example files**: `module.ae` (library file) and `main.ae` (uses local `import utils`) under `examples/packages/` were compiled as standalone programs by `aetherc`, which doesn't support module orchestration. Now skips files under `lib/` and `packages/` directories — these require `ae run` with full module orchestration, not bare `aetherc`

## [0.27.0]

### Added

- **Local `const` declarations**: `const` now works inside function bodies (previously only top-level). `const AGE = 5` inside `main()` emits `const int AGE = 5;` in generated C — the C compiler enforces immutability

### Fixed

- **`ae version use` didn't sync stdlib files**: Switching versions with `ae version use` updated binaries but left stale `lib/`, `include/`, and `share/` directories from previous versions — caused `string_length` to use `void*` params instead of `const char*` when a stale `module.ae` shadowed the version-managed one. Now syncs all subdirectories on version switch

## [0.26.0]

### Added

- **`std.os` module — shell & process execution** ([Issue #39](https://github.com/nicolasmd87/aether/issues/39)): New stdlib module with `os.system(cmd)` (run command, get exit code), `os.exec(cmd)` (run command, capture stdout as string), and `os.getenv(name)` (get environment variable). Cross-platform (POSIX `popen`/Windows `_popen`). Example: `examples/stdlib/os-demo.ae`, tests: `test_os_module.ae` (7 tests)
- **Release archive CI test (`test-release-archive`)**: New Makefile target and CI step [9/9] that packages a tarball exactly like the release pipeline, extracts it, and verifies `ae init` + `ae run` work from the extracted layout — catches archive structure bugs that `test-install` (which tests `install.sh`) would miss
- **Regression test for printing stdlib returns**: `test_print_stdlib_returns.ae` — covers `file.read_all`, `io.read_file`, `io.getenv` through `print()`, `println()`, and string interpolation paths
- **Filesystem return value regression test**: `test_fs_return_values.ae` — 15 tests covering `file.write`, `file.close`, `file.delete`, `dir.create`, `dir.delete`, `dir.exists` return values including success, failure, non-existent targets, duplicate operations, and NULL inputs

### Fixed

- **`ae version list` showed wrong "current" after `ae version use`**: The "current" marker was based on the compiled-in `AE_VERSION`, not the actually active version. After switching with `ae version use v0.21.0`, the list still showed v0.25.0 as current. Now reads the active version from `~/.aether/current` symlink (set by `ae version use`) or `~/.aether/active_version` file (set by `install.sh`), falling back to compiled-in version only if neither exists
- **`ae version use` didn't persist active version**: Switching versions updated the symlink and copied binaries but didn't write a version marker file. Now writes `~/.aether/active_version` so the active version is always queryable
- **Source-built installs invisible to `ae version list`**: `install.sh` installed to `~/.aether/` directly but never registered in `~/.aether/versions/`, so the source-built version never showed as "installed" or "current" in `ae version list`. Now writes `~/.aether/active_version` after install
- **Release pipeline computed next version from stale VERSION file**: The `prepare` job derived the next version by reading `VERSION` and incrementing — but if `VERSION` was stale (e.g. stuck at `0.21.0` while latest tag was `v0.25.0`), every bump PR proposed `0.22.0` instead of `0.26.0`, and the existing `release/v0.22.0` branch check caused it to skip silently. Now computes next version from the latest `v*.*.*` git tag, making the pipeline self-healing even if VERSION drifts

### Changed

- **`make ci` expanded to 9 steps**: Added `test-release-archive` as step [9/9] — every CI run now verifies both `install.sh` and release archive extraction paths end-to-end

## [0.25.0]

### Added

- **5 stdlib regression test suites (71 tests)**: Comprehensive edge-case coverage for every stdlib module:
  - `test_string_plain_char.ae` — 18 tests: every `std.string` function with plain `char*` (length, to_upper, to_lower, contains, starts_with, ends_with, index_of, substring, concat, equals, char_at, trim, split, to_cstr, release no-op, mixed managed+plain, empty string)
  - `test_stdlib_edge_cases.ae` — 25 tests: path return types and edge cases, file ops on missing files, JSON parsing edge cases, string ops on plain strings, mixed managed/plain equality
  - `test_json_edge_cases.ae` — 17 tests: escape handling (`\n`, `\t`, `\"`, `\\`, `\/`), booleans, negative numbers, floats, mixed-type arrays, type-safe getters on wrong types, stringify round-trip, nested arrays, empty strings, out-of-bounds, empty object/array creation
  - `test_io_edge_cases.ae` — 10 tests: read non-existent file, write+read round-trip, append, file_exists, delete, delete non-existent, empty content, getenv known/unknown, file_info on missing
  - `test_collections_edge_cases.ae` — 16 tests: empty list/map ops, out-of-bounds get, negative index, remove+re-add, set, clear+re-use, put+overwrite, remove non-existent key, many keys (trigger resize), all keys accessible after resize, managed string values in map

### Fixed

- **String interpolation used `%d` for ptr/string types**: `println("${file.read_all(f)}")` and similar interpolations with `TYPE_PTR` values fell through to the `%d` default in `EMIT_INTERP_FMT`, printing pointer addresses as integers instead of string content. Added `TYPE_PTR` case to format specifier switch and `_aether_safe_str()` wrapping for `TYPE_STRING`/`TYPE_PTR` in `EMIT_INTERP_ARGS`
- **`string.length()` returned garbage on plain `char*`**: `string_length("hello")` produced values like 168427553 because the `AetherString` struct layout interpreted raw bytes as the `length` field. Added `AETHER_STRING_MAGIC` (0xAE57C0DE) marker to `AetherString` struct with `is_aether_string()` runtime detection — all `std.string` functions now transparently handle both `AetherString*` and plain `char*` via `str_data()`/`str_len()` helpers. `string_retain()`/`string_release()` are safe no-ops on plain strings
- **`std/string/module.ae` param types caused `-Wincompatible-pointer-types-discards-qualifiers`**: String functions declared params as `ptr` (`void*`) but C signatures use `const void*` — codegen passed `const char*` to `void*`, triggering clang warnings. Changed all string-accepting params to `string` (`const char*`) in module.ae; return types for `string_concat`, `string_substring`, `string_to_upper`, `string_to_lower`, `string_trim` changed from `ptr` to `string` (they return `char*`)
- **`std/path/module.ae` returned `ptr` instead of `string`**: `path_join`, `path_dirname`, `path_basename`, `path_extension` declared `-> ptr` but return `char*` — fixed to `-> string`
- **`std/tcp/module.ae` declared `tcp_receive -> ptr` instead of `-> string`**: Inconsistent with `std/net/module.ae` which correctly declared `-> string`. Now both match
- **`std/string/module.ae` missing exports**: Added `string_to_long` and `string_to_double` declarations
- **JSON parser stack overflow on deeply nested input**: `parse_value`, `stringify_value`, and `json_free` recursed without depth limit — added `JSON_MAX_DEPTH` (256) guard to all three recursive paths
- **JSON parser read past end on truncated escape**: `parse_string` advanced past `\\` without checking for end of input — added `if (!**json) break;` guard
- **JSON parser missing `\/` escape**: Valid JSON escape `\/` (forward slash) was not handled — added `case '/'` to escape switch
- **JSON `parse_string` missing malloc NULL check**: `JsonValue` allocation after string parsing had no NULL check — added guard with `free(buffer)` cleanup
- **`io_read_file` / `file_read_all` missing `ftell`/`malloc` checks**: `ftell()` returning -1 was passed to `malloc()` causing undefined behavior — added `if (size < 0)` guard and malloc NULL check with proper `fclose()` cleanup
- **`file_open` missing malloc NULL check**: `malloc(sizeof(File))` failure caused NULL dereference — added check with `fclose(fp)` cleanup
- **`dir_list` unsafe realloc**: `realloc()` failure leaked original `entries` array — fixed with safe realloc pattern (temp variable, break on failure)
- **`list_add` unsafe realloc**: Same pattern — `realloc()` failure lost original `items` pointer. Fixed with temp variable
- **`hashmap_resize` unsafe calloc**: Failure overwrote map state with NULL — now allocates new buckets first, only updates map on success
- **`map_keys` on empty map**: `malloc(0)` is implementation-defined — added special case returning `keys->keys = NULL` with `count = 0`
- **`map_put` missing malloc NULL check**: `HashMapEntry` allocation had no NULL guard — added `if (!new_entry) return;`
- **`tcp_connect`/`tcp_accept`/`tcp_listen` NULL dereference on malloc failure**: All three allocated structs without checking — added NULL checks with `close(fd)` cleanup on failure
- **`tcp_receive` missing malloc NULL check**: Buffer allocation had no guard — added `if (!buffer) return NULL;`
- **HTTP `parse_url` buffer overflow**: Fixed-size `host[256]`/`path[1024]` buffers used `strcpy()`/`strncpy()` without bounds checking — refactored to pass buffer sizes and use `snprintf()`/bounded `memcpy()`
- **HTTP `http_request` missing malloc NULL checks**: `HttpResponse` and response buffer allocations had no guards — added NULL checks with proper cleanup
- **HTTP server header overflow**: Request parsing and `set_header` had no bounds check on header count — added `header_count < 50` guard

## [0.24.0]

### Fixed

- **Toolchain discovery silently used broken install**: `discover_toolchain()` accepted a `current` symlink or `AETHER_HOME` that had `aetherc` but no `lib/` or `share/aether/`, then passed non-existent source paths to the C compiler producing cryptic clang errors — now validates that sources or prebuilt lib exist before accepting a toolchain root, prints clear diagnostic ("installation is incomplete") and falls through to other strategies
- **`ae version install` extracted only one directory from release archives**: The POSIX extraction logic assumed release archives had a single wrapper directory and used `ls -d tmp/*/ | head -1` to find it — but release archives contain `bin/`, `lib/`, `share/`, `include/` at root with no wrapper. `head -1` picked only one directory (e.g. `bin/`), so `lib/libaether.a` and `share/aether/` were lost, causing "flat layout" detection and compilation failures when running `ae run` on an installed version
- **`ae version install` incomplete install detection**: Pre-existing version directories with binaries but missing `lib/` or `share/aether/` (caused by old extraction bug) were treated as complete — now detects and auto-reinstalls; also probes for `aetherc` binary and reinstalls if missing
- **Spurious "installation is incomplete" warning on `ae run`**: Toolchain discovery checked `~/.aether/current/lib/` first — if a stale `current` symlink existed from `ae version use` but `install.sh` had put files directly in `~/.aether/`, the warning fired even though the fallback strategy found a working toolchain. Now suppresses the warning when the direct `~/.aether/` layout has valid `lib/` or `share/`
- **`install.sh` left stale `current` symlink**: Direct installs via `install.sh` didn't remove the `~/.aether/current` symlink created by `ae version use`, causing the above warning on every `ae run`
- **Windows `ae version use` missing lib/share**: Only copied `bin/` subdirectory contents — now copies the entire version directory so `lib/`, `include/`, `share/` are available

## [0.23.0]

### Fixed

- **VERSION file stuck at `0.17.0`**: Release pipeline updates from 0.18.0–0.20.0 never persisted on main — corrected to `0.21.0` so the next merge triggers the `v0.21.0` release
- **Makefile version detection picked wrong tag**: `sort -t. -k1,1n` on `v0.X.0` tags tried numeric sort on the `v` prefix — behavior varies across `sort` implementations, causing some systems to pick e.g. `0.18.0` instead of `0.22.0`. Fixed by stripping the `v` prefix before sorting

## [0.22.0]

### Added

- **4 regression tests for CLI helper battle-testing**: `test_actor_print_char.ae` (print_char/escapes in actor handlers, self-send animation), `test_box_drawing.ae` (ASCII boxes, ANSI escapes, progress bars, tab tables, nested boxes), `test_interp_escape_combo.ae` (10 hex/octal + interpolation combos), `test_file_io_char_return.ae` (file I/O roundtrip, char* returns, append, cleanup)

### Fixed

- **`file.write`/`file.close`/`file.delete`/`dir.create`/`dir.delete` returned raw POSIX values**: These functions returned `0`/`-1` (C convention) instead of `1`/`0` (Aether convention where `1` = success, `0` = failure), inconsistent with `io.write_file`, `io.delete_file`, and the rest of the stdlib. Fixed all five to return `1` on success, `0` on failure

## [0.21.0]

### Fixed

- **Stdlib functions returned `AetherString*` instead of `char*`**: All stdlib modules (fs, io, json, net) returned opaque `AetherString*` pointers from functions like `file_read_all()`, `io_read_file()`, `json_stringify()`, `tcp_receive()` — but Aether's native string type is `const char*`, so codegen generated `printf("%s", ...)` which interpreted the struct pointer as a string, producing garbage output on all platforms. Changed all public stdlib APIs to return `char*` directly; module.ae declarations updated from `-> ptr` to `-> string`
- **`file_write` / `file_size` ABI mismatch**: `file_write()` C signature used `size_t length` (8 bytes on 64-bit) but module.ae declared `int` (4 bytes) — misaligned stack on ARM64. `file_size()` returned `size_t` but module declared `int`. Fixed C signatures to use `int` matching the module declarations
- **`json_stringify` crashed after `string_concat` API change**: `json_stringify` internally used `string_concat` expecting `AetherString*` return and accessed `->data` — after `string_concat` was changed to return `char*`, this dereferenced a `char*` as a struct, causing segfaults. Refactored JSON stringify to use its own `append_cstr` buffer approach, removing all dependency on `string_concat`

## [0.20.0]

### Added

- **6 new regression tests**: `test_actor_ref_message_fields.ae` (actor ref routing through messages), `test_format_specifier_typecheck.ae` (format specifier selection for all types), `test_stdlib_file_module.ae` (file module read/write), `test_stdlib_resolution.ae` (stdlib module resolution), `test_string_escape_sequences.ae` (escape sequence roundtrips), `test_win32_actor_self_scheduling.ae` (Windows actor self-scheduling)
- **2 new examples**: `actor-ref-routing.ae` (passing actor refs through messages), `self-scheduling.ae` (actor self-send patterns), `cross-platform-strings.ae` (cross-platform string handling)

### Fixed

- **Windows 11 thread compatibility**: Fixed `aether_thread.h` for Windows 11 — proper `CONDITION_VARIABLE` initialization and `SleepConditionVariableCS` usage
- **`--emit-c` flag handling**: Fixed flag parsing in `aetherc` for `--emit-c` output mode
- **Scheduler thread startup on Windows**: Added `scheduler_ensure_threads_running()` call in Windows codepath for actor self-scheduling

### Changed

- **`install.sh` improvements**: Better error handling, cleaner output, improved readline detection

## [0.19.0]

### Added

- **`--emit-c` compiler flag**: `aetherc --emit-c file.ae` prints the generated C code to stdout — useful for debugging codegen, inspecting optimizer output, and verifying MSVC compatibility guards
- **20 new integration tests** (46->66):
  - `test_print_null.ae` — 5 tests for `print`/`println` with NULL string values
  - `test_match_complex.ae` — 8 tests for match statement edge cases (NULL strings, many arms, sequential matches)
  - `test_series_long.ae` — 4 tests for series collapse optimizer with `long` (int64) types
  - `test_long_type.ae` — 7 tests for `long` type declarations, arithmetic, comparisons, and printing
  - `test_functions_advanced.ae` — 5 tests for recursive functions, nested calls, deep call chains
  - `test_while_edge_cases.ae` — 6 tests for zero-iteration loops, break, continue, nested loops
  - `test_nested_expressions.ae` — 7 tests for operator precedence, deep nesting, unary ops, if-expressions
  - `test_string_edge_cases.ae` — 7 tests for empty strings, escapes, interpolation with arithmetic
  - `test_type_coercion.ae` — 9 tests for long arithmetic, integer division, boolean logic, modulo
  - `test_control_flow.ae` — 6 tests for else-if chains, break/continue, nested loops, range-for, match default
  - `test_optimizer_booleans.ae` — 6 tests for `if true`/`if false` dead code elimination, constant folding type preservation
  - `test_reserved_words.ae` — 5 tests for C reserved word collision (functions named `double`, `auto`, `register`, `volatile`)
  - `test_edge_cases.ae` — 6 tests for nested loops, break, while-in-for, many variables, wildcard match, deep arithmetic
  - `test_actor_self_send.ae` — 4 tests for actor self-send pattern (finite loop, multiple self-sends, animation/stop, immediate exit)
  - `test_math_stdlib.ae` — 8 tests for math namespace functions (sqrt, abs, min/max, clamp, sin/cos, pow, floor/ceil/round, random)
  - `test_operator_precedence.ae` — 8 tests for operator precedence (mul/div before add/sub, parentheses, modulo, comparisons, logical ops, negation)
  - `test_string_compare_null.ae` — 3 tests for string comparison NULL safety (normal comparison, ordering, empty strings)
  - `test_nested_functions.ae` — 5 tests for nested/chained function calls (deep nesting, clamp composition, calls in conditions/arithmetic)
  - `test_logical_ops.ae` — 5 tests for logical operators (AND, OR, NOT, with comparisons, complex combinations)
  - `test_defer_advanced.ae` — 3 tests for defer edge cases (LIFO ordering, nested scopes, conditional defer)
- **3 new examples**: `recursion.ae` (factorial, fibonacci, GCD, fast exponentiation), `long-arithmetic.ae` (64-bit values, nanosecond timing, large multiplication), `string-processing.ae` (interpolation, escapes, multi-type printing)
- **17 new tests** (66->83):
  - `test_actor_communication.ae` — 4 tests for actor-to-actor messaging (bidirectional ping-pong, multi-phase wait_for_idle, actor ref in message fields)
  - `test_ask_reply.ae` — ask/reply pattern tests
  - `test_defer_loops.ae` — defer inside loops and nested scopes
  - `test_escape_hex_octal.ae` — hex (`\xNN`) and octal (`\NNN`) escape sequences
  - `test_escape_sequences.ae` — escape sequence coverage (hex, octal, interpolated strings, print_char)
  - `test_extern_functions.ae` — extern function declarations and calls
  - `test_float_operations.ae` — float arithmetic, comparisons, and printing
  - `test_io_operations.ae` — io module read/write/append/exists/delete
  - `test_list_operations.ae` — list create/add/get/size/remove operations
  - `test_map_operations.ae` — map put/get/has/remove/size operations
  - `test_pattern_guards.ae` — pattern matching with guard clauses
  - `test_print_char.ae` — print_char builtin for ASCII byte output
  - `test_print_format.ae` — print/println format specifiers for all types
  - `test_scope_shadowing.ae` — variable shadowing across scopes
  - `test_string_stdlib.ae` — string module functions (length, contains, split, etc.)
  - `test_struct_usage.ae` — struct creation, field access, nested structs
  - `test_typed_params.ae` — typed function parameters and return types
- **5 new examples**: `formatted-output.ae` (print formatting), `escape-codes.ae` (ANSI escape sequences), `ascii-art.ae` (character art with print_char), `io-demo.ae` (file I/O), `log-demo.ae` (logging module)
- **`print_char()` builtin**: `print_char(65)` emits a single byte by ASCII value — enables ANSI escape codes and character-level output without extern functions
- **Hex and octal escape sequences**: `\xNN` (hex) and `\NNN` (octal) in string literals and interpolated strings — enables ANSI terminal control codes like `"\x1b[1;32m"` directly in Aether strings

### Changed

- **Locality-aware actor placement**: Actors are now placed on the caller's core at spawn time instead of round-robin distribution. Main thread spawns default to core 0, keeping top-level actor groups co-located for efficient local messaging. Actors spawned from within actor handlers inherit the parent's core. This benefits tightly-coupled communication patterns such as ring and chain topologies where actors communicate with their immediate neighbors.
- **Aggressive message-driven migration**: Cross-core sends now set `migrate_to` to the sender's core directly, rather than migrating to the lower of the two core IDs. This produces faster convergence for communicating actor pairs.
- **Targeted migration checks**: The scheduler now checks migration hints immediately after processing messages from the coalesce buffer (O(batch_size)), in addition to the existing full actor scan during idle periods. This provides faster migration response without adding overhead to the idle scan path.
- **Non-destructive `scheduler_wait()`**: `scheduler_wait()` (backing `wait_for_idle()`) now only waits for quiescence without stopping or joining threads — programs can call `wait_for_idle()` multiple times to synchronize between phases of actor messaging. New `scheduler_shutdown()` handles final teardown (wait + stop + join) and is emitted once at program exit.
- **`Message.payload_int` widened to `intptr_t`**: Changed from `int` to `intptr_t` in the runtime Message struct — prevents 64-bit pointer truncation when actor refs are passed through single-int message fields on 64-bit platforms

### Fixed

- **Batch send buffer overflow on actor migration**: `scheduler_send_batch_flush()` re-read each actor's `assigned_core` at flush time, but the per-core counts (`by_core[]`) were recorded at `batch_add` time. If an actor migrated between buffering and flushing, the count/core mismatch caused the radix-sort to write past the end of the `sorted_actors[]` stack buffer — a stack buffer overflow confirmed by AddressSanitizer. Fixed by snapshotting `assigned_core` once per message at flush time and recomputing per-core counts from the consistent snapshot.
- **`printf("%s", NULL)` crash from `print(getenv(...))`**: Printing a NULL string (e.g. from `getenv()` on an unset variable) caused undefined behavior — now uses `_aether_safe_str()` inline helper in generated C that returns `"(null)"` for NULL pointers; helper evaluates the expression exactly once (no double-evaluation of side-effecting expressions like function calls)
- **Series collapse optimizer int32 overflow**: The closed-form formula `N*(N-1)/2` emitted by the loop optimizer overflowed for large N (e.g. 100000) because it used int32 arithmetic — now casts to `(int64_t)` for both linear-sum and constant-addend formulas; the int64 result correctly truncates back to the variable's declared type
- **Match expression evaluated multiple times**: `match (expr) { ... }` re-evaluated `expr` for every arm — if `expr` was a function call, the function ran N times with potential side effects; now emits `T _match_val = expr;` once and uses `_match_val` in all arm comparisons
- **Match `strcmp` crash on NULL strings**: String match arms used bare `strcmp()` which crashes on NULL input (e.g. `match (getenv("X")) { "a" -> ... }`) — now emits `_match_val && strcmp(_match_val, ...)` with proper NULL guard
- **Triple-evaluation of send target in actor handlers**: Inside actor receive handlers, `actor ! Msg { ... }` evaluated the target actor expression 3 times (condition check + local send + remote send) — if the target was a function call, it ran 3 times; now stores in `ActorBase* _send_target` once
- **NULL dereference in compound assignment codegen**: `stmt->children[0]->value` accessed without null check in `AST_COMPOUND_ASSIGNMENT` handler — added guard for `stmt->children[0] && stmt->children[0]->value`
- **NULL dereference on return type with `defer`**: `stmt->children[0]->node_type` could be NULL when type inference failed, causing crash in `get_c_type()` — added fallback to `int` when `node_type` is NULL or unresolved
- **`print`/`println` string literal overhead**: String literals (never NULL) were unnecessarily wrapped in `_aether_safe_str()` — literals now use `puts()` or `printf()` directly; only runtime string values go through the NULL-safe path
- **Message pool pre-allocation ignores OOM**: `message_pool_create()` called `malloc(256)` in a loop without checking return values — a failed allocation stored NULL in the pool, causing later NULL dereference; now cleans up and returns NULL on allocation failure
- **Overflow buffer out-of-bounds access**: `overflow_append(target, ...)` did not validate `target` against array bounds — an invalid core ID could write beyond `tls_overflow[MAX_CORES+1]`; now returns early with diagnostic on out-of-range target
- **Partial `realloc` failure corrupts overflow buffer**: Two sequential `realloc()` calls for `actors` and `msgs` arrays — if the second failed, `actors` pointed to the new allocation while `msgs` still pointed to the old (freed) memory; now updates `b->actors`/`b->msgs` immediately after each successful realloc so `abort()` on the second failure leaves consistent state
- **`strdup` return unchecked in message registry**: `register_message_type()` used `malloc()` and `strdup()` without checking return values — NULL results propagated silently, causing later `strcmp()` crash; now returns `-1` on allocation failure
- **Parser `is_at_end()` NULL dereference**: `peek_token()` returns NULL when past end of tokens, but `is_at_end()` immediately dereferenced it to check `->type == TOKEN_EOF` — now checks for NULL first
- **Parser `peek_ahead` negative index**: `peek_ahead(parser, -N)` could pass a negative `pos` to the token array — now returns NULL for negative positions
- **Parser direct token array access without bounds check**: Two call sites used `parser->tokens[current_token + 1]` directly — replaced with bounds-checked `peek_ahead(parser, 1)`
- **Lexer buffer overflow in error token**: Unknown characters created tokens with `&c` (pointer to single stack char) which `create_token()` passed to `strlen()`/`strcpy()` — now uses properly null-terminated 2-char array
- **Parser unchecked `realloc` in string interpolation**: Two `realloc()` calls during interpolation literal buffer growth did not check for NULL — realloc failure caused NULL pointer write; now checks and returns partial result on failure
- **Parser format string bugs**: Four `parser_message()` calls contained `%d` format specifiers without corresponding arguments — replaced with literal numbers
- **Lexer `create_token` missing malloc checks**: `malloc()` for token struct and value string not checked — now returns NULL on allocation failure
- **Parser `create_parser` missing malloc check**: `malloc(sizeof(Parser))` not checked — now returns NULL on failure
- **Type checker NULL dereference on `symbol->type`**: Module aliases have `symbol->type = NULL`, but 6 call sites in typechecker and type inference used `symbol->type` without NULL guards — crash when identifiers resolved to module aliases; added `!msg_sym->type ||` guards on send validation, `symbol->type ? ... : create_type(TYPE_UNKNOWN)` on identifier/function/compound-assignment type assignment
- **Missing semicolons in `sleep()` codegen**: Generated `Sleep()`/`usleep()` calls inside `#ifdef _WIN32`/`#else` blocks lacked semicolons — the preprocessor removed the `#endif` line leaving bare function calls without terminators; added `;` after both paths
- **Non-printable characters in string literal codegen**: Control characters (0x00-0x1F except \\n/\\t/\\r, and 0x7F) were emitted as raw bytes in generated C strings — could produce invalid C or compiler warnings; now escaped as `\\xHH`
- **Command injection in `ae test` and `ae examples`**: User-supplied directory paths passed directly to `popen(find "..." ...)` without validation — shell metacharacters in paths could execute arbitrary commands; now validates paths reject `` ` ``, `$`, `|`, `;`, `&` and other shell metacharacters
- **Unsafe `strcpy` in toolchain discovery**: `strcpy(tc.compiler, standard_paths[i])` used without bounds checking — replaced with `strncpy` + explicit null termination
- **`ftell()` failure causes `malloc(-1)` in `ae add`**: `ftell()` returns -1 on error, then `malloc(sz + 1)` with `sz=-1` causes `malloc(0)` and subsequent `fread()` with negative size (undefined behavior) — now checks `sz < 0` and returns error
- **TOML parser `strdup` NULL checks**: Three `strdup()` calls for section names and key/value pairs did not check for NULL returns — allocation failure stored NULL pointers later dereferenced by `strcmp()`; now checks all `strdup` returns and rolls back partial entries
- **TOML parser `realloc` capacity tracking**: Section capacity was incremented before `realloc()` — if `realloc` failed, the capacity variable was already wrong, causing OOB access on next insert; now only updates capacity after successful `realloc`
- **TOML parser `toml_get_value` NULL dereference**: `strcmp()` called on section/key names without NULL check — if a section had NULL name (from failed `strdup`), lookup would crash; added NULL guards in comparison loops
- **`if true { ... }` body silently eliminated by optimizer**: The dead code optimizer called `atof("true")` which returns `0.0`, treating `true` as falsy and removing the entire `if true` body — added `is_constant_condition()` helper that handles boolean literals separately from numeric constants; `true` -> truthy, `false` -> falsy
- **Constant folding always produced `TYPE_FLOAT`**: `create_numeric_literal()` unconditionally set `TYPE_FLOAT` for all folded constants, so `3 + 4` produced a float `7.0` — now takes an `is_int` parameter and preserves `TYPE_INT` when both operands are integers
- **C reserved word collision in function names**: Aether functions named `double`, `auto`, `register`, `volatile`, etc. generated invalid C because the function name is a C keyword — added `safe_c_name()` that prefixes colliding names with `ae_`; applied to function definitions, forward declarations, and call sites; `extern` functions are excluded since they refer to actual C symbols
- **AST `add_child()` silent failure on OOM**: `realloc()` failure in `add_child()` silently returned without adding the child — a corrupted AST caused unpredictable codegen; now calls `exit(1)` on allocation failure
- **Pattern variable mapping array bounds**: Guard clause codegen used a fixed `mapping[32]` array without bounds checking — more than 32 pattern variables silently overwrote the stack; now guards against overflow
- **Type checker namespace leak**: `imported_namespaces[]` strings allocated via `strdup()` were never freed on early return from `typecheck_program()` — added cleanup on all return paths
- **Extern registry unchecked `realloc`**: `register_extern_func()` updated capacity before confirming `realloc()` success — a failed realloc left capacity wrong and `extern_registry` pointing to freed memory; now only updates after success
- **Optimizer NULL checks in tail call detection**: `optimize_tail_calls()` accessed `node->children[i]` without NULL guards — a NULL child caused segfault during recursive optimization; added NULL checks before recursing
- **Runtime non-atomic message counters**: `messages_sent` and `messages_processed` in the scheduler were plain `uint64_t` read from the main thread while being written by worker threads (data race) — changed to `_Atomic uint64_t`
- **Codegen NULL dereference in `AST_IDENTIFIER`**: `strcmp(expr->value, ...)` in actor state variable lookup crashed if `expr->value` was NULL — added NULL guard before the loop
- **Codegen NULL dereference in `AST_ACTOR_REF`**: `strcmp(expr->value, "self")` crashed on NULL value — added NULL guard
- **Codegen NULL dereference in match arm iteration**: `match_arm->type` dereferenced without checking if `match_arm` was NULL — added `!match_arm ||` guard
- **Codegen NULL dereference in reply statement**: `reply_expr->value` passed to `lookup_message()` without NULL check — added `&& reply_expr->value` guard
- **Codegen NULL message name in error comments**: Two error-path `fprintf` calls used `message->value` which could be NULL — added ternary fallback to `"<?>"`
- **float/double ABI mismatch in std.math**: All 18 math functions (`sqrt`, `sin`, `cos`, `pow`, `floor`, `ceil`, `round`, `abs_float`, etc.) used C `float` with `f`-suffixed implementations (`sqrtf`, `sinf`, etc.) but Aether's `float` type maps to C `double` — caused wrong return values on ARM64 (e.g. `math.sqrt(16.0)` returned `0.0`); changed all signatures and implementations to `double`
- **float/double mismatch in `io_print_float`**: `io_print_float` took `float` parameter but received `double` from compiled Aether code — changed to `double`
- **`log_get_stats()` struct-by-value ABI mismatch**: Function returned `LogStats` struct by value but Aether's codegen expects pointer returns for non-primitive types — changed to return `LogStats*` (pointer to static storage)
- **Unary `!` operator precedence bug**: `!(a || b)` generated `!a || b` in C — the `!` only applied to the first operand because `AST_UNARY_EXPRESSION` codegen did not parenthesize complex subexpressions; now wraps binary/unary subexpressions in parentheses
- **String comparison NULL crash**: `strcmp()` in string equality/ordering codegen crashed on NULL string values — wrapped both operands with `_aether_safe_str()` to return `""` for NULL
- **io module.ae param type mismatches**: 8 functions (`io_print`, `io_print_line`, `io_read_file`, `io_write_file`, `io_append_file`, `io_file_exists`, `io_delete_file`, `io_file_info`) declared `ptr` parameters in module.ae but the C implementations take `const char*` — changed to `string` to match
- **collections module.ae map key type mismatch**: `map_put`, `map_get`, `map_has`, `map_remove` declared `ptr` for key parameter but the C implementations take `const char*` — changed to `string`
- **Lexer silently accepts unterminated strings**: Strings missing a closing `"` were lexed without error — now returns `TOKEN_ERROR` with "unterminated string literal"
- **Lexer silently accepts unterminated multi-line comments**: `/* ...` without `*/` was silently ignored — now prints error to stderr
- **Lexer missing `\0` escape sequence**: `\0` in string literals was not recognized — added null byte escape
- **Array index type not validated**: Array access like `arr["hello"]` passed through the type checker without error — added validation that array indices must be `int` or `long`
- **Extern function argument types not validated**: Calling an extern function with wrong argument types (e.g. passing `int` to a `string` parameter) produced no type error — added type validation for extern function arguments using declared parameter types
- **Type inference missing `long` (int64) arithmetic promotion**: `infer_from_binary_op()` only handled `TYPE_INT` and `TYPE_FLOAT` — mixed `int`/`long` arithmetic silently inferred as `TYPE_INT` instead of `TYPE_INT64`; now promotes to `TYPE_INT64` when either operand is int64
- **Type inference memory leak in binary expression**: `node->node_type` was overwritten without freeing the old type when reassigning from `infer_from_binary_op()` — added `free_type()` before reassignment
- **Type inference NULL guard in `has_unresolved_types`**: `ctx->constraints` could be NULL if no constraints were collected — added defensive NULL check
- **Parser match statement now supports optional parentheses**: `match val { ... }` and `match (val) { ... }` both work — parentheses are consumed if present but no longer required
- **Parser 10 unchecked `expect_token` calls**: Missing tokens (colons, parens, arrows, braces) caused the parser to continue with corrupted state — added return/break on failure in `parse_for_loop`, `parse_case_statement` (2x), `parse_match_statement` (3x), `parse_match_case`, `parse_message_definition`, `parse_reply_statement`, `parse_message_constructor`, `parse_struct_pattern` (2x)
- **Parser NULL dereference in `parse_for_loop`**: `name->value` accessed without checking if `expect_token(TOKEN_IDENTIFIER)` returned NULL — added NULL guard
- **Parser unchecked `malloc` in for-loop children**: Two `malloc(4 * sizeof(ASTNode*))` calls for for-loop child arrays did not check for NULL — OOM caused NULL pointer dereference when assigning children; now checks and returns NULL
- **`print()` not flushing stdout**: `print(".")` in a loop did not show dots immediately because `printf` buffers partial lines — now emits `fflush(stdout)` after every `print()` call in generated C
- **`self` in actor handler generated undefined `aether_self()`**: The codegen for `AST_ACTOR_REF` with value `"self"` emitted `aether_self()` which doesn't exist in the runtime — now emits `(ActorBase*)self` when inside an actor handler context
- **Actor self-send crashed in main-thread mode**: `self ! Message {}` from inside a handler in main-thread mode caused either a double-free (recursive `g_skip_free` flag corruption) or an infinite blocking loop (drain loop) or silent message loss (scheduler threads never started) — now properly transitions out of main-thread mode on self-send: disables `main_thread_mode`, clears `main_thread_only`, and starts scheduler threads on demand via `scheduler_ensure_threads_running()` so self-sent messages are processed asynchronously by the scheduler
- **`scheduler_wait()` destroyed threads after first call**: `scheduler_wait()` (backing `wait_for_idle()`) called `scheduler_stop()` + `pthread_join()`, permanently destroying scheduler threads — second call to `wait_for_idle()` hung forever because no threads existed to process messages; split into non-destructive `scheduler_wait()` (quiescence only) and `scheduler_shutdown()` (final teardown at program exit)
- **Actor refs truncated in message fields on 64-bit**: Passing an actor ref through a single-int message field (e.g. `PingWithRef { sender_ref: counter }`) truncated the 64-bit pointer to 32-bit `int` — `Message.payload_int` widened to `intptr_t`, codegen emits `intptr_t` for single-int message struct fields and proper `(intptr_t)` casts when storing actor refs
- **`void*`/`int` comparison warnings in generated C**: Comparing `list.get()` return (`void*`) with an `int` value produced C compiler warnings — codegen now auto-detects ptr/int mixed comparisons and emits `(intptr_t)` cast on the pointer side
- **`set_clear()` / `hashmap_clear()` left stale entries**: `hashmap_clear()` only set `occupied = false` per entry, leaving stale PSL, hash, key, and value data that could cause phantom entries on reinsertion — now uses `memset` to zero the entire entries array after freeing keys/values
- **`main_thread_sent` counter not reset between scheduler lifecycles**: The atomic `main_thread_sent` counter was never reset in `scheduler_init()` — stale sent count from a previous lifecycle caused `count_pending_messages()` to return permanently non-zero, spinning `scheduler_wait()` forever in C unit tests
- **Type inference not propagating parameter types through call chains**: Function parameters resolved from one call site were not propagated when the function was called from other sites — added constraint propagation pass that infers parameter types from all call sites

## [0.18.0]

### Fixed

- **Type checker did not validate message constructor field types**: Message constructors like `Greet { name: 42 }` were accepted even when the field was declared as `string` — now validates each field against the message definition's declared types
- **Missing `getpid()` header on Windows**: `<process.h>` is needed for `getpid()` on Windows, `<unistd.h>` on POSIX — added proper platform-guarded includes

## [0.17.0]

### Added

- **`exit()` builtin**: `exit(code)` terminates the program with the given exit code — no `extern` declaration needed; `exit()` with no argument defaults to 0
- **`--dump-ast` compiler flag**: `aetherc --dump-ast file.ae` prints the parsed AST tree and exits without generating C code — useful for debugging parser behavior and understanding program structure
- **`-g` debug symbols in dev builds**: `ae run` now compiles with `-g` flag, enabling `gdb`/`lldb` debugging on crashes without requiring manual gcc flags
- **`NO_COLOR` and `isatty()` support**: Error/warning output respects the `NO_COLOR` environment variable ([no-color.org](https://no-color.org/)) and automatically disables ANSI colors when stderr is not a terminal (e.g. piped to a file or CI log)
- **`null` keyword**: `null` is now a first-class literal typed as `ptr` — eliminates the need for C `null_ptr()` helpers; `x = null` and `if x == null` work as expected
- **Bitwise operators**: `&`, `|`, `^`, `~`, `<<`, `>>` with C-compatible precedence — enables bitmask flags, hash functions, and protocol parsing without C shim functions
- **Top-level constants**: `const NAME = value` at module scope — codegen emits `#define`; supports int, float, and string constant values
- **Compound assignment operators**: `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=` — emit directly to C compound assignments; work with both regular variables and actor state
- **Hex, octal, and binary numeric literals**: `0xFF`, `0o755`, `0b1010` with underscore separators (`0xFF_FF`, `0b1111_0000`) — converted to decimal at lex time so all downstream code (atoi, codegen) works unchanged
- **If-expressions**: `result = if cond { a } else { b }` produces a value — codegen emits C ternary `(cond) ? (a) : (b)`; works inline in function arguments and assignments
- **Range-based for loops**: `for i in 0..n { body }` — desugars to C-style `for (int i = start; i < end; i++)` at parse time; supports variable bounds and nests with other loop forms
- **Multi-statement arrow function bodies**: `f(x) -> { stmt1; stmt2; expr }` — the last expression is the implicit return value; supports intermediate variables, if/return, and loops inside arrow bodies

### Fixed

- **`state` keyword context-awareness**: `state` is now a regular identifier outside actor bodies — previously it was globally reserved, preventing common variable names like `state = 42` in non-actor code
- **int(0) to ptr type widening**: Variables initialized with `0` and later assigned a `ptr` value no longer cause type mismatch errors — `int` / `ptr` compatibility added for null-initialization patterns
- **Sibling if block variable scoping**: Reusing a variable name in sibling `if` blocks no longer causes "undeclared identifier" errors in generated C — each block now gets a fresh scope with `declared_var_count` properly restored after the entire if/else chain
- **String interpolation returns string pointer**: `"text ${expr}"` now produces a heap-allocated `char*` (via `snprintf` + `malloc`) instead of a `printf()` return value (`int`) — interpolated strings can now be passed to `extern` functions expecting `ptr` arguments; `print`/`println` retain the optimized `printf` path
- **`ae.c` buffer safety**: All `strncpy` calls into stack buffers now have explicit null termination; `get_exe_dir` buffer handling hardened against truncation
- **`ae.c` mixed-path separators on Windows**: `get_basename()` now handles both `/` and `\` path separators correctly, preventing incorrect toolchain discovery on Windows
- **`ae.c` source fallback completeness**: Added missing `std/io/aether_io.c` to both dev-mode and installed-mode source fallback lists; installed-mode include flags now cover all `std/` subdirectories and `share/aether/` fallback paths
- **`ae.c` temp directory consistency**: All temp file operations now use `get_temp_dir()` which checks `$TMPDIR` on POSIX and `%TEMP%` on Windows, instead of inconsistent inline checks
- **`install.sh` portability**: Changed shebang from `#!/bin/sh` to `#!/usr/bin/env bash` (script uses `local` keyword); added `cd "$(dirname "$0")"` so installer works when invoked from any directory
- **`install.sh` header completeness**: Added `std` and `std/io` directories to the header installation loop — `ae.c` generates `-I` flags for these paths
- **`install.sh` readline probe**: Uses the detected compiler (`$CC`) instead of hardcoded `gcc` for the readline compilation test
- **`install.sh` re-install handling**: Running installer again now updates the existing `AETHER_HOME` value via `sed` instead of skipping silently
- **Release archives missing headers**: Both Unix and Windows release packaging now include the `include/aether/` directory with all runtime and stdlib headers, matching the layout expected by `ae.c` in installed mode
- **Release pipeline command injection**: Commit message in the bump job is now passed via `env:` block instead of inline `${{ }}` expansion, preventing shell injection through crafted commit messages
- **`find` command quoting**: `ae test` discovery now quotes the search directory in `find` commands, preventing failures on paths with spaces
- **SPSCQueue codegen struct layout mismatch**: Codegen emitted `SPSCQueue spsc_queue` (3KB by-value) but runtime `ActorBase` uses `SPSCQueue* spsc_queue` (8-byte pointer) — caused struct layout corruption when the scheduler cast actor pointers; fixed codegen to emit pointer type
- **Codegen emits MSVC-incompatible GCC-isms**: Generated C contained bare `__thread`, `__attribute__((aligned(64)))`, `__attribute__((hot))`, and `__asm__` — replaced with portable `AETHER_TLS`, `AETHER_ALIGNED`, `AETHER_HOT`, and `AETHER_CPU_PAUSE()` macros from `aether_compiler.h`; generated code now includes `aether_compiler.h` for cross-platform builds
- **GCC statement expressions in generated C**: Three uses of `({ ... })` (clock_ns, string interpolation, ask operator) prevented MSVC compilation — each now guarded with `#if AETHER_GCC_COMPAT` with portable helper functions (`_aether_clock_ns`, `_aether_interp`, `_aether_ask_helper`) emitted in the generated preamble for the `#else` path
- **Computed goto dispatch in generated C**: Actor message dispatch used `&&label` / `goto *ptr` (GCC/Clang extension) — now guarded with `#if AETHER_GCC_COMPAT`; the `#else` path emits an equivalent `switch(_msg_id)` with `case N: goto handle_Msg;` entries; fast computed-goto path preserved for GCC/Clang
- **Message struct `__attribute__((aligned(64)))` unguarded**: Large message structs emitted bare GCC alignment attribute — now guarded with `__declspec(align(64))` for MSVC and `__attribute__((aligned(64)))` for GCC/Clang
- **`AETHER_GCC_COMPAT` macro override support**: Both the generated C preamble and `aether_compiler.h` now use `#ifndef AETHER_GCC_COMPAT` so users and tests can force a specific value via `-D` flag or early `#define`
