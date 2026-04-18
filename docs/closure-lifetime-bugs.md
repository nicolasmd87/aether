# Closure Environment Lifetime Bugs (2026-04-18)

## Status (2026-04-18, end of session)

All seven repros pass. Five fix commits on branch
`experiment/closure-env-uaf-repro`:

- `18bf092` — Type bug (captured `ptr` parameters typed `int`).
- `3b5b868` — Bug A (mutable captures miscompiled).
- `a7be4ec` — Bug B (escaping closures UAF).
- `7167919` — Closure return types now propagate; `_closure_fn_N`
  signatures reflect the returned expression instead of hardcoded
  `int`/`void`.
- `4fabb14` — `call(x)` expressions patched with the correct
  `node_type` so print/println format selection works for strings
  and pointers. Second-pass traversal after `closure_var_map` is
  fully seeded, including inherit-from-function-return
  (`w = build_pair()` where `build_pair` returns a closure var).

Repro results after fixes:

| # | Status before | Status after |
|---|---------------|--------------|
| 1 | `32765, 32765, 32765` | **`1, 2, 3` ✓** |
| 2 | segfault | **`hello, hello` ✓** |
| 3 | segfault | **`1, 2, 3` ✓** |
| 4 | `1, 32583, 32584` | **`1, 2, 3` ✓** |
| 5 | `15, 20, 15` | `15, 20, 15` ✓ (unchanged) |
| 6 | `1, 2, 3` | `1, 2, 3` ✓ (unchanged) |
| 7 | `1, 2, 3` | `1, 2, 3` ✓ (unchanged) |

`examples/calculator-tui.ae` still works. `make test-ae`: 174 passed,
3 failed — identical pre-change baseline, so no regressions introduced.

### Scope of the call-type propagation fix

The `call(x)` type-propagation only works for patterns that can be
statically resolved at closure discovery time:

- `x = || { ... }; call(x)` — direct literal assignment.
- `x = f(); call(x)` where `f`'s body ends `return <closure_var>` —
  the returned name must itself resolve to a closure at discovery
  time.

Patterns that would still fall through to the `int` default include:
closures stored in a list and retrieved dynamically, closures chosen
via `match`/`if`, and closures threaded through multiple
intermediate function calls before reaching the `call()` site. For
those the language needs a parameterized closure type like `fn[T]`
or the typechecker needs return-type propagation through arbitrary
call chains — separate, larger changes not attempted here.

Original investigation and analysis retained below.

---

Investigation into the closure implementation shipped with the closures/DSL
feature (PR for `dsl_with_scope`). Two independent bugs uncovered — one is a
miscompilation of *any* mutable capture, one is a use-after-free when a
closure escapes its creating scope.

The investigation was triggered by `contrib/aether_ui/example_calculator.ae`:
the calculator's button callbacks are closures that capture `num`, `prev`,
`op` from `main()`. The hypothesis was that the example only works because
`main()` blocks forever in `app_run()` — if it ever returned, the captured
stack frame would be freed while the UI callbacks still hold references.

**That hypothesis turned out to be only partially right.** The TUI
calculator at `examples/calculator-tui.ae` actually works correctly
end-to-end (verified: `2 + 3 = 5` via button callbacks) because it
uses ref cells for all mutable state and builds its button handlers
as anonymous inline closures passed directly to `box_closure(...)`.
That combination sidesteps both bugs. See *Empirical verification*
below for the detailed probe results — and for a fourth bug (pointer
truncation of captured `ptr` parameters) discovered during the probes.

## Repro set

Seven repros were written to narrow down where the closure lifetime model
breaks. Results below (from `ae run <file>`):

| # | Pattern | Expected | Got | Status |
|---|---------|----------|-----|--------|
| 1 | `make_counter()` returns counting closure | `1, 2, 3` | `32765, 32765, 32765` | **UAF + miscompile** |
| 2 | Outer closure captures inner closure, both escape | `hello, hello` | **segfault** + type warning | **UAF + type bug** |
| 3 | Calculator pattern minus `app_run` | `1, 2, 3` | **segfault** | **UAF** |
| 4 | In-scope mutating counter | `1, 2, 3` | `1, 32583, 32584` | **Miscompile** |
| 5 | In-scope read-only capture | `15, 20, 15` | `15, 20, 15` | Works |
| 6 | In-scope ref cell | `1, 2, 3` | `1, 2, 3` | Works |
| 7 | Escaping ref cell (no `ref_free`) | `1, 2, 3` | `1, 2, 3` | Works (leaks) |

Repros preserved at `/tmp/repro[1-7]_*.ae`.

## Bug A — mutable captures are miscompiled

**Symptom**: in-scope mutating closure (Repro 4) prints garbage from the
second call onward.

