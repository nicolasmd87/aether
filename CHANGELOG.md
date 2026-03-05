# Changelog

All notable changes to Aether are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- **std.http segfault on string arguments**: `http_get`, `http_post`, `http_put`, `http_delete` took `AetherString*` but the compiler passed raw `const char*` string literals — dereferencing `url->data` on a plain `char*` caused immediate segfault; all four functions changed to accept `const char*`
- **std.io, std.net, std.json, std.collections `AetherString*` mismatch**: 37 stdlib functions across IO, networking, JSON, and collections modules took `AetherString*` parameters but received `const char*` from compiled `.ae` code — all public APIs changed to `const char*`; internal storage (e.g. HashMap keys) still uses `AetherString*` with wrapping at the boundary
- **`defer` return type truncation**: `defer` blocks stored the return value in `int _defer_ret` regardless of actual return type — functions returning `ptr`, `long`, or `float` had values silently truncated; now uses `get_c_type()` to emit the correct type
- **String `==`/`!=` pointer comparison instead of content comparison**: `a == b` on two `string` variables emitted C pointer comparison (`==`) instead of `strcmp()` — two strings with identical content but different addresses compared as unequal; now emits `strcmp(a, b) == 0` for `TYPE_STRING` operands
- **`match` on strings used pointer comparison**: Match arms comparing string expressions used `==` instead of `strcmp()` — match against string literals always fell through to the default arm; now emits `strcmp()` for `TYPE_STRING` match expressions
- **`TYPE_UNKNOWN` silent fallback to `int`**: `get_c_type()` silently returned `"int"` for unresolved types, masking upstream inference failures — now emits a compiler warning with suggestion to add explicit type annotations
- **Pattern list element bindings hardcoded to `int`**: List pattern match codegen (`[h|t]`, `[a,b,c]`) always emitted `int` element types regardless of actual list element type — now uses `get_c_type()` from the element's `node_type`
- **Token overflow silent truncation**: Source files exceeding `MAX_TOKENS` (10,000) silently stopped lexing with no error — now emits a clear error message with suggestion to split into multiple files
- **`ae fmt` stub shown in help output**: `ae help` listed `fmt [file]` as a command even though the formatter is not implemented — removed from help output until the feature is ready
- **String interp return type lost through implicit arrow returns**: `f(x) -> { msg = "text ${x}"; msg }` generated C with `int` return type instead of `const char*` — type inference now unwraps `AST_EXPRESSION_STATEMENT` in implicit return nodes and resolves local variable types by scanning preceding block statements
- **`install.sh` silent build failures**: `make` errors piped through `wc -l` were silently swallowed; added `set -eo pipefail` so pipe failures propagate correctly
- **`install.sh` unsolicited sudo**: Installer ran `sudo apt-get install libreadline-dev` without asking — replaced with a printed install instruction so users retain control
- **`ae.c` missing dev-mode include flags**: `-I` flags for `std/fs` and `std/log` directories were present in installed-mode but missing in dev-mode, causing header-not-found errors during development
- **Runtime scheduler portability**: Replaced bare `__thread` with `AETHER_TLS`, removed duplicate `likely`/`unlikely` macros, replaced bare inline asm with `AETHER_CPU_PAUSE()`, guarded `<immintrin.h>` for MSVC (`<intrin.h>`), wrapped `_Static_assert` for 32-bit compatibility
- **`aether_actor_thread.c` bare inline asm**: Replaced GCC `__asm__ __volatile__("pause"/"yield")` with portable `AETHER_CPU_PAUSE()` macro
- **`aether_thread.h` missing EBUSY fallback**: Added `#define EBUSY 16` fallback for platforms that don't define it (alongside existing `ETIMEDOUT` and `ENOMEM` fallbacks)
- **`aether_http_server.c` Windows compat**: Added `socklen_t`, `strcasecmp` (`_stricmp`), and `strdup` (`_strdup`) fallback defines for MSVC
- **`aether_io.c` Windows compat**: Added `S_ISDIR` macro and `stat`/`_stat` mapping for MSVC
- **`aether_log.c` thread safety**: Replaced `localtime()` (returns shared static buffer) with `localtime_r()` (POSIX) / `localtime_s()` (MSVC)
- **Test struct layouts**: Updated 4 runtime test files (`test_scheduler_stress.c`, `test_scheduler_correctness.c`, `test_worksteal_race.c`, `test_scheduler.c`) to use `SPSCQueue*` pointer matching the current `ActorBase` layout

### Changed

