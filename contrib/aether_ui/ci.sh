#!/bin/bash
# ci.sh — full aether_ui test pipeline as a CI job would run it.
#
# Phases:
#   1. Build every example (catches C/Aether compile regressions).
#   2. Smoke-launch the non-driver examples to catch runtime crashes the
#      HTTP-driven tests can't reach (widget wiring, reactive state init,
#      platform-backend regressions). Each binary is launched, given 1.5s
#      to render, then killed; still-alive = pass.
#   3. Launch example_calculator with the AetherUIDriver test server and
#      run test_calculator.sh (11 assertions).
#   4. Launch example_testable and run test_automation.sh (17 assertions).
#
# Platform handling:
#   macOS    — runs directly (AppKit).
#   Linux    — runs directly if $DISPLAY or $WAYLAND_DISPLAY is set; otherwise
#              auto-wraps with xvfb-run when available. Falls back to build-only.
#   Windows  — native Win32 backend. Aether-level examples are skipped (the
#              DSL has a separate module-resolution issue on MINGW that
#              blocks `ae build`). The C-level backend test suite
#              (tests/test_widgets.c) and the HTTP driver test
#              (tests/test_driver.sh) run instead — invoked via
#              `make contrib-aether-ui-check`.
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

# All examples that must compile in Phase 1.
EXAMPLES=(counter form picker styled system canvas testable calculator)
# Examples without a test server — Phase 2 smoke-launches each.
# calculator and testable are exercised through their HTTP drivers in
# Phases 3-4, so they are not smoke-tested here.
SMOKE_EXAMPLES=(counter form picker styled system canvas)
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
    echo "NOTICE: Windows backend uses a separate test flow."
    echo "        Run: make contrib-aether-ui-check"
    echo "        (headless widget suite + HTTP driver integration test)"
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

run_smoke_test() {
    # Launch a GUI binary, give it 1.5s to open its window, then kill it.
    # Pass iff the process is still alive at the deadline. A process that
    # exited early is a crash (missing widget impl, null deref on init,
    # backend API mismatch) — propagate non-zero and dump the tail.
    local bin="$1" name="$2"
    $LAUNCH_PREFIX "$bin" > "/tmp/ci_smoke_${name}.log" 2>&1 &
    local pid=$!
    sleep 1.5
    if kill -0 "$pid" 2>/dev/null; then
        echo "  OK   $name (alive 1.5s)"
        kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
        return 0
    fi
    wait "$pid" 2>/dev/null; local rc=$?
    echo "  FAIL $name (exited early, rc=$rc)"
    tail -15 "/tmp/ci_smoke_${name}.log" | sed 's/^/       /'
    return 1
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
echo "=== Phase 2: smoke-launch non-driver examples ==="
for ex in "${SMOKE_EXAMPLES[@]}"; do
    run_smoke_test "$ROOT/build/${ex}" "$ex" || FAIL=$((FAIL + 1))
done

echo
echo "=== Phase 3: AetherUIDriver calculator tests ==="
run_server_test "$ROOT/build/calculator" \
                "$SCRIPT_DIR/test_calculator.sh" calculator || FAIL=$((FAIL + 1))

echo
echo "=== Phase 4: AetherUIDriver testable tests ==="
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
