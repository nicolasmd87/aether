# `std.json` — Design Notes

This document explains *why* `aether_json.c` is shaped the way it is.
It's meant to be read alongside the source — not as a standalone spec,
but as a map to the design decisions so future maintainers know what
can be changed freely and what's load-bearing.

## Goals

1. **Correctness first.** Pass RFC 8259 conformance. Never UB, never OOB,
   never silent data loss. JSONTestSuite (318 tests) gates CI.
2. **Portable to every CI target.** Compiles clean on GCC, Clang, Apple
   Clang, MinGW (32+64-bit), Emscripten under `-Wall -Wextra -Werror
   -pedantic`. SIMD kernels gated behind compile-time feature detection
   (`__SSE2__`, `__ARM_NEON`) with a scalar fallback that's always
   compiled — no target is SIMD-only.
3. **Fast enough that JSON isn't a bottleneck.** Post-rewrite throughput
   is a multiple of the original byte-at-a-time parser, and a meaningful
   fraction of yyjson's scalar throughput on the same hardware. Measured
   ratios live in [`benchmarks/json/baseline.md`](../benchmarks/json/baseline.md).
4. **Low maintenance.** One `.c` file, one design doc, no vendored deps.
   Anyone who reads this file should be able to maintain the parser
   without additional context.

These are in priority order. Correctness outranks speed. Portability
outranks a fast-path optimisation that only works on one arch.

## Memory model: one arena per parsed document

Every parsed document lives in a bump-pointer arena allocated at
`json_parse_raw_n` time. Every `JsonValue`, every string payload, every
container backing array for that document comes from the same arena.
`json_free(root)` calls `arena_destroy` which walks the chunk list and
returns everything to `malloc`'s free list in a single pass.

This eliminates the **per-node malloc** pattern that dominated the
original parser's profile: every JsonValue, every string buffer, and
every container (via `ArrayList`/`HashMap` wrappers) was its own
allocation. On `large.json` (10 MB, 676k values), that's a million
allocator round-trips; on our arena path it's a handful of chunk
allocations.

### Chunks and growth

The arena is a singly-linked list of `ArenaChunk`s. First chunk is 16 KB
(big enough that tiny documents never grow), subsequent chunks double
in size up to a 2 MB cap. An allocation bigger than the current chunk's
remaining space triggers a new chunk sized at least as large as the
requested bytes — so a single 5 MB string doesn't force the allocator
to keep doubling until it exceeds the request.

Alignment is fixed at 8 bytes. All allocations round up.

### Why not a tape?

yyjson, simdjson, and several other fast parsers store parsed values in
a **tape**: a flat array of fixed-size slots indexed by position. This
is terrific for cache locality but would break Aether's JSON API because
the creation path (`json_create_string`, `json_object_set`, …) needs
values that exist *outside* of any parse. The tape representation
couples value lifetimes to a tape, making standalone creation awkward.

We chose to preserve the API shape — values are reachable through a tree
of pointers, same as before — and win cache locality instead from the
arena's chunk-local allocation. Values allocated close in time end up
close in memory. Not as good as a tape but close enough; the remaining
gap vs. yyjson is partly this choice, documented in `baseline.md`.

### Heap path for `json_create_*`

Values created standalone (`json_create_null`, `json_create_string`,
`json_create_array`, etc.) are heap-allocated individually via `malloc`.
Their children are similarly heap-allocated. `json_free` on such a
value walks the tree and frees each node.

When a standalone value is handed to `json_array_add_raw` or
`json_object_set_raw` on an arena-backed container, the mutator
**deep-copies the subtree into the parent's arena** and frees the
original. After insertion the entire tree is arena-owned; a single
`json_free` at the root tears everything down.

This double-path (arena for parse, heap for create, deep-copy on merge)
is the only real complexity cost of preserving the mutation API. The
parse path is a tight loop with no branching on allocation mode; the
mutation path is rare and pays for its complexity only when invoked.

## Character classification via a 256-entry LUT

`JSON_CC_RW[256]` holds per-byte flag bits:
`CC_WHITESPACE`, `CC_DIGIT`, `CC_STRUCTURAL`, `CC_STR_OK`, `CC_HEX`,
`CC_NUM_START`.

Hot-path decisions become a single indexed load + bit-test:

```c
while (s->p < s->end && (CC(*s->p) & CC_STR_OK)) p_advance(s);
```

The table is initialized once on first parse via `cc_init()` (the
static initializer handles the digit/hex/structural entries; runtime
code fills `CC_STR_OK` for the rest of printable ASCII). `cc_init` is
idempotent and guarded by a simple non-atomic flag — the race on first
parse is benign because both racing threads would compute the same
table contents.

### Why runtime init?

