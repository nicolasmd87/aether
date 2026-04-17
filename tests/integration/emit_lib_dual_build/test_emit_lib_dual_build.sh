#!/bin/sh
# Verifies:
#   (a) `ae build --emit=both` is rejected with a helpful message (v1 scope)
#   (b) Same source can be built twice — once as exe, once as lib — and
#       both artifacts work independently.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        echo "  [SKIP] test_emit_lib_dual_build on Windows"; exit 0 ;;
esac
case "$(uname -s 2>/dev/null)" in
    Darwin) LIB_EXT=".dylib" ;;
    *)      LIB_EXT=".so" ;;
esac

TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT
pass=0; fail=0

# (a) --emit=both should fail cleanly.
cd "$SCRIPT_DIR"
if AETHER_HOME="" "$ROOT/build/ae" build --emit=both config.ae -o "$TMPDIR/both_out" >"$TMPDIR/both.log" 2>&1; then
    echo "  [FAIL] --emit=both should be rejected but succeeded"
    fail=$((fail + 1))
elif grep -q "not yet implemented" "$TMPDIR/both.log"; then
    echo "  [PASS] --emit=both rejected with helpful message"
    pass=$((pass + 1))
else
    echo "  [FAIL] --emit=both was rejected but without the expected message"
    cat "$TMPDIR/both.log"
    fail=$((fail + 1))
fi

# (b1) Build as exe — should run and print "ran as exe".
if ! AETHER_HOME="" "$ROOT/build/ae" build --emit=exe config.ae -o "$TMPDIR/dual_exe" >"$TMPDIR/exe.log" 2>&1; then
    echo "  [FAIL] --emit=exe build failed"
    cat "$TMPDIR/exe.log"
    fail=$((fail + 1))
elif out=$("$TMPDIR/dual_exe" 2>/dev/null); then
    if echo "$out" | grep -q "ran as exe"; then
        echo "  [PASS] exe artifact runs main()"
        pass=$((pass + 1))
    else
        echo "  [FAIL] exe ran but stdout didn't include 'ran as exe'"
        echo "       got: $out"
        fail=$((fail + 1))
    fi
else
    echo "  [FAIL] exe artifact failed to execute"
    fail=$((fail + 1))
fi

# (b2) Build same source as lib — should have aether_greet symbol, no main.
if ! AETHER_HOME="" "$ROOT/build/ae" build --emit=lib config.ae -o "$TMPDIR/libdual" >"$TMPDIR/lib.log" 2>&1; then
    echo "  [FAIL] --emit=lib build failed"
    cat "$TMPDIR/lib.log"
    fail=$((fail + 1))
else
    LIB_PATH=""
    for c in "$TMPDIR/libdual" "$TMPDIR/libdual${LIB_EXT}"; do
        [ -f "$c" ] && { LIB_PATH="$c"; break; }
    done
    if [ -z "$LIB_PATH" ]; then
        echo "  [FAIL] no lib produced"; fail=$((fail + 1))
    else
        # aether_greet present?
        if nm -D "$LIB_PATH" 2>/dev/null | grep -q " T aether_greet"; then
            echo "  [PASS] lib artifact exports aether_greet"
            pass=$((pass + 1))
        else
            echo "  [FAIL] aether_greet symbol missing from lib"
            fail=$((fail + 1))
        fi
        # main absent?
        if nm -D "$LIB_PATH" 2>/dev/null | grep -qE " T main$"; then
            echo "  [FAIL] lib artifact has 'main' symbol — should be suppressed"
            fail=$((fail + 1))
        else
            echo "  [PASS] lib artifact has no 'main' symbol"
            pass=$((pass + 1))
        fi
    fi
fi

echo ""
echo "emit_lib_dual_build: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
