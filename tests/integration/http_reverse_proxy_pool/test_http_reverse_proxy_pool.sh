#!/bin/sh
# std.http.proxy pool/LB/health/breaker integration tests, part 1 of 2.
#
# Tests 1-8 live here: round-robin / weighted-RR / ip-hash / cookie-
# hash / drain / health (× 2) / breaker-open. The remaining tests
# (breaker-recovery, cache × 3, retry, rate-limit, trace, metrics)
# live in test_http_reverse_proxy_pool_extra.sh — splitting the
# suite gives each half its own 180 s timeout budget on the slow
# Windows mingw CI runner, which the previous single-file form
# was tripping over.
#
# Each scenario boots three upstream processes (A, B, C on
# :19101/19102/19103) plus a proxy process (mode-specific config
# on :19100), runs the assertions, then tears all four down before
# moving on.

set -e

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
    # SIGKILL (-9), not SIGTERM. The aether http_server's signal
    # handler doesn't reap SIGTERM cleanly on Windows MSYS2, which
    # left `wait $pid` blocking forever in CI — pinning the test
    # at 180 s timeout despite the test logic itself finishing in
    # ~16 s locally. SIGKILL maps to TerminateProcess on MSYS2,
    # synchronous and uninterceptable, so the pid is dead by the
    # time `kill -9` returns. Drop the matching `wait` calls —
    # there's nothing left to wait on, and adding them back risks
    # the same hang.
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
    # the test runner's stderr file when stop_all calls kill -9.
    # The process is still our child for kill / wait purposes.
    disown "$new_pid" 2>/dev/null || true
    PIDS="$PIDS $new_pid"
    eval "PID_$role=\$new_pid"
}

wait_for_port() {
    port="$1"
    deadline=$(($(date +%s) + 15))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        # Probe /health (registered on every upstream + 404 on proxy
        # since the proxy mount is /echo). Avoid /echo: that's the
        # proxy mount and would forward to an upstream, incrementing
        # its counter and corrupting the post-test count_get checks.
        #
        # --connect-timeout fails fast on a refused / not-yet-bound
        # port. Without it, curl on Windows MSYS2 waits the full
        # --max-time (~1 s) before retrying — and with 15 setups ×
        # 4 ports per setup × multiple wasted attempts each, that
        # alone burned more than the CI's 180 s test timeout. The
        # 0.3 s connect-timeout is well above any sane bind latency
        # (loopback, in-process) but fast enough that a still-not-
        # bound port surfaces refusal quickly and the loop iterates.
        if curl -s -o /dev/null --connect-timeout 0.3 --max-time 1 \
                "http://127.0.0.1:$port/health" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    echo "  [FAIL] port $port never accepted"; exit 1
}

stop_all() {
    # SIGKILL (see cleanup() comment) + skip the matching `wait`.
    # kill -9 → TerminateProcess on MSYS2 is synchronous, the pid
    # is gone by the time we move on. The earlier SIGTERM + wait
    # pattern blocked indefinitely in Windows CI when the http
    # server didn't reap SIGTERM, pushing the suite past its
    # 180 s timeout despite local 16 s execution.
    for pid in $PIDS; do
        kill -9 "$pid" 2>/dev/null || true
    done
    PIDS=""
    # No port-release poll: lsof isn't on Windows MSYS2 and the
    # loop just returned-fast there anyway (cmd missing → "lsof"
    # exits non-zero → `! lsof` is true → return). The poll only
    # mattered on POSIX where SIGTERM-reaped-then-rebind races
    # could occur; with SIGKILL the close is forced and the
    # SO_REUSEADDR on the next bind handles any TIME_WAIT residue.
}

start_three_upstreams() {
    start_proc upstream_a
    start_proc upstream_b
    start_proc upstream_c
    wait_for_port 19101
    wait_for_port 19102
    wait_for_port 19103
}

start_proxy() {
    start_proc "$1"
    wait_for_port 19100
}

setup() {
    proxy_role="$1"
    start_three_upstreams
    start_proxy "$proxy_role"
    # `wait_for_port` already confirmed every listener is bound and
    # answering /health — no extra grace period needed. The previous
    # `sleep 0.3` here was a leftover from before wait_for_port
    # existed; on a slow Windows runner ×16 setups it added 4.8 s
    # of wasted wall-clock for no semantic benefit.
}

PROXY="http://127.0.0.1:19100"
fail() { echo "  [FAIL] $1"; exit 1; }

# Drive N parallel /echo requests, discarding all output. Used by
# tests that only need the proxy to observe N arrivals (counter
# checks, breaker / health detection). Sequential `while curl`
# loops accumulated ~150 ms × N of latency on Windows mingw CI;
# across four such tests with N=12 that pushed the suite over CI's
# 180 s timeout. `curl -Z` runs the requests in one libcurl
# process so wall-clock cost drops to roughly the slowest single
# request — same proxy semantics, much cheaper.
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

# count_get <port>  →  current shim counter for that upstream
count_get() {
    curl -s --max-time 3 "http://127.0.0.1:$1/count"
}

