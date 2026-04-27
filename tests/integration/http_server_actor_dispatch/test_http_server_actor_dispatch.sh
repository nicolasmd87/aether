#!/bin/sh
# #260 Tier 0 / Phase C3: parallel keep-alive sessions through the
# drain helper. Drives 4 parallel curl clients, each issuing 25
# requests over its own TCP connection, and verifies all 100
# responses arrive cleanly with no cross-session pollution.

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

# Drive 4 parallel sessions, each with 25 requests over one TCP
# connection. Each session uses a unique X-Session-Id; the server
# echoes it back. The runner verifies every response carries the
# right session ID with no cross-pollution.
REQS_PER_SESSION=25
SESSIONS="alpha beta gamma delta"

run_session() {
    sid="$1"
    out="$TMPDIR/out-$sid"
    : > "$out"
    # Multi-URL curl re-uses one TCP connection by default.
    # Distinct -o files per URL so each response stays separate.
    args=""
    n=0
    while [ "$n" -lt "$REQS_PER_SESSION" ]; do
        args="$args http://127.0.0.1:18104/echo -o $out.$n"
        n=$((n + 1))
    done
    # shellcheck disable=SC2086
    curl --silent --show-error --max-time 10 \
        -H "X-Session-Id: $sid" \
        $args 2>"$TMPDIR/curl-$sid.err"
}

# Start all 4 in parallel.
for sid in $SESSIONS; do
    run_session "$sid" &
done
wait

# Verify every response from every session contains the right
# echoed session ID and nothing else.
fail=0
for sid in $SESSIONS; do
    n=0
    while [ "$n" -lt "$REQS_PER_SESSION" ]; do
        f="$TMPDIR/out-$sid.$n"
        if [ ! -s "$f" ]; then
            echo "  [FAIL] session $sid request $n: empty/missing response"
            fail=1
            break
        fi
        body=$(cat "$f")
        if [ "$body" != "echo:$sid" ]; then
            echo "  [FAIL] session $sid request $n: expected 'echo:$sid', got '$body'"
            fail=1
            break
        fi
        n=$((n + 1))
    done
done

if [ "$fail" -ne 0 ]; then
    echo "--- server log ---"
    head -30 "$TMPDIR/srv.log"
    exit 1
fi

total=$((4 * REQS_PER_SESSION))
echo "  [PASS] http_server_actor_dispatch: $total parallel keep-alive responses, no cross-session pollution"