C89 static initializers can't include loops, and writing out every
byte's flags explicitly made the source very noisy. A one-time
initializer trades one branch on the first parse call for a much more
readable declaration. The check is predicted cold after the first parse.

## Fast string scanning

`parse_string_raw` is two-phase:

1. **Pre-scan** — walk forward from the opening `"`, skipping
   `\\<byte>` pairs verbatim, until the closing `"` is found.
   This gives us the exact span length. We allocate a decode buffer
   of `span + 1` bytes in the arena — tight upper bound because
   JSON escapes cannot *grow* their output (a 6-byte `\uXXXX` decodes
   to at most 3 UTF-8 bytes; a 12-byte surrogate pair to 4).
2. **Decode** — walk again, this time with the LUT fast loop:

   ```c
   while (s->p < s->end && (CC(*s->p) & CC_STR_OK)) {
       p_advance(s);  // tight loop, no branches except loop condition
   }
   memcpy(dst + di, run_start, run_len);
   ```

   When the fast loop exits we look at exactly one byte and dispatch:
   close-quote, escape, control char (reject), or non-ASCII (UTF-8 DFA).

### The critical bug the pre-scan prevents

The first rewrite didn't pre-scan. It allocated `max_len = s->end - s->p`
bytes per string as an upper bound on decoded length. That's fine for
one string in a small document, but for `large.json` with 676k strings
each allocating the remaining input size, the arena exploded to gigabytes
before the process was killed.

The pre-scan turns per-string allocation into `O(string bytes)` instead
of `O(remaining document bytes)`. That's what got us from OOM to
6× speedup.

## UTF-8 validation: Hoehrmann's DFA

Non-ASCII bytes in strings go through a ~30-line state machine from
[Bjoern Hoehrmann's 2010 paper](http://bjoern.hoehrmann.de/utf-8/decoder/dfa/).
Public domain, easy to verify against the reference, rejects every
malformed case RFC 3629 forbids: overlongs, surrogate halves,
codepoints > U+10FFFF, continuation bytes in lead position.

The DFA state `UTF8_ACCEPT = 0` starts a new codepoint,
`UTF8_REJECT = 12` latches an error, any other value means mid-sequence.
Per-byte cost: one table load + two arithmetic ops — much cheaper than
branching on each byte's bit pattern.

## SIMD string fast-loop

The string parser's inner loop skips over "safe" printable ASCII bytes
(>= 0x20, < 0x80, not `"`, not `\`). That loop is in the hot path of
every non-trivial JSON document and is embarrassingly parallel — each
byte is classified in isolation.

`scan_str_safe()` picks one implementation at **compile time**, never
at runtime:

- `__SSE2__` → SSE2 kernel (16-byte vectors). SSE2 is the AMD64
  baseline, so this lights up on every x86_64 CI row without any extra
  flags. Uses `_mm_cmpeq_epi8` for quote/backslash + a signed-compare
  trick (`_mm_cmplt_epi8(v, 0x20)`) that flags both control chars
  (0x00-0x1F) and non-ASCII (0x80-0xFF) in a single instruction, then
  `_mm_movemask_epi8` + `__builtin_ctz` for "offset of first unsafe
  byte."
- `__ARM_NEON && __aarch64__` → NEON kernel. Same classification as
  SSE2, different intrinsics. NEON lacks `movemask`, so we use the
  canonical `vshrn_n_u16` narrow-to-nibbles trick to produce a u64
  where each input byte contributes 4 bits — `__builtin_ctzll >> 2`
  gives the first-set-byte offset.
- fallback → a scalar loop over the CC_STR_OK lookup table. Byte-at-
  a-time but branch-predictor-friendly. Active on WASM, embedded, or
  any target that lacks both intrinsic headers.

All three produce byte-identical results — SIMD is purely additive
acceleration, never a semantic shift. The SIMD path always leaves a
scalar tail loop to clean up 0-15 bytes at the end. No runtime feature
detection = no per-call branch cost on the happy path.

Line/col accounting stays correct because safe bytes are by definition
>= 0x20, so no newline can ever appear inside a SIMD chunk: the string
loop bumps col by the run length without checking for '\n'.

Why not SIMD for UTF-8 validation or structural scan? Those either need
tape-based output (structural) or have significantly more state to
track (UTF-8 DFA state carries across bytes). The string fast-loop is
the one place SIMD drops in with no architectural follow-through. If
parse throughput ever shows up in a profile that matters more than
these ~50% of yyjson, the next step is tape representation, not more
SIMD.

## Number parsing: validate + dispatch

RFC 8259 numbers have a strict grammar:
`[ minus ] int [ frac ] [ exp ]` where `int` rejects leading zeros.
We walk the grammar explicitly **and accumulate all three fields in a
single pass** — sign, integer mantissa (int64), fractional mantissa
(int64) with digit count, signed exponent.

Three paths, chosen by which accumulators are in-range:

1. **Pure-integer path.** No `.`, no `e/E`, int accumulator hasn't
   overflowed. Return `(double)int_acc` (signed). No `strtod` call.
2. **Fast-double path.** Total significant digits ≤ 15 (double's
   fully-exact range) and |effective exponent| ≤ 22 (where
   `POW10_POS[n]` is exactly representable in a double). Fuse int and
   frac into one integer mantissa, multiply/divide by
   `POW10_POS[|exp|]` once. One multiply, one cast, one negate.
3. **Fallback.** Any of the above conditions fails — typically 16+
   significant digits, huge exponents, denormals, or overflow of the
   int64 mantissa. Hand the validated span off to `strtod` for
   correctly-rounded IEEE-754. Guarantees we never silently return a
   wrong double.

Paths 1 and 2 cover essentially every config file, API response, and
log record we've benchmarked. Path 3 is reserved for scientific data
sets, cryptographic constants, and similar edge-case corpora.

The `POW10_POS` table tops out at `1e22` because doubles can exactly
represent powers of ten only through that index; past 22 the `strtod`
fallback is the only correct answer.

## Containers: parallel flat arrays

`JsonValue::arr` is `{ JsonValue** items; uint32_t count, capacity }`.
`JsonValue::obj` is the same pattern with three parallel arrays:
`const char** keys; uint32_t* key_lens; JsonValue** values`.

Growth uses simple doubling (cap=4 initial) and copies via
`arena_grow`. Old buffers become arena garbage until the arena is
freed — waste is bounded by `O(final size)` per container.

### Object lookup: linear scan

`json_object_get_raw` does a byte-by-byte `memcmp` walk across all
keys. The typical object in API responses has <20 keys; linear scan
beats hashing at those sizes because branch prediction is perfect.
The `key_lens[]` array lets us reject mismatched lengths in a single
integer compare before touching `memcmp`.

For objects with hundreds of keys this becomes O(n) and pathological.
If that pattern ever shows up in profiles we'd switch to a sorted
keys representation and binary search, or a tiny open-addressed hash
for objects above some threshold. Not v1.

## Error reporting

All error paths funnel through `err_set(line, col, fmt, ...)` which
formats into a `_Thread_local` buffer. **First error wins**: once
`g_json_err_set` is true, subsequent calls no-op. This preserves the
innermost diagnostic, which is almost always the most specific.

Position accuracy is maintained by `p_advance(s)` — every cursor
motion goes through it, incrementing `line` on `\n` and `col`
otherwise. The fast string loop calls `p_advance` in every iteration
(not just on the escape branch), which costs a little throughput but
keeps error messages like "expected `:` at 3:17" correct.

## Depth limit

Hard cap at `JSON_MAX_DEPTH = 256`. Enforced on every container entry
via `parse_value_depth(depth + 1)`. Prevents both stack overflow on
pathological input and denial-of-service via deeply nested JSON
bombs (which DOS many naive parsers). No way to disable at runtime —
it's a bounded recursion guarantee, not a configurable limit.

## Thread safety

The parser itself is **reentrant and thread-safe** — there is no
mutable global state during parse. The two `_Thread_local` globals
(`g_json_err_buf`, `g_json_err_set`) and the `JSON_CC_RW` init flag
isolate per-thread parses from each other. Two threads parsing
concurrently hold independent arenas and see independent error slots.

`cc_init`'s first-run race is benign (both threads write the same bytes).

## Remaining gap vs. yyjson

Measured throughput in [baseline.md](../benchmarks/json/baseline.md)
shows we run at roughly half of yyjson's scalar speed. The gap is made
up of three pieces:

### 1. Tape representation (biggest remaining item — deferred)

yyjson stores parsed values in a flat 16-byte-slot array ("tape")
with parent/child relationships encoded via sibling offsets. Our
tree-based representation uses 48-byte `JsonValue` structs with
pointer fields. Same tree shape, but 3× the bytes per node — so it
fills the L2/L3 cache sooner, and pointer-chasing during
`json_object_get`/`json_array_get` dominates compared to an index
walk in contiguous memory.

Why we haven't shipped it: our `json_object_set` / `json_array_add`
mutation APIs let users add arbitrary children to existing values,
including heap-created values from `json_create_*` into parsed
arena-backed values. A pure tape layout cannot accept mutations
without a full rebuild, because "insert a child" would invalidate
every downstream sibling offset.

The proper design is **dual representation with copy-on-mutate**:
- `json.parse` produces a flat tape.
- `json.array_add` / `json.object_set` on a tape-backed value first
  promotes the subtree to the existing tree representation (one O(n)
  deep-copy per root), sets an "is-tree" flag, and then mutates.
- Subsequent calls on the promoted root skip the conversion.
- Accessors (`object_get`, `array_get`, `get_bool`, …) branch on the
  flag once to pick the right code path.

Estimated incremental throughput on parse: around +30-50% for
well-shaped documents (api-response, large), driven by cache
locality on the parse output. It does not make a difference for
tiny documents or deeply-nested structures. The architectural risk
is real: every accessor function gains a dispatch branch, the
mutation path has a new failure mode (copy allocation can OOM),
and every test needs to cover both flavours.

That's why it's deferred: the design is clear, the gain is
measurable but not dramatic, and ripping out the core data layout
needs its own review cycle. The change-point in "When to change
what" below names the files and touch points.

### 2. Full SIMD parsing (not on the roadmap)

simdjson and yyjson-with-SIMD use SIMD to classify every byte into
"structural / whitespace / string-char / escape" in a single vector
pass, producing a bitmap the parser walks. That requires tape output
(the bitmap is consumed while writing tape slots) and is an
architectural commitment to SIMD-everywhere. We ship a targeted SIMD
kernel (`scan_str_safe`) because it's surgical and has a clean scalar
fallback — going further than that trades maintainability for single-
digit-percent wins on top of tape.

### 3. Things we deliberately don't do

- **Streaming / incremental parsing** — everything is parsed into
  memory in one go. JSON payloads that don't fit in RAM are rare and
  streaming complicates both the arena and the error-reporting path.
- **UTF-16 / UTF-32 input** — RFC 8259 mandates UTF-8. We reject
  anything else at the byte layer.
- **Canonical JSON output / pretty-printing** — `json_stringify_raw`
  emits compact JSON. A pretty option is a trivial add if asked for.
- **Schema validation** — not a parser concern.
- **Runtime CPU feature detection** — compile-time dispatch only. If
  a binary is built for a target that has SSE2, it uses SSE2
  everywhere that target runs. No dispatch cost on the happy path.

## Testing surface

- **`tests/runtime/test_runtime_json.c`** — C-level unit tests,
  8 tests covering parse/create/stringify for each type.
- **`tests/regression/test_json_*.ae`** — Aether-level regression
  tests for edge cases (escapes, Unicode, RFC rejection, position
  errors, error tuples, edge-case wrappers).
- **`tests/conformance/json/`** — 318-file JSONTestSuite corpus.
  `make test-json-conformance` checks every file. Gates CI on
  `y_*` + `n_*` (283 cases); records `i_*` (35 cases) as
  implementation-defined.
- **`make test-json-asan`** — parses the bench corpus under
  `-fsanitize=address,undefined`. Catches leaks, OOB, UB.
- **`make test-json-valgrind`** — Valgrind run where available.
- **`make bench-json` / `make bench-json-compare`** — perf bench
  vs. committed baseline and optional yyjson reference.

## When to change what

| If you want to …                           | Change                                   |
| ---                                        | ---                                      |
| Land tape representation                   | New `TapeSlot` struct + `JV_FLAG_TAPE` flag on `JsonValue`; rewrite `parse_value_depth`/`parse_array`/`parse_object` to produce tape slots; add `tape_promote_to_tree()` helper called lazily from the mutators; branch in every accessor on `flags & JV_FLAG_TAPE`. |
| Add SIMD for structural scan (tape-only)   | New `scan_structurals()` beside `scan_str_safe`, called from the tape parser. Needs tape to land first. |
| Improve object lookup for large objects    | Threshold-based switch in `json_object_get_raw`: linear scan below ~32 keys, open-addressed hash above. |
| Support streaming                          | New `json_parse_stream` API + state object. Would likely need its own arena-resetting allocator. |
| Change max depth                           | `JSON_MAX_DEPTH` near top of file.        |
| Add a pretty-print mode                    | New flag in a `json_stringify_opts` struct, new builder path. |
| Tune arena chunk sizes                     | `ARENA_INITIAL_CHUNK`, `ARENA_MAX_AUTO_CHUNK`. |
| Track another error field                  | Extend `JSON_ERR_REASON_BUF`, update `err_set`. |
| Raise the fast-double digit cap            | `FAST_INT_SAFE_DIGITS`; only safe up to 15 for double-exactness. Past that, `strtod` is the only correct answer. |
| Add AVX2 string scan (32-byte block)       | New `#if defined(__AVX2__)` branch in `scan_str_safe`. Same shape as SSE2, wider vectors. Keep all existing branches. |

Each of these is a localized change. The one-file design is the whole
point: a new contributor reads this doc, scans the source, and can
change any single row of the table above without touching the others.