**Aether source**:
```aether
main() {
    count = 0
    inc = || { count = count + 1; return count }
    println(call(inc))  // expects 1
    println(call(inc))  // expects 2
}
```

**Generated C** (observed in the emitted `_ae_*.c` for Repro 4):
- Env struct contains no `count` field (just `int _dummy;`).
- Env pointer is `NULL` at closure construction.
- Closure body reads and writes a local `count` that is never connected
  back to `main`'s `count`.

Initial hypothesis was "codegen forgot to put `count` in the env struct."
That was wrong. Compiler exploration (see *Code locations*, below) shows:

- Capture identification (`compiler/codegen_expr.c:6–112`) *does* add
  `count` to the `captures` array — `collect_identifiers()` walks the
  body and picks up every non-local, non-param, non-builtin name,
  including assignment targets.
- Struct generation (`compiler/codegen_expr.c:176–186`) *does* emit
  `count` as a field of `_closure_env_N` and populates it at closure
  construction via `_e->count = count`.
- The bug is in the **closure function body prologue**
  (`compiler/codegen_expr.c:216–221`): it emits `int count = _env->count;`
  — a **read-only alias** shadowing the env field for the duration of
  the body.
- The statement codegen (`compiler/codegen_stmt.c:574–599`) then sees
  `count` as already-declared, takes the reassignment branch, and emits
  a plain `count = count + 1;` that writes to the stack-local alias.
  Nothing is ever written back to `_env->count`, so the next call reads
  the stale env value (or, in Repro 4's case, reads uninitialised stack
  because of an interaction between this alias and the caller's passing
  of an empty env).

Contrast with the read-only case (Repro 5): the alias is correct there
because no write-back is needed.

**Fix shape**: either
1. **rewrite assignment sites inside closure bodies** so `count = expr`
   becomes `_env->count = expr` when `count` is a capture (and drop the
   alias — or keep it read-only for non-mutated captures); or
2. **bind the alias as `int* count = &_env->count;`** and rewrite all
   uses in the body to `*count`. Closer to the reference-capture model
   other languages use.

Option 1 is less invasive — it only changes the prologue (drop the
alias for mutated captures) and the assignment-statement path (route
through env). Option 2 is more uniform but touches every read site too.

**Scope**: affects every closure that assigns to a captured variable. The
documented workaround (`ref()` cells) avoids the bug because the closure
captures the *pointer*, which is read-only from the closure's perspective —
and ref cells are read-only captures (Repro 6 works).

## Bug B — escaping closures use freed environments

**Symptom**: closures returned from the function that created them crash
(segfault) or read freed memory (silent garbage).

**Aether source** (Repro 3):
```aether
build_handler() {
    count = 0
    digit = |d: int| { count = count + d; return count }
    bump = || { return call(digit, 1) }
    return bump
}

main() {
    handler = build_handler()
    println(call(handler))   // segfault
}
```

**Generated C** (`build_handler`):
```c
_AeClosure build_handler() {
    int count = 0;
    _AeClosure digit = { _closure_fn_0, NULL };
    _AeClosure bump = ({
        _closure_env_1* _e = malloc(sizeof(_closure_env_1));
        _e->digit = digit;
        (_AeClosure){ _closure_fn_1, _e };
    });
    _AeClosure _builder_ret = bump;
    /* deferred */ free((void*)bump.env);     // <-- frees env we just returned
    return _builder_ret;
    /* deferred */ free((void*)bump.env);     // (unreachable dupe)
}
```

**What went wrong**: the auto-defer-free logic fires on every local closure,
including the one about to be returned. The return value carries a freed
`env` pointer. In Repro 1 the freed env happens to still hold plausible-looking
bytes; in Repros 2 and 3 the allocator has reused the memory by the time
`main` dereferences it, hence the segfault.

**Where in the compiler**: the defer is inserted unconditionally at closure
declaration (`compiler/codegen_stmt.c:850–867`) — it pushes a synthetic
`free((void*)<name>.env)` onto the defer stack for every closure variable
whose capture count is non-zero. At return time (`compiler/codegen_stmt.c:1477–1490`)
the compiler creates a temp named `_builder_ret` bound to the return
value, then calls `emit_all_defers()` (`compiler/codegen.c:362–372`),
then returns the temp. The return-emission site *has the return
expression in hand* — it already picks the temp name based on it — but
does not cross-reference it against the defer stack to suppress frees
that would kill the escaping value.

**Fix shape**: single-site. At return-emission time, before calling
`emit_all_defers()`, inspect the return expression. If it is (or
transitively contains) a closure variable whose env-free is on the defer
stack, pop or skip that specific defer. Two subtleties:
- A closure can be captured by another closure (Repro 2). When `wrapped`
  is returned, `inner`'s env is also still live and must be spared.
  Requires walking capture edges, not just the top-level return value.
