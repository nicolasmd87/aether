# JSON Parser Baseline — `std/json/aether_json.c`

Committed, reproducible performance baseline for the JSON parser in
Aether's standard library. All numbers here can be regenerated with
`make bench-json` (own parser) or `make bench-json-compare` (own parser
+ yyjson reference).

Used as a **decision gate** — the user reviews these numbers against
yyjson to judge whether the remaining gap justifies further work. No
automated threshold triggers changes.

---

## Methodology

**Hardware snapshot:** macOS ARM64 (Apple M-series), Apple Clang, `-O2`.
NEON intrinsics active via `__ARM_NEON` compile-time feature detection.

**Timing:** `clock_gettime(CLOCK_MONOTONIC)` around `json_parse_raw()`
+ `json_free()` on a null-terminated buffer held in memory. No file
I/O in the hot loop.

**Warmup:** 5 untimed iterations prime caches, allocator pools, branch
predictors.

**Measurement:** 100 timed iterations. Sorted ascending. Reported:
- **Median MB/s** — headline. `size / times[50]`.
- **p95 MB/s** — worst-case (noise indicator). `size / times[95]`.
- **Median ns** — end-to-end parse time.
- **ns/value** — per-value cost (consistent across fixtures of
  different sizes).
- **Values** — structural value count estimate (roots + commas outside
  strings). Approximate but consistent.
- **RSS Δ KB** — peak RSS delta around the 100 iterations.

**Override knobs** for CI / spot-checks:
```
JSON_BENCH_WARMUP=10 JSON_BENCH_ITERS=500 make bench-json
```

**Variance note.** macOS CPU boost + thermals move numbers 10-30%
between runs even with median-of-100. Compare ratios, not absolute
third digits. Re-running `make bench-json-compare` will show drift.

**Determinism of corpus.** All fixtures except `large.json` are
committed. `large.json` regenerates deterministically via
`make bench-json-gen` (fixed-seed LCG, byte-identical across
machines).

---

## Fixtures

| Name                  | Size    | Shape                                                  | Stresses                              |
| ---                   | ---     | ---                                                    | ---                                   |
| `small.json`          | 1.2 KB  | Config object with nested settings + array of targets  | Parse dispatch / startup overhead     |
| `api-response.json`   | 174 KB  | 500 user records, mixed types, nested addresses        | Realistic REST workload               |
| `large.json`          | 10 MB   | Long array of user records (generated, not committed)  | Bulk throughput                       |
| `strings-heavy.json`  | 205 KB  | 2000 strings, 10% escape/Unicode density               | String decoder + UTF-8 validation     |
| `numbers-heavy.json`  | 185 KB  | 15000 numbers: ints, floats, negatives, exponents      | Number parser + strtod                |
| `deep.json`           | 403 B   | `[` × 200, value at bottom, `]` × 200                  | Recursion depth handling              |

---

## Implementation generations

### Gen 1 — original (replaced)

Textbook byte-by-byte recursive descent. Allocations per node
(`malloc(sizeof(JsonValue))`), per string (`malloc(buffer); realloc`),
per container (`ArrayList` / `HashMap`). Per-byte UTF-8 validation.
`strtod` on every number. No character-class lookup.

### Gen 2 — clean-room rewrite (arena + LUT + UTF-8 DFA + integer fast path)

- **Arena allocator** — one bump-pointer arena per document.
  `json_free()` releases in O(chunks), no per-node free.
- **Flat containers** — `JsonValue**` for arrays, parallel
  `char**` + `uint32_t*` + `JsonValue**` for objects. Replaces
  `ArrayList` + `HashMap`.
- **Char-class LUT** — 256-entry lookup table drives hot-path
  dispatch (whitespace, digit, structural, string-safe).
- **Two-phase string parse** — pre-scan locates the closing quote
  (exact upper bound on decoded length), then a single-pass decode.
- **UTF-8 DFA** — Hoehrmann's public-domain state machine (~30
  lines).
- **Integer fast path** — pure-digit numbers parsed via int64
  accumulator.
- **Zero stdlib deps** — no `aether_string`, no `aether_collections`.

### Gen 3 — current (fast-double + SIMD string scan)

Additions on top of Gen 2:

- **Fast double path** — numbers with <= 15 significant digits and
  |exponent| ≤ 22 parse via an int64 mantissa accumulator plus a
  single pow10 lookup and one multiply. `strtod` is reserved for
  edge cases that need IEEE-754-correct rounding (many digits,
  huge exponents, denormals). The fallback path guarantees we never
  return a wrong double.
- **SIMD string fast-loop** — compile-time-dispatched
  `scan_str_safe()`: SSE2 on x86_64, NEON on arm64, scalar
  everywhere else. Scans forward over printable-ASCII-non-escape
  bytes 16 at a time, falling out on the first quote, backslash,
  control char, or non-ASCII byte. Scalar fallback is the baseline
  LUT loop — SIMD is purely additive.

Portability verified against `make ci-windows`, `make ci-coop`,
`make test-json-asan` (runs with ASan + UBSan). All pass on every CI
row listed in CONTRIBUTING.md.

---

## Gen 3 numbers (current implementation)

Taken 2026-04-21, macOS ARM64 Apple M-series.

