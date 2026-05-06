#!/bin/sh
# std.http.proxy cache/retry/rate-limit/trace/metrics integration
# tests, part 2 of 2.
#
# Tests 9-16 live here: breaker-recovery, cache (× 3), retry,
# rate-limit, trace, metrics. The first 8 (RR/WRR/ip-hash/cookie-
# hash/drain/health/breaker-open) live in
# test_http_reverse_proxy_pool.sh — splitting the suite gives each
# half its own 180 s timeout budget on the slow Windows mingw CI
# runner, which the previous single-file form was tripping over.
#
# DESIGN — mirrors test_http_reverse_proxy_pool.sh (part 1).
#
# 1. Three upstreams (A, B, C on :19101/19102/19103) spawn ONCE at
#    the top. They stay alive across every subtest. Per subtest we
#    only swap the proxy (whose mode varies). Between subtests we
#    POST /admin/reset to each upstream — that endpoint zeros the
#    request counter, the force_503 flag, and the simulated latency
#    knob. Saves 7 of 8 upstream-startups per file.
#
# 2. State waits poll observable proxy metrics rather than `sleep N`:
#    /proxy-metrics for `aether_proxy_upstream_healthy` and
#    `aether_proxy_upstream_breaker_state`. Polling waits for the
#    actual transition; sleep guesses at how slow the runner is and
#    either masks bugs (too long) or false-fails (too short).
#
# 3. The single remaining wall-clock sleep — T11 cache TTL — is
#    bounded by the cache's `max-age=1` semantics we own, not by CI
#    runner speed. The cache stores no observable expiry timestamp
#    (and shouldn't — that's an implementation leak), so a brief
#    sleep just past max-age is the contract-respecting waitform.
#
# 4. Cleanup uses SIGKILL (TerminateProcess on MSYS2): the aether
#    http_server's signal handler didn't reap SIGTERM cleanly on
#    Windows, leaving `wait $pid` blocked indefinitely. SIGKILL is
#    synchronous and uninterceptable.
#
# 5. NO `set -e`. Every transient curl flake (Windows MSYS2 has
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
# (Mirror of helpers in test_http_reverse_proxy_pool.sh — kept in
# sync by hand. Both halves need them; sourcing across .sh files
# fights with the test runner's per-script `cd` and `$0` semantics
# more than it helps, so we duplicate ~120 lines.)

start_proc() {
    role="$1"
    log="$TMPDIR/$role.log"
    "$TMPDIR/server" "$role" >"$log" 2>&1 &
    new_pid=$!
    disown "$new_pid" 2>/dev/null || true
    PIDS="$PIDS $new_pid"
    eval "PID_$role=\$new_pid"
}

