#!/bin/sh
# std.http.proxy pool/LB/health/breaker integration tests, part 1 of 2.
#
# Tests 1-8 live here: round-robin / weighted-RR / ip-hash / cookie-
# hash / drain / health (× 2) / breaker-open. The remaining tests
# (breaker-recovery, cache × 3, retry, rate-limit, trace, metrics)
# live in test_http_reverse_proxy_pool_extra.sh.
#
# DESIGN — warm upstreams, polling waits, no timeout band-aids.
#
# 1. Three upstreams (A, B, C on :19101/19102/19103) spawn ONCE at
#    the top. They stay alive across every subtest. Per subtest we
#    only swap the proxy (whose mode varies). Between subtests we
#    POST /admin/reset to each upstream — that endpoint zeros the
#    request counter, the force_503 flag, and the simulated latency
#    knob. Saves 7 of 8 upstream-startups per file × the slow
#    Windows fork+exec cost.
#
# 2. Health / breaker / port-ready waits poll `/proxy-metrics`
#    (or the /health endpoint) for the deterministic
#    state-transition rather than `sleep N`. Polling waits for the
#    actual observable condition; sleep guesses at how slow the
#    runner is and either masks bugs (too long) or false-fails
#    (too short).
#
# 3. Cleanup uses SIGKILL — the aether http_server's signal handler
#    didn't reap SIGTERM cleanly on Windows MSYS2, leaving `wait
#    $pid` blocked indefinitely. SIGKILL maps to TerminateProcess on
#    MSYS2: synchronous, uninterceptable, no `wait` needed.
#
# 4. NO `set -e`. Every transient curl flake (Windows MSYS2 has
#    plenty) would otherwise kill the script with no error message,
#    surfacing in CI as the dreaded "(no output)" failure. Real
#    failures call `fail` or `exit 1` explicitly with a diagnostic.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if ! command -v curl >/dev/null 2>&1; then
    echo "  [SKIP] curl not on PATH"
    exit 0
fi

TMPDIR="$(mktemp -d)"
PIDS=""
PROXY_PID=""

cleanup() {
    if [ -n "$PROXY_PID" ]; then
        kill -9 "$PROXY_PID" 2>/dev/null || true
    fi
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
    # Stop bash from tracking this child as a job — silences the
    # "Killed PID command" stderr lines that otherwise leak into
    # the test runner's stderr file when stop_proxy calls kill -9.
    disown "$new_pid" 2>/dev/null || true
    PIDS="$PIDS $new_pid"
    eval "PID_$role=\$new_pid"
}

# Poll /health on a port until it answers. The /health endpoint is
# registered on every upstream (A/B/C); the proxy is mounted at
# /echo so /health on the proxy 404s, which is also a positive
# "bound and answering" signal. Bounded by deadline; on timeout
# the build fails loudly with the role's log dumped — no silent
# wait-forever path.
wait_for_port() {
    port="$1"
    deadline=$(($(date +%s) + 15))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        # --connect-timeout fails fast on a refused port; without
        # it Windows MSYS2 curl waits the full --max-time before
        # retrying.
        if curl -s -o /dev/null --connect-timeout 0.3 --max-time 1 \
                "http://127.0.0.1:$port/health" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    echo "  [FAIL] port $port never accepted"; exit 1
}

# ----- warm-upstream lifecycle ---------------------------------

start_three_upstreams() {
    start_proc upstream_a
    start_proc upstream_b
    start_proc upstream_c
    wait_for_port 19101
    wait_for_port 19102
    wait_for_port 19103
}

# Reset every upstream's per-subtest state (counter, force_503,
# eta) so the next subtest sees a clean state without respawning
# the upstreams. Sequential per-port — `curl -Z` with multiple `-X`
# flags has reportedly inconsistent behaviour across MSYS2 curl
# builds; sequential is portable and adds <50ms total on localhost.
# Verifies counter actually reads 0 after reset; on persistent
# failure exits with a clear diagnostic so a broken /admin/reset
# surfaces immediately rather than as a confusing downstream
# arithmetic anomaly.
reset_upstreams() {
    for port in 19101 19102 19103; do
        curl -s -o /dev/null --max-time 3 \
            -X POST "http://127.0.0.1:$port/admin/reset" >/dev/null 2>&1 || true
    done
    for port in 19101 19102 19103; do
        ok=0
        for attempt in 1 2 3 4 5; do
            v=$(curl -s --max-time 3 "http://127.0.0.1:$port/count" 2>/dev/null)
            if [ "$v" = "0" ]; then ok=1; break; fi
            sleep 0.1
        done
        if [ "$ok" != "1" ]; then
            echo "  [FAIL] reset_upstreams: port $port did not return counter=0 (got '$v')"
            exit 1
        fi
    done
}

