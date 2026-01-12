# Aether Programming Language

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)]()

An experimental actor-based programming language with optional type annotations and type inference. Aether compiles to C and explores concurrent systems design through a native actor runtime.

**Project Status:** Early development. Not recommended for production use.

## Overview

Aether is a compiled language that brings actor-based concurrency to systems programming. The compiler generates readable C code, providing portability and interoperability with existing C libraries.

**Design Goals:**
- Actor-based concurrency with multi-core scheduling
- Type inference with optional annotations
- Native code generation through C compilation
- Zero-cost abstractions for performance-critical code

## Runtime Features

The Aether runtime implements a native actor system with optimized message passing:

### Concurrency Model
- **Multi-core scheduler** with per-core actor queues
- **Work-stealing** for dynamic load balancing
- **Lock-free SPSC queues** for same-core messaging (2-3x improvement)
- **Cross-core messaging** with lock-free mailboxes (1.8x improvement under contention)

### Memory Management
- **Arena allocators** for actor lifetimes
- **Memory pools** with thread-local allocation
- **Actor pooling** reducing allocation overhead (1.81x speedup)
- **Zero-copy message passing** for large messages (4.8x improvement for >256 bytes)

### Message Optimization
- **Sender-side batching** (256 messages per batch, 1.78x speedup)
- **Message coalescing** (15x throughput improvement)
- **Adaptive batching** (dynamically adjusts from 4-512 messages)
- **Direct send** for same-core actors (bypasses queue)

### Advanced Features
- **SIMD batch processing** with AVX2 support (1.5x improvement)
- **NUMA-aware allocation** for multi-socket systems
- **CPU feature detection** for runtime optimization selection
- **Performance profiling** with per-core cycle counting
- **Message tracing** for debugging

### Performance Characteristics
- **226M messages/sec** (ping-pong, 2 actors)
- **418M messages/sec** (ring, 100 actors)
- **3,125M msg/sec** (skynet, 1111 actors)
- **Sub-nanosecond latency** (0.96 cycles/msg on skynet)

See [benchmarks/cross-language/methodology.md](benchmarks/cross-language/methodology.md) for detailed methodology and [benchmarks/cross-language/](benchmarks/cross-language/) for comparisons with Go, Rust, C++, Erlang, and other languages.

## Quick Start

### Prerequisites

**Required:**
- GCC 11+ or Clang 14+
- Make (or mingw32-make on Windows)
- Git

**Optional:**
- Docker Desktop (recommended for consistent environment)

### Installation

```bash
git clone https://github.com/nicolasmd87/aether.git
cd aether

# Build compiler and CLI tool
make compiler
make ae

# Test it works
./build/ae version
```

**Windows:** Use `mingw32-make` instead of `make`.

### Your First Program

Create `hello.ae`:

```aether
main() {
    print("Hello from Aether!\\n");
    print("The answer is: %d\\n", 42);
}
```

Run it:

```bash
./build/ae run hello.ae
```

**Docker:**

```bash
# Build container
docker build -t aether:latest -f docker/Dockerfile .

# Start development shell
docker run -it -v $(pwd):/aether aether:latest

# Inside container
make compiler
make test
```

See [docker/README.md](docker/README.md) for detailed setup.

### Common Commands

**Using ae (recommended):**

```bash
./build/ae run hello.ae          # Run a program
./build/ae build app.ae -o myapp # Build executable
./build/ae compile lib.ae        # Compile to C only
./build/ae test                  # Run tests
./build/ae help                  # Show help
```

**Using Make:**

```bash
make compiler                    # Build compiler
make ae                          # Build ae CLI tool
make run FILE=hello.ae           # Run a program
make compile FILE=app.ae         # Build executable
make test                        # Run tests
make -j8                         # Parallel build
make help                        # Show all commands
```

**Windows:** Use `mingw32-make` instead of `make` and `.\\build\\ae.exe` instead of `./build/ae`.

## Project Structure

```
aether/
├── compiler/           # Aether compiler (lexer, parser, codegen)
│   ├── lexer.c/h      # Tokenization
│   ├── parser.c/h     # AST generation
│   ├── typechecker.c/h # Type inference and checking
│   ├── codegen.c/h    # C code generation
│   └── aetherc.c      # Compiler entry point
├── runtime/           # Runtime system
│   ├── actors/        # Actor implementation and lock-free mailboxes
│   ├── memory/        # Arena allocators, memory pools, batch allocation
│   ├── scheduler/     # Multi-core work-stealing scheduler
│   └── utils/         # CPU detection, SIMD, tracing, profiling
├── std/               # Standard library
│   ├── collections/   # HashMap, Vector, List
│   ├── io/           # File I/O, streams
│   ├── net/          # TCP/UDP networking
│   └── json/         # JSON parser
├── tests/            # Test suite
├── examples/         # Example programs (.ae files)
├── benchmarks/       # Performance benchmarks
│   ├── aether/       # Aether runtime benchmarks
│   └── cross-language/ # Comparisons with other languages
├── docs/            # Documentation
└── docker/          # Docker configuration
```

