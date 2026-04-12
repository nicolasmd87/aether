#!/bin/bash
# Baseline benchmark for thread-per-connection HTTP server
# Run from aether/ directory: bash benchmarks/http/run_baseline.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$ROOT_DIR"

echo "=== Building baseline benchmark ==="
gcc -O2 -Icompiler -Iruntime -Iruntime/actors -Iruntime/scheduler -Iruntime/utils \
    -Iruntime/memory -Iruntime/config -Istd -Istd/string -Istd/io -Istd/math \
    -Istd/net -Istd/collections -Istd/json \
    benchmarks/http/bench_thread_http.c \
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
    -o build/bench_thread_http \
    -pthread -lm
echo "Build OK"

RESULTS_FILE="benchmarks/http/baseline_results.txt"
echo "=== HTTP Baseline Benchmark (thread-per-connection) ===" > "$RESULTS_FILE"
echo "Date: $(date)" >> "$RESULTS_FILE"
echo "CPU: $(lscpu | grep 'Model name' | sed 's/.*: *//')" >> "$RESULTS_FILE"
echo "Cores: $(nproc)" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

# Start server in background
./build/bench_thread_http &
SERVER_PID=$!
sleep 1

# Verify server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    exit 1
fi

echo "=== Running benchmarks ==="
for conns in 10 100 500 1000; do
    echo ""
    echo "--- $conns concurrent connections ---"
    echo "--- $conns concurrent connections ---" >> "$RESULTS_FILE"
    wrk -t4 -c$conns -d10s http://localhost:8080/api/hello 2>&1 | tee -a "$RESULTS_FILE"
    echo "" >> "$RESULTS_FILE"
    sleep 2
done

# Cleanup
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null || true

echo ""
echo "=== Results saved to $RESULTS_FILE ==="
cat "$RESULTS_FILE"
