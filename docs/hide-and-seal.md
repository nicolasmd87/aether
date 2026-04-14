# `hide` and `seal except` — scope-level capability denial

Aether 0.51.0 adds two scope-level directives that let a block decline to
see selected names from its enclosing lexical scopes. They are the
language-level expression of [Paul Hammant's "Principles of Containment"](
https://paulhammant.com/2016/12/14/principles-of-containment/) — capability
flow becomes explicit not just on the way *in* (via dependency injection)
but also on the way *out* (via name denial).

## The two forms

### `hide` — blacklist

```aether
{
    hide secret_token, db_handle
    // From here on, `secret_token` and `db_handle` are not in scope, even
    // though they're declared in an outer block. Reading them is a compile
    // error. Reassigning them is a compile error. Declaring a fresh
    // variable with the same name in this block is a compile error.
}
```

### `seal except` — whitelist

```aether
{
    seal except req, res, inventory, response_write, response_status
    // Every name from every outer scope is now invisible EXCEPT the five
    // listed here. The block can still create its own local variables
    // freely; it just can't reach out for ambient state.
}
```

`seal except` is the form you reach for when writing a request handler
or any block where you want a one-line audit of "what does this code
even see?". The whitelist is the dependency surface.

## Semantics

### Scope-level, not statement-level

The position of a `hide` directive within its block does not matter.
Either of these is valid and equivalent:

```aether
{
    fooStr = "abc"
    hide fooStr        // hide appears AFTER the declaration
    use_helper()
}
```

```aether
{
    hide fooStr        // hide appears FIRST
    fooStr = "abc"     // ERROR: cannot declare 'fooStr', it is hidden
}
```

The first compiles cleanly because `fooStr` was declared in the outer
scope (not in the inner block), and the inner block hides it. The second
fails because the inner block is now trying to *create* a local binding
called `fooStr`, which collides with the hidden outer name.

### Reads AND writes both blocked

`hide x` blocks `println(x)`, `x = 5`, `x += 1`, and `do_stuff(x)` —
every form of access. Half-hiding (read-only or write-only) would be a
footgun.

### Propagates to all nested blocks

If the outer block hides `x`, every nested block, closure, and
trailing-block lambda inside it also has `x` hidden. There is no way to
"un-hide" a name in a nested block — declaring a fresh variable with
the same name in a nested block is allowed, but that's a fresh binding
in a child scope, not a re-exposure of the parent's hidden binding.

### Does NOT reach through call boundaries

A visible function defined in an outer scope can still use the hidden
name via its own lexical chain:

```aether
log_secret() { println(secret_token) }   // sees secret_token

main() {
    secret_token = "abc"
    {
        hide secret_token
        log_secret()                      // OK — log_secret is visible,
                                          //      and reads secret_token
                                          //      via its OWN lexical scope
    }
}
```

This is deliberate. `hide` is about *your scope's name resolution*, not
about *information flow* through your callees. If you wanted to deny
the capability transitively, you needed an effect type system; Aether
doesn't claim to have one.

The same caveat applies to closures captured into local variables:

```aether
{
    secret = 42
    incr = || { secret = secret + 1 }     // closure captures `secret`
    {
        hide secret
        incr()                             // legal — incr was already
                                           // captured before the hide
    }
}
```

If you don't want the closure's mutation to land, don't expose the
closure into the hiding scope.

### Local bindings are always visible

`hide` only affects lookups that walk OUT of the current block into a
parent scope. A local binding declared inside the hiding block is
always visible inside that block, regardless of what's hidden:

```aether
{
    seal except println
    local_thing = "fresh"
    println(local_thing)    // OK — local_thing is local, not from outside
}
```

## What `hide` is NOT

- **Not an effect system.** It cannot prevent a function you call from
  reaching its own ambient state. It can only stop *your scope* from
  naming things directly.
- **Not a security boundary.** It's compile-time hygiene. A determined
  caller can route around it by exposing a closure or accessor function.
  It catches *accidents* and *enforces intent*, not malice.
- **Not a privacy modifier.** Java `private` means "this field is not
  part of my class's API". `hide` means "this scope declines to see this
  name". Different layer.

## What `hide` IS for

1. **Containment of ambient authority in handlers.** When a request
   handler is buried 200 lines into a big function, it's easy to
   accidentally close over a sensitive variable. A one-line `hide` at
   the top of the handler block prevents that class of bug at compile
   time.
2. **Auditable dependency surfaces.** Reading a `seal except a, b, c`
   handler, you know in one glance the entire set of names it can
   reach. No more grepping outward for ambient state.
3. **Capability-style guarantees without buying into a capability
   language.** E and Joe-E achieve "only what you were handed" by
   *constructing* every reference through capability objects. `hide`
   achieves "only what you didn't decline" by *subtracting* from
   lexical scope. Cheaper, weaker, available today.
4. **The DI-flip-side of dependency injection.** DI delivers
   capabilities to a handler. Without `hide`, the handler can always
   reach around the injection and grab ambient state — the DI is
   convention. With `hide`/`seal except`, the handler is a closed
   container with a declared dependency surface, and the DI is
   enforced at the call site.

## Errors

Hide / seal violations all use the new error code:

```
E0304: 'secret' is hidden in this scope by `hide` or `seal except`
```

with a source line and column pointing at the offending use. Trying to
declare a hidden name produces the same code with the message
"cannot declare 'secret' — it is hidden in this scope by `hide`".

## Interaction with shadowing

You may NOT declare a variable in the same scope where its name is
hidden:

```aether
{
    hide x
    var x = 5     // E0304: cannot declare 'x' — it is hidden
}
```

You MAY declare it in a nested child block — that's a fresh binding in
the child's own scope, lexically unrelated to the hidden outer one:

```aether
{
    hide x
    {
        var x = 5     // OK — fresh binding in inner block
        println(x)    // refers to the inner binding
    }
}
```

This is consistent with normal lexical shadowing and means
`hide` cannot trap a programmer who wants to use a popular name like
`i` or `x` for unrelated purposes deeper in the call tree.

## Minimal reference card

| Form | Effect |
|---|---|
| `hide a, b, c` | Names `a`, `b`, `c` from outer scopes are not in this block (or any nested block within it). |
| `seal except a, b, c` | EVERY name from outer scopes is invisible in this block, except `a`, `b`, `c`. Local bindings inside the block are still visible. |
| `hide x` followed by `var x = …` in the same scope | Compile error — cannot redeclare a hidden name. |
| `hide x` then a nested block declaring `var x = …` | OK — fresh binding in the child scope. |
| `hide x` then calling a visible function that reads `x` from its own scope | OK — name resolution at the call site doesn't touch `x`. |
| `seal except printf, malloc` then trying to call `free` | Compile error — `free` is not in the whitelist. |

## Implementation note

`hide` and `seal except` are enforced entirely at compile time, in
`compiler/analysis/typechecker.c`'s `lookup_symbol()`. When a name is
not found in the current scope, the lookup checks the scope's
`hidden_names` list and `seal_whitelist` before walking to the parent.
Local bindings always win — the hide/seal sets only affect the
boundary crossing into outer scopes.

There is no runtime overhead. There is no codegen change. The feature
is a pure compile-time hygiene check.
