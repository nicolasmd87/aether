#!/bin/sh
# An upstream may close a pooled connection whenever it likes, and the proxy
# only finds out by using it. Every request must still be answered: the request
# has not been delivered, so it goes again down a fresh connection.
#
# Before this was handled, the empty read was parsed as status 0 and the client
# received "HTTP/1.1 0 Unknown", or the connection was dropped with no answer
# at all.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

PY="$(sh "$ROOT/tests/scripts/find_python.sh" 2>/dev/null)" || PY=""
if [ -z "$PY" ]; then
    echo "  [SKIP] http_proxy_stale_pooled_connection: no working Python (a Windows Store alias is not one)"
    exit 0
fi

. "$ROOT/tests/lib/wait_port.sh"

TMPDIR="$(mktemp -d)"
PX_PID=""
UP_PID=""
cleanup() {
    for pid in "$PX_PID" "$UP_PID"; do
        if [ -n "$pid" ]; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

# The reverse-proxy suite's driver, not a copy: a second copy would be a second
# thing to keep in step, and any .ae file here would be picked up by the sweep
# that runs every .ae as a standalone program.
SERVER_SRC="$SCRIPT_DIR/../http_reverse_proxy/server.ae"
if [ ! -f "$SERVER_SRC" ]; then
    echo "  [FAIL] missing $SERVER_SRC"; exit 1
fi
if ! AETHER_HOME="$ROOT" "$AE" build "$SERVER_SRC" \
        -o "$TMPDIR/server" >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] build:"; head -30 "$TMPDIR/build.log"; exit 1
fi

# The probe is the upstream: it binds port 0 and names the port it landed on,
# which is what the proxy has to be told.
$PY "$SCRIPT_DIR/stale_probe.py" upstream "$TMPDIR/up.port" >"$TMPDIR/up.log" 2>&1 &
UP_PID=$!
deadline=$(($(date +%s) + 15))
while [ ! -f "$TMPDIR/up.port" ]; do
    kill -0 "$UP_PID" 2>/dev/null || {
        echo "  [FAIL] upstream died:"; head -20 "$TMPDIR/up.log"; exit 1; }
    [ "$(date +%s)" -lt "$deadline" ] || {
        echo "  [FAIL] upstream never bound"; exit 1; }
    sleep 0.05
done
UP_PORT=$(cat "$TMPDIR/up.port")

AETHER_HOME="$ROOT" "$TMPDIR/server" proxy "$UP_PORT" >"$TMPDIR/px.log" 2>&1 &
PX_PID=$!

deadline=$(($(date +%s) + 15))
while [ "$(date +%s)" -lt "$deadline" ]; do
    if ! kill -0 "$PX_PID" 2>/dev/null; then
        echo "  [FAIL] proxy died:"; head -30 "$TMPDIR/px.log"; exit 1
    fi
    if grep -q '^READY ' "$TMPDIR/px.log" 2>/dev/null; then break; fi
    sleep 0.05
done
PX_PORT=$(read_ready_port "$TMPDIR/px.log") || exit 1
wait_port "$PX_PORT" || exit 1

if ! OUT=$($PY "$SCRIPT_DIR/stale_probe.py" client "$PX_PORT" 2>&1); then
    echo "  [FAIL] http_proxy_stale_pooled_connection: $OUT"; exit 1
fi

echo "  [PASS] http_proxy_stale_pooled_connection: 60/60 answered when the upstream closes every pooled connection"
