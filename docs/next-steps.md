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

**Why not now:** Aether currently supports only single return values. Adding multiple returns touches the parser (tuple destructuring), type inference (propagating paired types), and codegen (struct returns or out-params). The `1`/`0` int convention is consistent across the stdlib today and works for v0.x. Multiple returns is the natural next step once the core language stabilizes.

**Origin:** Observed during stdlib hardening — `file.delete()`, `file.write()`, `dir.create()` were returning raw POSIX values (`0`/`-1`) instead of Aether's `1`/`0` convention. Even after fixing the convention, the underlying problem remains: int return values can't carry error context and are easy to ignore.

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

### Optional Cooperative Preemption

Aether's scheduler is cooperative — each message handler runs to completion before the scheduler moves to the next actor. A handler that enters an infinite loop will block that core's scheduler thread. This is the same model as Go goroutines and Pony behaviours. BEAM is unique in having reduction-based preemption that prevents this.

The scheduler already enforces fairness *between* actors (caps at 64 messages per actor per batch, yields for cross-core messages), but within a single handler there is no preemption.

> **Note:** The cooperative scheduler (`aether_scheduler_coop.c`) is a different concept — it's the single-threaded backend for threadless platforms (WASM, embedded), not a fairness mechanism. Cooperative preemption is about interrupting long-running handlers within the existing multi-threaded scheduler.

**Planned approach (opt-in, zero cost when disabled):**

- **Scheduler-side:** After each `actor->step()` call in the drain loop, check a cycle counter. If a handler exceeded a time threshold (e.g., ~1ms), break out and re-queue the actor. Cost: ~1 `rdtsc` read per step call, only when enabled.
- **Codegen-side (advanced):** A compiler flag inserts `aether_check_preempt()` calls at loop back-edges in generated C code. This decrements a reduction counter and yields when it hits zero. Cost: 2-3 cycles per loop iteration. Default off.

**Design constraint:** Default off, zero overhead when disabled. Fits the "lock as little as possible" philosophy. Programs that keep handlers short (which is best practice in any actor system) pay nothing.

## Quick Wins

Near-term improvements that build on existing infrastructure.

### Actor Timeouts

Actors can currently wait forever for messages that never arrive. A timeout mechanism enables health checks, retries, and deadlock detection.

**Planned syntax (tentative):**

```aether
actor Worker {
    receive {
        Task(data) -> { process(data) }
    } after 5000 -> {
        println("No tasks for 5 seconds, shutting down")
    }
}
```

**What's needed:**
- `after` clause in receive blocks (parser + codegen)
- Timer infrastructure in scheduler (rdtsc-based deadline per actor)
- Timeout message delivered as a special reserved message type

### Package Registry

`ae add` can clone GitHub repos today but there's no versioned registry, no dependency resolution, and no lock files. Starting with a GitHub-based package index (similar to early Cargo), version constraints in `aether.toml`, and a lock file format.

## Future

Major features that require significant architectural work.

### WebAssembly Target

Aether compiles to C, and C compiles to WASM via Emscripten, so the path exists. The platform portability layer addresses the core blockers: pthreads, filesystem, and networking dependencies are now conditionally compiled via `AETHER_HAS_*` flags.


**What's remaining:**
- `ae build --target wasm` CLI integration
- Emscripten-specific output (`.wasm` + `.js` glue, HTML template)
- WASI support for non-browser environments
- End-to-end testing with real Emscripten toolchain in Docker

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
| `ae build --target wasm` | Not started | CLI integration for WebAssembly builds |
| Package registry v1 | Not started | Version constraints, lock files, dependency resolution |
