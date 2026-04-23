#!/bin/sh
# Regression: std.cryptography.sha1 / sha256 across known vectors + NUL-
# embedded AetherString payloads. See probe.ae for the matrix.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT

if ! "$ROOT/build/ae" build "$SCRIPT_DIR/probe.ae" -o "$TMPDIR/probe" \
        --extra "$SCRIPT_DIR/shim.c" >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] cryptography_sha: build failed"
    sed 's/^/    /' "$TMPDIR/build.log" | head -15
    exit 1
fi

if ! "$TMPDIR/probe" >"$TMPDIR/run.log" 2>&1; then
    echo "  [FAIL] cryptography_sha: probe exited non-zero"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi

if grep -q "All std.cryptography tests passed" "$TMPDIR/run.log"; then
    echo "  [PASS] cryptography_sha: 6 cases"
elif grep -q "std.cryptography tests skipped" "$TMPDIR/run.log"; then
    reason=$(grep '^SKIP cryptography_sha:' "$TMPDIR/run.log" | head -1)
    echo "  [PASS] cryptography_sha: ${reason:-skipped (no OpenSSL backend)}"
else
    echo "  [FAIL] cryptography_sha: didn't reach final PASS or SKIP line"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi
