# Aether Programming Language

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
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
- **Multi-core scheduler** with per-core actor queues
- **Work-stealing** for dynamic load balancing
- **Lock-free SPSC queues** for same-core messaging
- **Cross-core messaging** with lock-free mailboxes

### Memory Management
- **Arena allocators** for actor lifetimes
- **Memory pools** with thread-local allocation
- **Actor pooling** reducing allocation overhead
- **Zero-copy message passing** for large messages

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

```bash
git clone https://github.com/nicolasmd87/aether.git
cd aether
./install.sh
```

This builds Aether and installs it to `~/.aether`. After restarting your terminal (or running `source ~/.zshrc`), the `ae` command is available system-wide.

To install to a custom location: `./install.sh /usr/local` (requires sudo).

**Prerequisites:** GCC or Clang, Make, Git. The installer checks for these and tells you what's missing.

**Windows:** Use `mingw32-make ae` instead (see [Getting Started](docs/getting-started.md) for Windows setup).

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
ae add <package>         # Add a dependency
ae repl                  # Start interactive REPL
ae version               # Show version
ae help                  # Show all commands
```

In a project directory (with `aether.toml`), `ae run` and `ae build` work without arguments.

**Using Make (alternative):**

```bash
make compiler                    # Build compiler only
make ae                          # Build ae CLI tool
make test                        # Run C test suite (165 tests)
make test-ae                     # Run .ae source tests (15 tests)
make test-all                    # Run all tests (C + .ae)
make examples                    # Build all examples
make -j8                         # Parallel build
make help                        # Show all targets
```

**Windows:** Use `mingw32-make` instead of `make` and `.\\build\\ae.exe` instead of `./build/ae`.

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
│   ├── scheduler/     # Multi-core work-stealing scheduler
│   └── utils/         # CPU detection, SIMD, tracing, profiling
├── std/               # Standard library
│   ├── collections/   # HashMap, Vector, Set, List
│   ├── string/       # String operations
│   ├── net/          # TCP/UDP networking, HTTP
│   ├── json/         # JSON parser
│   └── fs/           # File system operations
├── tools/            # Developer tools
│   ├── ae.c          # Unified CLI tool (ae command)
│   └── apkg/         # Package manager, TOML parser
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
            print("Current count: ")
            print(count)
            print("\n")
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

Configure runtime optimizations at startup:

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
- [Memory Management](docs/memory-management.md) - Arena GC and pooling strategies
- [Runtime Optimizations](docs/runtime-optimizations.md) - Performance techniques
- [Cross-Language Benchmarks](benchmarks/cross-language/README.md) - Comparative performance analysis
- [Docker Setup](docker/README.md) - Container development environment

## Development

### Running Tests

```bash
# Runtime test suite (165 C tests)
make test

# Aether source tests (15 tests)
make test-ae

# All tests (C + .ae = 180 tests)
make test-all

# Build all examples (24 programs)
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
- Full compiler pipeline (lexer, parser, type checker, code generator)
- Multi-core actor runtime with work-stealing scheduler
- Lock-free message passing with adaptive optimizations
- Standard library (collections, networking, JSON, file I/O)
- IDE support (VS Code, Cursor) with syntax highlighting
- Cross-platform (macOS, Linux, Windows)

**Roadmap:**
- Distribution (multi-node actor systems)
- Hot code reloading
- Improved error messages
- Package registry

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Areas of interest:**
- Runtime optimizations
- Standard library expansion
- Error message improvements
- Documentation and examples

## Acknowledgments

Aether draws inspiration from:
- **Erlang/OTP** — Actor model, message passing semantics
- **Go** — Pragmatic tooling, simple concurrency primitives
- **Rust** — Systems programming practices, zero-cost abstractions
- **Pony** — Actor-based type safety concepts

## License

MIT License. See [LICENSE](LICENSE) for details.
