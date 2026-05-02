#!/bin/sh
# Regression: prove `gcc --coverage` + gcov produces .ae.gcov files
# attributed back to .ae source via PR #352's #line directives. This
# is the foundational claim that `make ci-coverage` rests on.
#
# Lightweight: compiles a 12-line .ae standalone (no full coverage
# rebuild of stdlib) and inspects the resulting .ae.gcov directly.
# `make ci-coverage` itself is too heavyweight to gate on a regression
# (rebuilds compiler + libaether.a with --coverage + runs all tests).

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AETHERC="$ROOT/build/aetherc"

if [ ! -x "$AETHERC" ]; then
    echo "  [SKIP] ci_coverage_smoke: toolchain not built"
    exit 0
fi

if ! command -v gcov >/dev/null 2>&1; then
    echo "  [SKIP] ci_coverage_smoke: gcov not installed"
    exit 0
fi

# Where is the install prefix? `ae build` discovers it relative to
# its own location; for this test we just need libaether.a + headers.
# Skip if we can't find a complete install — the coverage path is
# only meaningful after `make install`.
PREFIX="$HOME/.local"
if [ ! -f "$PREFIX/lib/aether/libaether.a" ] || \
   [ ! -d "$PREFIX/include/aether" ]; then
    echo "  [SKIP] ci_coverage_smoke: no install at $PREFIX (run 'make install PREFIX=\$HOME/.local')"
    exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cat > "$tmpdir/cov_demo.ae" <<'AE'
import std.string

main() {
    n = 10
    if n > 5 {
        println("big number")
    } else {
        println("small")
    }
    println("done")
}
AE

# Compile via the dev aetherc — must produce a .c with #line
# directives (Phase 1 of the coverage work).
if ! "$AETHERC" "$tmpdir/cov_demo.ae" "$tmpdir/cov_demo.c" >/dev/null 2>&1; then
    echo "  [FAIL] ci_coverage_smoke: aetherc failed on cov_demo.ae"
    exit 1
fi

if ! grep -q '^#line .* "[^"]*cov_demo\.ae"' "$tmpdir/cov_demo.c"; then
    echo "  [FAIL] ci_coverage_smoke: aetherc emitted no #line for cov_demo.ae"
    exit 1
fi

# Build with gcc --coverage so .gcno is produced alongside the
# object, and .gcda is written when the binary runs.
INC_FLAGS=$(find "$PREFIX/include/aether" -type d 2>/dev/null | sed 's|^|-I|' | tr '\n' ' ')
if ! gcc --coverage -O0 -g $INC_FLAGS \
        "$tmpdir/cov_demo.c" \
        -L"$PREFIX/lib/aether" -laether \
        -lpthread -lm -ldl \
        -o "$tmpdir/cov_demo" 2>"$tmpdir/link.err"; then
    echo "  [FAIL] ci_coverage_smoke: gcc --coverage link failed"
    head -5 "$tmpdir/link.err"
    exit 1
fi

# Run, then gcov produces both .c.gcov and (the prize) .ae.gcov.
if ! (cd "$tmpdir" && ./cov_demo >/dev/null 2>&1); then
    echo "  [FAIL] ci_coverage_smoke: cov_demo failed at runtime"
    exit 1
fi

if ! (cd "$tmpdir" && gcov -p -b cov_demo.c >"$tmpdir/gcov.log" 2>&1); then
    echo "  [FAIL] ci_coverage_smoke: gcov failed"
    cat "$tmpdir/gcov.log"
    exit 1
fi

# Locate the .ae.gcov that proves Phase 1's #line directives flowed
# all the way through gcc -> .gcno -> .gcda -> gcov.
ae_gcov=$(find "$tmpdir" -name '*cov_demo.ae*.gcov' | head -1)
if [ -z "$ae_gcov" ]; then
    echo "  [FAIL] ci_coverage_smoke: no .ae.gcov produced — gcov didn't follow #line directives"
    ls "$tmpdir"/*.gcov 2>/dev/null
    exit 1
fi

# Verify the .ae.gcov has real per-line hit counters (not just metadata).
# Format: hit_count : line_number : source_line
# Numeric or `#####` in the count column = real coverage data.
if ! grep -qE '^[ ]*([0-9]+|#####)[ ]*:[ ]*[0-9]+:' "$ae_gcov"; then
    echo "  [FAIL] ci_coverage_smoke: .ae.gcov has no hit-count rows"
    head -10 "$ae_gcov"
    exit 1
fi

# Specifically verify the unreached `else` branch is flagged.
# Line 8 in cov_demo.ae is `println("small")` inside the else.
if ! grep -qE '^[ ]*#####[ ]*:[ ]*8:' "$ae_gcov"; then
    echo "  [FAIL] ci_coverage_smoke: line 8 (unreached else) not flagged as ##### in .ae.gcov"
    grep ':[[:space:]]*[78]:' "$ae_gcov"
    exit 1
fi

echo "  [PASS] ci_coverage_smoke"
exit 0
