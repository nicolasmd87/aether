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
# DESIGN — see test_http_reverse_proxy_pool.sh part 1 for full
# rationale. Per-subtest spawn (3 upstreams + 1 proxy, fresh each
# test, killed via SIGKILL after). No `set -e`. count_get retries
# on transient curl failures and exits loudly on persistent ones.

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

# ----- helpers (mirror of part 1) -----------------------------

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
# to CLOSED (state=0). Breaker state transitions are LAZY: the
# proxy's admit() check only re-evaluates the OPEN→HALF_OPEN
# timeout when a request actually arrives, so polling /proxy-metrics
# without traffic would never observe recovery. Each iteration fires
# one probe and rechecks the gauge.
wait_for_breaker_recovery() {
    upstream_url="$1"
    deadline_sec="${2:-10}"
    deadline=$(($(date +%s) + deadline_sec))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        curl -s -o /dev/null --max-time 3 "$PROXY/echo" 2>/dev/null || true
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

# ---- Test 9: circuit breaker half-open recovery ------------
start_all proxy_breaker
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503" || true
parallel_echo 8
wait_for_breaker_state "http://localhost:19102" 1 || exit 1
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/200" || true
wait_for_breaker_recovery "http://localhost:19102" || exit 1
B_AFTER_RECOVERY=$(count_get 19102)
i=0
while [ $i -lt 9 ]; do
    curl -s -o /dev/null --max-time 3 "$PROXY/echo" || true
    i=$((i + 1))
done
B_FINAL=$(count_get 19102)
DELTA=$((B_FINAL - B_AFTER_RECOVERY))
[ "$DELTA" -ge 2 ] || fail "breaker recovery: B delta=$DELTA expected ≥2"
stop_all

# ---- Test 10: cache hit — counter unchanged on second call --
start_all proxy_cache
curl -s -o /dev/null --max-time 3 "$PROXY/echo_cacheable" || true
COUNT_BEFORE=$(count_total)
i=0
while [ $i -lt 5 ]; do
    curl -s -o /dev/null --max-time 3 "$PROXY/echo_cacheable" || true
    i=$((i + 1))
done
COUNT_AFTER=$(count_total)
DELTA=$((COUNT_AFTER - COUNT_BEFORE))
[ "$DELTA" = "0" ] || fail "cache hit: counter delta=$DELTA expected 0"
HDRS=$(curl -s -D - --max-time 3 -o /dev/null "$PROXY/echo_cacheable" 2>/dev/null)
echo "$HDRS" | grep -qi '^X-Cache: HIT' || fail "cache hit: missing X-Cache: HIT header"
stop_all

# ---- Test 11: cache TTL expiry ------------------------------
# Wall-clock dependency on `Cache-Control: max-age=1`. Sleep 1.2s
# = 1s TTL + 200ms slack; bounded by cache semantics we own, not
# CI runner speed.
start_all proxy_cache
curl -s -o /dev/null --max-time 3 "$PROXY/echo_cacheable_ttl" || true
COUNT_BEFORE=$(count_total)
sleep 1.2
curl -s -o /dev/null --max-time 3 "$PROXY/echo_cacheable_ttl" || true
COUNT_AFTER=$(count_total)
DELTA=$((COUNT_AFTER - COUNT_BEFORE))
[ "$DELTA" -ge 1 ] || fail "cache TTL: counter delta=$DELTA expected ≥1"
stop_all

# ---- Test 12: cache Vary — different Accept-Encoding = different entry --
start_all proxy_cache
curl -s -o /dev/null --max-time 3 -H 'Accept-Encoding: gzip' "$PROXY/echo_vary" || true
COUNT_BEFORE=$(count_total)
curl -s -o /dev/null --max-time 3 -H 'Accept-Encoding: gzip' "$PROXY/echo_vary" || true
COUNT_AFTER_GZIP=$(count_total)
[ "$COUNT_AFTER_GZIP" = "$COUNT_BEFORE" ] || fail "cache Vary: same key delta=$((COUNT_AFTER_GZIP - COUNT_BEFORE)) expected 0"
curl -s -o /dev/null --max-time 3 -H 'Accept-Encoding: identity' "$PROXY/echo_vary" || true
COUNT_AFTER_BOTH=$(count_total)
[ $((COUNT_AFTER_BOTH - COUNT_AFTER_GZIP)) -ge 1 ] \
    || fail "cache Vary: identity didn't refetch (delta=$((COUNT_AFTER_BOTH - COUNT_AFTER_GZIP)))"
stop_all

# ---- Test 13: idempotent retry on 5xx ----------------------
start_all proxy_retry
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503" || true
i=0
ALL_OK=1
while [ $i -lt 9 ]; do
    body=$(curl -s --max-time 5 "$PROXY/echo" 2>/dev/null)
    tag=$(echo "$body" | head -1 | cut -d: -f1)
    case "$tag" in
        tag=A|tag=B|tag=C) : ;;  # ok — proxy returned a tagged response
        *) ALL_OK=0 ;;
    esac
    i=$((i + 1))