- **Windows CI upgraded to full CI suite**: `windows.yml` now runs `make ci` (8-step suite) instead of a minimal build-only check
- **`make ci` includes install smoke test**: `test-install` folded into `make ci` as step 8/8 — every CI run verifies the installed toolchain end-to-end
- **WinLibs GCC updated to 14.2.0**: Auto-download for Windows users without GCC now fetches GCC 14.2.0 (from 13.2.0) for better C11/C17 support and codegen improvements
- **Release pipeline workflow_dispatch guard**: Manual workflow dispatch no longer triggers build/publish jobs without a valid version tag
- **MSVC compat codegen regression test**: New `tests/compiler/test_msvc_compat.sh` — compiles generated C with `AETHER_GCC_COMPAT=0` under `-std=c11 -pedantic` to verify all GCC-extension fallback paths; covers string interpolation, actor dispatch, and helper functions

## [0.16.0]

### Fixed

- **macOS x86_64 release built on ARM runner**: GitHub's `macos-latest` shifted to ARM64 images, causing x86_64 release binaries to be ARM binaries mislabeled as x86_64 — fixed by explicitly using `macos-15-intel` for x86_64 builds in both `release.yml` and `ci.yml`

## [0.15.0]

### Fixed

- **`ae run` fails after install** (`Error opening output file: No such file or directory`): Installed `ae` binary falsely detected dev mode because `aetherc` sits next to `ae` in both `build/` (repo) and `bin/` (installed prefix). The tool then tried to write to a non-existent `build/` directory. Fixed by requiring the dev-mode heuristic to verify `../runtime/` exists (only true in the repo root).
- **`install.sh` corrupts shell RC files**: Appending PATH/AETHER_HOME export lines without checking for a trailing newline caused the first export to concatenate with the last existing line in `.zshrc`/`.bash_profile`. Fixed with a `tail -c 1` check that inserts a newline before appending when needed.
- **`install.sh` Fish shell syntax**: Fish shell was configured with bash `export` syntax which Fish doesn't understand. Fixed to use `set -gx` and `fish_add_path`.
- **`make install` missing source files**: The `install` target created empty `share/aether/` directories but never copied runtime/std source files into them, breaking the source-fallback compilation path when `libaether.a` was absent. Fixed by copying all `.c`/`.h` files from runtime and std subdirectories.
- **Installed `ae` missing source fallback**: When `libaether.a` was not present in an installed prefix, `ae` had no fallback to compile from individual source files. Added source-fallback path using `share/aether/{runtime,std}/*.c` and dual include paths for both `include/aether/` and `share/aether/`.

### Changed

- **CI install smoke test**: Added `make test-install` target and CI step across all 4 platform variants (Linux/GCC, Linux/Clang, macOS/Clang, Windows/MSYS2) — installs to a temp directory, runs `ae init` + `ae run`, and verifies correct output.

## [0.14.0]

### Fixed

- **Scheduler migration race — actor setup phase**: Freshly-spawned actors could be migrated to a new core before their `Setup` message was delivered — if a subsequent message arrived on the same core, the work-inlining path executed `step()` before initialization, causing null dereference / state corruption. Fixed by snapshotting `was_active` before message processing: actors where `was_active=0` and whose mailbox is empty are not migrated (setup still in flight).
- **Portable `usleep`/`sched_yield` wrappers for Windows**: Scheduler code used POSIX `usleep()` and `sched_yield()` which are unavailable on Windows. Added platform wrappers using `Sleep()` and `SwitchToThread()` on Windows, fixing MSYS2/MinGW CI failures.
- **Overflow buffer back-pressure and adaptive wait**: Overflow buffers could grow unbounded under sustained queue saturation. Added head-index drain (O(drained) instead of O(total) memmove), amplification limiter for own-core overflow, and adaptive `scheduler_wait` that spins tight when few messages remain.
- **Codegen if/else variable scoping**: Variables declared in an `if` branch were suppressing re-declaration in the `else` branch — the `declared_vars` tracking was function-level instead of block-scoped. Fixed by saving/restoring `declared_var_count` around if/else branches.
- **Skynet benchmark non-power-of-10 leaf counts**: Fixed incorrect sums when leaf count is not a power of 10; remainder leaves now distributed correctly; root with `size=1` no longer crashes.

## [0.13.0]

### Added

