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

✅ **Items (1) and (2) shipped.** `import contrib.aeocha` now works
end-to-end; the self-test exercises the full surface. See the
README for the call shape and `example_self_test.ae` for a worked
example.

1. **Top-level module state.** ~~`_passed = ref(0)` / `_failed = ref(0)`
   / `_depth = ref(0)` at module scope aren't accepted by the
   import-expansion path.~~ **Fixed.** The framework now exposes
   `aeocha.init() -> ptr` returning a framework context (`fw`)
   that the caller threads through. No module-level mutable state.

2. **`after` is a reserved keyword.** ~~Currently both `after` and
   `after_each` exist as functions in `module.ae`; the bare `after`
   has to go.~~ **Fixed.** Bare `after` deleted; `after_each` is the
   survivor.

3. **Bare-call ergonomics — already supported by the language.** The
   compiler ships `import mod (*)` which exposes every public name
   from `mod` as a bare alias (see Language Reference: Glob Import).
   Earlier drafts of this TODO assumed Aether needed a new
   `unqualified` keyword — it doesn't, the existing parenthesised
   selection list with `(*)` does the same job. The aeocha-side gap
   that *isn't* covered by `(*)` is `fw` threading: even with bare
   names, callers still pass `fw` as the first arg to every matcher.
   Removing `fw` would need either a module-level "current framework"
   sentinel inside aeocha or a hidden first-param convention — both
   are aeocha-side refinements, not language work. Tracked here as
   item (3a) below if anyone wants to pick it up.

3a. **`fw` threading.** Optional refinement: replace the explicit
    `fw` parameter with a per-thread "current framework" the
    matchers read from a module-static cell. `init()` would set it,
    `run_summary()` would read it, and every `expect_*` / `assert_*`
    matcher would lose its `fw` parameter. Tradeoff: matcher
    signatures get cleaner but tests can't run two frameworks in
    parallel within one process. For aeocha's actual use case
    (one driver per `aeb test` invocation), the tradeoff is
    almost certainly worth it.

### Target shape (today, with `(*)` — works as of this writing)

```aether
import contrib.aeocha (*)
import std.list

main() {
    fw = init()
    describe(fw, "Counter") {
        before_each(fw) callback { reset() }

        it(fw, "starts at zero") callback {
            assert_eq(fw, count(), 0, "initial count")
        }

        it(fw, "increments") callback {
            increment()
            assert_eq(fw, count(), 1, "after one inc")
        }
    }
    run_summary(fw)
}
```

`fw` threading is what's left of the verbosity tax once the
`aeocha.` prefix is gone. Item (3a) above is what would peel
that layer too.

### Original shape (qualified import — also still supported)

```aether
import contrib.aeocha

main() {
    fw = aeocha.init()
    aeocha.describe(fw, "Counter") {
        aeocha.before_each(fw) callback { reset() }
        aeocha.it(fw, "starts at zero") callback {
            aeocha.assert_eq(fw, count(), 0, "initial count")
        }
        aeocha.it(fw, "increments") callback {
            increment()
            aeocha.assert_eq(fw, count(), 1, "after one inc")
        }
    }
    aeocha.run_summary(fw)
}
```

Qualified vs. glob is purely a per-file style choice — both are
importable, both are `aeb test`-able.

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
