# Aether syntax tests

Single-file `.ae` programs that exercise the compiler's syntactic
+ semantic surface. Each file is self-contained: `ae run <file>`
should exit zero on success, non-zero on failure.

## Scope

This directory is for tests that:

- exercise a parser/typechecker/codegen path that was just landed,
- demonstrate a syntax form should be accepted, OR
- use `assert`/`println` to verify a runtime outcome.

For tests that need a backing process (HTTP server, subprocess,
shell harness), use [`tests/integration/`](../integration/) — that
tree has the multi-file fixture pattern.

For tests anchored to a specific historical bug fix, use
[`tests/regression/`](../regression/) — same single-file shape,
different intent (see that directory's README).

## Conventions

- One feature per file. Filename starts with `test_` and describes
  the surface (`test_actor_timeout.ae`, `test_string_interp.ae`).
- Self-checking. Use `assert` (or fail-fast `if !ok { exit(1) }`)
  rather than expecting a human to read stdout.
- Keep them fast. Each test runs as part of `make test-ae`; large
  test counts × build-and-run on Windows CI is the slow leg.
- Skip-via-runtime is fine when the feature genuinely depends on a
  POSIX-only primitive (e.g. `os.run` is gated). Print a clear
  `SKIP` line and exit zero.

## Adding a new test

```
$ touch tests/syntax/test_<feature>.ae
$ # write a self-checking program
$ make test-ae    # auto-discovered; should pass
```

No Makefile changes needed — the per-file harness picks it up.
