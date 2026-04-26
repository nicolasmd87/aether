# Compiler / runtime notes accumulated during the C → Aether VCR port

Items here surfaced while porting `std/http/server/vcr/aether_vcr.c` to
Aether. None block the port — every issue had a workaround — but each
one cost time, so they're worth fixing in the compiler/runtime when
their owners get to them.

## Resolution status (re-triaged)

| # | Item | Status |
|---|------|--------|
| 1 | String interpolation recycles backing buffer | **No longer reproduces.** A simple `out = "${out}${chunk}"` accumulator emits the expected concatenated output. Either the underlying interpolation lowering changed since this note was written, or the original repro depended on a more complex shape. Closed. |
| 2 | Variable scope leak from `if`/`else` branches | **Fixed.** Codegen now hoists variables that are first-assigned in BOTH arms of an `if`/`else` to the enclosing scope, so the post-block code can read them. Sibling-block scope isolation (the existing scope-restore behavior) is preserved — only common names get hoisted. Regression test: `tests/regression/test_if_else_branch_scope.ae` (4 cases including the sibling-isolation regression guard). |
| 3 | Actor state-type inference fragile under import-order changes | **Open.** Hard to reproduce deterministically — needs the original import sequence to surface. Left as-is. |
| 4 | FFI naming collision between extern and same-named wrapper | **Filed as issue #234** (`@extern("c_symbol")` annotation). Not in this batch. |
| 5 | Tuple return-type annotation `-> (int, string)` | **Open.** The parser falls through to expression-body parsing on `(`. Adding tuple return-type parsing is non-trivial (parser grammar + type-system propagation through the existing TYPE_TUPLE machinery). The bare `-> {` form already works for multi-return, so this is documentation/clarity, not blocker. Workaround: omit the annotation. |
| 6 | Stdlib archive doesn't rebuild on `.ae` changes | **Open.** Build-system polish. The dev-loop friction is real but the workaround (`make all`) is fast. |
| 7 | Per-symbol import hides namespace | **Closed as not-a-bug.** The selective-import filter is intentional — `import std.fs (file_exists)` deliberately blocks `fs.<other>` to enforce the user's allow list. New glob form `import std.fs (*)` (issue #171 P1) removes the blocker for callers who want both a short alias *and* the namespace: glob registers short aliases for every public name and does not engage the selective filter. Documented under "Glob Import" in `docs/language-reference.md`. |

