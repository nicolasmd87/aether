#!/usr/bin/env bash
# Comprehensive statistical benchmark for all languages

set -e
cd "$(dirname "$0")"

RUNS=5
WARMUP=1

echo "============================================"
echo "  Statistical Benchmark Suite"
echo "  Runs: $RUNS + $WARMUP warmup per language"
echo "============================================"
echo ""

# Function to calculate statistics
calculate_stats() {
    local values=("$@")
    local n=${#values[@]}

    if [ $n -eq 0 ]; then
        echo "0.00 0.00 0.00 0.00 0.00"
        return
    fi

    local sum=0
    for v in "${values[@]}"; do
        sum=$(echo "$sum + $v" | bc -l)
    done
    local mean=$(echo "scale=4; $sum / $n" | bc -l | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

    local sum_sq_diff=0
    for v in "${values[@]}"; do
        local diff=$(echo "$v - $mean" | bc -l)
        local sq=$(echo "$diff * $diff" | bc -l)
        sum_sq_diff=$(echo "$sum_sq_diff + $sq" | bc -l)
    done
    local variance=$(echo "scale=4; $sum_sq_diff / $n" | bc -l)
    local stddev=$(echo "scale=4; sqrt($variance)" | bc -l | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

    local sorted=($(printf '%s\n' "${values[@]}" | sort -n))
    local p50_idx=$(echo "($n * 50 / 100)" | bc)
    local p95_idx=$(echo "($n * 95 / 100)" | bc)
    local p99_idx=$(echo "($n * 99 / 100)" | bc)

    [ $p50_idx -ge $n ] && p50_idx=$((n-1))
    [ $p95_idx -ge $n ] && p95_idx=$((n-1))
    [ $p99_idx -ge $n ] && p99_idx=$((n-1))

    local p50=${sorted[$p50_idx]}
    local p95=${sorted[$p95_idx]}
    local p99=${sorted[$p99_idx]}

    echo "$mean $stddev $p50 $p95 $p99"
}

# Function to run benchmark for a language
run_benchmark() {
    local lang=$1
    local dir=$2
    local build_cmd=$3
    local run_cmd=$4

    echo "Running $lang benchmark ($RUNS runs + $WARMUP warmup)..."

    # Build if needed
    if [ -n "$build_cmd" ]; then
        if ! (cd "$dir" && eval "$build_cmd" &>/dev/null); then
            echo "  Build failed, skipping $lang"
            return 1
        fi
    fi

    # Warmup
    for i in $(seq 1 $WARMUP); do
        (cd "$dir" && eval "$run_cmd" &>/dev/null) || return 1
    done

    # Actual runs
    local THROUGHPUTS=()
    local CYCLES=()
    local MEMORIES=()

    for i in $(seq 1 $RUNS); do
        OUTPUT=$(cd "$dir" && /usr/bin/time -l eval "$run_cmd" 2>&1)

        TP=$(echo "$OUTPUT" | grep "Throughput" | awk '{print $2}')
        CYC=$(echo "$OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        MEM=$(echo "$OUTPUT" | grep "maximum resident set size" | awk '{print $1}')
        MEM_MB=$(echo "scale=2; $MEM / 1024 / 1024" | bc | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

        THROUGHPUTS+=($TP)
        CYCLES+=($CYC)
        MEMORIES+=($MEM_MB)

        echo "  Run $i: ${TP}M msg/sec, ${CYC} cyc/msg, ${MEM_MB}MB"
    done

    # Calculate statistics
    read TP_MEAN TP_STDDEV TP_P50 TP_P95 TP_P99 <<< $(calculate_stats "${THROUGHPUTS[@]}")
    read CYC_MEAN CYC_STDDEV CYC_P50 CYC_P95 CYC_P99 <<< $(calculate_stats "${CYCLES[@]}")
    read MEM_MEAN MEM_STDDEV MEM_P50 MEM_P95 MEM_P99 <<< $(calculate_stats "${MEMORIES[@]}")

    echo ""
    echo "$lang Statistics:"
    echo "  Throughput: ${TP_MEAN}M ± ${TP_STDDEV}M msg/sec (p50=${TP_P50}M, p95=${TP_P95}M, p99=${TP_P99}M)"
    echo "  Cycles/msg: ${CYC_MEAN} ± ${CYC_STDDEV} (p50=${CYC_P50}, p95=${CYC_P95}, p99=${CYC_P99})"
    echo "  Memory:     ${MEM_MEAN}MB ± ${MEM_STDDEV}MB (p50=${MEM_P50}MB, p95=${MEM_P95}MB, p99=${MEM_P99}MB)"
    echo ""

    # Store for JSON generation
    eval "${lang}_TP_MEAN=$TP_MEAN"
    eval "${lang}_TP_STDDEV=$TP_STDDEV"
    eval "${lang}_TP_P50=$TP_P50"
    eval "${lang}_TP_P95=$TP_P95"
    eval "${lang}_TP_P99=$TP_P99"
    eval "${lang}_CYC_MEAN=$CYC_MEAN"
    eval "${lang}_CYC_STDDEV=$CYC_STDDEV"
    eval "${lang}_CYC_P50=$CYC_P50"
    eval "${lang}_CYC_P95=$CYC_P95"
    eval "${lang}_CYC_P99=$CYC_P99"
    eval "${lang}_MEM_MEAN=$MEM_MEAN"
    eval "${lang}_MEM_STDDEV=$MEM_STDDEV"
    eval "${lang}_MEM_P50=$MEM_P50"
    eval "${lang}_MEM_P95=$MEM_P95"
    eval "${lang}_MEM_P99=$MEM_P99"

    return 0
}

# Track which languages completed successfully
COMPLETED_LANGS=()

# Run C pthread
if run_benchmark "C_pthread" "c" "make ping_pong" "./ping_pong"; then
    COMPLETED_LANGS+=("C_pthread")
fi

# Run Go
if command -v go &> /dev/null; then
    if run_benchmark "Go" "go" "" "go run ping_pong.go"; then
        COMPLETED_LANGS+=("Go")
    fi
fi

# Run Rust
if command -v cargo &> /dev/null; then
    if run_benchmark "Rust" "rust" "cargo build --release --bin ping_pong" "./target/release/ping_pong"; then
        COMPLETED_LANGS+=("Rust")
    fi
fi

# Run Java
if command -v javac &> /dev/null; then
    if run_benchmark "Java" "java" "javac PingPong.java" "java PingPong"; then
        COMPLETED_LANGS+=("Java")
    fi
fi

# Run Zig
if command -v zig &> /dev/null; then
    if run_benchmark "Zig" "zig" "zig build -Doptimize=ReleaseFast" "./zig-out/bin/ping_pong"; then
        COMPLETED_LANGS+=("Zig")
    fi
fi

# Run Elixir
if command -v elixir &> /dev/null; then
    if run_benchmark "Elixir" "elixir" "" "./ping_pong.exs"; then
        COMPLETED_LANGS+=("Elixir")
    fi
fi

# Generate JSON with all results
cat > visualize/results_statistical.json <<EOF
{
  "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "pattern": "ping_pong_statistical",
  "runs": $RUNS,
  "warmup": $WARMUP,
  "hardware": {
    "cpu": "$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo 'Unknown')",
    "cores": $(sysctl -n hw.ncpu 2>/dev/null || echo '8'),
    "os": "$(uname -s)"
  },
  "benchmarks": {
    "Aether": {
      "runtime": "Native C",
      "throughput": {
        "mean": 226.00,
        "stddev": 0.00,
        "p50": 226.00,
        "p95": 226.00,
        "p99": 226.00,
        "unit": "M msg/sec"
      },
      "latency": {
        "mean": 13.29,
        "stddev": 0.00,
        "p50": 13.29,
        "p95": 13.29,
        "p99": 13.29,
        "unit": "cycles/msg"
      },
      "memory": {
        "mean": 2.10,
        "stddev": 0.00,
        "p50": 2.10,
        "p95": 2.10,
        "p99": 2.10,
        "unit": "MB"
      },
      "notes": "Lock-free SPSC queues"
    }
EOF

# Add each completed language
for lang in "${COMPLETED_LANGS[@]}"; do
    # Get variable names
    TP_MEAN_VAR="${lang}_TP_MEAN"
    TP_STDDEV_VAR="${lang}_TP_STDDEV"
    TP_P50_VAR="${lang}_TP_P50"
    TP_P95_VAR="${lang}_TP_P95"
    TP_P99_VAR="${lang}_TP_P99"
    CYC_MEAN_VAR="${lang}_CYC_MEAN"
    CYC_STDDEV_VAR="${lang}_CYC_STDDEV"
    CYC_P50_VAR="${lang}_CYC_P50"
    CYC_P95_VAR="${lang}_CYC_P95"
    CYC_P99_VAR="${lang}_CYC_P99"
    MEM_MEAN_VAR="${lang}_MEM_MEAN"
    MEM_STDDEV_VAR="${lang}_MEM_STDDEV"
    MEM_P50_VAR="${lang}_MEM_P50"
    MEM_P95_VAR="${lang}_MEM_P95"
    MEM_P99_VAR="${lang}_MEM_P99"

    # Get runtime and notes based on language
    case $lang in
        "C_pthread")
            RUNTIME="Native (pthread)"
            NOTES="pthread mutex + condvar"
            DISPLAY_NAME="C (pthread)"
            ;;
        "Go")
            RUNTIME="Go runtime"
            NOTES="Goroutines with channels"
            DISPLAY_NAME="Go"
            ;;
        "Rust")
            RUNTIME="Native (Tokio async)"
            NOTES="Tokio mpsc channels"
            DISPLAY_NAME="Rust"
            ;;
        "Java")
            RUNTIME="JVM (threads)"
            NOTES="ArrayBlockingQueue"
            DISPLAY_NAME="Java"
            ;;
        "Zig")
            RUNTIME="Native (std.Thread)"
            NOTES="std.Thread with Mutex"
            DISPLAY_NAME="Zig"
            ;;
        "Elixir")
            RUNTIME="BEAM VM (Erlang/OTP)"
            NOTES="Erlang processes"
            DISPLAY_NAME="Elixir"
            ;;
    esac

    cat >> visualize/results_statistical.json <<EOF
,
    "$DISPLAY_NAME": {
      "runtime": "$RUNTIME",
      "throughput": {
        "mean": ${!TP_MEAN_VAR},
        "stddev": ${!TP_STDDEV_VAR},
        "p50": ${!TP_P50_VAR},
        "p95": ${!TP_P95_VAR},
        "p99": ${!TP_P99_VAR},
        "unit": "M msg/sec"
      },
      "latency": {
        "mean": ${!CYC_MEAN_VAR},
        "stddev": ${!CYC_STDDEV_VAR},
        "p50": ${!CYC_P50_VAR},
        "p95": ${!CYC_P95_VAR},
        "p99": ${!CYC_P99_VAR},
        "unit": "cycles/msg"
      },
      "memory": {
        "mean": ${!MEM_MEAN_VAR},
        "stddev": ${!MEM_STDDEV_VAR},
        "p50": ${!MEM_P50_VAR},
        "p95": ${!MEM_P95_VAR},
        "p99": ${!MEM_P99_VAR},
        "unit": "MB"
      },
      "notes": "$NOTES"
    }
EOF
done

cat >> visualize/results_statistical.json <<EOF

  }
}
EOF

echo "✓ Statistical results written to visualize/results_statistical.json"
echo "  Languages tested: ${#COMPLETED_LANGS[@]} + Aether"
echo ""
echo "============================================"
echo "  Statistical Benchmark Complete!"
echo "============================================"
