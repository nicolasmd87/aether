# Aether Programming Language

[![CI](https://github.com/nicolasmd87/aether/actions/workflows/ci.yml/badge.svg)](https://github.com/nicolasmd87/aether/actions/workflows/ci.yml)
[![Windows](https://github.com/nicolasmd87/aether/actions/workflows/windows.yml/badge.svg)](https://github.com/nicolasmd87/aether/actions/workflows/windows.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)]()

A compiled, actor-based language with type inference that targets C. Built for concurrent systems where predictability, performance, and C interoperability matter.

## Why Aether

Most languages make concurrency hard to get right or slow to run. Aether takes the actor model — proven in Erlang, used in production for decades — and compiles it to native C with no GC, no VM, no runtime surprises.

Every actor is isolated state with a message queue. No shared memory, no mutexes, no data races by construction. The scheduler pins actors to cores, uses lock-free SPSC queues for same-core messaging, and automatically batches cross-core sends. A single-actor program bypasses the scheduler entirely and runs synchronously on the main thread with zero allocation overhead.

```aether
message Increment {}
message GetCount {}

actor Counter {
    state count = 0

    receive {
        Increment() -> { count = count + 1 }
        GetCount()  -> { println("Count: ${count}") }
    }
}

main() {
    counter = spawn(Counter())
    counter ! Increment {}
    counter ! Increment {}
    counter ! GetCount {}
}
```

## For Game Engine Developers

Aether maps cleanly onto patterns game engines already use — but removes the friction that comes with shared mutable state, GC pauses, and FFI overhead.

### Actors as game entities

Each game object — a player, an enemy, a physics body, a network session — is an actor. Its state is private. It receives messages. It cannot corrupt another entity's data. No mutex, no lock ordering, no "the physics thread read a half-written transform."

```aether
message Move { dx: int, dy: int }
message TakeDamage { amount: int }
message Tick {}

actor Enemy {
    state x = 0
    state y = 0
    state hp = 100

    receive {
        Move(dx, dy) -> {
            x = x + dx
            y = y + dy
        }
        TakeDamage(amount) -> {
            hp = hp - amount
            if hp <= 0 { println("Enemy defeated") }
        }
        Tick() -> {
            // AI update — isolated, safe to schedule on any core
        }
    }
}
```

The scheduler distributes actors across cores automatically. Thousands of enemies, each with their own AI loop, fan out with a single batch send from main — no thread pool boilerplate, no job queue to manage.

### No GC, no pauses, no surprise allocations

Aether has no garbage collector. Cleanup is explicit via `defer` — declared right after allocation, runs at scope exit in LIFO order. Actor internal state uses arena allocators: when an actor is destroyed, its entire arena is freed in one call. There is no background collection, no stop-the-world pause, no frame-time spike from the runtime deciding to clean up at the wrong moment.

### Native C interop — plug into any engine or library

Aether compiles to readable C and calls C functions directly via `extern`. SDL2, OpenGL, Vulkan, Box2D, PhysX, your in-house engine — all reachable with zero FFI overhead, no binding generator, no marshalling layer.

```aether
extern SDL_Init(flags: int): int
extern SDL_CreateWindow(title: ptr, x: int, y: int, w: int, h: int, flags: int): ptr
extern SDL_PollEvent(event: ptr): int

main() {
    SDL_Init(0x00000020)  // SDL_INIT_VIDEO
    window = SDL_CreateWindow("Aether Game", 100, 100, 800, 600, 0)
    // game loop, actor dispatch, rendering — all in the same process
}
```

You can also go the other direction: compile Aether actors to a C header (`--emit-header`) and embed them in an existing C or C++ engine. Drop in an Aether simulation subsystem, a network layer, or an AI system without rewriting anything.

### Benchmark: actor throughput vs Go

| Pattern | vs Go |
|---|---|
| Ping-pong (actor ↔ actor) | **5x faster** |
| Counting (single actor, main-thread mode) | **3.8x faster** |
| Thread ring (N actors chained) | **2.5x faster** |
| Fork-join (main → many actors) | **3.6x faster** |

Single-actor programs — a game loop driving one simulation actor — use a zero-copy synchronous path that skips the scheduler entirely. No allocation, no queue, no wakeup latency.

## Install

**Linux / macOS:**

```bash
git clone https://github.com/nicolasmd87/aether.git
cd aether
./install.sh
```

Installs to `~/.aether`, adds `ae` to PATH. Restart your terminal or source your shell rc file.

**Windows:**

1. Download `aether-*-windows-x86_64.zip` from [Releases](https://github.com/nicolasmd87/aether/releases)
2. Extract and add `bin/` to your PATH
3. Run `ae run hello.ae` — GCC is downloaded automatically on first use, no MSYS2 required

**Version management:**

```bash
ae version list              # available releases
ae version install v0.10.0   # install specific version
ae version use v0.10.0       # switch active version
```

## Quick Start

```bash
ae init mygame
cd mygame
ae run
```

Or run any `.ae` file directly:

```bash
ae run examples/actors/ping_pong.ae
```

## The `ae` CLI

```bash
ae init <name>       # new project with aether.toml, src/main.ae, tests/
ae run [file.ae]     # compile and run (with build cache)
ae build [file.ae]   # compile to binary
ae test              # discover and run tests
ae examples          # build all examples
ae repl              # interactive REPL
ae cache             # show/clear build cache
ae help              # all commands
```

`ae run` uses a content-hash build cache. Cache hits execute in ~8ms. Cache misses compile and store for next time.

## Language Features

- **Actor-based concurrency** — isolated state, message queues, no shared mutable memory
- **Type inference** — `x = 42` infers `int`; annotate where it adds clarity
- **`defer` for cleanup** — explicit, LIFO, no GC, no surprises
- **String interpolation** — `"Player ${name} has ${hp} HP"`
- **Pattern matching** in receive blocks with destructuring
- **Compiles to C** — portable, debuggable with gdb/lldb, links against any C library
- **`--emit-header`** — generate C headers to embed Aether actors in existing C applications

## Runtime

### Scheduler
- Partitioned multi-core scheduler — actors assigned to cores, minimal cross-core traffic by default
- Work-stealing fallback for idle cores
- Lock-free SPSC queues for same-core messaging
- **Main-thread actor mode** — single-actor programs bypass the scheduler entirely (zero overhead synchronous path)
- **Batch send** — fan-out from main reduces N atomic operations to one per core

### Memory
- Explicit `defer` for stdlib allocations (`list`, `map`, etc.)
- Arena allocators for actor internal state — single free on actor death, no per-object tracking
- Thread-local memory pools for message allocation
- Actor pooling to amortize spawn cost

### Optimization tiers

**Always on:** actor pooling, direct same-core send, adaptive batching, message coalescing, thread-local pools

**Auto-detected:** SIMD batch processing (AVX2/NEON), MWAIT idle (x86), CPU core pinning

**Opt-in:** lock-free mailbox, message deduplication

## C Embedding

Embed Aether actors in any C/C++ application:

```bash
aetherc --emit-header simulation.ae simulation.h
```

```c
#include "runtime/aether_runtime.h"
#include "simulation.h"

int main() {
    aether_runtime_init(4, AETHER_FLAG_AUTO_DETECT);

    ActorRef* sim = spawn_Simulation();
    send_Step(sim, &(Step_msg){ .dt = 16 });

    aether_runtime_shutdown();
}
```

Available runtime flags:
- `AETHER_FLAG_AUTO_DETECT` — detect CPU features, enable optimizations automatically
- `AETHER_FLAG_LOCKFREE_MAILBOX` — lock-free SPSC mailboxes (better under contention)
- `AETHER_FLAG_ENABLE_SIMD` — AVX2/NEON batch operations
- `AETHER_FLAG_ENABLE_MWAIT` — MWAIT-based idle (x86 only)
- `AETHER_FLAG_VERBOSE` — print runtime configuration at startup

## Project Structure

```
aether/
├── compiler/       # Lexer, parser, type checker, type inference, C codegen
├── runtime/
│   ├── actors/     # Actor base, mailboxes, message registry
│   ├── memory/     # Arena allocators, pools
│   ├── scheduler/  # Multi-core partitioned scheduler, work-stealing
│   └── utils/      # CPU detection, SIMD, profiling, tracing
├── std/            # Collections, string, networking, JSON, file I/O
├── tools/          # ae CLI, TOML parser, package tooling
├── tests/          # Runtime C tests, .ae syntax and integration tests
├── examples/       # Basics, actors, stdlib, applications, C-interop
├── benchmarks/     # Cross-language actor benchmark suite (11 languages)
├── docs/           # Full documentation
└── docker/         # CI images with Valgrind for memory checking
```

## Documentation

- [Getting Started](docs/getting-started.md)
- [Language Reference](docs/language-reference.md)
- [Language Tutorial](docs/tutorial.md)
- [C Interoperability](docs/c-interop.md)
- [C Embedding](docs/c-embedding.md)
- [Memory Management](docs/memory-management.md)
- [Runtime Optimizations](docs/runtime-optimizations.md)
- [Architecture Overview](docs/architecture.md)
- [Cross-Language Benchmarks](benchmarks/cross-language/README.md)

## Status

Actively developed. Compiler, runtime, and standard library are functional and tested.

**Working today:**
- Full compiler pipeline with Rust-style diagnostics — file, line, column, source context, caret, fix hints
- Multi-core actor runtime — partitioned scheduler, work-stealing, lock-free queues
- Main-thread actor mode for single-actor programs (zero-overhead synchronous path)
- Batch fan-out send for main-to-many patterns
- Standard library — collections, networking, JSON, file I/O
- C embedding via `--emit-header`
- Cross-platform — macOS, Linux, Windows
- VS Code / Cursor syntax highlighting

**Known limitations:**
- No generics or parameterized types
- Module system is early-stage — imports resolve at compile time, no versioned package registry yet

**Roadmap:**
- Distribution — multi-node actor systems across machines
- Hot code reloading
- Package registry

## Development

```bash
make compiler        # build compiler
make ae              # build ae CLI
make test            # runtime C tests (162)
make test-ae         # .ae integration tests (24)
make test-all        # everything
make ci              # full CI suite (build + test + examples, -Werror)
make docker-ci       # CI + Valgrind memory check in Linux container
make benchmark       # cross-language benchmark suite
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Open areas: runtime optimizations, standard library expansion, documentation, examples.

## Acknowledgments

- **Erlang/OTP** — actor model and message passing semantics
- **Go** — pragmatic tooling philosophy
- **Rust** — systems programming practices
- **Pony** — actor-based type safety

## License

MIT. See [LICENSE](LICENSE).
