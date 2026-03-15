#!/bin/bash
# Comparison benchmark: thread-per-connection vs actor dispatch
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$ROOT_DIR"

RESULTS="benchmarks/http/comparison_results.txt"
echo "=== HTTP Server Benchmark: Thread vs Actor ===" > "$RESULTS"
echo "Date: $(date)" >> "$RESULTS"
echo "CPU: $(lscpu | grep 'Model name' | sed 's/.*: *//')" >> "$RESULTS"
echo "Cores: $(nproc)" >> "$RESULTS"
echo "" >> "$RESULTS"

# Build both
echo "=== Building benchmarks ==="
bash benchmarks/http/run_baseline.sh 2>/dev/null | grep "Build OK" || true

gcc -O2 -Icompiler -Iruntime -Iruntime/actors -Iruntime/scheduler -Iruntime/utils \
    -Iruntime/memory -Iruntime/config -Istd -Istd/string -Istd/io -Istd/math \
    -Istd/net -Istd/collections -Istd/json \
    benchmarks/http/bench_actor_http.c \
    std/net/aether_http_server.c std/net/aether_http.c std/net/aether_net.c \
    std/string/aether_string.c std/collections/aether_collections.c \
    std/collections/aether_hashmap.c std/json/aether_json.c \
    std/fs/aether_fs.c std/math/aether_math.c std/log/aether_log.c \
    std/io/aether_io.c \
    runtime/scheduler/multicore_scheduler.c \
    runtime/scheduler/scheduler_optimizations.c \
    runtime/config/aether_optimization_config.c \
    runtime/memory/memory.c runtime/memory/aether_arena_optimized.c \
    runtime/memory/aether_batch.c runtime/memory/aether_pool.c \
    runtime/memory/aether_memory_stats.c \
    runtime/utils/aether_tracing.c runtime/utils/aether_test.c \
    runtime/utils/aether_simd_vectorized.c runtime/utils/aether_cpu_detect.c \
    runtime/utils/aether_bounds_check.c \
    runtime/aether_runtime.c runtime/aether_runtime_types.c runtime/aether_numa.c \
    runtime/actors/aether_send_buffer.c runtime/actors/aether_send_message.c \
    runtime/actors/aether_actor_thread.c runtime/actors/aether_message_registry.c \
    -o build/bench_actor_http \
    -pthread -lm
echo "Both builds OK"

for conns in 10 100 500 1000; do
    echo ""
    echo "========================================" | tee -a "$RESULTS"
    echo "  $conns concurrent connections" | tee -a "$RESULTS"
    echo "========================================" | tee -a "$RESULTS"

    # Thread mode
    echo "" | tee -a "$RESULTS"
    echo "--- THREAD MODE ---" | tee -a "$RESULTS"
    ./build/bench_thread_http &
    PID=$!
    sleep 1
    if kill -0 $PID 2>/dev/null; then
        wrk -t4 -c$conns -d10s http://localhost:8080/api/hello 2>&1 | tee -a "$RESULTS"
        kill $PID 2>/dev/null; wait $PID 2>/dev/null || true
    else
        echo "THREAD SERVER FAILED TO START" | tee -a "$RESULTS"
    fi
    sleep 2

    # Actor mode
    echo "" | tee -a "$RESULTS"
    echo "--- ACTOR MODE ---" | tee -a "$RESULTS"
    ./build/bench_actor_http &
    PID=$!
    sleep 1
    if kill -0 $PID 2>/dev/null; then
        wrk -t4 -c$conns -d10s http://localhost:8080/api/hello 2>&1 | tee -a "$RESULTS"
        kill $PID 2>/dev/null; wait $PID 2>/dev/null || true
    else
        echo "ACTOR SERVER FAILED TO START" | tee -a "$RESULTS"
    fi
    sleep 2
done

echo ""
echo "=== Full results saved to $RESULTS ==="
