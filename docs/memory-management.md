# Aether Memory Management

Aether's memory model is **deterministic scope-exit cleanup**, not garbage collection.

The guiding principle:
> **Allocations visible at call site. Cleanup explicit and composable. `defer` is your primary tool.**

No hidden allocations. No GC pauses. No magic.

### Why `defer` and not auto-free?

Auto-free (where the compiler silently injects cleanup) is available as an opt-in convenience,
but the default is explicit `defer` because:

1. **Visible** -- you can see every allocation and its cleanup in the source
2. **Composable** -- works with any function, not just stdlib types
3. **Predictable** -- no special naming conventions, no hidden registry, no surprises
4. **Familiar** -- same pattern as Go's `defer` and Zig's `defer`

The one-line cost (`defer type_free(x)`) is the price for knowing exactly what your program does.

---

## The Actual Model

Aether uses two allocation mechanisms:

| Layer | What | When |
|-------|------|------|
| **Actor arena** | Actor state, message queues | Freed when actor is destroyed |
| **Stdlib heap** | `map_new()`, `list_new()`, etc. | Freed via `defer` or explicit `_free()` call |

There is **no garbage collector**.

---

## Stdlib Convention

All stdlib types follow one consistent naming pattern:

```
type_new()    -> allocates on the heap, returns a pointer (must be freed)
type_free(t)  -> frees the allocation
```

| Module | Constructor | Destructor |
|--------|-------------|------------|
| `std.map` | `map_new()` | `map_free(m)` |
| `std.list` | `list_new()` | `list_free(l)` |
| `std.string` | `string_new()` | `string_free(s)` |
| `std.fs` | `dir_list(path)` | `dir_list_free(l)` |

**Rule**: Any function whose name ends in `_new()` or `_create()` returns an allocated object. Its matching `_free()` is its destructor.

---

## The `defer` Pattern (default)

Aether's primary memory management pattern is `defer`: allocate, immediately defer the free, then use the resource. Cleanup runs at scope exit in LIFO order.

```aether
import std.map

main() {
    m = map_new()
    defer map_free(m)

    map_put(m, "k", "v")
    print(map_get(m, "k"))
    print("\n")
    // map_free(m) runs here (scope exit)
}
```

This is explicit, visible, and composable. It works with any function -- not just stdlib types.

### Multiple allocations

```aether
import std.list
import std.map

main() {
    m = map_new()
    defer map_free(m)

    items = list_new()
    defer list_free(items)

    // Use both...
    // At scope exit: list_free(items) runs first (LIFO), then map_free(m)
}
```

### Returning allocated values

When a function allocates and returns a value, the caller receives ownership:

```aether
import std.list

build_items(n) : ptr {
    result = list_new()
    i = 0
    while i < n {
        list_add(result, i)
        i = i + 1
    }
    return result
}

main() {
    items = build_items(10)
    defer list_free(items)

    print(list_size(items))
    print("\n")
}
```

---

## Auto-Free Mode (opt-in)

For convenience in scripts and small programs, auto-free mode can be enabled. The compiler automatically injects `_free()` calls at scope exit for local variables initialized from recognized constructors.

Enable in `aether.toml`:

```toml
[package]
name = "myapp"
version = "0.1.0"

[memory]
mode = "auto"
```

Or for a single run:

```bash
ae run --auto-free file.ae
ae build --auto-free file.ae
```

In auto mode, the compiler scans imported modules for constructor/destructor pairs (functions matching `_new`/`_free` or `_create`/`_free`) and injects the corresponding free at scope exit.

**When using auto mode**, use `@manual` on variables that must outlive their declaration scope:

```aether
import std.list

build_items(n) : ptr {
    @manual result = list_new()
    i = 0
    while i < n {
        list_add(result, i)
        i = i + 1
    }
    return result
}
```

---

## Actor State

Actor `state` variables initialized with `*_new()` **must always be `@manual`** (in auto mode) because they outlive any single message handler:

```aether
import std.map

message Store { key: string, value: string }
message Lookup { key: string }

actor Cache {
    @manual state data = map_new()

    receive {
        Store(key, value) -> {
            map_put(data, key, value)
        }
        Lookup(key) -> {
            print(map_get(data, key))
            print("\n")
        }
    }
}
```

The actor runtime frees the actor's arena (and its internal state) when the actor is shut down.

---

## Common Mistakes

**Forgetting `defer` after allocation:**

```aether
m = map_new()
map_put(m, "k", "v")
// LEAK: m is never freed
```

Fix: always pair allocation with `defer`:

```aether
m = map_new()
defer map_free(m)
```

**Deferring before the allocation succeeds:**

`defer` registers immediately. Place it right after the allocation, not before.

**Allocating inside a loop:**

`defer` fires at scope exit, not at end of each iteration. If you allocate inside a
loop, free explicitly at the end of each iteration instead:

```aether
while i < n {
    item = list_new()
    // ... use item ...
    list_free(item)
    i = i + 1
}
```

---

## Summary: When to Use What

| Situation | Approach |
|-----------|----------|
| Typical local allocation | `defer type_free(x)` right after allocation |
| Value returned from function | Caller defers the free |
| Value passed to an actor via `!` | Actor owns it; no defer in sender |
| Actor `state` initialized with `*_new()` | `@manual state ...` (auto mode) |
| Scripts / small programs | `[memory] mode = "auto"` for convenience |

---

## Examples

See the following runnable examples:

- [examples/basics/memory_defer.ae](../examples/basics/memory_defer.ae) -- defer pattern (recommended)
- [examples/basics/memory_manual.ae](../examples/basics/memory_manual.ae) -- default mode with defer
- [examples/basics/memory_escape.ae](../examples/basics/memory_escape.ae) -- returning allocated values
- [examples/basics/memory_auto.ae](../examples/basics/memory_auto.ae) -- opt-in auto-free mode
- [examples/actors/memory_actor.ae](../examples/actors/memory_actor.ae) -- `@manual state` in an actor

---

## Future Work (post v0.5.0)

**Arena-per-actor for stdlib types**: Actor state variables using `map_new()` / `list_new()` would automatically allocate from the actor's arena. Actor death -> total cleanup, zero explicit `*_free()` needed anywhere in actor code.

Requires threading an allocator parameter through the stdlib C implementations.
