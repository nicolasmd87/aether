#!/bin/sh
# Strict-match permutation tests — proves VCR's negative-path
# diagnostic surface (last_kind, last_index, last_error) reports
# what tests need to navigate to the root cause of a mismatch
# without parsing 599 response bodies.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if [ ! -x "$AE" ]; then
    echo "  [SKIP] svn_vcr_strict_match: ae not built"
    exit 0
fi

cd "$ROOT" || exit 1

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

if ! AETHER_HOME="$ROOT" "$AE" run "$SCRIPT_DIR/probe.ae" >"$TMPDIR/out.log" 2>&1; then
    echo "  [FAIL] svn_vcr_strict_match"
    tail -60 "$TMPDIR/out.log" | sed 's/^/    /'
    exit 1
fi

if ! grep -q "svn_vcr_strict_match: 4 passing, 0 failing" "$TMPDIR/out.log"; then
    echo "  [FAIL] svn_vcr_strict_match — not all 4 cases passed"
    tail -60 "$TMPDIR/out.log" | sed 's/^/    /'
    exit 1
fi

echo "  [PASS] svn_vcr_strict_match"
exit 0