The two currently-actionable items (#3 and #5) have clear workarounds
documented below. The notes are kept verbatim for the historical
record.

## 1. String interpolation recycles its backing buffer

**Symptom**: `out = "${out}${chunk}"` in a loop produces corrupted output.
First iteration is fine, second iteration's interpolation reuses the same
heap buffer for `out`, overwriting earlier content. Looks like UAF / shared
mutable buffer.

**Repro shape**:
```ae
out = ""
i = 0
while i < n {
    out = "${out}${some_string}"  // BAD: out's buffer aliases on each iter
    i = i + 1
}
```

**Workaround**: use `string_concat` directly. Returns a fresh
AetherString every call.
```ae
out = string.copy("")
while i < n {
    out = string_concat(out, some_string)  // OK: each call allocates fresh
    i = i + 1
}
```

**Why it matters**: every accumulator pattern in Aether (markdown
emitter, JSON builder, log formatter) hits this. The interpolation
form reads more naturally; right now it's a footgun in any code that
builds a string by concatenation in a loop.

**Fix direction**: interpolation codegen should call `string_concat`
internally rather than reusing the LHS buffer. Alternatively, the
`${...}` machinery should always allocate fresh and never alias.

---

## 2. Variable scope leak from `if` branches

**Symptom**: assigning the same variable in both arms of an `if` /
`else` doesn't make it visible after the `if`.

**Repro**:
```ae
if cond {
    heading = "A"
} else {
    heading = "B"
}
out = heading  // E0300: Undefined variable 'heading'
```

**Workaround**: declare the variable before the `if` with a default,
then reassign:
```ae
heading = "B"  // default
if cond {
    heading = "A"
}
out = heading  // works
```

**Why it matters**: every match-style branch dispatch hits this. The
fix changes idiomatic Aether — currently you can't write the
"branches assign, post-block uses" pattern that's standard in most
languages.

**Fix direction**: typechecker should union the bindings from both
branches when the `if` has an `else` and the variable is assigned in
both. (When there's no `else` or only one branch assigns, current
behavior is correct.)

---

## 3. Actor state-type inference fragile under import-order changes [RESOLVED 2026-04-26 in PR #238]

**Symptom**: this pattern works in one test file and fails in another
with seemingly identical code:
```ae
actor VCRActor {
    state s = 0
    receive { StartVCR(raw) -> { s = raw; ... } }   // E0200: type mismatch
}
```

The error is "Type mismatch in variable initialization" — the
typechecker inferred `s: int` from `= 0` and then objected when `raw`
(a `ptr`) is assigned.

**Trigger**: surfaced after I added `import std.fs` to a test that
already had `import std.http`, `import std.string`, etc. Removing
the actor wrapper and inlining the message-handler body into `main`
made the error go away.

**Workaround**: the explicit `state s: ptr = null` form failed too
(separate type-mismatch). Inlining was the only path forward. Or:
keep a working test file structure and don't add new top-level
imports without verifying actors still compile.

**Why it matters**: actors are first-class. Their state inference
should be stable regardless of import order or surrounding function
context.

**Fix direction**: actor state should accept an explicit type
annotation (`state s: ptr = null`) without complaining, AND the
inference should look at all writes to the state, not just the
initializer.

---

## 4. FFI naming collision between extern and same-named wrapper

**Symptom**: an Aether wrapper function named `foo` in module `bar`
ends up colliding with the C extern `bar_foo` it forwards to. Calls
to `bar.foo()` from user code resolve to the int-returning extern
instead of the string-returning wrapper, producing comparison-against-
empty-string warnings and wrong behavior.

**Repro**: I hit this three times during the VCR port —
`vcr_note` / `vcr.note`, `vcr_static_content` / `vcr.static_content`,
`vcr_clear_redactions` / `vcr.clear_redactions`.

**Workaround**: rename the C function to `vcr_add_*` (not the same
verb as the Aether wrapper). Cosmetic, but it's the only thing that
works today.

**Why it matters**: there's no good naming convention left. C wants
`vcr_foo`, Aether wants `vcr.foo`, but they can't both be `foo`.

**Fix direction**: filed as issue #234 — `@extern("symbol")` annotation
to bind a language-level Aether function to a chosen C symbol,
without the name collision. Once that lands, every `extern foo_raw + thin
wrapper foo` pair in the stdlib shrinks to one line.

---

## 5. Tuple return-type annotation flaky for multi-return funcs [RESOLVED 2026-04-26 in PR #238]

The cross-module tuple destructure case from issue #240's JSON sugar
work compiles cleanly: `parsed, perr = client.response_body_json(resp)`,
`echoed_ct, sctterr = json.get_string(ct_node)`, etc. all work first
time without the `-> {` workaround.

**Symptom**: declaring an explicit `-> (int, string)` annotation on a
function and then destructuring its result fails:
```ae
get_status_and_body(url: string) -> (int, string) {
    return 200, "ok"
}
// caller:
s, b = get_status_and_body(url)  // E0200: not a tuple
```

**Workaround**: use the bare `-> {` form (no annotation). The
multi-return-and-destructure pattern works fine without the annotation.

**Why it matters**: docs would benefit from explicit return-type
annotations; right now they're silently broken on multi-return.

**Fix direction**: parser/typechecker should accept the `(T1, T2)`
return-type form and propagate that to call-site destructuring.

---

## 6. The pre-port stdlib archive doesn't rebuild on .ae changes

**Symptom**: edits to `std/http/server/vcr/module.ae` don't take effect
until `make all` is run — `./build/ae run somefile.ae` reads from the
precompiled `build/libaether.a` archive.

**Workaround**: `make all` after every stdlib edit.

**Why it matters**: tightens the dev loop.

**Fix direction**: the stdlib archive should track .ae timestamps in
its dependency graph so `make` rebuilds it when any module's source
changes.

---

## 7. Per-symbol import hides the namespace

**Symptom**: `import std.fs.file_exists` makes `file_exists()` callable
as a bare identifier, but breaks `fs.read` / `fs.write` access from
the same file — the `fs` namespace becomes invisible. Adding a
matching `import std.fs` afterwards doesn't help (per-symbol import
shadows the namespace either way).

**Repro**:
```ae
import std.fs.file_exists
import std.fs   // doesn't restore the namespace

main() {
    if file_exists("foo") == 1 { ... }   // OK
    contents, err = fs.read("bar")       // E0301: Undefined function 'fs.read'
}
```

**Workaround**: drop the per-symbol import; declare the extern by
hand alongside the namespace import:
```ae
import std.fs
extern file_exists(path: string) -> int   // bypasses the namespace shadow
```

**Why it matters**: the per-symbol import form is documented and used
elsewhere; the namespace shadow is non-obvious. Hit during the
climate-record-then-replay port to a committed-tape model.

**Fix direction**: the namespace and per-symbol forms should compose
— importing `std.fs.file_exists` should add `file_exists` to the
local scope without removing the `fs.*` namespace.

---

## Summary

These are all tractable. None block the C → Aether port. The two most
impactful for everyday Aether ergonomics are #1 (interpolation buffer)
and #2 (if-branch scope) — both touch every Aether program that does
any non-trivial string assembly or branch-based variable assignment.
