#!/bin/sh
# Run the worked trading example end-to-end. Builds the namespace,
# compiles the Java demo, runs it, and asserts the expected output
# (every event fires with the right id, the trade book ends in the
# expected state).
#
# Skips cleanly when the toolchain isn't available — needs javac/java
# JDK 22+. The example itself lives at examples/embedded-java/trading/
# so its README points the user there directly.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
EXAMPLE_DIR="$ROOT/examples/embedded-java/trading"

case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        echo "  [SKIP] embedded_java_trading_e2e on Windows"; exit 0 ;;
esac

if ! command -v javac >/dev/null 2>&1 || ! command -v java >/dev/null 2>&1; then
    echo "  [SKIP] embedded_java_trading_e2e (javac/java not installed)"; exit 0
fi
JVER=$(javac -version 2>&1 | sed -n 's/^javac \([0-9][0-9]*\).*/\1/p')
if [ -z "$JVER" ] || [ "$JVER" -lt 22 ]; then
    echo "  [SKIP] embedded_java_trading_e2e (need JDK 22+, found '$JVER')"; exit 0
fi

case "$(uname -s 2>/dev/null)" in
    Darwin) LIB_EXT=".dylib" ;;
    *)      LIB_EXT=".so" ;;
esac

# The example's build.sh expects `ae` on PATH; provide it from the build dir.
TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT
pass=0; fail=0

# Copy the example into TMPDIR so we don't pollute the source tree
# with build artifacts (libtrading.so, generated Trading.java, build/).
cp -R "$EXAMPLE_DIR" "$TMPDIR/trading"

cd "$TMPDIR/trading"
PATH="$ROOT/build:$PATH" AETHER_HOME="" ./build.sh >"$TMPDIR/run.out" 2>&1
rc=$?

if [ "$rc" -ne 0 ]; then
    echo "  [FAIL] build.sh exited with $rc"
    cat "$TMPDIR/run.out"
    fail=$((fail + 1))
else
    # Spot-check expected output lines.
    expected="
Loaded Manifest(namespace=\"trading\", inputs=1, events=4)
[event] OrderPlaced id=100
[event] OrderRejected id=101
[event] UnknownTicker id=102
[event] TradeKilled id=100
ticker = ACME
Final trade book: {100=KILLED}
"
    miss=0
    for line in \
        'Loaded Manifest(namespace="trading"' \
        'OrderPlaced id=100' \
        'OrderRejected id=101' \
        'UnknownTicker id=102' \
        'TradeKilled id=100' \
        'ticker = ACME' \
        '{100=KILLED}'
    do
        if ! grep -qF "$line" "$TMPDIR/run.out"; then
            echo "  [FAIL] missing expected line: $line"
            miss=$((miss + 1))
        fi
    done
    if [ "$miss" -eq 0 ]; then
        echo "  [PASS] worked trading example end-to-end"
        pass=$((pass + 1))
    else
        cat "$TMPDIR/run.out"
        fail=$((fail + 1))
    fi
fi

echo ""
echo "embedded_java_trading_e2e: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
