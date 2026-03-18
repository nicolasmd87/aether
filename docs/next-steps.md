# Next Steps

Planned features and improvements for upcoming Aether releases.

## Standard Library

### ~~`std.os` — Shell & Process Execution~~ ✓ Done

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

## Tooling

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| `ae fmt` | Not started | Source code formatter |
| `ae check` | Not started | Type-check without compiling |
| Dead code diagnostics | Not started | Warn on unused variables/functions |

> See [CHANGELOG.md](../CHANGELOG.md) for what shipped in each release.

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
