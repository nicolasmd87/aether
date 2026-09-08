#!/bin/sh
# A client that goes away mid-answer must not take the server with it.
#
# A write to a peer that has closed raises SIGPIPE, whose default disposition
# is to kill the process. Nothing in the server ignored it and only one send in
# the whole codebase passed MSG_NOSIGNAL, so a client pressing stop could end
# the server: found by putting a TLS listener under load, where it died at
# eight concurrent connections with exit status 141.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"
SRC="$ROOT/tests/integration/http_reverse_proxy/server.ae"

PY="$(sh "$ROOT/tests/scripts/find_python.sh" 2>/dev/null)" || PY=""
[ -n "$PY" ] || { echo "  [SKIP] no working Python (a Windows Store alias is not one)"; exit 0; }

. "$ROOT/tests/lib/wait_port.sh"

TMPDIR="$(mktemp -d)"
UP_PID=""; PX_PID=""
cleanup() {
    [ -n "$UP_PID" ] && { kill "$UP_PID" 2>/dev/null || true; wait "$UP_PID" 2>/dev/null || true; }
    [ -n "$PX_PID" ] && { kill "$PX_PID" 2>/dev/null || true; wait "$PX_PID" 2>/dev/null || true; }
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

if ! AETHER_HOME="$ROOT" "$AE" build "$SRC" -o "$TMPDIR/server" >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] build:"; head -20 "$TMPDIR/build.log"; exit 1
fi

wait_ready() {
    pid="$1"; log="$2"; tag="$3"
    deadline=$(($(date +%s) + 15))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        if grep -q '^READY ' "$log" 2>/dev/null; then return 0; fi
        kill -0 "$pid" 2>/dev/null || { echo "  [FAIL] $tag died:"; head -20 "$log"; exit 1; }
        sleep 0.05
    done
    echo "  [FAIL] $tag never READY:"; head -20 "$log"; exit 1
}

AETHER_HOME="$ROOT" "$TMPDIR/server" upstream >"$TMPDIR/up.log" 2>&1 &
UP_PID=$!
wait_ready "$UP_PID" "$TMPDIR/up.log" upstream
UP_PORT=$(read_ready_port "$TMPDIR/up.log") || exit 1

AETHER_HOME="$ROOT" "$TMPDIR/server" proxy "$UP_PORT" >"$TMPDIR/px.log" 2>&1 &
PX_PID=$!
wait_ready "$PX_PID" "$TMPDIR/px.log" proxy
PX_PORT=$(read_ready_port "$TMPDIR/px.log") || exit 1
wait_port "$PX_PORT" || exit 1

curl -s -o /dev/null --max-time 3 "http://127.0.0.1:$PX_PORT/echo" || {
    echo "  [FAIL] proxy never answered"; head -20 "$TMPDIR/px.log"; exit 1; }

if OUT=$($PY "$SCRIPT_DIR/disconnect_probe.py" "$PX_PORT" 2>&1); then
    :
else
    echo "  [FAIL] $OUT"
    if kill -0 "$PX_PID" 2>/dev/null; then
        echo "         the proxy is still running, so the answer itself was wrong"
    else
        wait "$PX_PID" 2>/dev/null
        echo "         the proxy exited with status $? (141 is SIGPIPE)"
        PX_PID=""
    fi
    exit 1
fi

kill -0 "$PX_PID" 2>/dev/null || { echo "  [FAIL] the proxy died during the run"; PX_PID=""; exit 1; }
echo "  [PASS] http_client_disconnect: 40 clients reset mid-answer, the proxy served the next request"
