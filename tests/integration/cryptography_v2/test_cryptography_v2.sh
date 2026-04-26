#!/bin/sh
# Regression: std.cryptography Base64 codec + algorithm-by-name
# dispatcher (hash_hex, hash_supported). See probe.ae for the
# eight-case matrix.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT

if ! "$ROOT/build/ae" build "$SCRIPT_DIR/probe.ae" -o "$TMPDIR/probe" \
        --extra "$SCRIPT_DIR/shim.c" >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] cryptography_v2: build failed"
    sed 's/^/    /' "$TMPDIR/build.log" | head -15
    exit 1
fi

if ! "$TMPDIR/probe" >"$TMPDIR/run.log" 2>&1; then
    echo "  [FAIL] cryptography_v2: probe exited non-zero"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi

if grep -q "All std.cryptography v2 tests passed" "$TMPDIR/run.log"; then
    echo "  [PASS] cryptography_v2: 8 cases"
elif grep -q "std.cryptography v2 tests skipped" "$TMPDIR/run.log"; then
    reason=$(grep '^SKIP cryptography_v2:' "$TMPDIR/run.log" | head -1)
    echo "  [PASS] cryptography_v2: ${reason:-skipped (no OpenSSL backend)}"
else
    echo "  [FAIL] cryptography_v2: didn't reach final PASS or SKIP line"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi
