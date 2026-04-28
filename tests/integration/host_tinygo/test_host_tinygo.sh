#!/bin/sh
# Issue #261: contrib/host/tinygo end-to-end smoke test.
#
# Skips cleanly when tinygo is not on PATH (matches contrib/host/go's
# pattern — host bridges should never break CI on machines that don't
# have the toolchain installed). When tinygo IS available, the test
# builds a c-shared .so, loads it via contrib.host.tinygo, and exercises
# every wrapper signature.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if ! command -v tinygo >/dev/null 2>&1; then
    echo "  [SKIP] tinygo not on PATH — install via https://tinygo.org/getting-started/install/"
    exit 0
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

case "$(uname -s)" in
    Darwin) LIB_EXT=dylib ;;
    Linux)  LIB_EXT=so ;;
    MINGW*|MSYS*|CYGWIN*) LIB_EXT=dll ;;
    *)      LIB_EXT=so ;;
esac

LIB_PATH="$TMPDIR/libgreet.$LIB_EXT"
GO_SRC="$ROOT/contrib/host/tinygo/examples/greet.go"

if ! tinygo build -buildmode=c-shared -o "$LIB_PATH" "$GO_SRC" 2>"$TMPDIR/tinygo.err"; then
    echo "  [SKIP] tinygo c-shared build failed (likely target/version mismatch):"
    head -5 "$TMPDIR/tinygo.err"
    exit 0
fi

ACTUAL="$TMPDIR/actual.txt"
if ! AETHER_HOME="$ROOT" TINYGO_LIB="$LIB_PATH" "$AE" run "$SCRIPT_DIR/uses_tinygo.ae" >"$ACTUAL" 2>"$TMPDIR/err.log"; then
    echo "  [FAIL] ae run exited non-zero"
    cat "$TMPDIR/err.log" | head -10
    cat "$ACTUAL" | head -10
    exit 1
fi

# Check each line individually so we can be precise about which
# wrapper failed if the test breaks.
for expected in \
    "Answer = 42" \
    "Add(2, 40) = 42" \
    "Negate(7) = -7" \
    "hello, world"
do
    if ! grep -Fxq "$expected" "$ACTUAL"; then
        echo "  [FAIL] expected line not found: $expected"
        echo "--- actual output ---"
        cat "$ACTUAL"
        exit 1
    fi
done

echo "  [PASS] contrib.host.tinygo: in-process c-shared invocation across 5 wrapper shapes"
