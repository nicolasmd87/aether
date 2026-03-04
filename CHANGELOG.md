# Changelog

All notable changes to Aether are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.13.0]

### Added

- **`long` type (64-bit integer)**: `long` keyword maps to `int64_t` in generated C — use `long x = 0` for values that exceed 32-bit range; arithmetic with `long` promotes to 64-bit; `print(long_val)` uses `%lld`; actor state fields and message fields support `long`; full support in typechecker, codegen, and pattern-variable extraction
- **Skynet benchmark**: Added the [atemerev/skynet](https://github.com/atemerev/skynet) recursive actor tree benchmark across Aether, Go, Rust, Erlang, Elixir, C (pthreads), Zig (std.Thread), and C++ (std::thread) — 6 levels × 10 = 1,111,111 actors, measures actor creation + aggregation throughput; controlled via `SKYNET_LEAVES` env var
- **Actor state type inference**: Member access on actor refs (`root.total`) now correctly resolves the state field's declared type — enables proper format specifier selection for `print` and type-safe arithmetic on long state fields

### Fixed

- **Scheduler queue back-pressure deadlock** (skynet N≥100k and other many-actor workloads): All scheduler threads could permanently deadlock in a circular-wait: thread A spinning on a full queue waiting for thread B, while thread B spins on a full queue waiting for thread A (and so on around all cores). Root cause: `scheduler_send_remote` and two migration-forwarding paths used unbounded spin-retries inside the scheduler loop — a blocked thread could not return to drain its own `from_queues`, so the circular wait was unresolvable. Fixed by: (1) replacing unbounded spins with a bounded 8-retry loop; (2) on failure, deferring the `(actor, msg)` pair to a per-target-core thread-local overflow buffer (`OverflowBuf` TLS arrays); (3) flushing deferred sends at the top of every scheduler loop iteration before draining new messages. Main-thread senders retain their original spin-retry (safe because the main thread does not drain `from_queues`). Per-target-core FIFO ordering is preserved: new sends to a backed-up target core are also deferred, not bypassed. Memory for overflow buffers is only allocated under actual queue saturation and grows proportionally to in-flight count.
- **Scheduler thread startup race** (test suite, fast producer patterns): A race existed between `scheduler_start()` returning and scheduler threads actually polling `from_queues`. Under OS scheduling pressure (common in test suites with many rapid thread create/join cycles), a producer could enqueue messages before any thread was processing them — causing tests to time out waiting for messages that sat unread for hundreds of milliseconds. Fixed with a `g_threads_ready` barrier: each scheduler thread atomically increments the counter (with release) just before entering its main loop; `scheduler_start()` spins (with AETHER_PAUSE) until the counter reaches `num_cores`, guaranteeing all threads are actively polling before returning.
- **Scheduler migration race** (multi-core, recursive spawn patterns): Two related races fixed. (1) Freshly-spawned actors could be migrated to a new core *before* their `Setup` message was delivered — if a subsequent message arrived on the same core, the work-inlining path executed `step()` before initialization, causing null dereference / state corruption. Fixed by snapshotting `was_active` before message processing: actors where `was_active=0` and whose mailbox is empty are not migrated (setup still in flight). (2) The initial guard (`active=0 && mailbox empty`) was too broad — after processing a message both conditions are true for *any* idle actor, permanently blocking co-location and causing ~15× throughput regression (ping_pong: 1.4 M → 21 M msg/sec). The `was_active` snapshot distinguishes "freshly spawned" (was_active=0) from "just finished processing" (was_active=1), allowing migration in the latter case. Verified with `SKYNET_LEAVES=1000` multi-core and all 26 tests pass.
- **Work-stealing TOCTOU race on ARM64** (skynet crash at N≥150k): The idle-loop work-stealing path had a TOCTOU race with `aether_send_message`'s same-core fast path: (1) sender reads `actor->assigned_core = C0`, decides to call `scheduler_send_local` (direct mailbox write); (2) work-stealing fires on C1, moves actor to C1 and sets `assigned_core = C1`; (3) C0 writes to `actor->mailbox` without a memory barrier; (4) C1's scheduler reads `actor->mailbox` concurrently — data race on the non-atomic ring buffer. On ARM64 (weakly-ordered), this produces stale reads and heap corruption (`SIGSEGV` at address 0x302c). Fixed by making `Mailbox.count` a `_Atomic int`: `mailbox_send` increments it with `memory_order_release` (publishing message data + the preceding `active=1` write), and `mailbox_receive` reads it with `memory_order_acquire` (establishing happens-before after a work-steal handoff). The `active = 1` flag is now set *before* `mailbox_send` in `scheduler_send_local` so it is covered by the release. The scheduler loop performs an `acquire` read of `mailbox.count` before checking `actor->active` to ensure cross-thread active-flag writes are visible. Work-stealing is retained at the original `idle_count > 5000` threshold — the atomic ordering makes it safe. Also fixed a pre-existing bug in the `spsc_wrap_around` test: it tried to enqueue 100 items into a 64-slot SPSC queue (max 63 usable); corrected to `SPSC_QUEUE_SIZE - 1` per batch.
- **Actor ref state field type propagation**: `infer_type` and `typecheck_expression` now look up state declarations in the actor's AST definition when resolving member access on actor refs — previously returned `TYPE_UNKNOWN`, causing `printf("%d", ...)` for `long` fields
- **`clock_ns()` return type**: Corrected from `TYPE_INT` to `TYPE_INT64` — nanosecond timestamps overflow `int32` after ~2.1 seconds, silently truncating benchmark timings; variables assigned from `clock_ns()` now correctly infer as `long`
- **Actor ref ↔ int/ptr type compatibility**: `is_type_compatible` now allows assigning an actor ref to a state field declared as `int` or `ptr` (e.g., `state pong_ref = 0`) — the common wiring pattern of storing a spawned actor ref in a zero-initialized field; the new actor state type lookup caused spurious `E0200` type mismatch errors on all benchmark files that use this pattern
- **Overflow buffer O(N) memmove stall** (skynet N≥200k): Overflow buffers used `memmove` to compact entries after each partial flush — with 100k+ entries per buffer, multi-MB memmoves consumed 85% of CPU, stalling throughput to ~900 msg/sec. Fixed by adding a `head` index to `OverflowBuf`: cross-core flushes advance the head pointer (O(drained) not O(total)); own-core direct delivery path uses `memmove` only for the small processed window (≤128 entries). When own-core overflow exceeds 4096 entries (spawn cascade), falls back to the queue-based drain path to prevent the 20:1 amplification cascade where each `step()` call generates many new cross-core overflow entries.
- **Premature scheduler_wait termination** (ping-pong, thread-ring INCOMPLETE): `count_pending_messages()` only checked from_queues and overflow buffers — messages sitting in actor mailboxes (between work-inline bursts) were invisible, causing `scheduler_wait` to terminate early with actors still mid-conversation. Fixed by including the `messages_sent − messages_processed` counter delta in the pending count, which captures messages in any stage of the pipeline (mailboxes, in-flight step() calls, etc.).
- **Skynet benchmark non-power-of-10 leaf counts**: Fixed incorrect sums for any leaf count not a power of 10. (1) When `child_size = size/10 = 0` (size < 10), nodes now spawn individual child actors per leaf instead of 10 children with size 0. (2) When `size % 10 != 0`, remainder leaves are distributed across the first `remainder` children (each gets `child_size+1`) instead of being silently dropped. (3) Root with `size=1` no longer sends to NULL parent (N=1 segfault). Verified correct sums for N=1,2,5,7,10,11,15,99,123,9999,12345 and all powers of 10 up to 1M.
- **Codegen if/else variable scoping**: Variables declared in an `if` branch were suppressing re-declaration in the `else` branch — the `declared_vars` tracking was function-level instead of block-scoped. For example, `int i = 0` in the if-body marked `i` as declared; the else-body then emitted `i = 0` (no type), causing "undeclared identifier" C compilation errors. Fixed by saving/restoring `declared_var_count` around if/else branches, with union merge after both branches for variables that survive their declaring block.
- **Slow scheduler_wait tail drain** (skynet 1M: 49s → 30s): `scheduler_wait` used a fixed `usleep(100)` between pending-message checks — during the convergence phase with few remaining messages, the 100µs sleep dominated wall time (280k iterations × 100µs ≈ 28s of pure sleeping). Fixed with adaptive wait: tight spin+yield when `pending ≤ 10000`, `usleep(10)` for moderate counts, `usleep(100)` only for bulk processing.
- **`ae run` fails after install** (`Error opening output file: No such file or directory`): Installed `ae` binary falsely detected dev mode because `aetherc` sits next to `ae` in both `build/` (repo) and `bin/` (installed prefix). The tool then tried to write to a non-existent `build/` directory. Fixed by requiring the dev-mode heuristic to verify `../runtime/` exists (only true in the repo root).
- **`install.sh` corrupts shell RC files**: Appending PATH/AETHER_HOME export lines without checking for a trailing newline caused the first export to concatenate with the last existing line in `.zshrc`/`.bash_profile`. Fixed with a `tail -c 1` check that inserts a newline before appending when needed.
- **`install.sh` Fish shell syntax**: Fish shell was configured with bash `export` syntax which Fish doesn't understand. Fixed to use `set -gx` and `fish_add_path`.
- **`make install` missing source files**: The `install` target created empty `share/aether/` directories but never copied runtime/std source files into them, breaking the source-fallback compilation path when `libaether.a` was absent. Fixed by copying all `.c`/`.h` files from runtime and std subdirectories.
- **Installed `ae` missing source fallback**: When `libaether.a` was not present in an installed prefix, `ae` had no fallback to compile from individual source files. Added source-fallback path using `share/aether/{runtime,std}/*.c` and dual include paths for both `include/aether/` and `share/aether/`.

### Changed

- **SPSCQueue lazy allocation**: `ActorBase.spsc_queue` changed from embedded 3,136-byte struct to an 8-byte pointer, lazily allocated only for `auto_process` actors. Reduces per-actor memory from 4,744 to 1,616 bytes (66% reduction) — critical for million-scale actor workloads (skynet 1M: ~1.1M actors × 3 KB = 3.3 GB saved).
- **QUEUE_SIZE reduced from 4096 to 1024**: Per-sender SPSC channels in `from_queues` reduced from 4096 to 1024 slots. Saves ~24 MB per core (17 channels × 3072 slots × 56 bytes). Overflow buffers handle the rare burst case efficiently.
- **Diagnostic prints gated behind AETHER_DEBUG_ORDERING**: `scheduler_wait` progress diagnostics (WAIT Nk lines) no longer printed to stderr in production builds.
- **CI install smoke test**: Added `make test-install` target and CI step across all 4 platform variants (Linux/GCC, Linux/Clang, macOS/Clang, Windows/MSYS2) — installs to a temp directory, runs `ae init` + `ae run`, and verifies correct output.

- **Benchmark README fairness labeling**: C is labeled `C (pthreads + mutex)`, C++ is labeled `C++ (std::mutex)`, and Zig is labeled `Zig (std.Mutex)` to be transparent about the synchronization primitives used; expanded fairness note linking to [tzcnt/runtime-benchmarks](https://github.com/tzcnt/runtime-benchmarks) for C++ actor/tasking library comparisons; skynet note clarifies thread-depth approach for OS-thread languages

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
