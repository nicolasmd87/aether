# Known limitation: closures in actor handlers can't mutate actor state

A closure declared inside an actor's receive arm body can:
- Capture and mutate **arm-local** variables (Route 1, tested by
  `test_closure_in_actor_handler.ae`).
- Be stored in an actor state field (tested by
  `test_closure_in_actor_state.ae`).

A closure declared inside an actor's receive arm body CANNOT currently:
- Mutate actor state fields.

The failing pattern:

```aether
actor Counter {
    state count = 0

    receive {
        Go() -> {
            inc = || { count = count + 1 }  // writes actor state
            call(inc)
            call(inc)
        }
    }
}
```

Generated code for the closure body is:

```c
static void _closure_fn_0(_closure_env_0* _env) {
    int count = (count + 1);  // wrong — reads uninitialised local
}
```

Root cause: the closure doesn't know about `self` (the actor pointer).
Actor state fields are accessed as `self->field` in generated C, but
closures are hoisted to static functions that don't take `self`.
Properly handling this requires threading `self` through the closure's
env — a real feature, not a quick fix.

Workaround: copy state into an arm-local first, mutate that, then
write back at end of handler:

```aether
actor Counter {
    state count = 0

    receive {
        Go() -> {
            local = count
            inc = || { local = local + 1 }  // Route 1 promotes local
            call(inc)
            call(inc)
            count = local
        }
    }
}
```

Tracked for a future PR. No silent-wrong-answer protection yet — the
compiler doesn't reject the broken pattern. File an issue if you hit
this in real code.
