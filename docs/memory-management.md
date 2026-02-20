# Aether Memory Management

Aether's memory model is **deterministic scope-exit cleanup**, not garbage collection.

The guiding principle:
> **Allocations visible at call site. Cleanup automatic by default. Manual when needed.**

No hidden allocations. No GC pauses. No dangling pointers from forgotten `free()`.

---

## The Actual Model

Aether uses two allocation mechanisms:

| Layer | What | When |
|-------|------|------|
| **Actor arena** | Actor state, message queues | Freed when actor is destroyed |
| **Stdlib heap** | `map_new()`, `list_new()`, etc. | Freed at scope exit (auto) or explicitly (manual) |

There is **no garbage collector**. Docs that previously said "reference counting" or "GC" were incorrect.

---

## Stdlib Convention

All stdlib types follow one consistent naming pattern:

```
type_new()    → allocates on the heap, returns a pointer (must be freed)
type_free(t)  → frees the allocation
```

| Module | Constructor | Destructor |
|--------|-------------|------------|
| `std.map` | `map_new()` | `map_free(m)` |
| `std.list` | `list_new()` | `list_free(l)` |
| `std.string` | `string_new()` | `string_free(s)` |
| `std.fs` | `dir_list(path)` | `dir_list_free(l)` |

**Rule**: Any function whose name ends in `_new()` returns an allocated object. Its matching `_free()` is its destructor.

---

## Memory Modes

Aether gives you three levels of control.

### Auto Mode (default)

The compiler automatically injects `*_free()` calls at scope exit for any local variable assigned from a `*_new()` call.

```aether
import std.map

main() {
    m = map_new()          // allocated here
    map_put(m, "k", "v")
    print(map_get(m, "k"))
    print("\n")
    // map_free(m) ← injected automatically at end of scope
}
```

You never write `map_free`. The compiler does it.

### Manual Mode

Disable auto-free for an entire project in `aether.toml`:

```toml
[package]
name = "myapp"
version = "0.1.0"

[memory]
mode = "manual"    # default is "auto"
```

Or for a single compile/run:

```bash
ae run --no-auto-free file.ae
ae build --no-auto-free file.ae
```

In manual mode, you are fully responsible for every allocation:

```aether
import std.map

main() {
    m = map_new()
    map_put(m, "k", "v")
    print(map_get(m, "k"))
    print("\n")
    map_free(m)    // required in manual mode
}
```

### Per-Variable: `@manual`

Use `@manual` to skip auto-free for a **single variable** while keeping auto mode everywhere else.

This is required when a value **escapes its declaration scope**:

```aether
import std.list

// The list escapes via return — do NOT auto-free it here.
// The caller receives ownership and is responsible for freeing.
build_items(n) {
    @manual result = list_new()
    i = 0
    while i < n {
        list_add(result, i)
        i = i + 1
    }
    return result    // ownership transferred to caller
}

main() {
    items = build_items(10)   // auto mode: list_free(items) injected here
    print(list_size(items))
    print("\n")
}
```

**Rule of thumb**: Use `@manual` when you return a value, pass it to an actor via `!`, or store it somewhere that outlives the current block.

---

## Actor State

Actor `state` variables initialized with `*_new()` **must always be `@manual`**:

```aether
import std.map

message Store { key: string, value: string }
message Lookup { key: string }

actor Cache {
    @manual state data = map_new()   // lives for the actor's entire lifetime

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

Without `@manual`, auto mode would inject `map_free(data)` at the end of each message handler — destroying the map after the first message. `@manual` tells the compiler "I own this for the actor's lifetime."

The actor runtime frees the actor's arena (and its internal state) when the actor is shut down.

---

## Summary: When to Use What

| Situation | Mode |
|-----------|------|
| Typical local variable — used and discarded | **auto** (default, write nothing) |
| Value returned from function | `@manual` on the declaration |
| Value passed to an actor via `!` | `@manual` |
| Actor `state` initialized with `*_new()` | `@manual state ...` |
| Whole project: you want full control | `[memory] mode = "manual"` in aether.toml |
| Single file override | `ae run --no-auto-free file.ae` |

---

## Examples

See the following runnable examples:

- [examples/basics/memory_auto.ae](../examples/basics/memory_auto.ae) — auto mode, no explicit free
- [examples/basics/memory_manual.ae](../examples/basics/memory_manual.ae) — manual mode with explicit free
- [examples/basics/memory_escape.ae](../examples/basics/memory_escape.ae) — `@manual` for returned values
- [examples/actors/memory_actor.ae](../examples/actors/memory_actor.ae) — `@manual state` in an actor

---

## Future Work (post v0.5.0)

**Arena-per-actor for stdlib types**: Actor state variables using `map_new()` / `list_new()` would automatically allocate from the actor's arena. Actor death → total cleanup, zero explicit `*_free()` needed anywhere in actor code.

Requires threading an allocator parameter through the stdlib C implementations. Tracked in `next_steps.md`.
