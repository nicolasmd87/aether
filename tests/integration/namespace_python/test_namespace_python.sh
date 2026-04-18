#!/bin/sh
# End-to-end Python SDK generator test. `ae build --namespace` emits
# both the .so and the .py module; the Python script imports the
# module and exercises the full surface.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        echo "  [SKIP] test_namespace_python on Windows"; exit 0 ;;
esac

if ! command -v python3 >/dev/null 2>&1; then
    echo "  [SKIP] test_namespace_python (python3 not installed)"
    exit 0
fi

case "$(uname -s 2>/dev/null)" in
    Darwin) LIB_EXT=".dylib" ;;
    *)      LIB_EXT=".so" ;;
esac

TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT
pass=0; fail=0

cp "$SCRIPT_DIR"/manifest.ae "$SCRIPT_DIR"/calc.ae "$SCRIPT_DIR"/check.py "$TMPDIR/"

cd "$TMPDIR"
if ! AETHER_HOME="" "$ROOT/build/ae" build --namespace . >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] ae build --namespace ."; cat "$TMPDIR/build.log"; fail=$((fail + 1))
elif [ ! -f "$TMPDIR/calc_sdk.py" ]; then
    echo "  [FAIL] calc_sdk.py was not generated"; ls -la "$TMPDIR"; fail=$((fail + 1))
else
    LIB_PATH=""
    for c in "$TMPDIR/lib"*"${LIB_EXT}"; do
        [ -f "$c" ] && { LIB_PATH="$c"; break; }
    done
    if [ -z "$LIB_PATH" ]; then
        echo "  [FAIL] no namespace lib"; fail=$((fail + 1))
    elif python3 "$TMPDIR/check.py" "$LIB_PATH" >"$TMPDIR/run.out" 2>&1; then
        echo "  [PASS] generated Python SDK round-trips"; pass=$((pass + 1))
    else
        echo "  [FAIL] python check.py failed"; cat "$TMPDIR/run.out"; fail=$((fail + 1))
    fi
fi

echo ""
echo "namespace_python: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