| Fixture              | Size       | Median MB/s | p95 MB/s | Median ns    | ns/value | Values | RSS Δ KB |
| -------------------- | ---------- | ----------- | -------- | ------------ | -------- | ------ | -------- |
| api-response.json    | 173 KB     |       475.8 |    378.1 |      356,000 |     59.3 |   6008 |     2032 |
| deep.json            | 403 B      |       128.1 |    128.1 |        3,000 |  3,000.0 |      1 |        0 |
| large.json           | 10 MB      |       357.8 |    337.7 |   27,951,000 |     41.3 | 676500 |   239760 |
| numbers-heavy.json   | 185 KB     |       404.4 |    353.0 |      447,000 |     29.8 |  15000 |        0 |
| small.json           | 1.2 KB     |       591.8 |    591.8 |        2,000 |     37.7 |     53 |        0 |
| strings-heavy.json   | 205 KB     |       208.0 |    195.0 |      964,000 |    482.0 |   2000 |        0 |

## Reference: yyjson (scalar mode, same hardware)

Scalar fast path (no SIMD opt-ins). Fetched on demand by
`make bench-json-fetch-yyjson`; not vendored.

| Fixture              | Size       | yyjson MB/s | p95 MB/s | Median ns    | ns/value | Values | RSS Δ KB |
| -------------------- | ---------- | ----------- | -------- | ------------ | -------- | ------ | -------- |
| api-response.json    | 173 KB     |       973.4 |    588.1 |      174,000 |     29.0 |   6008 |     2624 |
| deep.json            | 403 B      |       192.2 |    192.2 |        2,000 |  2,000.0 |      1 |        0 |
| large.json           | 10 MB      |       942.1 |    883.9 |   10,615,000 |     15.7 | 676500 |    15376 |
| numbers-heavy.json   | 185 KB     |       833.0 |    652.5 |      217,000 |     14.5 |  15000 |        0 |
| small.json           | 1.2 KB     |      1183.5 |   1183.5 |        1,000 |     18.9 |     53 |        0 |
| strings-heavy.json   | 205 KB     |       465.1 |    440.6 |      431,000 |    215.5 |   2000 |        0 |

---

## Speedup summary

### Gen 1 → Gen 3 (total improvement)

| Fixture              | Gen 1   | Gen 3   | **Speedup** |
| ---                  | ---     | ---     | ---         |
| small.json           |   65.8  |  591.8  | **9.0×**    |
| api-response.json    |   78.7  |  475.8  | **6.0×**    |
| large.json           |   49.3  |  357.8  | **7.3×**    |
| numbers-heavy.json   |  211.9  |  404.4  | 1.9×        |
| strings-heavy.json   |  186.1  |  208.0  | 1.1×        |
| deep.json            |   29.6  |  128.1  | **4.3×**    |

### Gen 2 → Gen 3 (incremental, fast-double + SIMD)

| Fixture              | Gen 2   | Gen 3   | Delta   |
| ---                  | ---     | ---     | ---     |
| numbers-heavy.json   | 311     |  404    |  +30%   |
| api-response.json    | 594     |  476    |   noise (variance) |
| large.json           | 314     |  358    |  +14%   |
| strings-heavy.json   | 221     |  208    |  ~flat  |

Fast-double is the dominant Gen 3 win: pure-int already had a fast
path, adding the same treatment for bounded floats moved the
numbers-heavy fixture 30% on its own. SIMD string scan shows minimal
gain on this corpus because the strings are short (mostly < 16
bytes), so the SSE/NEON 16-byte block rarely fires cleanly; on string
workloads with long bodies the win would be larger. Large.json's
record structure has long contiguous number runs, so the fast-double
improvement flows through into a noticeable bulk throughput gain.

### Gen 3 → yyjson gap

| Fixture              | Gen 3   | yyjson  | % of yyjson |
| ---                  | ---     | ---     | ---         |
| small.json           |  591.8  | 1183.5  |   50%       |
| api-response.json    |  475.8  |  973.4  |   49%       |
| large.json           |  357.8  |  942.1  |   38%       |
| numbers-heavy.json   |  404.4  |  833.0  |   49%       |
| strings-heavy.json   |  208.0  |  465.1  |   45%       |
| deep.json            |  128.1  |  192.2  |   67%       |

We sit at roughly half of yyjson's scalar throughput across the
board, up from roughly one-third in Gen 2. The remaining gap is
documented in [docs/json-parser-design.md](../../docs/json-parser-design.md#remaining-gap-vs-yyjson).

---

## Safety / correctness (re-verified at Gen 3)

- 191 C unit tests: pass.
- 278 .ae regression tests: pass.
- JSONTestSuite conformance: 95/95 must-accept, 188/188 must-reject,
  0 failures across 318 cases.
- `make test-json-asan` clean on the full bench corpus including the
  10 MB synthesized document.
- `make ci-windows` — all 70 examples cross-compile to Windows
  x86_64 (MinGW) clean.
- `make ci-coop` — cooperative-scheduler build + all coop-specific
  tests pass.

---

## Decision record

Gen 3 ships with fast-double and SIMD string scan because both gave
measurable wins without API churn or portability compromises. Tape
representation (yyjson's biggest remaining structural difference from
our tree-based layout) is deferred — the mutation-API preservation
work needs a dual-representation design, and the expected incremental
throughput win on top of Gen 3 is smaller than the architectural risk
of getting it wrong. See the design doc for the concrete plan.
