# Closure Environment Lifetime Bugs

The closure/DSL feature shipped with four independent bugs in capture
handling and env lifetime, plus a chained typechecker hole that surfaced
once the first four were fixed. All five landed together in a single PR
because testing any one in isolation was blocked by the others.

Regression tests live at `tests/syntax/test_closure_*.ae`.

## The bugs

### 1. Captured `ptr` parameters typed `int`

A closure capturing a `ptr` parameter of its enclosing function got
`int <name>` in the env struct — pointer truncated on store, segfault on
deref. Capture-type resolution in `compiler/codegen/codegen_expr.c` walked
only top-level `AST_VARIABLE_DECLARATION` nodes across all functions and
returned the first match by name, so it never saw function parameters and
silently returned the type of any unrelated same-named variable elsewhere
in the program.

**Fix:** thread the enclosing function name through `discover_closures`
into each `ClosureInfo.parent_func`. Capture-type lookup now searches that
function's parameters (`AST_PATTERN_VARIABLE`) and its locals in nested
blocks first, only falling back to the program-wide scan when scope is
unknown.

### 2. Mutable captures miscompiled

`count = count + 1` inside a closure body emitted a shadowing local that
wrote to uninitialised stack — silent wrong answers, even in-scope. Two
collaborating problems: `is_local_var` treated any assignment target as a
fresh local, and the closure prologue unconditionally aliased each capture
to a read-only C local.

**Fix:** keep the alias pattern for read-only captures (zero cost), but
detect captures that are assigned to in the body and route their reads
and writes through `_env->name` directly. Scope analysis distinguishes
captures from fresh body-locals while honouring Python-style `x = expr`
shadowing: if the RHS does not read `x`, treat `x` as a fresh local.

**Semantics preserved:** closures capture by value (as documented in
`docs/closures-and-builder-dsl.md`). Mutations inside a closure mutate the
env's copy — which persists across calls — but are not visible to the
enclosing scope. Shared mutable state still requires ref cells.

### 3. Escaping-closure use-after-free

A closure variable `bump = || { ... }` pushed an unconditional
`free(bump.env)` onto the defer stack; at return the compiler emitted
every defer before returning, including the env-free of the closure being
returned. The caller received a closure whose env was already freed.

**Fix:** at return emission, walk the return expression to collect every
closure variable that appears (including `box_closure` wrappers) and
transitively any closure vars they capture. `emit_all_defers_protected`
skips the matching env-free defers and emits a
`/* deferred (suppressed: escapes via return) */` marker in their place.
Ownership transfers to the caller — matching the documented contract for
`box_closure`.

### 4. Closure return types hardcoded to `int`/`void`

The static `_closure_fn_N` wrapper was typed `int` (or `void`) regardless
of what the closure actually returned. Closures returning a string or
pointer either tripped `-Wint-conversion` and truncated the return, or
the caller cast through int and dereferenced a truncated pointer.

**Fix:** pick the return type from the returned expression's `node_type`.
For `return call(<captured_closure>)` chains the typechecker leaves the
inner call as `TYPE_INT`, so we resolve through the captured closure's
own body.

### 5. `call()` expression `node_type` stuck at `TYPE_INT`

The global `call` builtin is symbol-typed as `TYPE_INT`, so the
typechecker set `node_type=TYPE_INT` on every `call(x)` expression
regardless of what `x`'s closure actually returned. Downstream,
`print`/`println` picked `"%d"` for calls that really returned strings,
and `s = call(w)` declared `s` as `int` so later comparisons dereferenced
a truncated pointer.

**Fix:** a post-discovery pass walks the program and, for every
`call(<known_closure_var>)` expression, rewrites `node_type` to match the
closure's actual return type. It also back-propagates into variable
declarations whose initializer is such a call. `closure_var_map` seeding
is extended to inherit a closure id through `w = f()` when `f` ends
`return <closure_var>`, so chains like `w = build_pair(); call(w)`
resolve correctly.

## Code layout

Nearly all changes are in the codegen layer. The typechecker is unchanged.

| Concern | File |
|---------|------|
| Capture discovery, type resolution, return-type inference, `call()` node_type propagation | `compiler/codegen/codegen_expr.c` |
| Mutated-capture write path (routes through `_env->`) | `compiler/codegen/codegen_stmt.c` |
| Bug-3 return-defer protection | `compiler/codegen/codegen.c`, `codegen_stmt.c` |
| Small additions to `CodeGenerator` state | `compiler/codegen/codegen.h` |
| New helpers on the public header | `compiler/codegen/codegen_internal.h` |

## Known limitations

Bug 5's `call()` type propagation resolves patterns where the target
closure can be statically identified at codegen time:

- `x = || { ... }; call(x)` — direct literal assignment.
- `x = f(); call(x)` where `f` ends `return <closure_var>`.

Patterns that still fall through to the `int` default: closures retrieved
dynamically from a list, closures chosen via `match`/`if`, closures
threaded through multiple intermediate function calls. A proper fix needs
either a parameterised closure type (`fn[T]`) or full return-type
propagation through the typechecker — out of scope here.

## Why the UI calculator works

`examples/calculator-tui.ae` uses ref cells for all mutable state and
builds handlers as anonymous inline closures passed to `box_closure(...)`.
Ref cells sidestep bug 2 (the closure captures a pointer, read-only from
its perspective), and anonymous box-wrapped closures sidestep bug 3 (no
named variable, no defer-free insertion). It worked before these fixes
and still works.
