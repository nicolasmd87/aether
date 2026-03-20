# Next Steps

Planned features and improvements for upcoming Aether releases.

**Design principles guiding this roadmap:**
- Lock as little as possible — minimal ceremony, actors can't share state
- Zero-cost when not used — opt-in features add no overhead when disabled
- Compile to clean C — generated code stays readable and debuggable
- Actors are the native abstraction — everything else serves them
- Manual memory, no GC — `defer` is the tool, arenas for actors
- Practical over academic — ship what programs need

> See [CHANGELOG.md](../CHANGELOG.md) for what shipped in each release.

## Done

### ~~`std.os` — Shell & Process Execution~~ ✓

Shipped. `import std.os` provides `os.system()`, `os.exec()`, `os.getenv()`. See `examples/stdlib/os-demo.ae`.

**Origin:** [Issue #39](https://github.com/nicolasmd87/aether/issues/39)

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

**Planned approach (opt-in, zero cost when disabled):**

- **Scheduler-side:** After each `actor->step()` call in the drain loop, check a cycle counter. If a handler exceeded a time threshold (e.g., ~1ms), break out and re-queue the actor. Cost: ~1 `rdtsc` read per step call, only when enabled.
- **Codegen-side (advanced):** A compiler flag inserts `aether_check_preempt()` calls at loop back-edges in generated C code. This decrements a reduction counter and yields when it hits zero. Cost: 2-3 cycles per loop iteration. Default off.

**Design constraint:** Default off, zero overhead when disabled. Fits the "lock as little as possible" philosophy. Programs that keep handlers short (which is best practice in any actor system) pay nothing.

## Quick Wins

Near-term improvements that build on existing infrastructure.

### Actor Supervision Basics

The header `runtime/actors/aether_supervision.h` exists but is an empty stub. The goal is not full OTP — just the basics: crash detection, notification, and simple restart.

**What's planned:**
- `link(actor)` — get notified when a linked actor crashes
- Exit signal propagation between linked actors
- Simple one-for-one restart strategy (restart the crashed actor, leave others alone)

This is incremental and doesn't require the Result type to start — crash signals can use int status initially and upgrade to structured errors later.

### Export Visibility Enforcement

The `export` keyword is already parsed by the compiler but has zero semantic effect — all functions in a module are currently visible to importers. The quick win is making the typechecker reject calls to non-exported symbols from other modules.

### Selective Imports

`import std.math (sqrt, PI)` already parses and the typechecker has partial filtering logic, but it's unreliable. Fixing the selective import path so only the listed symbols are visible in the importing module.

### Pure Aether Modules

Currently all modules require a C backing file with `extern` declarations pointing to C implementations. The goal is allowing `.ae` files with function bodies to be imported directly. The module orchestrator already parses imported `.ae` files — the missing piece is merging their AST into the main program instead of only extracting extern declarations.

### Package Registry

`ae add` can clone GitHub repos today but there's no versioned registry, no dependency resolution, and no lock files. Starting with a GitHub-based package index (similar to early Cargo), version constraints in `aether.toml`, and a lock file format.

## Future

Major features that require significant architectural work.

### WebAssembly Target

Aether compiles to C, and C compiles to WASM via Emscripten, so the path exists. The main blocker is that the multi-core scheduler assumes pthreads. However, single-actor programs in main-thread mode already bypass the scheduler entirely — no threads, no locks, just straight function calls.

**Incremental approach:**
- **Phase 1:** Single-actor / no-actor programs compile to WASM via Emscripten with the scheduler stripped out. Main-thread mode handles this naturally today.
- **Phase 2:** Multi-actor programs using Web Workers as scheduler threads, with message passing over `postMessage`.

**What's needed:**
- Emscripten build target in `ae build --target wasm`
- Conditional compilation to strip pthreads-dependent scheduler code
- Stdlib shims for browser/WASI environments (no filesystem, limited I/O)

### Async I/O Integration

All I/O in Aether is currently blocking. There is no io_uring (Linux), kqueue (macOS), or IOCP (Windows) integration. The actor model naturally maps to the submit/complete pattern (send a request, receive a completion message), but the runtime doesn't use it yet.

**What's needed:**
- I/O event loop thread(s) using platform-native async APIs
- I/O completions delivered as actor messages
- Scheduler awareness of I/O-blocked actors (don't count them as idle)
- Async variants of file and network operations in the stdlib

### Generics / Monomorphization

Type inference is currently monomorphic. The stdlib uses `void*` with manual casts (e.g., `list_add(list, (void*)(intptr_t)i)`). Generics would make collections type-safe with zero runtime cost.

**What's needed:**
- Type parameter syntax (e.g., `List<T>` or `List[T]`)
- Type checker binding and substitution
- Monomorphization in codegen — stamp out one C function per concrete type, no vtables, no runtime dispatch

**Design constraint:** Monomorphization only. This generates more code but adds zero runtime overhead, consistent with "compile to clean C."

## Tooling

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| `ae fmt` | Not started | Source code formatter |
| `ae check` | Not started | Type-check without compiling |
| Dead code diagnostics | Not started | Warn on unused variables/functions |

## Compiler Diagnostics

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| Type mismatch hints | Not started | "expected 'string', got 'ptr'" with help text |
| Unused variable warnings | Not started | Warn on declared-but-unused locals |
| Unreachable code warnings | Not started | Detect dead branches after return/exit |

**Goal:** Python 3.10-style "the compiler is teaching you" error messages. Example:

```
error[E0201]: type mismatch — expected 'string', got 'ptr'
  --> src/main.ae:12:11
   |
12 |     print(result)
   |           ^^^^^^ this is a 'ptr' (raw pointer), not a 'string'
   |
   help: use string.to_cstr(result) to convert to a printable string
```
