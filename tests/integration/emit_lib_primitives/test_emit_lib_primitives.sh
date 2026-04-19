#!/bin/sh
# int64/bool/float primitive round-trip for --emit=lib.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        echo "  [SKIP] test_emit_lib_primitives on Windows"; exit 0 ;;
esac
case "$(uname -s 2>/dev/null)" in
    Darwin) LIB_EXT=".dylib" ;;
    *)      LIB_EXT=".so" ;;
esac

TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT
pass=0; fail=0

cd "$SCRIPT_DIR"
if ! AETHER_HOME="" "$ROOT/build/ae" build --emit=lib config.ae -o "$TMPDIR/libprims" >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] build"; cat "$TMPDIR/build.log"; fail=$((fail + 1))
else
    LIB_PATH=""
    for c in "$TMPDIR/libprims" "$TMPDIR/libprims${LIB_EXT}"; do
        [ -f "$c" ] && { LIB_PATH="$c"; break; }
    done
    if [ -z "$LIB_PATH" ]; then
        echo "  [FAIL] no lib"; fail=$((fail + 1))
    elif ! gcc -I"$ROOT/runtime" "$SCRIPT_DIR/consume.c" "$LIB_PATH" -Wl,-rpath,"$TMPDIR" -ldl -lm -o "$TMPDIR/consume" 2>"$TMPDIR/gcc.log"; then
        echo "  [FAIL] gcc"; cat "$TMPDIR/gcc.log"; fail=$((fail + 1))
    elif "$TMPDIR/consume" "$LIB_PATH" >"$TMPDIR/run.out" 2>&1; then
        echo "  [PASS] int64 / bool / float round-trip"; pass=$((pass + 1))
    else
        echo "  [FAIL] runtime"; cat "$TMPDIR/run.out"; fail=$((fail + 1))
    fi
fi

echo ""
echo "emit_lib_primitives: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