- **`long` type (64-bit integer)**: `long` keyword maps to `int64_t` in generated C — use `long x = 0` for values that exceed 32-bit range; arithmetic with `long` promotes to 64-bit; `print(long_val)` uses `%lld`; actor state fields and message fields support `long`; full support in typechecker, codegen, and pattern-variable extraction
- **Skynet benchmark**: Added the [atemerev/skynet](https://github.com/atemerev/skynet) recursive actor tree benchmark across Aether, Go, Rust, Erlang, Elixir, C (pthreads), Zig (std.Thread), and C++ (std::thread) — 6 levels x 10 = 1,111,111 actors, measures actor creation + aggregation throughput; controlled via `SKYNET_LEAVES` env var
- **Actor state type inference**: Member access on actor refs (`root.total`) now correctly resolves the state field's declared type — enables proper format specifier selection for `print` and type-safe arithmetic on long state fields

### Fixed

- **Scheduler queue back-pressure deadlock** (skynet N>=100k): All scheduler threads could permanently deadlock in a circular-wait. Fixed with bounded 8-retry loop, per-target-core thread-local overflow buffers, and flush-before-drain ordering.
- **Scheduler thread startup race**: Race between `scheduler_start()` returning and threads actually polling. Fixed with `g_threads_ready` barrier that guarantees all threads are actively polling before returning.
- **Work-stealing TOCTOU race on ARM64** (skynet crash at N>=150k): Data race between work-stealing and same-core fast path. Fixed by making `Mailbox.count` atomic with acquire/release ordering.
- **Actor ref state field type propagation**: `infer_type` and `typecheck_expression` now look up state declarations in the actor's AST definition when resolving member access on actor refs
- **`clock_ns()` return type**: Corrected from `TYPE_INT` to `TYPE_INT64` — nanosecond timestamps overflow `int32` after ~2.1 seconds
- **Actor ref / int/ptr type compatibility**: `is_type_compatible` now allows assigning an actor ref to a state field declared as `int` or `ptr`
- **Premature `scheduler_wait` termination**: Fixed by including the `messages_sent - messages_processed` counter delta in the pending count

### Changed

- **SPSCQueue lazy allocation**: `ActorBase.spsc_queue` changed from embedded 3,136-byte struct to an 8-byte pointer, lazily allocated only for `auto_process` actors (66% per-actor memory reduction)
- **QUEUE_SIZE reduced from 4096 to 1024**: Saves ~24 MB per core; overflow buffers handle burst cases
- **Diagnostic prints gated behind AETHER_DEBUG_ORDERING**: `scheduler_wait` progress diagnostics no longer printed in production builds
- **Benchmark README fairness labeling**: Transparent synchronization primitive labels for C, C++, and Zig benchmarks

## [0.12.0]

### Added

- **`cflags` in `aether.toml`**: `[build] cflags = "-O3 -march=native"` is now honoured by `ae build` — previously it was silently ignored; `ae run` continues to use `-O0` for fast development builds
- **`extra_sources` in `[[bin]]`**: Declare extra C files to compile alongside your Aether program directly in the project file — `extra_sources = ["src/ffi.c"]`; merged additively with any `--extra` flags passed on the command line
- **`aether_scheduler_poll(int max_per_actor)`**: New runtime API for C-hosted event loops (e.g. raylib, SDL, game loops) — drains pending actor messages without blocking; call between frames to keep actors alive while C owns the main thread; declared in `runtime/scheduler/multicore_scheduler.h`
- **`ae run` now accepts `--extra`**: The `--extra src/ffi.c` flag previously only worked with `ae build`; now works with both

### Fixed

- **Type inference through call chains**: `propagate_call_types_in_tree` was returning early on every `AST_FUNCTION_DEFINITION` node, making all function bodies invisible to the propagator — calls inside functions (e.g. `seed_glider` calling `set_cell`) were never found
- **Single-pass propagation**: Propagation ran exactly once; a call chain `main → f → g → h` needed 3 passes but only got 1, causing unresolved types to silently fall back to `int`; the pipeline now interleaves propagation with constraint solving in a loop until stable
- **Parameter type annotations now genuinely optional**: As a result of the above two fixes, explicit `: int` / `: ptr` annotations on function parameters are no longer required — types propagate correctly from call sites through arbitrarily deep chains
- **Clear error for `?` in expression context**: Writing `x > 0 ? a : b` (expecting a ternary) now produces an actionable diagnostic — "`?` is the actor ask operator; use if/else for conditional values" — instead of a confusing parse failure

## [0.11.0]

### Changed

- **Auto-free removed**: The experimental compiler-injected auto-free system (`@manual` annotation, `--no-auto-free` flag, `[memory] mode` in `aether.toml`) has been removed. The memory model is now `defer`-first exclusively — explicit, composable, and predictable. See [docs/memory-management.md](docs/memory-management.md)
- **Improved compiler diagnostics**: Better source-context error messages across type checker and parser; structured error codes with help suggestions

### Fixed

- **Module orchestrator**: Improved module import resolution and handling of dot-qualified function calls (`module.func()`)
- **Stale documentation**: Multiple doc files corrected to match actual implemented behaviour

## [0.10.0]

### Fixed

- **Scheduler race on ARM64**: Fixed a work-stealing TOCTOU race in the drain path; added `test_worksteal_race.c` to CI
- **`assigned_core` atomicity**: Changed `assigned_core` field from `int` to `atomic_int` in `ActorBase` for safe cross-core reads; updated all codegen, tests, and examples
- GCC/Clang warnings in `tools/ae.c` eliminated (clean at `-Wall -Wextra`)

## [0.9.0]

### Added

- **`defer`-first memory model**: Explicit `defer type.free(x)` is now the primary and recommended memory management pattern; stdlib types follow consistent `type.new()` / `type.free()` naming
- **Dynamic destructor registry**: Codegen tracks which stdlib constructors (`map_new`, `list_new`, etc.) map to their destructors — used at scope exit
- **`ae` temp directory**: `ae` now uses a stable temp directory for intermediate C files instead of cwd
- **`cmd_examples`**: `ae` CLI gained an examples command for discovering and running bundled examples

### Changed

- Memory management documentation overhauled — `defer` pattern documented comprehensively with multiple allocation, actor state, and escape patterns
- `aether.toml` dot-style module imports enforced as the only import syntax
- Windows: `ae.c` library path now uses mixed separators correctly; codegen adds `NOMINMAX`, `Sleep`, `windows.h` guards

### Fixed

- `realloc` failure handling in AST, module registry, and type inference (prevents leaks on OOM)
- Actor state init type inference in typechecker
- HTTP server: explicit free of `build_response` buffer; correct `Content-Length` in responses

## [0.8.0]

### Added

- **Windows CI**: Full CI pipeline on Windows via MSYS2 MinGW; `make ci` runs compiler, ae, stdlib, REPL, C tests, .ae tests, and examples with no skips

### Fixed

- **Windows build**: Fixed MSYS2/MinGW build failures (`NOMINMAX`, `Sleep`, `windows.h` guards, mixed path separators)
- **NUMA allocator**: `aether_numa_alloc` fallback now uses `numa_alloc_local()` instead of `malloc()` when libnuma is present — prevents allocator mismatch
- **macOS `-Werror` quoting**: Fixed shell quoting in Makefile for macOS clang strict mode
- `fread` return value warnings resolved

## [0.7.0]

### Added

- **Ask/reply (`?` operator)**: Production-ready typed ask — compiler infers the reply message type from the actor's receive block; reply slot travels with the message (not per-actor) for correct concurrent asks; reply payload is properly freed after use
- **Error handling**: `error-handling.ae` example with structured error propagation patterns; typechecker handles error result types

### Fixed

- **Toolchain resolver**: Dev builds (`./build/ae`) now take priority over `$AETHER_HOME`, preventing stale installed compilers from shadowing fresh builds

## [0.6.0]

### Added

- **`ae` CLI tool**: Single entry point for building, running, testing, and managing Aether projects (`ae run`, `ae build`, `ae test`, `ae init`)
- **Version manager**: `ae version list/install/use` to install and switch between Aether releases
- **Project tooling (`apkg`)**: Project scaffolding with `aether.toml`, dependency declarations, and GitHub-based `ae add`
- **`println` and string interpolation**: `println("Hello ${name}!")` with `${}` expressions in strings
- **`defer` statement**: Deferred cleanup in LIFO order, matching Go-style resource management
- **`switch` statement**: C-style switch with fall-through and break
- **Match expressions**: Pattern matching with literal, wildcard, list, and head|tail patterns
- **List patterns**: `[]`, `[x]`, `[h|t]`, `[a, b]` destructuring in match arms
- **Standard library**: Collections (HashMap, Vector, Set, List), JSON parser, HTTP server, TCP/UDP networking, file I/O
- **REPL**: Interactive read-eval-print loop (`ae repl`)
- **LSP server**: Diagnostics from lexer and parser errors for editor integration
- **VS Code / Cursor extension**: Syntax highlighting, file icons, and custom theme
- **Cross-language benchmarks**: Actor-model benchmark suite comparing 11 languages with interactive visualization UI
- **Docker support**: Development container with all toolchains pre-installed

### Changed

- **Ask/reply (`?` operator)**: Now production-ready — typed results (compiler infers reply message type from actor receive blocks), concurrent asks (reply slot travels with the message, not per-actor), proper `free()` of reply payload, configurable timeout via AST
- **HTTP server**: Multi-connection support via thread-per-connection model (POSIX `pthread`); accept loop no longer blocks on a single client
- **Error messages**: Source-context error reporting across lexer, parser, and type checker — errors now show the source line, a caret pointing to the exact column, error codes (`E0100` syntax, `E0300` undefined variable, etc.), and contextual help suggestions
- **Tail call optimization**: Honest reporting — Aether detects tail calls, GCC/Clang optimize them into loops at `-O2` (used by `ae build`)
- **Toolchain resolver**: Dev builds (`./build/ae`) now take priority over `$AETHER_HOME`, preventing stale installed compilers from shadowing fresh builds
- **Type inference**: Function return types are now inferred only from explicit `return` statements and arrow-body expressions; no longer incorrectly inferred from arguments to `print()` or other nested expressions
- **Codegen split**: `codegen.c` refactored into `codegen_expr.c`, `codegen_stmt.c`, `codegen_actor.c`, `codegen_func.c` for maintainability
- **Valgrind CI target**: Reports all Valgrind errors (uninitialised reads, not just leaks)
- **Benchmark visualization**: All methodology descriptions and message counts are dynamic from JSON data (no hardcoded values)
- **Latency metric**: Benchmarks report `ns/msg` (nanoseconds per message) instead of architecture-dependent `cycles/msg`

### Fixed

- Functions with no return statement now correctly generate `void` in C (previously defaulted to `const char*`)
- Match expressions returning strings now generate correct return types
- `ae version use` now actually switches the active compiler on POSIX (copies binaries to `~/.aether/bin/`)
- `snprintf` truncation warnings in type checker and codegen path buffers
- Valgrind uninitialized-memory reads in scheduler tests (replaced `malloc` with `calloc`, aligned test actor structs to `ActorBase` layout)
- NUMA allocator mismatch: `aether_numa_alloc` fallback now uses `numa_alloc_local()` instead of `malloc()` when libnuma is available
- JSON parser string buffer: replaced fixed 4KB stack buffer with dynamically growing heap allocation
- HTTP server response buffer: replaced static 64KB buffer with heap-allocated, correctly sized buffer
- HTTP server request reading: now reads body up to `Content-Length` with dynamic reallocation
- `realloc` failure handling in AST, module registry, and type inference (prevents memory leaks on OOM)

## [0.5.0] 

### Added

- **Main Thread Actor Mode**: Single-actor programs now bypass the scheduler entirely for synchronous message processing
  - Zero-copy message passing using caller's stack pointer
  - Automatic detection when only one actor exists
  - Manual control via `AETHER_NO_INLINE` and `AETHER_INLINE` environment variables
- **Memory Profiles**: Configure pool sizes via `AETHER_PROFILE` environment variable (micro/small/medium/large)
- **New Benchmarks**: counting, thread_ring, fork_join benchmark patterns
- **wait_for_idle()**: Block until all actors have finished processing messages
- **sleep(ms)**: Pause execution for specified milliseconds
- **Cross-platform thread affinity**: Linux hard binding, macOS advisory with QoS hints, Windows SetThreadAffinityMask

### Changed

- Scheduler threads now check `main_thread_only` flag with atomic operations to prevent data races
- LSP server uses `snprintf()` instead of `strcat()` for buffer safety
- LSP document management includes proper error handling for memory allocation
- Documentation updated across architecture, scheduler, runtime optimization guides

### Fixed

- Pattern variable renaming in receive blocks
- Race condition when second actor spawns during main thread mode transition
- LSP buffer overflow vulnerability in diagnostics publishing
- LSP memory allocation error handling

### Security

- Fixed potential buffer overflow in LSP diagnostics
- Added bounds checking to all LSP string operations

## [0.4.0]

### Added

- Thread affinity support for all architectures
- Apple Silicon P-core detection for consistent performance
- C interoperability improvements
- NUMA-aware memory allocation
- Computed goto dispatch for message handlers
- Thread-local message pools

### Changed

- Updated documentation for runtime optimizations
- Improved install script with platform detection

### Fixed

- Install script fixes for various platforms

## [0.3.0]

### Added

- Multicore scheduler with work stealing
- Lock-free SPSC queues for cross-core messaging
- Adaptive batch processing
- Message coalescing

### Changed

- Scheduler redesign for partitioned per-core processing

## [0.2.0]

### Added

- Basic actor system
- Message passing primitives
- Type inference
- Pattern matching in receive blocks

## [0.1.0]

### Added

- Initial release
- Lexer, parser, type checker
- Code generation to C
- Basic runtime with single-threaded scheduler