done
[ "$ALL_OK" = "1" ] || fail "retry: not all 9 requests returned a tag= response"
stop_all

# ---- Test 14: per-upstream rate limit (1 rps) -------------
start_all proxy_rate_limit
URLS=""
i=0
while [ $i -lt 12 ]; do
    URLS="$URLS $PROXY/echo"
    i=$((i + 1))
done
RESULTS=$(curl -Z -s -o /dev/null -w '%{http_code}\n' --max-time 3 $URLS 2>/dev/null)
SUCCESSES=$(printf '%s\n' "$RESULTS" | grep -c '^200$' || true)
FIVE_OH_THREES=$(printf '%s\n' "$RESULTS" | grep -c '^503$' || true)
[ "$SUCCESSES" -le 6 ] || fail "rate limit: $SUCCESSES successes (expected ≤6 with parallel arrivals)"
[ "$FIVE_OH_THREES" -ge 6 ] || fail "rate limit: only $FIVE_OH_THREES 503s (expected ≥6)"
stop_all

# ---- Test 15: W3C Trace-Context inject -------------------
start_all proxy_trace
INBOUND='00-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-bbbbbbbbbbbbbbbb-01'
RESP=$(curl -s --max-time 3 -H "traceparent: $INBOUND" "$PROXY/echo" 2>/dev/null)
echo "$RESP" | head -1 | grep -qE '^tag=[ABC]' || fail "trace passthrough: no tag in response"
stop_all

# ---- Test 16: Prometheus metrics endpoint ----------------
start_all proxy_rr
i=0
URLS=""
while [ $i -lt 6 ]; do
    URLS="$URLS $PROXY/echo"
    i=$((i + 1))
done
curl -Z -s -o /dev/null --max-time 5 $URLS >/dev/null 2>&1 || true
METRICS=$(curl -s --max-time 3 "$PROXY/proxy-metrics" 2>/dev/null)
echo "$METRICS" | grep -q 'aether_proxy_upstream_requests_total' || fail "metrics: requests_total absent"
echo "$METRICS" | grep -q 'aether_proxy_upstream_inflight'        || fail "metrics: inflight absent"
echo "$METRICS" | grep -q 'aether_proxy_upstream_healthy'         || fail "metrics: healthy absent"
echo "$METRICS" | grep -q 'aether_proxy_upstream_breaker_state'   || fail "metrics: breaker absent"
echo "$METRICS" | grep -q 'aether_proxy_cache_hits_total'         || fail "metrics: cache_hits absent"
echo "$METRICS" | grep -q '# TYPE'                                || fail "metrics: TYPE block absent"
stop_all

echo "  [PASS] http_reverse_proxy_pool_extra: 8/8 — breaker-recovery/cache(3)/retry/rate-limit/trace/metrics"
