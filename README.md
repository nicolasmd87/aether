# Aether Programming Language

[![CI](https://github.com/nicolasmd87/aether/actions/workflows/ci.yml/badge.svg)](https://github.com/nicolasmd87/aether/actions/workflows/ci.yml)
[![Windows](https://github.com/nicolasmd87/aether/actions/workflows/windows.yml/badge.svg)](https://github.com/nicolasmd87/aether/actions/workflows/windows.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)]()

A compiled actor-based programming language with type inference, designed for concurrent systems. Aether compiles to C for native performance and seamless C interoperability.

## Overview

Aether is a compiled language that brings actor-based concurrency to systems programming. The compiler generates readable C code, providing portability and interoperability with existing C libraries.

**Core Features:**
- Actor-based concurrency with automatic multi-core scheduling
- Type inference with optional annotations
- Compiles to readable C for portability and C library interop
- Lock-free message passing with adaptive optimizations

## Runtime Features

The Aether runtime implements a native actor system with optimized message passing:

### Concurrency Model
- **Multi-core partitioned scheduler** with per-core actor queues
- **Work-stealing fallback** for idle core balancing (primary strategy is partitioned assignment)
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

### Advanced Features
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
ae version install v0.17.0   # download and install a specific version
ae version use v0.17.0       # switch to that version
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
ae test [file|dir]       # Discover and run tests
ae examples [dir]        # Build all example programs
ae add <package>         # Add a dependency (GitHub repos)
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
make test-ae                     # Run .ae source tests (60 tests)
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
│   ├── memory/        # Arena allocators, memory pools, batch allocation
│   ├── scheduler/     # Multi-core partitioned scheduler with work-stealing fallback
│   └── utils/         # CPU detection, SIMD, tracing, profiling
├── std/               # Standard library
│   ├── collections/   # HashMap, Vector, Set, List
│   ├── string/       # String operations
│   ├── net/          # TCP/UDP networking, HTTP
│   ├── json/         # JSON parser
│   └── fs/           # File system operations
├── tools/            # Developer tools
│   ├── ae.c          # Unified CLI tool (ae command)
│   └── apkg/         # Project tooling, TOML parser
├── tests/            # Test suite (runtime, syntax, integration)
├── examples/         # Example programs (.ae files)
│   ├── basics/       # Hello world, variables, arrays, etc.
│   ├── actors/       # Actor patterns (ping-pong, pipeline, etc.)
│   └── applications/ # Complete applications
├── docs/            # Documentation
└── docker/          # Docker configuration
```

## Language Example

```aether
// Counter actor with message handling
message Increment {}
message Decrement {}
message GetCount {}
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
        GetCount() -> {
            println("Current count: ${count}")
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
    counter ! GetCount {}
    counter ! Decrement {}
    counter ! GetCount {}
    counter ! Reset {}
    counter ! GetCount {}
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
- Multi-core actor runtime with partitioned scheduler and work-stealing fallback
- Main-thread actor mode — single-actor programs bypass the scheduler entirely (zero-overhead path)
- Batch fan-out send for main-to-many patterns
- Lock-free message passing with adaptive optimizations
- Standard library (collections, networking, JSON, file I/O)
- C embedding via `--emit-header`
- IDE support (VS Code, Cursor) with syntax highlighting
- Cross-platform (macOS, Linux, Windows)

**Known Limitations:**
- Module system is early-stage (imports resolve at compile time; no versioned package registry yet)

**Roadmap:**
- Hot code reloading
- Package registry

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Areas of interest:**
- Runtime optimizations
- Standard library expansion
- Documentation and examples

## Acknowledgments

Aether draws inspiration from:
- **Erlang/OTP** — Actor model, message passing semantics
- **Go** — Pragmatic tooling, simple concurrency primitives
- **Rust** — Systems programming practices, zero-cost abstractions
- **Pony** — Actor-based type safety concepts

## License

MIT License. See [LICENSE](LICENSE) for details.