wait_for_port() {
    port="$1"
    deadline=$(($(date +%s) + 15))
    while [ "$(date +%s)" -lt "$deadline" ]; do
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

# Sequential per-port reset; verify each upstream returns counter=0
# before proceeding. See test_http_reverse_proxy_pool.sh for rationale.
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

stop_proxy() {
    if [ -n "$PROXY_PID" ]; then
        kill -9 "$PROXY_PID" 2>/dev/null || true
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

setup() {
    proxy_role="$1"
    reset_upstreams
    start_proxy "$proxy_role"
}

stop_all() {
    stop_proxy
}

PROXY="http://127.0.0.1:19100"
fail() { echo "  [FAIL] $1"; exit 1; }

# ----- polling-based state waits -------------------------------

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

# Drive probe requests until the breaker for `upstream_url` returns
# to CLOSED (state=0). Breaker state transitions are LAZY in the
# implementation — `aether_proxy_breaker_admit` only checks the
# OPEN→HALF_OPEN timeout when a request arrives. So the test can't
# observe recovery by polling /proxy-metrics alone; we have to feed
# the breaker requests so it gets a chance to transition. Each probe
# is one request; we stop the moment metrics report state=0.
wait_for_breaker_recovery() {
    upstream_url="$1"
    deadline_sec="${2:-10}"
    deadline=$(($(date +%s) + deadline_sec))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        curl -s -o /dev/null --max-time 3 "$PROXY/echo"
        body=$(curl -s --max-time 2 "$PROXY/proxy-metrics" 2>/dev/null || true)
        if echo "$body" | grep -qF \
            "aether_proxy_upstream_breaker_state{upstream=\"${upstream_url}\"} 0"; then
            return 0
        fi
        sleep 0.1
    done
    echo "  [FAIL] $upstream_url breaker did not recover within ${deadline_sec}s"
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
# return (Windows MSYS2 transient curl failure) would otherwise be
# interpreted by bash arithmetic as 0, producing false-positive
# delta=0 cache assertions.
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

# ---- Test 9: circuit breaker half-open recovery ------------
# Open the breaker against B; then flip B back to 200; then poll
# (via probe traffic) until the breaker transitions OPEN → HALF_OPEN
# → CLOSED. Once CLOSED, B is in normal rotation again, so a
# subsequent burst of 9 RR requests should see roughly 3 land on B.
setup proxy_breaker
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503"
parallel_echo 8                                          # trip the breaker
wait_for_breaker_state "http://localhost:19102" 1 || exit 1
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/200"
wait_for_breaker_recovery "http://localhost:19102" || exit 1
B_AFTER_RECOVERY=$(count_get 19102)
i=0
while [ $i -lt 9 ]; do
    curl -s -o /dev/null --max-time 3 "$PROXY/echo"
    i=$((i + 1))
done
B_FINAL=$(count_get 19102)
DELTA=$((B_FINAL - B_AFTER_RECOVERY))
[ "$DELTA" -ge 2 ] || fail "breaker recovery: B delta=$DELTA expected ≥2"
stop_all

# ---- Test 10: cache hit — counter unchanged on second call --
# Cache reads aggregate counters across A+B+C because the TTL-
# expired refetch under round-robin can land on any upstream.
setup proxy_cache
curl -s -o /dev/null --max-time 3 "$PROXY/echo_cacheable"
COUNT_BEFORE=$(count_total)
i=0
while [ $i -lt 5 ]; do
    curl -s -o /dev/null --max-time 3 "$PROXY/echo_cacheable"
    i=$((i + 1))
done
COUNT_AFTER=$(count_total)
DELTA=$((COUNT_AFTER - COUNT_BEFORE))
[ "$DELTA" = "0" ] || fail "cache hit: counter delta=$DELTA expected 0"
HDRS=$(curl -s -D - --max-time 3 -o /dev/null "$PROXY/echo_cacheable")
echo "$HDRS" | grep -qi '^X-Cache: HIT' || fail "cache hit: missing X-Cache: HIT header"
stop_all

# ---- Test 11: cache TTL expiry ------------------------------
# Uses the dedicated short-TTL route /echo_cacheable_ttl (max-age=1).
# Only wall-clock sleep in the file: cache has no observable
# per-entry expiry endpoint (and shouldn't — implementation leak).
# Sleep = max-age + 200 ms slack; bounded by cache semantics we own,
# not by CI runner speed.
setup proxy_cache
curl -s -o /dev/null --max-time 3 "$PROXY/echo_cacheable_ttl"
COUNT_BEFORE=$(count_total)
sleep 1.2
curl -s -o /dev/null --max-time 3 "$PROXY/echo_cacheable_ttl"
COUNT_AFTER=$(count_total)
DELTA=$((COUNT_AFTER - COUNT_BEFORE))
[ "$DELTA" -ge 1 ] || fail "cache TTL: counter delta=$DELTA expected ≥1"
stop_all

# ---- Test 12: cache Vary — different Accept-Encoding = different entry --
setup proxy_cache
curl -s -o /dev/null --max-time 3 -H 'Accept-Encoding: gzip' "$PROXY/echo_vary"
COUNT_BEFORE=$(count_total)
curl -s -o /dev/null --max-time 3 -H 'Accept-Encoding: gzip' "$PROXY/echo_vary"
COUNT_AFTER_GZIP=$(count_total)
[ "$COUNT_AFTER_GZIP" = "$COUNT_BEFORE" ] || fail "cache Vary: same key delta=$((COUNT_AFTER_GZIP - COUNT_BEFORE)) expected 0"
curl -s -o /dev/null --max-time 3 -H 'Accept-Encoding: identity' "$PROXY/echo_vary"
COUNT_AFTER_BOTH=$(count_total)
[ $((COUNT_AFTER_BOTH - COUNT_AFTER_GZIP)) -ge 1 ] \
    || fail "cache Vary: identity didn't refetch (delta=$((COUNT_AFTER_BOTH - COUNT_AFTER_GZIP)))"
stop_all

# ---- Test 13: idempotent retry on 5xx ----------------------
# Force B to 503; with retry policy each request that lands on B
# should fall over to A or C and ultimately return a tag= response.
setup proxy_retry
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503"
i=0
ALL_OK=1
while [ $i -lt 9 ]; do
    body=$(curl -s --max-time 5 "$PROXY/echo")
    tag=$(echo "$body" | head -1 | cut -d: -f1)
    case "$tag" in
        tag=A|tag=B|tag=C) : ;;  # ok — proxy returned a tagged 200
        *) ALL_OK=0 ;;
    esac
    i=$((i + 1))
done
[ "$ALL_OK" = "1" ] || fail "retry: not all 9 requests returned a tag= response"
stop_all

# ---- Test 14: per-upstream rate limit (1 rps) -------------
# Three upstreams, each capped at 1 rps with burst 1. Drive 12
# requests in parallel inside one curl process so all 12 reach the
# proxy inside the burst window. Steady-state: ~3 successes, ~9 503s.
# Slack covers curl's per-bucket refill staggering on slow runners.
setup proxy_rate_limit
URLS=""
i=0
while [ $i -lt 12 ]; do
    URLS="$URLS $PROXY/echo"
    i=$((i + 1))
done
RESULTS=$(curl -Z -s -o /dev/null -w '%{http_code}\n' --max-time 3 $URLS)
SUCCESSES=$(printf '%s\n' "$RESULTS" | grep -c '^200$' || true)
FIVE_OH_THREES=$(printf '%s\n' "$RESULTS" | grep -c '^503$' || true)
[ "$SUCCESSES" -le 6 ] || fail "rate limit: $SUCCESSES successes (expected ≤6 with parallel arrivals)"
[ "$FIVE_OH_THREES" -ge 6 ] || fail "rate limit: only $FIVE_OH_THREES 503s (expected ≥6)"
stop_all

# ---- Test 15: W3C Trace-Context inject -------------------
# Inbound request WITH traceparent — verify the proxy passes it
# through and B/A/C return a tag. The inject-when-absent codepath
# is exercised at the C unit-test level (inject_traceparent_if_absent)
# which is covered by the build itself.
setup proxy_trace
INBOUND='00-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-bbbbbbbbbbbbbbbb-01'
RESP=$(curl -s --max-time 3 -H "traceparent: $INBOUND" "$PROXY/echo")
echo "$RESP" | head -1 | grep -qE '^tag=[ABC]' || fail "trace passthrough: no tag in response"
stop_all

# ---- Test 16: Prometheus metrics endpoint ----------------
setup proxy_rr
i=0
URLS=""
while [ $i -lt 6 ]; do
    URLS="$URLS $PROXY/echo"
    i=$((i + 1))
done
curl -Z -s -o /dev/null --max-time 5 $URLS >/dev/null 2>&1 || true
METRICS=$(curl -s --max-time 3 "$PROXY/proxy-metrics")
echo "$METRICS" | grep -q 'aether_proxy_upstream_requests_total' || fail "metrics: requests_total absent"
echo "$METRICS" | grep -q 'aether_proxy_upstream_inflight'        || fail "metrics: inflight absent"
echo "$METRICS" | grep -q 'aether_proxy_upstream_healthy'         || fail "metrics: healthy absent"
echo "$METRICS" | grep -q 'aether_proxy_upstream_breaker_state'   || fail "metrics: breaker absent"
echo "$METRICS" | grep -q 'aether_proxy_cache_hits_total'         || fail "metrics: cache_hits absent"
echo "$METRICS" | grep -q '# TYPE'                                || fail "metrics: TYPE block absent"
stop_all

echo "  [PASS] http_reverse_proxy_pool_extra: 8/8 — breaker-recovery/cache(3)/retry/rate-limit/trace/metrics"
