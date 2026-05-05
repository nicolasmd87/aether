# JSON Conformance Corpus

This directory holds the JSONTestSuite test cases used by
`make test-json-conformance` to verify `std/json/aether_json.c`
against RFC 8259.

## Contents

- `cases/` — 318 test files imported from
  [JSONTestSuite](https://github.com/nst/JSONTestSuite).
  The filename prefix encodes the expected outcome:
  - `y_*.json` — any conforming parser MUST accept.
  - `n_*.json` — any conforming parser MUST reject.
  - `i_*.json` — implementation-defined; parsers may accept or reject,
    and we record the outcome without gating CI on it.
- `LICENSE` — the MIT license from the JSONTestSuite upstream.
- `run_conformance.c` — our test runner. Iterates `cases/`, parses each
  file with `json_parse_raw_n`, and tabulates the outcome against the
  filename prefix.

## Running

```
make test-json-conformance
```

Exits non-zero if any `y_*` case fails to parse, or any `n_*` case
parses successfully. `i_*` outcomes are reported but do not fail CI.

## Maintenance

JSONTestSuite has been stable since 2017. Updates can be pulled by
rerunning the import step in `Makefile` (see `make bench-json-import-conformance`)
or by replacing `cases/` in bulk. No code in this repo changes upstream
test files — we just read them as data.

The runner itself is ~100 lines of pure C and needs no maintenance
beyond compiler-warning cleanups.
