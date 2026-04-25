# Aeocha / TinyWeb Test TODO

## Blocking: Compiler trailing block codegen

The TinyWeb unit tests and integration tests are blocked by a compiler issue
where trailing blocks are silently dropped when many `_ctx: ptr` functions
are defined in the same file.

### Observed behavior
- `web_server(8080) { end_point(GET, "/hello") |...| { } }` works in a small file
- The same pattern produces `web_server(8080);` (no trailing block) when 6+ `_ctx` functions exist
- The generated C has no `_aether_ctx_push`/`_aether_ctx_pop` for these calls

### Reproduction
```
build/ae build tests/syntax/test_tinyweb_spec.ae -o build/test_tinyweb_spec
build/aetherc tests/syntax/test_tinyweb_spec.ae build/test_tinyweb_spec.c
grep "web_server\|_aether_ctx_push" build/test_tinyweb_spec.c
# Shows web_server(8080) without ctx_push — trailing block dropped
```

### Also blocking
- Deeply nested callback → function → trailing block → closure handler
  segfaults (callback variables lose scope in generated C)
- `after` is a reserved keyword — Aeocha uses `after_each` instead

## When fixed
- Migrate test_tinyweb_spec.ae to use the TinyWeb DSL directly
- Add integration tests (server in actor + HTTP client round-trips)
- Add Aeocha before/after hooks to TinyWeb tests

## Make Aeocha actually feel like Mocha / Jest / Cuppa

Today `contrib/aeocha/module.ae` is not importable — `import
contrib.aeocha` followed by `aeocha.describe(...)` produces ~117 parse
errors, because:

1. **Top-level module state.** `_passed = ref(0)` / `_failed = ref(0)`
   / `_depth = ref(0)` at module scope aren't accepted by the
   import-expansion path. The self-test works only because the
   framework gets *inlined* into `example_self_test.ae`, so the refs
   end up inside `main()`. Aeocha needs an `aeocha.init() -> ptr`
   that returns a context the caller threads through, not module
   globals — same shape as the `_ctx` builders elsewhere.

2. **`after` is a reserved keyword** (collides with the actor-receive
   `after N ->` timeout syntax). Currently both `after` and
   `after_each` exist as functions in `module.ae`; the bare `after`
   has to go. `after_each` is the survivor.

3. **Bare-call ergonomics need a language feature.** Even after (1)
   and (2) are fixed, the import gives you `aeocha.describe(...)` /
   `aeocha.it(...)` (mirrors `sqlite.open` / `http.get`), not the
   Mocha-feel bare `describe(...)` / `it(...)`. To get bare names a
   test framework genuinely benefits from, Aether needs an unqualified
   import variant — `import contrib.aeocha unqualified` or
   `use contrib.aeocha::*`. This is a compiler change (parser +
   module resolver + symbol table), not an aeocha-side fix; deserves
   its own design discussion before coding. Scope: probably a day's
   work end-to-end with tests.

### Target shape (after all three land)

```aether
import contrib.aeocha unqualified

main() {
    describe("Counter") {
        before { reset() }

        it("starts at zero") {
            assert_eq(count(), 0, "initial count")
        }

        it("increments") {
            increment()
            assert_eq(count(), 1, "after one inc")
        }
    }
}
```

No `aeocha.init()`, no `aeocha.run_summary()`, no `aeocha.` prefixes.
That's the bar.

### What this unblocks

Once Aeocha is importable + bare-callable, migrate the four existing
hand-rolled `exit(1)` test files to it:

- `contrib/tinyweb/test_integration.ae` (176 LOC)
- `contrib/tinyweb/test_inventory.ae` (316 LOC)
- `contrib/tinyweb/test_spec.ae` (317 LOC)
- `tests/integration/sqlite_roundtrip/probe.ae` (105 LOC)

Each gets `describe`/`it` grouping, proper pass/fail counts, and
shared before/after hooks instead of inlined per-test setup/teardown.
The tinyweb tests in particular currently abort on first failure
(`exit(1)`); under Aeocha they'd surface every regression in one run.
