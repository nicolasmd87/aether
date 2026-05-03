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

### Integration-shape matchers (`expect_*`)

Sit on top of the `assert_*` primitives above to absorb the bash-shaped "spawn / capture / awk-and-compare" idiom into a single Aether call. Use these when porting integration tests off shell scripts to pure Aether.

**Process matchers** consume the `(stdout, exit, err)` triple from `os.run_capture(prog, argv, env)`. The triple is destructured at the binding site (Aether tuples don't pass around as a single value) and the matcher takes the relevant slot directly. The captured-stdout slot is conventionally named `out` (not `stdout`) at call sites to avoid the Windows MinGW `<stdio.h>` macro clash — see the Known limitations section below.

| Function | Purpose |
|----------|---------|
| `aeocha.expect_exit(fw, exit_code, want, msg)` | Child exited with the expected status |
| `aeocha.expect_no_spawn_error(fw, err, msg)` | Fork/exec itself succeeded (binary found, not denied by sandbox) |
| `aeocha.expect_stdout_contains(fw, out, needle, msg)` | Substring of captured stdout |
| `aeocha.expect_stdout_line_field(fw, out, prefix, n, want, msg)` | Find first line starting with `prefix`, tokenise on runs of whitespace (spaces and tabs, awk-style), compare field index `n` (0-based) to `want`. Replaces `awk '/^Revision:/{print $2}'`-style assertions |
| `aeocha.expect_stdout_line_after(fw, out, prefix, want, msg)` | Find first line starting with `prefix`, take everything after the prefix (trimmed), compare to `want`. Replaces `sed -n 's/^Log: *//p'`-style captures of multi-token values |
| `aeocha.expect_stdout_line_count(fw, out, want, msg)` | Stdout has exactly `want` newline-separated lines (trailing-newline aware) |
| `aeocha.expect_stdout_matches(fw, out, pattern, msg)` | At least one stdout line matches a glob pattern (`*`, `?`, `[abc]` per `std.string.glob_match`) |

**HTTP matchers** consume the response handle returned by `http.get` / `http.post` / `client.send_request`. A single `resp: ptr` arg works for both v1 and v2 responses (same C struct underneath).

| Function | Purpose |
|----------|---------|
| `aeocha.expect_http_status(fw, resp, want, msg)` | Response has the expected status code |
| `aeocha.expect_http_no_error(fw, resp, msg)` | Transport succeeded (DNS, connect, TLS, timeout all OK). Distinct from "200 OK" — non-2xx is not a transport error |
| `aeocha.expect_http_body_contains(fw, resp, needle, msg)` | Substring of response body |
| `aeocha.expect_http_header(fw, resp, name, want, msg)` | Case-insensitive header equality |
| `aeocha.expect_http_body_json_field(fw, resp, key, want, msg)` | Top-level JSON-string-field equality (`"key":"want"` substring match — sufficient for status / probe / smoke endpoints; reach for `std.json` directly for nesting or non-string values) |

#### Worked example

```aether
import contrib.aeocha
import std.os
import std.list

main() {
    fw = aeocha.init()

    // Spawn `git rev-parse HEAD` and check the output.
    argv = list.new()
    list.add(argv, "rev-parse")
    list.add(argv, "HEAD")
    out, exit_code, err = os.run_capture("/usr/bin/git", argv, null)
    aeocha.expect_no_spawn_error(fw, err, "git found")
    aeocha.expect_exit(fw, exit_code, 0, "git rev-parse cleanly")
    aeocha.expect_stdout_matches(fw, out, "[0-9a-f]*", "looks like a sha")

    aeocha.run_summary(fw)
}
```

`os.run_capture` synthesises argv[0] from the prog string, so the list holds only the remaining args. See [docs/stdlib-api.md](../../docs/stdlib-api.md#process-spawn-argv-based-no-shell) for the full process-spawn surface.

#### Known limitations

- **Don't name a local variable `stdout` or `stderr` when calling these matchers on Windows MinGW.** `<stdio.h>` `#define`s `stdout` and `stderr` as preprocessor macros pointing at runtime stdio handles, so when Aether codegen lowers a local named `stdout` to C, MinGW rewrites it mid-declaration and the build fails. The matchers themselves use `out` for the param to dodge the clash; conventional call-site naming follows suit (`out, exit_code, err = os.run_capture(...)`). Linux GCC and Apple Clang allow the literal name through, but the consistent `out` convention is portable. This is a Windows quirk only — the underlying `os.run_capture` works fine on every platform.
- **`expect_stderr_contains` / `expect_stderr_empty` are not yet shipped.** `os.run_capture`'s third return slot is the spawn-error string, not the child's captured stderr — fd 2 from the child currently passes through to the parent's stderr unchanged. Adding child-stderr capture is a stdlib-side change tracked separately. Until it lands, callers that need to assert on stderr can wrap the child in `/bin/sh -c 'cmd 2>&1'` and use `expect_stdout_contains` on the merged stream.
- **`expect_stdout_line_field` tokenises on runs of ASCII space + tab (awk's default).** Other separators (commas, pipes, fixed-width column boundaries that aren't whitespace) aren't handled — fall back to `assert_str_eq` with a hand-derived value for those, or use `expect_stdout_line_after` if the value of interest is "everything after the prefix."
- **`expect_stdout_matches` uses glob patterns, not regex.** `std.string.glob_match` (POSIX fnmatch: `*`, `?`, `[abc]`) is what's available in stdlib today; PCRE-style regex is a separate ask.
- **`expect_http_body_json_field` does compact-JSON substring matching**, not real JSON parsing. Doesn't tolerate pretty-printed whitespace between `:` and the value. For nested keys or non-string values, use `std.json` directly and call `assert_*` on the parsed result.

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
  worked example for the `assert_*` primitives.
- See `tests/integration/aeocha_expect_matchers/probe.ae` for an
  end-to-end run of the integration-shape matchers (process
  spawn + VCR-replayed HTTP).