start_proxy() {
    start_proc "$1"
    PROXY_PID="$new_pid"
    wait_for_port 19100
}

# Kill the per-subtest proxy (only) without touching upstreams.
# kill -9 maps to TerminateProcess on MSYS2 (synchronous); no
# `wait` needed and no `lsof` poll for port release — the next
# proxy's bind uses SO_REUSEADDR so any TIME_WAIT residue is
# tolerated.
stop_proxy() {
    if [ -n "$PROXY_PID" ]; then
        kill -9 "$PROXY_PID" 2>/dev/null || true
        # Drop the dead PID from $PIDS so cleanup() doesn't try to
        # double-kill it on exit.
        new_pids=""
        for pid in $PIDS; do
            if [ "$pid" != "$PROXY_PID" ]; then
                new_pids="$new_pids $pid"
            fi
        done
        PIDS="$new_pids"
        PROXY_PID=""
    fi
}

# Per-subtest setup: reset upstream state, spawn the proxy.
setup() {
    proxy_role="$1"
    reset_upstreams
    start_proxy "$proxy_role"
}

# Backward-compatible alias — the per-subtest body sites still
# read `stop_all`; today only the proxy needs killing, the
# upstreams stay warm.
stop_all() {
    stop_proxy
}

PROXY="http://127.0.0.1:19100"
fail() { echo "  [FAIL] $1"; exit 1; }

# ----- polling-based state waits -------------------------------
# These replace the old sleep-N "should be enough on most CI
# runners" pattern. Polling waits for the actual deterministic
# state transition published on /proxy-metrics; on a fast machine
# the wait collapses to a single round-trip, on a slow machine
# the wait stretches until the state change actually happens.

# Wait until the proxy reports the named upstream's health-check
# state matches `expected_value` (0 = unhealthy, 1 = healthy).
# upstream_url is the URL the proxy was configured with (e.g.
# http://localhost:19102). Bounded by `deadline_sec`; on timeout
# the test fails with a clear diagnostic.
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

# Wait until the named upstream's circuit breaker matches
# `expected_state` (0 = closed, 1 = open, 2 = half_open).
wait_for_breaker_state() {
    upstream_url="$1"
    expected_state="$2"
    deadline_sec="${3:-10}"
    deadline=$(($(date +%s) + deadline_sec))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        body=$(curl -s --max-time 2 "$PROXY/proxy-metrics" 2>/dev/null || true)
        if echo "$body" | grep -qF \
            "aether_proxy_upstream_breaker_state{upstream=\"${upstream_url}\"} ${expected_state}"; then
            return 0
        fi
        sleep 0.1
    done
    echo "  [FAIL] $upstream_url breaker did not reach state=$expected_state within ${deadline_sec}s"
    return 1
}

# ----- request helpers -----------------------------------------

# Drive N parallel /echo requests via curl's libcurl multiplexer.
# Used by tests that only need the proxy to observe N arrivals
# (counter checks, breaker / health detection). Sequential
# `while curl` loops accumulated ~150 ms × N latency on Windows
# CI; one curl process amortises that to roughly the slowest
# single request.
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
# return (Windows MSYS2 transient curl failure) would otherwise be
# interpreted by bash arithmetic as 0, producing false-positive
# delta=0 cache assertions that look like a flaky proxy when really
# the test harness lost the counter read.
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

# ----- spawn the warm upstreams ONCE ---------------------------
start_three_upstreams

# ---- Test 1: round-robin distribution ---------------------
setup proxy_rr
i=0
while [ $i -lt 9 ]; do
    curl -s -o /dev/null --max-time 3 "$PROXY/echo"
    i=$((i + 1))