## Language Example

```aether
// Distributed counter with actor supervision
actor Counter {
    var count = 0

    receive {
        Increment => count += 1

        Decrement => count -= 1

        GetCount(reply) => {
            reply.send(count)
        }

        Reset => count = 0
    }
}

actor Supervisor {
    var counters: [ActorRef<Counter>] = []

    init() {
        // Spawn 4 counter actors
        for i in 0..4 {
            counters.push(spawn Counter)
        }
    }

    receive {
        IncrementAll => {
            for counter in counters {
                counter ! Increment
            }
        }

        GetTotal(reply) => {
            var total = 0
            for counter in counters {
                let count = counter !? GetCount
                total += count
            }
            reply.send(total)
        }
    }
}

// Main program
let supervisor = spawn Supervisor
supervisor ! IncrementAll
let total = supervisor !? GetTotal
println("Total count: {total}")
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
- Actor pooling (1.81x speedup)
- Direct send for same-core actors
- Adaptive batching (4-64 messages)
- Message coalescing (15x throughput)
- Thread-local message pools

**TIER 2 - Auto-Detected:**
- SIMD batch processing (requires AVX2/NEON)
- MWAIT idle (requires x86 MONITOR/MWAIT)
- CPU core pinning (OS-dependent)

**TIER 3 - Opt-In:**
- Lock-free mailbox (trade-off: faster under contention, slower single-threaded)
- Message deduplication (semantic change, adds overhead)

## Documentation

- [Getting Started Guide](docs/getting-started.md) - Installation and first steps
- [Language Tutorial](docs/tutorial.md) - Learn Aether syntax and concepts
- [Language Reference](docs/language-reference.md) - Complete language specification
- [Architecture Overview](docs/architecture.md) - Runtime and compiler design
- [Memory Management](docs/memory-management.md) - Arena GC and pooling strategies
- [Runtime Optimizations](docs/runtime-optimizations.md) - Performance techniques
- [Benchmark Methodology](benchmarks/cross-language/methodology.md) - Fair comparison principles
- [Docker Setup](docker/README.md) - Container development environment

## Development

### Running Tests

```bash
# All tests
make test

# Specific test suite
./build/test_harness compiler    # Compiler tests
./build/test_harness runtime     # Runtime tests
./build/test_harness integration # Integration tests
```

### Running Benchmarks

```bash
# Quick benchmark (1 run per language)
cd benchmarks/cross-language
./quick_bench.sh

# Start web UI
make benchmark-ui
# Open http://localhost:8080

# Statistical analysis (5 runs + warmup)
bash run_statistical_bench.sh
```

## Project Status

**Aether is experimental software.** It is a research project exploring actor-based concurrency and compiler design.

**Current capabilities:**
- Functional compiler (lexer, parser, type checker, code generator)
- Native actor runtime with multi-core scheduling
- Lock-free message passing optimizations
- Standard library (collections, I/O, networking)
- Cross-language benchmarks showing competitive performance

**Limitations:**
- No distribution (single-node only)
- No hot code reloading
- Manual memory management
- Limited error messages
- Small ecosystem
- Breaking changes expected

**Better alternatives for production:**
- Erlang/Elixir (distributed actors, proven at scale)
- Go (goroutines, mature ecosystem)
- Rust (memory safety, systems programming)
- Pony (actor-based with reference capabilities)

## Roadmap

### Current Status
- Core compiler pipeline functional
- Native actor runtime with multi-core scheduling
- Type inference with optional annotations
- Standard library (collections, I/O, networking)
- Cross-language benchmarks

### Development Priorities
- Stabilize core language features
- Improve error messages
- Expand test coverage
- Documentation completeness

### Exploratory Features
- Package management concepts
- Editor integration experiments
- Runtime optimization research

### Not Planned
- Production hardening (requires significant resources)
- Enterprise support
- Distributed actors
- Hot code reloading

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

This project explores ideas from:
- Erlang/OTP (actor model, supervision trees)
- Go (pragmatic concurrency, simple tooling)
- Rust (systems programming practices, zero-cost abstractions)
- Pony (actor-based type safety, reference capabilities)

Aether is a learning project and research platform.

## Contact

**Note:** This is an experimental project for learning and research. For production actor systems, consider established alternatives.

---

**Built for understanding concurrent systems design through hands-on implementation.**
