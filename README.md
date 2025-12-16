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

- [Language Reference](docs/language-reference.md) - Complete syntax and semantics
- [Runtime Guide](docs/runtime.md) - Actors and concurrency model
- [Build Guide](docs/build.md) - Compilation and build instructions
- [Architecture](docs/architecture.md) - Compiler internals and design

## Implementation Status

**Complete:**
- Lexer, Parser, AST
- Type checking
- Code generation
- State machine actors (single-threaded: 166M msg/sec)
- Multi-core scheduler (fixed core partitioning)
- Message passing with lock-free queues
- Spawn and send functions

**Optional Future:**
- Pattern matching
- Work-stealing scheduler
- NUMA-aware placement

## Architecture

Actors compile to C structs with step functions. No threads, no locking, pure function calls. Scheduler runs actors cooperatively.

See [Runtime Guide](docs/runtime.md) for actor details.

## Project Structure

- `compiler/` - Compiler source (lexer, parser, typechecker, codegen)
- `runtime/` - Actor runtime (mailbox, scheduler, queues)
- `examples/` - Example Aether programs
- `tests/` - Test suite
- `docs/` - Documentation
- `editor/` - Editor support files (VS Code/Cursor syntax highlighting)
- `asm/` - Assembly stubs (not integrated, see [asm-status.md](docs/asm-status.md))

## Editor Support

VS Code and Cursor syntax highlighting with Erlang-inspired color scheme is available. See [editor/README.md](editor/README.md) for installation instructions.

## License

MIT