done
[ "$(count_get 19101)" = "3" ] || fail "RR counter A: $(count_get 19101) expected 3"
[ "$(count_get 19102)" = "3" ] || fail "RR counter B: $(count_get 19102) expected 3"
[ "$(count_get 19103)" = "3" ] || fail "RR counter C: $(count_get 19103) expected 3"
stop_all

# ---- Test 2: weighted RR (3:1:1) — 50 requests ----------
setup proxy_wrr
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
setup proxy_ip_hash
i=0
TAG=""
while [ $i -lt 12 ]; do
    body=$(curl -s --max-time 3 -H 'X-Forwarded-For: 192.0.2.42' "$PROXY/echo")
    cur_tag=$(echo "$body" | head -1 | cut -d: -f1)
    if [ -z "$TAG" ]; then TAG="$cur_tag"; fi
    [ "$cur_tag" = "$TAG" ] || fail "ip_hash inconsistent: $cur_tag vs $TAG"
    i=$((i + 1))
done
stop_all

# ---- Test 4: cookie_hash determinism ----------------------
setup proxy_cookie_hash
i=0
TAG=""
while [ $i -lt 12 ]; do
    body=$(curl -s --max-time 3 -H 'Cookie: SESSIONID=user-12345' "$PROXY/echo")
    cur_tag=$(echo "$body" | head -1 | cut -d: -f1)
    if [ -z "$TAG" ]; then TAG="$cur_tag"; fi
    [ "$cur_tag" = "$TAG" ] || fail "cookie_hash inconsistent: $cur_tag vs $TAG"
    i=$((i + 1))
done
TAG2=""
i=0
while [ $i -lt 8 ]; do
    body=$(curl -s --max-time 3 -H 'Cookie: SESSIONID=user-other' "$PROXY/echo")
    cur_tag=$(echo "$body" | head -1 | cut -d: -f1)
    if [ -z "$TAG2" ]; then TAG2="$cur_tag"; fi
    [ "$cur_tag" = "$TAG2" ] || fail "cookie_hash second-cookie inconsistent"
    i=$((i + 1))
done
stop_all

# ---- Test 5: drain — A drained on startup; A counter stays 0 -
setup proxy_drain
parallel_echo 12
A=$(count_get 19101); B=$(count_get 19102); C=$(count_get 19103)
[ "$A" = "0" ] || fail "drain: A served $A requests (expected 0)"
[ $((B + C)) -eq 12 ] || fail "drain: B+C=$((B + C)) expected 12"
stop_all

# ---- Test 6: health checks kill an unhealthy upstream -----
setup proxy_health
# Force B's /health endpoint to start serving 503.
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503"
# Poll the proxy's /proxy-metrics until it sees B as unhealthy
# (instead of sleeping for an estimated threshold * interval). On
# fast machines this collapses to ~one health-check interval; on
# slow machines it waits as long as the actual state transition
# takes.
wait_for_upstream_health "http://localhost:19102" 0 || exit 1
parallel_echo 12
B=$(count_get 19102)
# Up to 2 requests may have landed on B before the health-check
# thread observed and broadcast the unhealthy state.
[ "$B" -le 2 ] || fail "health: B served $B requests after marked unhealthy"
stop_all

# ---- Test 7: health recovery — flip B back, it rejoins -----
setup proxy_health
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503"
wait_for_upstream_health "http://localhost:19102" 0 || exit 1
parallel_echo 6
B_BEFORE=$(count_get 19102)
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/200"
wait_for_upstream_health "http://localhost:19102" 1 || exit 1
parallel_echo 12
B_AFTER=$(count_get 19102)
DELTA=$((B_AFTER - B_BEFORE))
[ "$DELTA" -ge 2 ] || fail "health recovery: B delta=$DELTA expected ≥2"
stop_all

# ---- Test 8: circuit breaker opens after consecutive failures ----
setup proxy_breaker
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503"
parallel_echo 12
B=$(count_get 19102)
# Once the breaker opens (at threshold=3), B shouldn't receive
# more new requests. Allow a small tolerance for half-open probe.
[ "$B" -le 5 ] || fail "breaker: B served $B requests (expected ≤5)"
stop_all

echo "  [PASS] http_reverse_proxy_pool: 8/8 — RR/WRR/ip_hash/cookie_hash/drain/health(2)/breaker-open"
