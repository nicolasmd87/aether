# Aeocha — BDD Test Framework for Aether

A Mocha/Cuppa-style spec framework using Aether's trailing blocks and closures.

Named after **Ae**ther + M**ocha**. Inspired by [Cuppa](https://cuppa.forgerock.org)
(the Java BDD framework used by [Tiny](https://github.com/phamm/tiny)'s test suite),
which itself melds ideas from Mocha, Jasmine, and RSpec.

## Side-by-Side

### Cuppa (Java, used by Tiny)

```java
@Test
public class MyTests {
    {
        describe("Given a widget", () -> {
            before(() -> { widget = new Widget(); });

            it("should have a name", () -> {
                assertThat(widget.getName(), equalTo("default"));
            });

            it("should be enabled", () -> {
                assertThat(widget.isEnabled(), is(true));
            });

            after(() -> { widget.close(); });
        });
    }
}
```

### Aeocha (Aether)

```aether
import contrib.aeocha

main() {
    describe("Given a widget") {
        before() callback { widget = create_widget() }

        it("should have a name") callback {
            assert_str_eq(widget_name(widget), "default", "name")
        }

        it("should be enabled") callback {
            assert_true(widget_enabled(widget), "enabled")
        }

        after() callback { close_widget(widget) }
    }

    run_summary()
}
```

## API

### Structure

| Cuppa (Java)                | Aeocha (Aether)                    |
|-----------------------------|------------------------------------|
| `describe("x", () -> {})` | `describe("x") { }`             |
| `it("x", () -> {})`       | `it("x") callback { }`          |
| `before(() -> {})`         | `before() callback { }`         |
| `after(() -> {})`          | `after() callback { }`          |
| `beforeEach(() -> {})`     | `before_each() callback { }`    |
| `afterEach(() -> {})`      | `after_each() callback { }`     |

### Assertions

| Hamcrest (Java)                       | Aeocha (Aether)                        |
|---------------------------------------|----------------------------------------|
| `assertThat(x, equalTo(1))`         | `assert_eq(x, 1, "msg")`             |
| `assertThat(s, equalTo("hi"))`      | `assert_str_eq(s, "hi", "msg")`      |
| `assertThat(x, is(true))`           | `assert_true(x, "msg")`              |
| `assertThat(x, is(false))`          | `assert_false(x, "msg")`             |
| `assertThat(x, not(equalTo(1)))`    | `assert_not_eq(x, 1, "msg")`         |
| `assertThat(x, greaterThan(5))`     | `assert_gt(x, 5, "msg")`             |
| `assertThat(s, containsString("x"))` | `assert_contains(s, "x", "msg")`     |
| `assertThat(x, equalTo(null))`      | `assert_null(x, "msg")`              |
| `assertThat(x, not(nullValue()))`   | `assert_not_null(x, "msg")`          |

### Runner

Call `run_summary()` at the end of `main()`. It prints the pass/fail summary
and calls `exit(1)` if any tests failed — compatible with `make test-ae`.

## Output

```
Given a widget
  ✓ should have a name
  ✓ should be enabled

  2 passing
```

On failure:
```
Given a widget
  ✓ should have a name
  ✗ should be enabled
      FAIL: enabled — expected true

  1 passing
  1 failing
```

## How It Works

- `describe()` creates a suite context map and returns it for `_ctx` injection
- `before()`/`after()` store closures via `callback` blocks into the suite
- `it()` runs before hooks, executes the test closure, then runs after hooks
- Assertions set global failure counters; `run_summary()` reports and exits
- Nesting works because Aether's builder context stack pushes/pops automatically

## Files

- `aeocha.ae` — The framework (describe, it, before, after, assertions, runner)
- `example_self_test.ae` — Self-test that validates the framework itself
