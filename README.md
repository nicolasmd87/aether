# Aether Programming Language

A high-performance actor-based language with automatic memory management and strong type inference.

## Features

- **Actor-based concurrency**: Lightweight actors with message passing
- **Type inference**: Optional type annotations with full inference
- **Memory safety**: Automatic memory management without GC pauses
- **High performance**: 2.3B messages/sec on commodity hardware
- **Cross-platform**: Windows, Linux, and macOS support

## Building

```bash
apkg build
```
Installation

```bash
# Build from source
./build.sh          # Unix/Linux/macOS
.\build.ps1         # Windows

# Run tests
cd tests && ./run_tests.sh
```

## Quick Example

```aether
actor Counter {
    var count = 0
    
    receive {
        Increment => count += 1
        GetCount(reply) => reply.send(count)
    }
}

let counter = spawn Counter
counter ! Increment
```

See [docs/tutorial.md](docs/tutorial.md) for a complete guide.