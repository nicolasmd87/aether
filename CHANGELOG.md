# Changelog

All notable changes to Aether are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

**Workflow**: New changes go under `## [0.30.0]`. When a PR merges to `main`,
the release pipeline automatically replaces `[current]` with the next version
number before tagging the release.

## [current]

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

- **`ae check` command**: Type-check without compiling. Runs lexer → parser → typechecker → type inference, then exits. No C code generated, no gcc invoked. ~30x faster than `ae build`. Also available as `aetherc --check`
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
