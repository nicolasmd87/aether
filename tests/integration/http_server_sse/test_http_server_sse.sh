#!/bin/sh
# #260 Tier 2 / Phase E3: Server-Sent Events end-to-end.
#
# Connects to /events with curl -N (disable buffering) and verifies:
#   - Content-Type: text/event-stream header
#   - 3 events received with the expected event names + data
#   - third event carries id: evt-3
#
# Skips when curl is missing.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if ! command -v curl >/dev/null 2>&1; then
    echo "  [SKIP] curl not on PATH"
    exit 0
fi

TMPDIR="$(mktemp -d)"
cleanup() {
    if [ -n "${SRV_PID:-}" ]; then
        kill "$SRV_PID" 2>/dev/null || true
        wait "$SRV_PID" 2>/dev/null || true
    fi
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

AETHER_HOME="$ROOT" "$AE" run "$SCRIPT_DIR/server.ae" >"$TMPDIR/srv.log" 2>&1 &
SRV_PID=$!

deadline=$(($(date +%s) + 5))
while [ "$(date +%s)" -lt "$deadline" ]; do
    if grep -q READY "$TMPDIR/srv.log" 2>/dev/null; then break; fi
    if ! kill -0 "$SRV_PID" 2>/dev/null; then
        echo "  [FAIL] server died:"
        head -20 "$TMPDIR/srv.log"
        exit 1
    fi
    sleep 0.1
done
sleep 0.3

OUT="$TMPDIR/sse.out"
HEADERS="$TMPDIR/sse.h"
# -N: no buffering. Server closes after 3 events; max-time bounds
# the wait if it didn't.
curl -s -N -D "$HEADERS" -o "$OUT" --max-time 5 \
    "http://127.0.0.1:18109/events" || true

if ! grep -qi "Content-Type: text/event-stream" "$HEADERS"; then
    echo "  [FAIL] missing Content-Type: text/event-stream"
    cat "$HEADERS"
    exit 1
fi

# Expect three events, each with event:tick and matching data.
for n in 1 2 3; do
    if ! grep -q "^event: tick" "$OUT"; then
        echo "  [FAIL] missing 'event: tick' lines"
        cat "$OUT"
        exit 1
    fi
    if ! grep -q "^data: $n$" "$OUT"; then
        echo "  [FAIL] missing 'data: $n'"
        cat "$OUT"
        exit 1
    fi
done

# The third event should also carry id: evt-3.
if ! grep -q "^id: evt-3$" "$OUT"; then
    echo "  [FAIL] missing 'id: evt-3' on the third event"
    cat "$OUT"
    exit 1
fi

echo "  [PASS] http_server_sse: 3 events received with correct framing + id"
