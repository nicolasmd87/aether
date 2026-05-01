# Aeocha — BDD Test Framework for Aether

A describe/it/before_each/after_each spec framework using trailing blocks and closures.

Inspired by [Cuppa](https://cuppa.forgerock.org).

## Usage

```aether
import contrib.aeocha

main() {
    fw = aeocha.init()

    aeocha.describe(fw, "My feature") {
        aeocha.before_each() callback {
            // setup
        }

        aeocha.it("works") callback {
            aeocha.assert_eq(fw, 1 + 1, 2, "math")
        }

        aeocha.it("handles strings") callback {
            aeocha.assert_str_eq(fw, "hi", "hi", "string equality")
        }
    }

    aeocha.run_summary(fw)
}
```

Run with `ae run my_test.ae` or `aeb test`. Exit code is `0` if all
tests pass, `1` if anything failed (compatible with the `aeb`
program-test contract).

## Call shape

Aeocha follows Aether's `_ctx` auto-injection convention (see
[docs/closures-and-builder-dsl.md](../../docs/closures-and-builder-dsl.md)):

- The **top-level** `aeocha.describe(fw, "name") { ... }` passes `fw`
  explicitly because there's no enclosing trailing block to
  auto-inject from.
- **Nested** calls inside a describe's trailing block — `aeocha.it`,
  `aeocha.before_each`, `aeocha.after_each`, and nested `aeocha.describe`
  — omit the first arg. Aether auto-injects the surrounding suite as
  `_ctx`.
- **Inside `it()` test callbacks**, the callback runs outside any
  builder context, so assertions need `fw` passed explicitly:
  `aeocha.assert_eq(fw, actual, expected, msg)`.

## API

| Function | Purpose |
|----------|---------|
| `aeocha.init()` → `ptr` | Create the framework context (call once in `main`) |
| `aeocha.describe(fw, name) { … }` | Top-level grouping (also `aeocha.describe(name) { … }` when nested) |
| `aeocha.it(name) callback { … }` | Define a test case (inside a describe block) |
| `aeocha.before_each() callback { … }` | Run before each `it()` in the surrounding describe |
| `aeocha.after_each() callback { … }` | Run after each `it()` in the surrounding describe |
| `aeocha.assert_eq(fw, a, b, msg)` | Integer equality |
| `aeocha.assert_str_eq(fw, a, b, msg)` | String equality |
| `aeocha.assert_true(fw, cond, msg)` | Truthy check |
| `aeocha.assert_false(fw, cond, msg)` | Falsy check |
| `aeocha.assert_not_eq(fw, a, b, msg)` | Integer inequality |
| `aeocha.assert_gt(fw, a, b, msg)` | Greater than |
| `aeocha.assert_contains(fw, s, sub, msg)` | String contains |
| `aeocha.assert_null(fw, p, msg)` | Null pointer |
| `aeocha.assert_not_null(fw, p, msg)` | Non-null pointer |
| `aeocha.fail(fw, msg)` | Unconditional failure |
| `aeocha.run_summary(fw)` | Print results, `exit(1)` on failure |

## Output

```
My feature
  ✓ works
  ✗ broken
      FAIL: math — expected 2, got 3

  1 passing
  1 failing
```

## Reserved-keyword note

`after` is a reserved keyword in Aether (collides with the actor
`receive { … } after N -> { … }` timeout syntax). Use `after_each`.

## Files

- `module.ae` — The framework, `import contrib.aeocha`-able
- `example_self_test.ae` — Self-test (11 passing). Doubles as the
  worked example.
