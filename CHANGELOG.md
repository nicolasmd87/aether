# Changelog

All notable changes to Aether are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