# count_total  →  sum across A+B+C
count_total() {
    a=$(count_get 19101)
    b=$(count_get 19102)
    c=$(count_get 19103)
    echo $((a + b + c))
}

# ---- Test 1: round-robin distribution ---------------------
setup proxy_rr
RESP_TAGS=""
i=0
while [ $i -lt 9 ]; do
    body=$(curl -s --max-time 3 "$PROXY/echo")
    tag=$(echo "$body" | head -1)
    RESP_TAGS="$RESP_TAGS $tag"
    i=$((i + 1))
done
# Each upstream should have served 3 of the 9 requests (counter=3).
[ "$(count_get 19101)" = "3" ] || fail "RR counter A: $(count_get 19101) expected 3"
[ "$(count_get 19102)" = "3" ] || fail "RR counter B: $(count_get 19102) expected 3"
[ "$(count_get 19103)" = "3" ] || fail "RR counter C: $(count_get 19103) expected 3"
stop_all

# ---- Test 2: weighted RR (3:1:1) — 50 requests ----------
setup proxy_wrr
# 50 sequential curl calls × ~150 ms per call on Windows mingw CI
# pushed this loop alone past 7-8 s. Drive them in parallel within
# a single curl process — the proxy still observes 50 distinct
# arrivals and applies WRR per request, so the per-upstream
# counters end up the same; only the wall-clock cost drops.
URLS=""
i=0
while [ $i -lt 50 ]; do
    URLS="$URLS $PROXY/echo"
    i=$((i + 1))
done
curl -Z -s -o /dev/null --max-time 5 $URLS >/dev/null 2>&1 || true
A=$(count_get 19101); B=$(count_get 19102); C=$(count_get 19103)
# 3:1:1 over 50 → A≈30, B≈10, C≈10. Allow ±5 for boundary effects.
[ "$A" -ge 25 ] && [ "$A" -le 35 ] || fail "WRR A=$A expected 25-35"
[ "$B" -ge 5  ] && [ "$B" -le 15 ] || fail "WRR B=$B expected 5-15"
[ "$C" -ge 5  ] && [ "$C" -le 15 ] || fail "WRR C=$C expected 5-15"
[ $((A + B + C)) -eq 50 ] || fail "WRR total=$((A + B + C)) expected 50"
stop_all

# ---- Test 3: ip_hash determinism --------------------------
setup proxy_ip_hash
# Same X-Forwarded-For across 12 requests → all hit same upstream.
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
# Different cookie value should be deterministic on its own (may
# coincidentally hash to the same upstream — just verify
# determinism with the second value).
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
URLS=""
i=0
while [ $i -lt 12 ]; do
    URLS="$URLS $PROXY/echo"
    i=$((i + 1))
done
curl -Z -s -o /dev/null --max-time 5 $URLS >/dev/null 2>&1 || true
A=$(count_get 19101); B=$(count_get 19102); C=$(count_get 19103)
[ "$A" = "0" ] || fail "drain: A served $A requests (expected 0)"
[ $((B + C)) -eq 12 ] || fail "drain: B+C=$((B + C)) expected 12"
stop_all

# ---- Test 6: health checks kill an unhealthy upstream -----
setup proxy_health
# First, force B to start serving 503 + flip its /health to 503.
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503"
# Wait long enough for unhealthy_threshold * interval (2 * 200ms = 400ms),
# plus one extra interval to ensure the first 503 probe completes.
sleep 1.5
# Now drive 12 requests; none should land on B.
parallel_echo 12
B=$(count_get 19102)
# Tolerance: B may have served 1-2 before being marked down.
[ "$B" -le 2 ] || fail "health: B served $B requests after marked unhealthy"
stop_all

# ---- Test 7: health recovery — flip B back, it rejoins -----
setup proxy_health
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503"
sleep 1.5  # marked unhealthy
# Verify B is now skipped
parallel_echo 6
B_BEFORE=$(count_get 19102)
# Flip B back to 200
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/200"
# Wait for healthy_threshold * interval (2 * 200ms) + slack
sleep 1.5
# Drive more requests; B should be served again
parallel_echo 12
B_AFTER=$(count_get 19102)
DELTA=$((B_AFTER - B_BEFORE))
[ "$DELTA" -ge 2 ] || fail "health recovery: B delta=$DELTA expected ≥2"
stop_all

# ---- Test 8: circuit breaker opens after consecutive failures ----
setup proxy_breaker
# Force B to 503 immediately.
curl -s -o /dev/null --max-time 3 -X POST "http://127.0.0.1:19102/admin/503"
# Drive ~10 RR requests; ~3 should hit B (3 failures), then breaker
# opens. With breaker open, subsequent traffic to B routes elsewhere.
parallel_echo 12
B=$(count_get 19102)
# Once the breaker opens (at threshold=3), B shouldn't receive
# more new requests. Allow a small tolerance for half-open probe.
[ "$B" -le 5 ] || fail "breaker: B served $B requests (expected ≤5)"
stop_all

echo "  [PASS] http_reverse_proxy_pool: 8/8 — RR/WRR/ip_hash/cookie_hash/drain/health(2)/breaker-open"
