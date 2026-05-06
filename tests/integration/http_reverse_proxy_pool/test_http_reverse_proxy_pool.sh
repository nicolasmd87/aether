#!/bin/sh
# std.http.proxy pool/LB/health/breaker integration tests, part 1 of 2.
#
# Tests 1-8 live here: round-robin / weighted-RR / ip-hash / cookie-
# hash / drain / health (× 2) / breaker-open. The remaining tests
# (breaker-recovery, cache × 3, retry, rate-limit, trace, metrics)
# live in test_http_reverse_proxy_pool_extra.sh.
#
# DESIGN — per-subtest spawn, polling waits, SIGKILL teardown.
#
# Why per-subtest spawn rather than warm-upstream + /admin/reset:
# the latter looks faster on paper (8 vs 1 spawn cycle) but
# accumulates fragile state between subtests on Windows MSYS2 in
# ways that cost more debug time than the spawn cost saves. Each
# subtest now starts 3 upstreams + 1 proxy, runs, kills all 4 —
# the same pattern the passing http_reverse_proxy test uses.
#
# Polling waits (no `sleep N` for state transitions): health and
# breaker waits poll /proxy-metrics for the actual gauge value. On
# fast machines this collapses to a single round-trip; on slow
# machines it stretches until the state actually changes.
#
# SIGKILL teardown: SIGTERM left `wait $pid` blocked indefinitely on
# MSYS2 because the http_server's signal handler didn't fully reap.
# kill -9 maps to TerminateProcess (synchronous, uninterceptable).
#
# NO `set -e`. Every transient curl flake (Windows MSYS2 has
# plenty) would otherwise kill the script with no error message,
# surfacing as the dreaded "(no output)" failure. Real failures
# call `fail` or `exit 1` explicitly with a diagnostic.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if ! command -v curl >/dev/null 2>&1; then
    echo "  [SKIP] curl not on PATH"
    exit 0
fi

TMPDIR="$(mktemp -d)"
PIDS=""

cleanup() {
    for pid in $PIDS; do
        kill -9 "$pid" 2>/dev/null || true
    done
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

cd "$SCRIPT_DIR"
if ! AETHER_HOME="$ROOT" "$AE" build server.ae -o "$TMPDIR/server" >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] build:"; head -30 "$TMPDIR/build.log"; exit 1
fi

# ----- helpers ------------------------------------------------

start_proc() {
    role="$1"
    log="$TMPDIR/$role.log"
    "$TMPDIR/server" "$role" >"$log" 2>&1 &
    new_pid=$!
    disown "$new_pid" 2>/dev/null || true
    PIDS="$PIDS $new_pid"
    eval "PID_$role=\$new_pid"
}

# Poll /health on the named port until it answers OR the named pid
# dies. On timeout, dump the role's log so failures explain
# themselves rather than appearing as a mysterious "(no output)".
wait_for_port() {
    role="$1"; port="$2"
    log="$TMPDIR/$role.log"
    pid=$(eval echo \$PID_$role)
    deadline=$(($(date +%s) + 15))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        if ! kill -0 "$pid" 2>/dev/null; then
            echo "  [FAIL] $role died:"; head -30 "$log"; exit 1
        fi
        if curl -s -o /dev/null --connect-timeout 0.3 --max-time 1 \
                "http://127.0.0.1:$port/health" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    echo "  [FAIL] $role never accepted on port $port:"; head -30 "$log"; exit 1
}

start_all() {
    proxy_role="$1"
    start_proc upstream_a
    start_proc upstream_b
    start_proc upstream_c
    start_proc "$proxy_role"
    wait_for_port upstream_a 19101
    wait_for_port upstream_b 19102
    wait_for_port upstream_c 19103
    wait_for_port "$proxy_role" 19100
}

stop_all() {
    for pid in $PIDS; do
        kill -9 "$pid" 2>/dev/null || true
    done
    PIDS=""
}

PROXY="http://127.0.0.1:19100"
fail() { echo "  [FAIL] $1"; exit 1; }

# ----- polling-based state waits -------------------------------
# Wait until the proxy reports the named upstream's health-check
# state matches `expected_value` (0 = unhealthy, 1 = healthy).

wait_for_upstream_health() {
    upstream_url="$1"
    expected_value="$2"
    deadline_sec="${3:-15}"
    deadline=$(($(date +%s) + deadline_sec))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        body=$(curl -s --max-time 2 "$PROXY/proxy-metrics" 2>/dev/null || true)
        if echo "$body" | grep -qF \
            "aether_proxy_upstream_healthy{upstream=\"${upstream_url}\"} ${expected_value}"; then
            return 0
        fi
        sleep 0.1
    done
    echo "  [FAIL] $upstream_url did not reach healthy=$expected_value within ${deadline_sec}s"
    return 1
}

# ----- request helpers -----------------------------------------

parallel_echo() {
    n="$1"
    URLS=""
    i=0
    while [ $i -lt $n ]; do
        URLS="$URLS $PROXY/echo"
        i=$((i + 1))
    done
    curl -Z -s -o /dev/null --max-time 5 $URLS >/dev/null 2>&1 || true
}

# count_get must return a non-empty integer or fail loudly. A blank
# return (transient curl failure) would otherwise be interpreted by
# bash arithmetic as 0, producing false-positive delta=0 cache
# assertions that look like a flaky proxy when really the test
# harness lost the counter read.
count_get() {
    for attempt in 1 2 3; do
        v=$(curl -s --max-time 3 "http://127.0.0.1:$1/count" 2>/dev/null)
        case "$v" in
            ''|*[!0-9]*) sleep 0.1; continue ;;
            *) printf '%s' "$v"; return 0 ;;
        esac
    done
    echo "  [FAIL] count_get $1 returned non-integer after 3 attempts: '$v'"
    exit 1
}

count_total() {
    a=$(count_get 19101)
    b=$(count_get 19102)
    c=$(count_get 19103)
    echo $((a + b + c))
}

# ---- Test 1: round-robin distribution ---------------------
start_all proxy_rr
i=0
while [ $i -lt 9 ]; do
    curl -s -o /dev/null --max-time 3 "$PROXY/echo" || true
    i=$((i + 1))
done
[ "$(count_get 19101)" = "3" ] || fail "RR counter A: $(count_get 19101) expected 3"
[ "$(count_get 19102)" = "3" ] || fail "RR counter B: $(count_get 19102) expected 3"
[ "$(count_get 19103)" = "3" ] || fail "RR counter C: $(count_get 19103) expected 3"
stop_all

# ---- Test 2: weighted RR (3:1:1) — 50 requests ----------
start_all proxy_wrr
URLS=""
i=0
while [ $i -lt 50 ]; do
    URLS="$URLS $PROXY/echo"
    i=$((i + 1))
done
curl -Z -s -o /dev/null --max-time 5 $URLS >/dev/null 2>&1 || true
A=$(count_get 19101); B=$(count_get 19102); C=$(count_get 19103)
[ "$A" -ge 25 ] && [ "$A" -le 35 ] || fail "WRR A=$A expected 25-35"
[ "$B" -ge 5  ] && [ "$B" -le 15 ] || fail "WRR B=$B expected 5-15"
[ "$C" -ge 5  ] && [ "$C" -le 15 ] || fail "WRR C=$C expected 5-15"
[ $((A + B + C)) -eq 50 ] || fail "WRR total=$((A + B + C)) expected 50"
stop_all

# ---- Test 3: ip_hash determinism --------------------------
start_all proxy_ip_hash
i=0
TAG=""
while [ $i -lt 12 ]; do
    body=$(curl -s --max-time 3 -H 'X-Forwarded-For: 192.0.2.42' "$PROXY/echo" 2>/dev/null)
    cur_tag=$(echo "$body" | head -1 | cut -d: -f1)
    if [ -z "$TAG" ]; then TAG="$cur_tag"; fi
    [ "$cur_tag" = "$TAG" ] || fail "ip_hash inconsistent: $cur_tag vs $TAG"
    i=$((i + 1))
done
stop_all

# ---- Test 4: cookie_hash determinism ----------------------
start_all proxy_cookie_hash
i=0
TAG=""
while [ $i -lt 12 ]; do
    body=$(curl -s --max-time 3 -H 'Cookie: SESSIONID=user-12345' "$PROXY/echo" 2>/dev/null)
    cur_tag=$(echo "$body" | head -1 | cut -d: -f1)
    if [ -z "$TAG" ]; then TAG="$cur_tag"; fi
    [ "$cur_tag" = "$TAG" ] || fail "cookie_hash inconsistent: $cur_tag vs $TAG"
    i=$((i + 1))
done
TAG2=""
i=0
while [ $i -lt 8 ]; do
    body=$(curl -s --max-time 3 -H 'Cookie: SESSIONID=user-other' "$PROXY/echo" 2>/dev/null)
    cur_tag=$(echo "$body" | head -1 | cut -d: -f1)
    if [ -z "$TAG2" ]; then TAG2="$cur_tag"; fi
    [ "$cur_tag" = "$TAG2" ] || fail "cookie_hash second-cookie inconsistent"
    i=$((i + 1))
done
stop_all

# ---- Test 5: drain — A drained on startup; A counter stays 0 -
start_all proxy_drain
parallel_echo 12
A=$(count_get 19101); B=$(count_get 19102); C=$(count_get 19103)
[ "$A" = "0" ] || fail "drain: A served $A requests (expected 0)"
[ $((B + C)) -eq 12 ] || fail "drain: B+C=$((B + C)) expected 12"
stop_all

# ---- Test 6: health checks kill an unhealthy upstream -----
start_all proxy_health
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503" || true
wait_for_upstream_health "http://localhost:19102" 0 || exit 1
parallel_echo 12
B=$(count_get 19102)
# Up to 2 requests may have landed on B before the health-check
# thread observed and broadcast the unhealthy state.
[ "$B" -le 2 ] || fail "health: B served $B requests after marked unhealthy"
stop_all

# ---- Test 7: health recovery — flip B back, it rejoins -----
start_all proxy_health
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503" || true
wait_for_upstream_health "http://localhost:19102" 0 || exit 1
parallel_echo 6
B_BEFORE=$(count_get 19102)
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/200" || true
wait_for_upstream_health "http://localhost:19102" 1 || exit 1
parallel_echo 12
B_AFTER=$(count_get 19102)
DELTA=$((B_AFTER - B_BEFORE))
[ "$DELTA" -ge 2 ] || fail "health recovery: B delta=$DELTA expected ≥2"
stop_all

# ---- Test 8: circuit breaker opens after consecutive failures ----
start_all proxy_breaker
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503" || true
parallel_echo 12
B=$(count_get 19102)
# Once the breaker opens (at threshold=3), B shouldn't receive
# more new requests. Allow a small tolerance for half-open probe.
[ "$B" -le 5 ] || fail "breaker: B served $B requests (expected ≤5)"
stop_all

echo "  [PASS] http_reverse_proxy_pool: 8/8 — RR/WRR/ip_hash/cookie_hash/drain/health(2)/breaker-open"
