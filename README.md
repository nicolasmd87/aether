# Aether Programming Language

[![CI](https://github.com/nicolasmd87/aether/actions/workflows/ci.yml/badge.svg)](https://github.com/nicolasmd87/aether/actions/workflows/ci.yml)
[![Windows](https://github.com/nicolasmd87/aether/actions/workflows/windows.yml/badge.svg)](https://github.com/nicolasmd87/aether/actions/workflows/windows.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20WASM%20%7C%20Embedded-lightgrey)]()

A compiled actor-based programming language with type inference, designed for concurrent systems. Aether compiles to C for native performance and seamless C interoperability.

## Overview

Aether is a compiled language that brings actor-based concurrency to systems programming. The compiler generates readable C code, providing portability and interoperability with existing C libraries.

**Core Features:**
- Actor-based concurrency with automatic multi-core scheduling
- Type inference with optional annotations
- Compiles to readable C for portability and C library interop
- Lock-free message passing with adaptive optimizations
- Go-style result types: `a, err = func()` with `_` discard
- Package management: `ae add host/user/repo[@version]` (GitHub, GitLab, Bitbucket, any git host)

## Runtime Features

The Aether runtime implements a native actor system with optimized message passing:

### Concurrency Model
- **Multi-core partitioned scheduler** with locality-aware actor placement
- **Locality-aware spawning** — actors placed on the caller's core for efficient parent-child messaging
- **Message-driven migration** — communicating actors automatically converge onto the same core
- **Work-stealing fallback** for idle core balancing
- **Lock-free SPSC queues** for same-core messaging
- **Cross-core messaging** with lock-free mailboxes

### Memory Management
- **Manual by default** — use `defer` for cleanup. All allocations cleaned up explicitly.
- **Arena allocators** for actor lifetimes
- **Memory pools** with thread-local allocation
- **Actor pooling** reducing allocation overhead
- **Zero-copy message delivery** in single-actor main-thread mode (caller stack passed directly)

### Message Optimization
- **Sender-side batching** for reduced overhead
- **Message coalescing** for higher throughput
- **Adaptive batching** dynamically adjusts batch sizes
- **Direct send** for same-core actors bypasses queues

### Platform Portability
- **Compile-time platform detection** via `AETHER_HAS_*` flags (threads, atomics, filesystem, networking, NUMA, SIMD, affinity)
- **Cooperative scheduler** for single-threaded platforms (WebAssembly, embedded, bare-metal)
- **Graceful degradation** — stdlib stubs return errors when features are unavailable
- **`ae build --target wasm`** compiles to WebAssembly via Emscripten
- **`PLATFORM=wasm|embedded`** Makefile targets for cross-compilation
- **Docker CI images** for Emscripten (WASM) and ARM (embedded) verification

### Advanced Features
- **Actor timeouts** — `receive { ... } after N -> { ... }` fires handler if no message arrives within N ms
- **Cooperative preemption** (opt-in) — `AETHER_PREEMPT=1` breaks long handlers, `--preempt` yields at loop back-edges
- **SIMD batch processing** with AVX2 support
- **NUMA-aware allocation** for multi-socket systems
- **CPU feature detection** for runtime optimization selection
- **Performance profiling** with per-core cycle counting
- **Message tracing** for debugging

### Benchmarks

Aether includes a comprehensive cross-language benchmark suite comparing actor implementations across 11 languages. Run `make benchmark` to evaluate performance on your system.

See [benchmarks/cross-language/](benchmarks/cross-language/) for methodology and implementation details.

## Quick Start

### Install

**Linux / macOS — one-line install:**

```bash
git clone https://github.com/nicolasmd87/aether.git
cd aether
./install.sh
```

Installs to `~/.aether` and adds `ae` to your PATH. Restart your terminal or run `source ~/.bashrc`, `~/.zshrc`, or `~/.bash_profile`.

**Windows — download and run:**

1. Download `aether-*-windows-x86_64.zip` from [Releases](https://github.com/nicolasmd87/aether/releases)
2. Extract to any folder (e.g. `C:\aether`)
3. Add `C:\aether\bin` to your PATH
4. **Restart your terminal** (so PATH takes effect)
5. Run `ae init hello && cd hello && ae run`

GCC is downloaded automatically the first time you run a program (~80 MB, one-time) — no MSYS2 or manual toolchain setup required.

**All platforms — manage versions with `ae version`:**

```bash
ae version list              # see all available releases
ae version install v0.25.0   # download and install a specific version
ae version use v0.25.0       # switch to that version
```

### Your First Program

```bash
# Create a new project
ae init hello
cd hello
ae run
```

Or run a single file directly:

```bash
ae run examples/basics/hello.ae
```

### Editor Setup (Optional)

Install syntax highlighting for a better coding experience:

**VS Code / Cursor:**
```bash
cd editor/vscode
./install.sh
```

This provides:
- Syntax highlighting with TextMate grammar
- Custom "Aether Erlang" dark theme
- `.ae` file icons

### Development Build (without installing)

If you prefer to build without installing:

```bash
make ae
./build/ae version
./build/ae run examples/basics/hello.ae
```

### The `ae` Command

`ae` is the single entry point for everything — like `go` or `cargo`:

```bash
ae init <name>           # Create a new project
ae run [file.ae]         # Compile and run (file or project)
ae build [file.ae]       # Compile to executable
ae check [file.ae]       # Type-check without compiling (~30x faster)
ae test [file|dir]       # Discover and run tests
ae examples [dir]        # Build all example programs
ae add <host/user/repo>  # Add a dependency (any git host)
ae repl                  # Start interactive REPL
ae cache                 # Show build cache info
ae cache clear           # Clear the build cache
ae version               # Show current version
ae version list          # List all available releases
ae version install <v>   # Install a specific version
ae version use <v>       # Switch to an installed version
ae help                  # Show all commands
```

In a project directory (with `aether.toml`), `ae run` and `ae build` compile `src/main.ae` as the program entry point. You can also pass `.` as the directory: `ae run .` or `ae build .`.

**Using Make (alternative):**

```bash
make compiler                    # Build compiler only
make ae                          # Build ae CLI tool
make test                        # Run runtime C test suite (166 tests)
make test-ae                     # Run .ae source tests (95 tests)
make test-all                    # Run all tests
make examples                    # Build all examples
make -j8                         # Parallel build
make help                        # Show all targets
```

**Windows:** Use the [release binary](https://github.com/nicolasmd87/aether/releases) — no MSYS2 needed. To build from source, use MSYS2 MinGW 64-bit shell; `make ci` runs the full suite (compiler, ae, stdlib, REPL, C tests, .ae tests, examples) with no skips.

## Project Structure

```
aether/
├── compiler/           # Aether compiler (lexer, parser, codegen)
│   ├── parser/        # Lexer, parser, tokens
│   ├── analysis/      # Type checker, type inference
│   ├── codegen/       # C code generation, optimizer
│   └── aetherc.c      # Compiler entry point
├── runtime/           # Runtime system
│   ├── actors/        # Actor implementation and lock-free mailboxes
│   ├── config/        # Platform detection, optimization tiers, runtime config
│   ├── memory/        # Arena allocators, memory pools, batch allocation
│   ├── scheduler/     # Multi-core scheduler + cooperative single-threaded backend
│   └── utils/         # CPU detection, SIMD, tracing, thread portability
├── std/               # Standard library
│   ├── string/       # String operations
│   ├── file/         # File operations (open, read, write, delete)
│   ├── dir/          # Directory operations (create, delete, list)
│   ├── path/         # Path utilities (join, basename, dirname)
│   ├── fs/           # Combined file/dir/path module
│   ├── collections/  # List, HashMap, Vector, Set, PQueue
│   ├── list/         # Dynamic array (ArrayList)
│   ├── map/          # Hash map
│   ├── json/         # JSON parser and builder
│   ├── http/         # HTTP client and server
│   ├── tcp/          # TCP client and server
│   ├── net/          # Combined TCP/HTTP networking module
│   ├── math/         # Math functions and random numbers
│   ├── io/           # Console I/O, environment variables
│   ├── os/           # Shell execution, command capture, env vars
│   └── log/          # Structured logging
├── tools/            # Developer tools
│   ├── ae.c          # Unified CLI tool (ae command)
│   └── apkg/         # Project tooling, TOML parser
├── tests/            # Test suite (runtime, syntax, integration)
├── examples/         # Example programs (.ae files)
│   ├── basics/       # Hello world, variables, arrays, etc.
│   ├── actors/       # Actor patterns (ping-pong, pipeline, etc.)
│   └── applications/ # Complete applications
├── docs/            # Documentation
└── docker/          # Docker (CI, dev, WASM, embedded)
```

## Language Example

```aether
// Counter actor with message handling
message Increment {}
message Decrement {}
message Reset {}

actor Counter {
    state count = 0

    receive {
        Increment() -> {
            count = count + 1
        }
        Decrement() -> {
            count = count - 1
        }
        Reset() -> {
            count = 0
        }
    }
}

main() {
    // Spawn counter actor
    counter = spawn(Counter())

    // Send messages
    counter ! Increment {}
    counter ! Increment {}
    counter ! Decrement {}
    counter ! Reset {}
    counter ! Increment {}

    // Wait for all messages to be processed
    wait_for_idle()

    println("Final count: ${counter.count}")
}
```

## Runtime Configuration

When embedding the Aether runtime in a C application, configure optimizations at startup:

```c
#include "runtime/aether_runtime.h"

int main() {
    // Auto-detect CPU features and enable optimizations
    aether_runtime_init(4, AETHER_FLAG_AUTO_DETECT);

    // Or manually configure
    aether_runtime_init(4,
        AETHER_FLAG_LOCKFREE_MAILBOX |
        AETHER_FLAG_ENABLE_SIMD |
        AETHER_FLAG_ENABLE_MWAIT
    );

    // Your actor system runs here

    return 0;
}
```

Available flags:
- `AETHER_FLAG_AUTO_DETECT` - Detect CPU features and enable optimizations
- `AETHER_FLAG_LOCKFREE_MAILBOX` - Use lock-free SPSC mailboxes
- `AETHER_FLAG_ENABLE_SIMD` - AVX2 vectorization for batch operations
- `AETHER_FLAG_ENABLE_MWAIT` - MWAIT-based idle (x86 only)
- `AETHER_FLAG_VERBOSE` - Print runtime configuration

## Optimization Tiers

The runtime employs a tiered optimization strategy:

**TIER 0 - Platform Capabilities (compile-time):**
- `AETHER_HAS_THREADS` — pthreads/Win32 threads (auto-detected; disabled on WASM/embedded)
- `AETHER_HAS_ATOMICS` — C11 stdatomic (fallback: volatile for single-threaded)
- `AETHER_HAS_FILESYSTEM` / `AETHER_HAS_NETWORKING` — stdlib feature gates
- `AETHER_HAS_SIMD` / `AETHER_HAS_NUMA` / `AETHER_HAS_AFFINITY` — hardware feature gates
- Override any flag with `-DAETHER_NO_<FEATURE>` (e.g. `-DAETHER_NO_THREADING`)

**TIER 1 - Always Enabled:**
- Actor pooling (reduces allocation overhead)
- Direct send for same-core actors (bypasses queues)
- Adaptive batching (adjusts batch size dynamically)
- Message coalescing (combines small messages)
- Thread-local message pools

**TIER 2 - Auto-Detected:**
- SIMD batch processing (requires AVX2/NEON)
- MWAIT idle (requires x86 MONITOR/MWAIT)
- CPU core pinning (OS-dependent)

**TIER 3 - Opt-In:**
- Lock-free mailbox (better under contention)
- Message deduplication (prevents duplicate processing)

## Documentation

- [Getting Started Guide](docs/getting-started.md) - Installation and first steps
- [Language Tutorial](docs/tutorial.md) - Learn Aether syntax and concepts
- [Language Reference](docs/language-reference.md) - Complete language specification
- [C Interoperability](docs/c-interop.md) - Using C libraries and the `extern` keyword
- [Architecture Overview](docs/architecture.md) - Runtime and compiler design
- [Memory Management](docs/memory-management.md) - defer-first manual model, arena allocators
- [Runtime Optimizations](docs/runtime-optimizations.md) - Performance techniques
- [Cross-Language Benchmarks](benchmarks/cross-language/README.md) - Comparative performance analysis
- [Docker Setup](docker/README.md) - Container development environment

## Development

### Running Tests

```bash
# Runtime C test suite
make test

# Aether source tests
make test-ae

# All tests
make test-all

# Build all examples
make examples
```

### Testing

```bash
# Full CI suite (8 steps, -Werror) — runs on your current platform
make ci

# Unit tests only (166 tests)
make test

# Integration tests only (108 .ae tests)
make test-ae

# Build all examples (61 programs)
make examples

# Full CI + Valgrind + ASan in Docker (Linux)
make docker-ci
```

### Cross-Platform Testing

**CI runs automatically on:** Linux (GCC + Clang), macOS (ARM64 + x86_64), Windows (MinGW/MSYS2)

```bash
# Cooperative scheduler (no Docker needed)
make ci-coop

# Windows cross-compile syntax check (requires mingw-w64 or Docker)
make ci-windows              # needs: brew install mingw-w64
make docker-ci-windows       # or use Docker

# WebAssembly (requires Docker with Emscripten)
make docker-ci-wasm

# ARM embedded syntax check (requires Docker with arm-none-eabi-gcc)
make docker-ci-embedded

# All portability checks (coop + WASM + embedded)
make ci-portability
```

**`make ci` tests your current OS only.** No OS can locally test another OS natively — macOS cannot be virtualized on Linux/Windows, Windows build+run requires MSYS2. GitHub Actions CI automatically tests all 5 platform targets (Linux GCC, Linux Clang, macOS ARM64, macOS x86_64, Windows MinGW) on every PR. Docker targets (`docker-ci-windows`, `docker-ci-wasm`, `docker-ci-embedded`) provide cross-compilation syntax checking from any host.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full pre-PR checklist.

### Running Benchmarks

```bash
# Run cross-language benchmark suite with interactive UI
make benchmark
# Open http://localhost:8080 to view results

# Or run directly
cd benchmarks/cross-language
./run_benchmarks.sh
```

The benchmark suite compares Aether against C, C++, Go, Rust, Java, Zig, Erlang, Elixir, Pony, and Scala using baseline actor implementations. Results are system-dependent.

## Status

Aether is under active development. The compiler, runtime, and standard library are functional and tested.

**What works today:**
- Full compiler pipeline with Rust-style diagnostics (file, line, column, source context, caret, hints)
- Multi-core actor runtime with locality-aware placement, message-driven migration, and work-stealing fallback
- Main-thread actor mode — single-actor programs bypass the scheduler entirely (zero-overhead path)
- Batch fan-out send for main-to-many patterns
- Lock-free message passing with adaptive optimizations
- Module system with pure Aether modules, export visibility, and namespace-qualified calls
- Standard library (collections, networking, JSON, file I/O, math, OS/shell)
- Interactive REPL (`ae repl`) with session persistence and error recovery
- C embedding via `--emit-header`
- IDE support (VS Code, Cursor) with syntax highlighting
- Cross-platform (macOS, Linux, Windows) with cooperative scheduler for WASM and embedded targets
- Platform portability layer with compile-time feature detection and graceful degradation
- Docker CI for cross-platform verification (Emscripten WASM, ARM embedded)

**Known Limitations:**
- Package registry supports any git host but lacks transitive dependency resolution and lock file integrity checking
- Stdlib still uses `int` returns for error handling (migration to result types planned)

**Roadmap:**
- Closures and first-class functions
- Stdlib migration to result types
- Async I/O (io_uring/kqueue/IOCP)
- WebAssembly Phase 2 (Web Workers)

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Areas of interest:**
- Runtime optimizations
- Standard library expansion
- Documentation and examples

## Supporting Aether

Aether is free and open source, built and maintained in personal time. CI runners, cross-platform testing infrastructure, and future project hosting cost real money.

If Aether is useful to you, consider [sponsoring the project on GitHub](https://github.com/sponsors/nicolasmd87). Every contribution goes directly into development and infrastructure.

[![Sponsor](https://img.shields.io/badge/Sponsor-Aether-blue?logo=github-sponsors)](https://github.com/sponsors/nicolasmd87)

## Acknowledgments

Aether draws inspiration from:
- **Erlang/OTP** — Actor model, message passing semantics
- **Go** — Pragmatic tooling, simple concurrency primitives
- **Rust** — Systems programming practices, zero-cost abstractions
- **Pony** — Actor-based type safety concepts

## License

MIT License. See [LICENSE](LICENSE) for details.
