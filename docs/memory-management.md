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

The one-line cost (`defer type.free(x)`) is the price for knowing exactly what your program does.

---

## The Actual Model

Aether uses two allocation mechanisms:

| Layer | What | When |
|-------|------|------|
| **Actor arena** | Actor state, message queues | Freed when actor is destroyed |
| **Stdlib heap** | `map.new()`, `list.new()`, etc. | Freed via `defer` or explicit `.free()` call |

There is **no garbage collector**.

### Arena vs defer: when to use which

They solve different problems and are used in different layers:

| | Arena | defer |
|---|--------|--------|
| **What** | A region of memory; many small allocations share one region. One "free" destroys the whole region. | A language construct: run cleanup when leaving the current scope. |
| **Who uses it** | The **runtime** uses arenas for actor state and message queues. You don't allocate from arenas directly. | **You** use it in Aether code for stdlib types (`list.new`/`list.free`, `map.new`/`map.free`) and FFI buffers. |
| **Lifetime** | Everything in the arena lives until the arena is destroyed (e.g. when the actor is destroyed). | The resource lives until scope exit; the deferred call runs then. |
| **Typical use** | Actor internals: the runtime allocates actor state and messages from an arena; when the actor goes away, the arena is freed in one shot. No per-field frees. | Any heap allocation you make: `items = list.new(); defer list.free(items);` so the list is freed when the function (or block) exits. |

---

## Stdlib Convention

All stdlib types follow one consistent naming pattern. In Aether you call them with dot syntax; the underlying C functions use underscores (`type_new`/`type_free`):

```
type.new()    -> allocates on the heap, returns a pointer (must be freed)
type.free(t)  -> frees the allocation
```

| Module | Constructor | Destructor |
|--------|-------------|------------|
| `std.map` | `map.new()` | `map.free(m)` |
| `std.list` | `list.new()` | `list.free(l)` |
| `std.string` | `string.new()` | `string.free(s)` |
| `std.dir` | `dir.list(path)` | `dir.list_free(l)` |

**Rule**: Any function whose name ends in `_new()` or `_create()` (at the C level) returns an allocated object. Its matching `_free()` is its destructor. In Aether, use `type.new()` and `type.free(x)`.

---

## The `defer` Pattern (default)

Aether's primary memory management pattern is `defer`: allocate, immediately defer the free, then use the resource. Cleanup runs at scope exit in LIFO order.

```aether
import std.map

main() {
    m = map.new()
    defer map.free(m)

    map.put(m, "k", "v")
    print(map.get(m, "k"))
    print("\n")
    // map.free(m) runs here (scope exit)
}
```

This is explicit, visible, and composable. It works with any function -- not just stdlib types.

### Multiple allocations

```aether
import std.list
import std.map

main() {
    m = map.new()
    defer map.free(m)

    items = list.new()
    defer list.free(items)

    // Use both...
    // At scope exit: list.free(items) runs first (LIFO), then map.free(m)
}
```

### Returning allocated values

When a function allocates and returns a value, the caller receives ownership:

```aether
import std.list

build_items(n) : ptr {
    result = list.new()
    i = 0
    while i < n {
        list.add(result, i)
        i = i + 1
    }
    return result
}

main() {
    items = build_items(10)
    defer list.free(items)

    print(list.size(items))
    print("\n")
}
```

---

## Auto-Free Mode (opt-in)

For convenience in scripts and small programs, auto-free mode can be enabled. The compiler automatically injects the matching `.free()` call at scope exit for local variables initialized from recognized constructors (e.g. `list.new()` gets a `list.free()` at scope exit).

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

In auto mode, the compiler scans imported modules for constructor/destructor pairs (`.new()`/`.free()` or `.create()`/`.free()`) and injects the corresponding free at scope exit.

**When using auto mode**, use `@manual` on variables that must outlive their declaration scope:

```aether
import std.list

build_items(n) : ptr {
    @manual result = list.new()
    i = 0
    while i < n {
        list.add(result, i)
        i = i + 1
    }
    return result
}
```

---

## Actor State

Actor `state` variables initialized with `*.new()` **must always be `@manual`** (in auto mode) because they outlive any single message handler:

```aether
import std.map

message Store { key: string, value: string }
message Lookup { key: string }

actor Cache {
    @manual state data = map.new()

    receive {
        Store(key, value) -> {
            map.put(data, key, value)
        }
        Lookup(key) -> {
            print(map.get(data, key))
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
m = map.new()
map.put(m, "k", "v")
// LEAK: m is never freed
```

Fix: always pair allocation with `defer`:

```aether
m = map.new()
defer map.free(m)
```

**Deferring before the allocation succeeds:**

`defer` registers immediately. Place it right after the allocation, not before.

**Allocating inside a loop:**

`defer` fires at scope exit, not at end of each iteration. If you allocate inside a
loop, free explicitly at the end of each iteration instead:

```aether
while i < n {
    item = list.new()
    // ... use item ...
    list.free(item)
    i = i + 1
}
```

---

## Summary: When to Use What

| Situation | Approach |
|-----------|----------|
| Typical local allocation | `defer type.free(x)` right after allocation |
| Value returned from function | Caller defers the free |
| Value passed to an actor via `!` | Actor owns it; no defer in sender |
| Actor `state` initialized with `*.new()` | `@manual state ...` (auto mode) |
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

**Arena-per-actor for stdlib types**: Actor state variables using `map.new()` / `list.new()` would automatically allocate from the actor's arena. Actor death -> total cleanup, zero explicit `*.free()` needed anywhere in actor code.

Requires threading an allocator parameter through the stdlib C implementations.
