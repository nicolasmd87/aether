#!/bin/sh
# Regression: `aetherc --concat-ae` merges N sources into one synthetic
# .ae with deduped imports, accepts at most one main(), and produces a
# file that compiles + runs end-to-end. Issue #268.1.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AETHERC="$ROOT/build/aetherc"
AE="$ROOT/build/ae"

if [ ! -x "$AETHERC" ] || [ ! -x "$AE" ]; then
    echo "  [SKIP] aetherc_concat_ae: toolchain not built"
    exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cat > "$tmpdir/lib_a.ae" <<'AE'
import std.io
import std.string

double_it(n: int) -> int {
    return n * 2
}
AE

cat > "$tmpdir/lib_b.ae" <<'AE'
import std.string
import std.math

triple_it(n: int) -> int {
    return n * 3
}
AE

cat > "$tmpdir/main.ae" <<'AE'
import std.io

main() {
    println(double_it(triple_it(5)))
}
AE

out="$tmpdir/all.ae"
if ! "$AETHERC" --concat-ae "$tmpdir/lib_a.ae" "$tmpdir/lib_b.ae" "$tmpdir/main.ae" -o "$out" >"$tmpdir/concat.log" 2>&1; then
    echo "  [FAIL] aetherc_concat_ae: --concat-ae failed"
    cat "$tmpdir/concat.log"
    exit 1
fi

# Imports deduped — `std.string` should appear exactly once even though
# both lib_a.ae and lib_b.ae carry it.
string_imports=$(grep -c '^import std.string$' "$out")
if [ "$string_imports" != "1" ]; then
    echo "  [FAIL] aetherc_concat_ae: 'import std.string' appears $string_imports times in merged output (expected 1)"
    exit 1
fi

# Build + run the merged source. Output should be 30 (5 * 3 * 2).
bin="$tmpdir/all"
if ! "$AE" build "$out" -o "$bin" >"$tmpdir/build.log" 2>&1; then
    echo "  [FAIL] aetherc_concat_ae: merged file failed to build"
    cat "$tmpdir/build.log"
    exit 1
fi

actual="$("$bin")"
if [ "$actual" != "30" ]; then
    echo "  [FAIL] aetherc_concat_ae: merged binary printed '$actual' (expected '30')"
    exit 1
fi

# Reject duplicate main(). Two files each with main() — concat-ae must
# fail the merge rather than silently produce an unbuildable file.
cat > "$tmpdir/dup_main.ae" <<'AE'
main() {
    println("second main")
}
AE

if "$AETHERC" --concat-ae "$tmpdir/main.ae" "$tmpdir/dup_main.ae" -o "$tmpdir/dup.ae" >"$tmpdir/dup.log" 2>&1; then
    echo "  [FAIL] aetherc_concat_ae: duplicate main() should be rejected, but merge succeeded"
    exit 1
fi
if ! grep -q "main() definitions" "$tmpdir/dup.log"; then
    echo "  [FAIL] aetherc_concat_ae: duplicate main() rejection produced an unhelpful error"
    cat "$tmpdir/dup.log"
    exit 1
fi

echo "  [PASS] aetherc_concat_ae: 3 files merge, imports dedup, dup-main rejected, build runs"
exit 0
