#!/bin/sh
# Regression: std.zlib.deflate / inflate round-trip across text,
# empty streams, garbage input, and binary payloads with embedded
# NULs. See probe.ae for the matrix.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT

if ! "$ROOT/build/ae" build "$SCRIPT_DIR/probe.ae" -o "$TMPDIR/probe" \
        --extra "$SCRIPT_DIR/shim.c" >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] zlib_roundtrip: build failed"
    sed 's/^/    /' "$TMPDIR/build.log" | head -15
    exit 1
fi

if ! "$TMPDIR/probe" >"$TMPDIR/run.log" 2>&1; then
    echo "  [FAIL] zlib_roundtrip: probe exited non-zero"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi

if grep -q "All std.zlib tests passed" "$TMPDIR/run.log"; then
    echo "  [PASS] zlib_roundtrip: 6 cases"
elif grep -q "std.zlib tests skipped" "$TMPDIR/run.log"; then
    reason=$(grep '^SKIP zlib_roundtrip:' "$TMPDIR/run.log" | head -1)
    echo "  [PASS] zlib_roundtrip: ${reason:-skipped (no zlib backend)}"
else
    echo "  [FAIL] zlib_roundtrip: didn't reach final PASS or SKIP line"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi
