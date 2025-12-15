# Aether Programming Language

Aether is a compiled programming language focused on lightweight, high-performance actor-based concurrency.

## Key Features

- State machine actors with 166M msg/sec throughput
- Zero-copy message passing
- 264 bytes per actor (vs 1-8MB for OS threads)
- Compiles to efficient C code
- No runtime overhead

## Quick Start

```bash
make
./build/aetherc examples/test_actor_working.ae output.c
gcc output.c -Iruntime -o program
./program
```

## Example

```aether
actor Counter {
    state int count = 0;
    
    receive(msg) {
        if (msg.type == 1) {
            count = count + 1;
        }
    }
}

main() {
    Counter c = spawn_Counter();
    send_Counter(c, 1, 0);
    Counter_step(c);
}
```

## Performance

Ring benchmark (1000 actors, 1M messages):
- Throughput: 166.7 M msg/sec
- Memory: 264 KB total
- Time: 6ms

## Documentation

- [Getting Started](docs/getting-started.md)
- [Language Specification](docs/language-spec.md)
- [Actor Implementation](docs/actors-complete.md)
- [Multi-Core Architecture](docs/multicore-architecture.md)
- [Build Instructions](BUILD_INSTRUCTIONS.md)

## Implementation Status

**Complete:**
- Lexer, Parser, AST
- Type checking
- Code generation
- State machine actors
- Message passing
- Spawn and send functions

**Planned:**
- Work-stealing scheduler
- Multi-threaded runtime
- Pattern matching

## Architecture

Actors compile to C structs with step functions. No threads, no locking, pure function calls. Scheduler runs actors cooperatively.

See [docs/ACTORS_COMPLETE.md](docs/ACTORS_COMPLETE.md) for full details.

## License

MIT
