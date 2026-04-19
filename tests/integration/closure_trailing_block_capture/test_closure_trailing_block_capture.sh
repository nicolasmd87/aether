#!/bin/sh
# Regression: captures across nested trailing blocks.
#
# A variable declared in a trailing block is bound in the enclosing
# function's scope (trailing blocks are inlined at the call site, not
# hoisted). A closure inside a sibling or nested trailing block should
# be able to capture such variables. Previously, capture discovery's
# subtree_declares stopped at ANY AST_CLOSURE — including trailing-block
# closures (value == "trailing") — so names declared inside one trailing
# block were invisible to inner closures that needed to capture them.
#
# Surfaced by contrib/aether_ui/example_canvas.ae (aether-ui branch).
# Without the fix: `error: 'c' undeclared` at the closure body.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

SRC="$SCRIPT_DIR/capture_from_trailing.ae"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

BIN="$TMPDIR/capture"

if ! "$AE" build "$SRC" -o "$BIN" >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] ae build — capture across trailing block rejected"
    head -20 "$TMPDIR/build.log"
    exit 1
fi

if ! "$BIN" >"$TMPDIR/run.out" 2>&1; then
    echo "  [FAIL] binary exited non-zero"
    cat "$TMPDIR/run.out"
    exit 1
fi

if grep -q "FAIL:" "$TMPDIR/run.out"; then
    echo "  [FAIL] runtime assertion failed"
    cat "$TMPDIR/run.out"
    exit 1
fi

echo "  [PASS] closure_trailing_block_capture: c=42 seen by inner closure"
