# Aeocha — BDD Test Framework for Aether

A describe/it/before/after_each spec framework using trailing blocks and closures.

Inspired by [Cuppa](https://cuppa.forgerock.org).

## Usage

```aether
import std.list
import std.map

// Framework functions defined inline (module system is extern-only)
// See example_self_test.ae for the full pattern.

main() {
    fw = map_new()
    map_put(fw, "fw", fw)
    map_put(fw, "passed", ref(0))
    map_put(fw, "failed", ref(0))
    _f = map_get(fw, "failed")

    suite(fw) {
        describe("My feature") {
            it("works") callback { _chk(_f, 1 == 1, "math") }
        }
    }

    run_summary(fw)
}
```

## API

| Function | Purpose |
|----------|---------|
| `describe(name) { }` | Group tests |
| `it(name) callback { }` | Define a test case |
| `before() callback { }` | Run before each `it()` |
| `after_each() callback { }` | Run after each `it()` |
| `assert_eq(a, b, msg)` | Integer equality |
| `assert_str_eq(a, b, msg)` | String equality |
| `assert_true(cond, msg)` | Truthy check |
| `assert_false(cond, msg)` | Falsy check |
| `assert_not_eq(a, b, msg)` | Integer inequality |
| `assert_gt(a, b, msg)` | Greater than |
| `assert_contains(s, sub, msg)` | String contains |
| `assert_null(p, msg)` | Null pointer |
| `assert_not_null(p, msg)` | Non-null pointer |
| `run_summary(fw)` | Print results, exit(1) on failure |

## Output

```
My feature
  ✓ works
  ✗ broken
      FAIL: expected 2, got 3

  1 passing
  1 failing
```

## Note

`after` is a reserved keyword in Aether — use `after_each` instead.

## Files

- `module.ae` — The framework
- `example_self_test.ae` — Self-test (11 passing)
