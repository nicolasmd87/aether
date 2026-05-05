# Aether regression tests

Anchored tests that lock in fixes for specific historical bugs.
Each file's top-of-file comment names the bug — what failed, how
it failed, why it now passes — so the test stays meaningful when
revisited five releases from now.

## Difference from `tests/syntax/`

| | `tests/syntax/` | `tests/regression/` |
|---|---|---|
| Intent | "this syntax is supported" | "this specific bug must not come back" |
| Top-of-file comment | brief, describes the surface | required; describes the bug + fix shape |
| When added | when a feature lands | when a bug is fixed |
| When deleted | when the feature is removed | almost never (the test is the contract) |

## Adding a regression

When fixing a bug:

1. Reproduce in the smallest possible `.ae` program.
2. Save it as `tests/regression/test_<descriptive-name>.ae`.
3. Top-of-file comment must include:
   - what the bug was (one sentence),
   - why the test would have failed pre-fix (one sentence),
   - what part of the fix the test exercises (one sentence).
4. Run `make test-ae` to confirm it passes against the fix.

## Conventions

- Self-checking — `assert` or `exit(1)` on failure. The harness
  builds and runs each file; non-zero exit = fail.
- No person names in comments. Describe the bug shape and the
  use case (e.g. "polling-loop iteration", "actor handler that
  blocks") instead of attributing to who reported it.
- Don't delete a regression test when a feature is rewritten.
  The bug it locks in is independent of the surface.
