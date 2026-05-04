#!/bin/sh
# aeocha aeb-ipc-reporting integration test.
#
# Verifies that contrib/aeocha's run_summary() emits a v1
# structured report through std.ipc.parent_channel() when one
# is available — the path aeb's driver_test consumes. probe.ae
# spawns a compiled child via os.run_pipe_drain_and_wait, drains
# the parent channel, parses header KV + tab-packed per-it rows,
# and asserts the report shape.
#
# Skips on Windows — std.ipc parent channel is POSIX-only in v1
# and parent_channel() returns -1 unconditionally on Windows, so
# the report path is silently a no-op there (which is the intended
# fallback behavior — tested separately).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if [ "$OS" = "Windows_NT" ]; then
    echo "  [SKIP] aeocha_aeb_ipc_reporting: std.ipc is POSIX-only in v1"
    exit 0
fi

if [ ! -x "$AE" ]; then
    echo "  [SKIP] aeocha_aeb_ipc_reporting: ae not built"
    exit 0
fi

cd "$ROOT" || exit 1

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"; rm -f /tmp/aeocha_aeb_ipc_reporting_child' EXIT

if ! AETHER_HOME="$ROOT" "$AE" run "$SCRIPT_DIR/probe.ae" >"$TMPDIR/out.log" 2>&1; then
    echo "  [FAIL] aeocha_aeb_ipc_reporting"
    tail -60 "$TMPDIR/out.log" | sed 's/^/    /'
    exit 1
fi

if ! grep -q "^OK$" "$TMPDIR/out.log"; then
    echo "  [FAIL] aeocha_aeb_ipc_reporting — probe did not print OK"
    tail -60 "$TMPDIR/out.log" | sed 's/^/    /'
    exit 1
fi

echo "  [PASS] aeocha_aeb_ipc_reporting"
exit 0
