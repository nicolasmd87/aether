#!/bin/sh
# #260 Tier 2: HTTP/2 server end-to-end test.
#
# Verifies:
#   1. The server advertises h2 via http.server_set_h2() — when the
#      build doesn't link libnghttp2, the driver prints
#      READY-NOH2 and the test cleanly skips.
#   2. `curl --http2-prior-knowledge` over plain HTTP succeeds and
#      returns the expected body. (This is the simplest h2c smoke
#      test — bypasses the Upgrade negotiation, just speaks h2
#      framed-protocol on a plain TCP socket from the first byte.)
#   3. `curl --http2 -X POST` with a body successfully echoes back
#      via the /echo route, exercising HEADERS + DATA frame paths
#      and the request-body assembly across DATA chunks.
#   4. curl reports protocol="HTTP/2" in -w '%{http_version}'.
#
# Skips cleanly when curl is missing or doesn't link libnghttp2.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if ! command -v curl >/dev/null 2>&1; then
    echo "  [SKIP] curl not on PATH"
    exit 0
fi

# Curl needs to itself link libnghttp2 to speak --http2 / -prior-knowledge.
if ! curl --version 2>&1 | grep -q nghttp2; then
    echo "  [SKIP] curl built without HTTP/2 (no nghttp2)"
    exit 0
fi

TMPDIR="$(mktemp -d)"
SRV_PID=""
cleanup() {
    if [ -n "$SRV_PID" ]; then
        kill "$SRV_PID" 2>/dev/null || true
        wait "$SRV_PID" 2>/dev/null || true
    fi
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

AETHER_HOME="$ROOT" "$AE" run "$SCRIPT_DIR/server.ae" >"$TMPDIR/srv.log" 2>&1 &
SRV_PID=$!

# Wait for READY (or READY-NOH2 to skip).
deadline=$(($(date +%s) + 8))
ready=""
while [ "$(date +%s)" -lt "$deadline" ]; do
    if grep -q '^READY' "$TMPDIR/srv.log" 2>/dev/null; then
        ready=$(grep '^READY' "$TMPDIR/srv.log" | head -1)
        break
    fi
    if ! kill -0 "$SRV_PID" 2>/dev/null; then
        echo "  [FAIL] server died:"
        head -20 "$TMPDIR/srv.log"
        exit 1
    fi
    sleep 0.1
done

if [ -z "$ready" ]; then
    echo "  [FAIL] server didn't print READY within timeout"
    head -20 "$TMPDIR/srv.log"
    exit 1
fi

if echo "$ready" | grep -q READY-NOH2; then
    echo "  [SKIP] $ready"
    exit 0
fi

# Give the server one more breath after READY.
sleep 0.3

PORT=18260
URL="http://127.0.0.1:$PORT/"

# Test 1 — GET via prior-knowledge h2c. -w prints the negotiated
# HTTP version after curl finishes; we assert HTTP/2 specifically.
RESP="$TMPDIR/get.body"
HTTPVER=$(curl --silent --show-error --max-time 5 \
              --http2-prior-knowledge \
              -o "$RESP" \
              -w '%{http_version}' \
              "$URL" 2>"$TMPDIR/curl1.err") || {
    echo "  [FAIL] curl --http2-prior-knowledge GET failed:"
    cat "$TMPDIR/curl1.err"
    exit 1
}

if [ "$HTTPVER" != "2" ]; then
    echo "  [FAIL] expected http_version=2, got '$HTTPVER'"
    cat "$TMPDIR/curl1.err"
    exit 1
fi

if ! grep -q '^h2-ok$' "$RESP"; then
    echo "  [FAIL] GET body mismatch"
    cat "$RESP"
    exit 1
fi

# Test 2 — POST /echo with a body. Exercises HEADERS + DATA frames
# in both directions. The handler echoes the request body verbatim.
ECHO_BODY="hello-from-h2-stream"
RESP2="$TMPDIR/echo.body"
HTTPVER2=$(curl --silent --show-error --max-time 5 \
               --http2-prior-knowledge \
               -X POST \
               -H 'Content-Type: text/plain' \
               -d "$ECHO_BODY" \
               -o "$RESP2" \
               -w '%{http_version}' \
               "${URL}echo" 2>"$TMPDIR/curl2.err") || {
    echo "  [FAIL] curl --http2 POST /echo failed:"
    cat "$TMPDIR/curl2.err"
    exit 1
}

if [ "$HTTPVER2" != "2" ]; then
    echo "  [FAIL] echo http_version=2 expected, got '$HTTPVER2'"
    exit 1
fi

if ! grep -q "^${ECHO_BODY}\$" "$RESP2"; then
    echo "  [FAIL] echo body mismatch — expected '$ECHO_BODY', got:"
    cat "$RESP2"
    exit 1
fi

# Test 3 — h2c upgrade path. curl --http2 (without -prior-knowledge)
# starts on HTTP/1.1 and offers Upgrade: h2c; if the server accepts,
# curl reports http_version=2 in the -w output. Some curl builds
# don't actively probe upgrade and stay on 1.1 even when the server
# advertises h2c — we accept either outcome here, only flagging
# truly broken responses.
HTTPVER3=$(curl --silent --show-error --max-time 5 \
               --http2 \
               -o "$TMPDIR/upgrade.body" \
               -w '%{http_version}' \
               "$URL" 2>"$TMPDIR/curl3.err") || {
    echo "  [FAIL] curl --http2 (upgrade probe) GET failed:"
    cat "$TMPDIR/curl3.err"
    exit 1
}

if [ "$HTTPVER3" != "2" ] && [ "$HTTPVER3" != "1.1" ]; then
    echo "  [FAIL] unexpected http_version='$HTTPVER3' on upgrade probe"
    exit 1
fi

if ! grep -q '^h2-ok$' "$TMPDIR/upgrade.body"; then
    echo "  [FAIL] upgrade-probe response body mismatch"
    cat "$TMPDIR/upgrade.body"
    exit 1
fi

echo "  [PASS] HTTP/2 server (issue #260 Tier 2)"
