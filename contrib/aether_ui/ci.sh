#!/bin/bash
# ci.sh — full aether_ui test pipeline as a CI job would run it.
#
# Phases:
#   1. Build every example (catches C/Aether compile regressions).
#   2. Launch example_calculator with the AetherUIDriver test server
#      and run test_calculator.sh (11 assertions).
#   3. Launch example_testable and run test_automation.sh.
#
# Platform handling:
#   macOS    — runs directly (AppKit).
#   Linux    — runs directly if $DISPLAY or $WAYLAND_DISPLAY is set; otherwise
#              auto-wraps with xvfb-run when available. Falls back to build-only.
#   Windows  — no Windows backend exists yet; Phase 1 skipped, runtime phases
#              skipped, script exits 0 with a notice so CI stays green until a
#              backend lands.
#
# Exits non-zero only when an implemented platform fails. Leaves no
# background processes.
#
# Usage: ./contrib/aether_ui/ci.sh [port]

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PORT="${1:-9222}"

cd "$ROOT"
mkdir -p build

EXAMPLES=(counter form picker styled system canvas testable calculator)
FAIL=0

OS="$(uname -s)"
case "$OS" in
    Darwin)  PLATFORM=macos ;;
    Linux)   PLATFORM=linux ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM=windows ;;
    *)       PLATFORM=unknown ;;
esac
echo "=== aether_ui CI on $OS ($PLATFORM) ==="

if [ "$PLATFORM" = "windows" ]; then
    echo "NOTICE: aether_ui has no Windows backend yet — skipping all phases."
    echo "        When a Win32/WinUI backend lands, extend build.sh + this script."
    exit 0
fi
if [ "$PLATFORM" = "unknown" ]; then
    echo "ERROR: unrecognized platform '$OS'."
    exit 1
fi

# Decide how to launch GUI binaries. On Linux CI runners without a display,
# wrap with xvfb-run so GTK4 has a framebuffer.
LAUNCH_PREFIX=""
if [ "$PLATFORM" = "linux" ]; then
    if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
        if command -v xvfb-run > /dev/null 2>&1; then
            LAUNCH_PREFIX="xvfb-run -a"
            echo "no display detected — wrapping GUI launches with xvfb-run"
        else
            echo "NOTICE: no display and xvfb-run missing — will build-only."
            LAUNCH_PREFIX="SKIP_RUNTIME"
        fi
    fi
fi

run_server_test() {
    # Launch a binary with AETHER_UI_TEST_PORT set, wait for the test server,
    # run the given test script against it, kill the binary, propagate status.
    local bin="$1" script="$2" name="$3"
    echo "--- launching $bin ---"
    AETHER_UI_TEST_PORT="$PORT" $LAUNCH_PREFIX "$bin" > "/tmp/ci_${name}.app.log" 2>&1 &
    local pid=$!

    # Wait up to 6s for the server to come up.
    local up=0
    for _ in $(seq 1 30); do
        if curl -sf -o /dev/null "http://127.0.0.1:$PORT/widgets"; then up=1; break; fi
        sleep 0.2
    done
    if [ "$up" -ne 1 ]; then
        echo "  FAIL: $name test server never responded"
        kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
        tail -20 "/tmp/ci_${name}.app.log" | sed 's/^/       /'
        return 1
    fi

    "$script" "$PORT"
    local rc=$?
    kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
    return $rc
}

echo "=== Phase 1: build all aether_ui examples ==="
for ex in "${EXAMPLES[@]}"; do
    src="contrib/aether_ui/example_${ex}.ae"
    out="build/${ex}"
    if "$SCRIPT_DIR/build.sh" "$src" "$out" > "/tmp/ci_build_${ex}.log" 2>&1; then
        echo "  OK   $ex"
    else
        echo "  FAIL $ex"
        tail -15 "/tmp/ci_build_${ex}.log" | sed 's/^/       /'
        FAIL=$((FAIL + 1))
    fi
done

if [ "$FAIL" -gt 0 ]; then
    echo
    echo "=== CI result: $FAIL build failure(s) — skipping runtime phases ==="
    exit 1
fi

if [ "$LAUNCH_PREFIX" = "SKIP_RUNTIME" ]; then
    echo
    echo "=== CI result: builds passed; runtime phases skipped (no display) ==="
    exit 0
fi

echo
echo "=== Phase 2: AetherUIDriver calculator tests ==="
run_server_test "$ROOT/build/calculator" \
                "$SCRIPT_DIR/test_calculator.sh" calculator || FAIL=$((FAIL + 1))

echo
echo "=== Phase 3: AetherUIDriver testable tests ==="
run_server_test "$ROOT/build/testable" \
                "$SCRIPT_DIR/test_automation.sh" testable || FAIL=$((FAIL + 1))

echo
if [ "$FAIL" -eq 0 ]; then
    echo "=== CI result: all phases passed ==="
    exit 0
else
    echo "=== CI result: $FAIL phase(s) failed ==="
    exit 1
fi
