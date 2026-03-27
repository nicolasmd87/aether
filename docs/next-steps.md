# Next Steps

Planned features and improvements for upcoming Aether releases.

> See [CHANGELOG.md](../CHANGELOG.md) for what shipped in each release.

## Language Features

### Result Type — Structured Error Handling

Currently all stdlib functions return `int` where `1` = success and `0` = failure. This works but has drawbacks: the caller can silently ignore errors, there's no way to carry error details, and it's easy to confuse success/failure when mixing with C conventions.

Go-style multiple return values would fit Aether's inferred-type philosophy — no generics or sum types needed. The compiler already knows types through inference, so a `(value, err)` pair just works.

**Planned syntax (tentative):**

```aether
import std.file
import std.io

main() {
    // Multiple return values — err is a string (or empty "" on success)
    f, err = file.open("data.txt", "r")
    if err {
        println("Failed: ${err}")
        exit(1)
    }
    content = file.read_all(f)
    file.close(f)

    // Ignore the error (explicit _ discard)
    data, _ = io.read_file("config.txt")

    // Or default on failure
    content = io.read_file("config.txt") or "default config"
}
```

**What's needed:**
- Multiple return values from functions (codegen emits a C struct or out-parameter)
- Tuple destructuring in assignment (`a, b = func()`)
- Error type convention: empty string `""` = no error, non-empty = error message
- Optional: `or` keyword for inline defaults on error

### Closures and First-Class Functions

Arrow functions exist but are named functions, not values. There are no anonymous functions and no way to capture variables from an enclosing scope. This blocks higher-order patterns like `list.map()`, `list.filter()`, and callbacks.

**Planned syntax (tentative):**

```aether
import std.list

main() {
    numbers = [1, 2, 3, 4, 5]

    // Anonymous function as argument
    doubled = list.map(numbers, fn(x) { x * 2 })

    // Closure capturing a local variable
    threshold = 3
    big = list.filter(numbers, fn(x) { x > threshold })
}
```

**What's needed:**
- Anonymous function expression syntax (e.g., `fn(x) { x * 2 }`)
- `AST_CLOSURE` node with a capture list in the compiler
- Codegen emits a struct holding captured variables + a function pointer (standard closure conversion to C)
- Function types in type inference (e.g., `(int) -> int` as a first-class type)

**Design constraint:** Capture by value (copy into closure struct) is the default. No hidden heap allocation. This keeps closures predictable and compatible with manual memory management.

## Quick Wins

Near-term improvements that build on existing infrastructure.

### Package Registry

`ae add` can clone GitHub repos today but there's no versioned registry, no dependency resolution, and no lock files. Starting with a GitHub-based package index (similar to early Cargo), version constraints in `aether.toml`, and a lock file format.

## Future

Major features that require significant architectural work.

### WebAssembly Target — Phase 2

Phase 1 is complete: `ae build --target wasm` compiles Aether to WebAssembly via Emscripten. Multi-actor programs work cooperatively.

**What's remaining (Phase 2):**
- Multi-actor programs using Web Workers as scheduler threads with `postMessage`
- Emscripten-specific output (HTML template for browser)
- WASI support for non-browser environments

### Async I/O Integration

All I/O in Aether is currently blocking. There is no io_uring (Linux), kqueue (macOS), or IOCP (Windows) integration. The actor model naturally maps to the submit/complete pattern (send a request, receive a completion message), but the runtime doesn't use it yet.

**What's needed:**
- I/O event loop thread(s) using platform-native async APIs
- I/O completions delivered as actor messages
- Scheduler awareness of I/O-blocked actors (don't count them as idle)
- Async variants of file and network operations in the stdlib

## Tooling

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| `ae fmt` | Not started | Source code formatter (deferred until syntax stabilizes) |
| Package registry v1 | Not started | Version constraints, lock files, dependency resolution |
