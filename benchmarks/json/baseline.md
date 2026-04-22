# JSON Parser Bench — `std/json/aether_json.c`

Reproducible benchmark harness for the JSON parser in Aether's standard
library. This document describes **how** to measure the parser, not the
absolute throughput figures — those belong in your own run of
`make bench-json`, on your own hardware, against your own corpus.

Running the harness at head:

```
make bench-json           # parser under test, own corpus
make bench-json-compare   # parser under test + yyjson reference (fetched on demand)
```

---

## Methodology

**Timing.** `clock_gettime(CLOCK_MONOTONIC)` bracketing a `json_parse_raw()`
+ `json_free()` cycle over a null-terminated buffer held in memory. No
file I/O inside the hot loop — each fixture is read once before the
measurement window opens.

**Warmup.** 5 untimed iterations prime caches, allocator pools, and
branch predictors so the first measurement isn't biased by one-shot
costs.

**Measurement window.** 100 timed iterations. Sorted ascending. Reported:

- **Median MB/s** — `size / times[50]`. The headline.
- **p95 MB/s** — `size / times[95]`. Flags tail noise; large gap between
  median and p95 means thermals or the allocator are interfering.
- **Median ns** — end-to-end parse time at the median sample.
- **ns/value** — per-structural-value cost; comparable across fixtures
  of different sizes.
- **Values** — structural-value count estimate (roots + commas outside
  strings). Approximate but stable across runs of the same fixture.
- **RSS Δ KB** — peak RSS delta across the 100 iterations; catches leaks
  or runaway growth.

**Override knobs.** For CI and spot-checks:

```
JSON_BENCH_WARMUP=10 JSON_BENCH_ITERS=500 make bench-json
```

**Variance.** On laptop-class hardware (macOS thermals, Linux DVFS,
Windows background services), numbers move 10–30% between runs even
with median-of-100. Use ratios between fixtures on the same run as your
signal; don't compare the third significant digit of a median across
runs.

**Corpus determinism.** All fixtures except `large.json` are committed
verbatim. `large.json` regenerates via `make bench-json-gen` (fixed-seed
LCG), so every machine sees the exact same bytes for that fixture.

---

## Fixtures

| Name                  | Size    | Shape                                                  | What it exercises                     |
| ---                   | ---     | ---                                                    | ---                                   |
| `small.json`          | 1.2 KB  | Config object with nested settings + array of targets  | Parse dispatch / startup overhead     |
| `api-response.json`   | 174 KB  | 500 user records, mixed types, nested addresses        | Realistic REST workload               |
| `large.json`          | 10 MB   | Long array of user records (generated, not committed)  | Bulk throughput                       |
| `strings-heavy.json`  | 205 KB  | 2000 strings, 10% escape/Unicode density               | String decoder + UTF-8 validation     |
| `numbers-heavy.json`  | 185 KB  | 15000 numbers: ints, floats, negatives, exponents      | Number parser + strtod fallback       |
| `deep.json`           | 403 B   | `[` × 200, value at bottom, `]` × 200                  | Recursion depth handling              |

Each fixture isolates one part of the parser. A regression on
`numbers-heavy.json` with `strings-heavy.json` unchanged points at the
number path, not shared infrastructure.

---

## Reference implementation

`make bench-json-compare` fetches [yyjson](https://github.com/ibireme/yyjson)
on demand and runs it against the same corpus under the same harness.
yyjson is **not vendored** — the fetch step downloads a pinned tag and
deletes it on `make clean`. Use it to sanity-check the harness itself
(same fixture, same timing code, two parsers) rather than as a fixed
performance gate.

---

## Safety / correctness

These checks gate CI; every one is re-run on every PR. Numbers are
pass/fail, not throughput:

- **C unit tests** — [`make test`](../../Makefile).
- **Aether regression tests** — [`make test-ae`](../../Makefile).
- **JSONTestSuite conformance** — [`make test-json-conformance`](../../Makefile).
  95/95 must-accept and 188/188 must-reject cases pass; 35
  implementation-defined cases are recorded without gating.
- **ASan + UBSan** — [`make test-json-asan`](../../Makefile). Parser
  under sanitizer across the full bench corpus including `large.json`.
- **Windows cross-compile** — [`make ci-windows`](../../Makefile). MinGW
  cross-build of every example.
- **Cooperative scheduler** — [`make ci-coop`](../../Makefile). No
  threading assumptions anywhere in the parser.

---

## What we don't commit here

Absolute MB/s figures are **not** committed to this file. Any number
measured on one CI runner rots the moment another runner sees different
hardware, or the Aether compiler output changes, or Apple/Intel/AMD
ships a new uarch. If you want to know how the parser performs on your
box, run `make bench-json` on your box. The shape of the numbers — which
fixtures are fastest, where the tail is wider than the median, which
path is allocation-bound vs. compute-bound — travels across machines;
the absolute numbers don't.

Design rationale for the fast paths (arena, LUT, UTF-8 DFA, fast-double,
SIMD string scan) lives in
[docs/json-parser-design.md](../../docs/json-parser-design.md). Changes
that alter the shape of the numbers get their "why" written there.