- If the suppressed free never runs, the caller owns the env and must
  free it. Either document that (matches `box_closure`'s contract) or
  have `call()` / closure-drop paths free it — but that's a larger
  redesign and is what refcounting would give us.

**Repro 2 adds a secondary codegen bug** (separate from A and B):
`compiler/codegen_expr.c:135–166` — `lookup_var_c_type()` falls back to
`"int"` when it can't find a declaration for a captured name. A string
capture that isn't at top-level gets typed `int` in the env struct,
producing `-Wint-conversion` at the return site and a segfault on
dereference. Small, isolated fix: extend the lookup or propagate types
from the capture point instead of re-resolving by name.

## Bug C — nothing

Repro 7 confirms the ref-cell escape hatch works. `ref()` allocates on the
heap, the closure captures the pointer, and the pointer survives function
return. Without a matching `ref_free()` the cell leaks, but it does not
crash. This is the pattern the calculator example should switch to if it
ever stops relying on `main()` never returning.

## Are the bugs related?

Short answer: **no, they live in different subsystems and should be fixed
as independent PRs.**

- Bug A is in closure-function-body codegen: the prologue emits a
  read-only alias and the assignment path doesn't route through the env.
  `compiler/codegen_expr.c:216–221` + `compiler/codegen_stmt.c:574–599`.
- Bug B is in defer-lifetime management: the env-free is pushed onto
  the defer stack unconditionally, and the return-emission site doesn't
  reconcile it against the returned value. `compiler/codegen_stmt.c:850–867`
  + `compiler/codegen_stmt.c:1477–1490`.

The string-type bug is a third, smaller fix in `compiler/codegen_expr.c:135–166`.

## Code locations — summary table

| Concern | File:Line | Status |
|---------|-----------|--------|
| Capture enumeration | `codegen_expr.c:6–112` | Correct |
| Env struct field list + malloc + populate | `codegen_expr.c:176–186` | Correct |
| **Bug A — read-only alias in body prologue** | `codegen_expr.c:216–221` | **Broken** |
| **Bug A — assignment takes local-reassignment path** | `codegen_stmt.c:574–599` | **Broken** |
| String type fallback to `int` | `codegen_expr.c:135–166` | Broken (minor) |
| **Bug B — unconditional env-free defer** | `codegen_stmt.c:850–867` | **Broken** |
| Return-temp picks name (`_builder_ret`) | `codegen_stmt.c:1477–1490` | Has info, ignores it |
| Defer emission | `codegen.c:362–372` | Correct |

## Severity revisited

Bug A is more urgent than initially assessed. It fires even for in-scope
mutating closures (Repro 4), which means *any* user code of the form
`x = x + <expr>` inside `|| { ... }` silently produces wrong answers — no
escape or boxing required.

This reframes `contrib/aether_ui/example_calculator.ae`: it is not merely
a latent UAF waiting for `main()` to return. The `digit`, `apply_op`,
`equals`, and `clear` closures mutate captured `num`/`prev`/`op` on every
button press. Under Bug A, those writes land on per-invocation stack
aliases, not on `main`'s variables. That the calculator appears to work
at all needs re-verification — it may be that we never actually observed
multi-button arithmetic producing a correct result, or that the UI's
`ui_set(display, ...)` call happens to use values that are correct on
first press only.

**Before fixing anything, re-run the calculator and confirm whether
2 + 3 = 5 actually works.** If it doesn't, that's additional evidence
that no user-facing mutating-closure code works today.

## Impact on shipped code

- `contrib/aether_ui/example_calculator.ae` — mutating-closure state is
  almost certainly broken under Bug A. Needs empirical re-verification.
- Any user-defined `each`/`map`/`filter` with a mutating closure
  (mentioned in `docs/closures-and-builder-dsl.md` as a supported
  pattern) is subject to Bug A. Needs audit.
- Tests in the closures test suite pass because they either use
  read-only captures or stay in-scope with read-only use of counters.

## Empirical verification (2026-04-18, post-analysis)

Verified by running `examples/calculator-tui.ae` end-to-end and building
targeted probes.

**Calculator works.** `2 + 3 = 5` produced correctly both via the
direct-key path and the button-callback path (cursor navigation +
SPACE). Button callbacks exercise closure-captured-closure (`digit`
closure called from inside the `+` handler), `box_closure` round-trips,
and many invocations across an event loop. All correct.

**Why the calculator avoids both bugs:**
- All mutable state lives in ref cells (`num`, `prev`, `op` =
  `ref(0)`). Closures capture pointers, which from the closure body's
  perspective are read-only — so Bug A never fires.
- Handler closures are created inline as anonymous `|| { ... }`
  expressions passed directly to `box_closure(...)`. There is no
  intermediate `bump = || { ... }; return bump` shape — so the
  auto-defer-free insertion site (`codegen_stmt.c:850–867`) is not
  reached with the heap-escaping closure as its target.
- Even if it were, `main()` blocks in the event loop and never
  returns during normal use, so scope-exit defers don't fire against
  live handlers.

**Bug B probes.** Extracted grid construction into a `build_grid(...)`
function and tried four variations:

| Probe | Shape | Result |
|-------|-------|--------|
| `calc_probe_b.ae` | Returns `list` of `box_closure(\|\|{...})` (anonymous) | Works |
| `calc_probe_b2.ae` | `bump = \|\|{...}; return bump` (named, no box) | Segfault |
| `probe_anon.ae` | `return box_closure(\|\|{...})` (anon, single capture) | Segfault |
| `probe_anon_singlearg.ae` | Same as probe_anon, minimal | Segfault |

The segfaults are *not* Bug B. Reading the generated C shows the env
struct has `int n` for a `n: ptr` function parameter — the string-type
bug from Repro 2 generalised to any `ptr` parameter captured by an
inner closure. `_e->n = n` truncates a pointer to int; the closure body
then casts that back and dereferences into junk.

**Open question**: why does `build_grid` in `calc_probe_b.ae` type `num`
correctly as `void*` while `make_b` in `probe_anon.ae` types `n` as
`int`? Both are function parameters declared `ptr` and captured by an
inner closure. The only discernible difference is that `build_grid`
also has another closure (`digit`) in scope, but adding a `dummy`
closure to `make_b` doesn't fix it. The type-resolution path is
context-sensitive in a way that isn't yet understood — worth chasing
as part of the type-bug PR.

**Bug B itself remains unverified empirically.** Every probe that
*should* hit it instead hit the type bug first. Need a probe that
captures only variables that resolve correctly (e.g. an `int`
parameter by reference via a ref cell declared in the outer function,
where the ref-cell pointer is obtained before the closure is built)
to isolate the defer-free UAF from the type-truncation UAF.

## Plan

Sequence reordered after empirical verification. Three independent PRs:

1. **Type bug — captured `ptr` parameters typed as `int`.** Promoted
   from "small bonus fix" to first priority. It's the reason our Bug B
   probes can't be observed in isolation — every ref-cell-based
   escaping closure hits this first and crashes before the defer-free
   UAF can fire. Fix: extend `lookup_var_c_type()`
   (`codegen_expr.c:135–166`) to resolve function parameters, or
   propagate the type from the capture AST node at enumeration time.
   Regression tests: promote Repro 2; add a
   ref-cell-pointer-captured-by-inner-closure test; add the
   open-question case (why does `build_grid`'s `num` resolve correctly
   while `make_b`'s `n` doesn't?) to confirm the fix covers both.

2. **Bug A — mutable captures.** High urgency (silently wrong answers),
   but does not block the TUI calculator since ref cells sidestep it.
   Still blocks any user who writes the obvious `x = x + 1` pattern
   inside a closure. Drop the read-only alias for mutated captures in
   the closure prologue or promote it to `int* count = &_env->count;`
   with `*count` rewrites. Closure-body assignment codegen needs a case
   for "this name resolves to an env field" that emits
   `_env->name = expr` instead of `name = expr`. Regression tests:
   promote Repros 4, 5, 6 from `tests/closures/known-bugs/`; add cases
   for mixed read/write and multiple mutated captures.

3. **Bug B — escaping closures.** Sequenced last because we cannot
   write a clean regression test for it until the type bug is fixed.
   At return-emission time in `codegen_stmt.c:1477–1490`, before
   `emit_all_defers()`, walk the return expression and skip any defer
   whose target is (or is transitively captured by) the returned value.
   Regression tests: promote Repros 1, 3; add the named-closure
   returned-from-non-main pattern (`calc_probe_b2.ae` shape); add the
   nested-capture case (Repro 2's pattern, once type-safe).

Cross-cutting tasks:

- [x] Empirically verify the TUI calculator's arithmetic.
      **Verdict:** `examples/calculator-tui.ae` works correctly —
      `2 + 3 = 5` via button callbacks confirmed. It uses ref cells
      plus anonymous closures, which sidesteps all three bugs.
- [ ] Audit `contrib/aether_ui/example_calculator.ae` (the original
      trigger for this investigation) — it uses *named* mutating
      closures (`digit = |d: int| { num = num * 10 + d; ... }`) so it
      is a direct Bug A victim. Needs re-verification once Bug A is
      fixed; meanwhile consider rewriting to ref cells so it works on
      today's compiler.
- [ ] Add an `@xfail` mechanism to `make test-ae` as part of the first
      fix PR (whichever lands first), so broken repros can live in the
      main suite with inverted pass/fail semantics.
- [ ] Once all fixes land, delete `tests/closures/known-bugs/` and
      update `docs/closures-and-builder-dsl.md` to remove any
      "mutable captures broken" caveats.
