#!/bin/bash
# Safe Incremental Benchmark Runner
# Runs tests from tiny to large, stops if system resources get stressed

echo "╔════════════════════════════════════════════╗"
echo "║   Aether Safe Incremental Benchmark       ║"
echo "║   Evidence Gathering Mode                 ║"
echo "╚════════════════════════════════════════════╝"
echo ""

# Create results file
RESULTS="EVIDENCE.csv"
echo "Model,Actors,Messages,Time,Throughput,Memory_MB,Status" > $RESULTS

# Function to run and log
run_test() {
    local model=$1
    local actors=$2
    local msgs=$3
    local cmd=$4
    
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Test: $model with $actors actors"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    # Run the test
    output=$($cmd 2>&1)
    status=$?
    
    if [ $status -eq 0 ]; then
        echo "$output"
        
        # Extract metrics (basic parsing)
        time=$(echo "$output" | grep "Time:" | grep -oE '[0-9]+\.[0-9]+')
        throughput=$(echo "$output" | grep "Throughput:" | grep -oE '[0-9]+')
        memory=$(echo "$output" | grep "Memory:" | grep -oE '[0-9]+\.[0-9]+')
        
        echo "$model,$actors,$msgs,$time,$throughput,$memory,PASS" >> $RESULTS
        echo "✅ PASS"
    else
        echo "❌ FAILED"
        echo "$model,$actors,$msgs,N/A,N/A,N/A,FAIL" >> $RESULTS
    fi
    
    echo ""
}

# Build benchmarks
echo "Building benchmarks..."
cd "$(dirname "$0")"

cd 02_state_machine
gcc -O2 -o safe_bench safe_bench.c 2>&1
if [ $? -ne 0 ]; then
    echo "❌ Failed to build state machine benchmark"
    exit 1
fi
echo "✅ State machine benchmark built"

cd ../01_pthread_baseline
gcc -O2 -o pthread_bench pthread_bench.c -lpthread 2>&1
if [ $? -ne 0 ]; then
    echo "❌ Failed to build pthread benchmark"
    exit 1
fi
echo "✅ Pthread benchmark built"
echo ""

cd ..

# Phase 1: State Machine (Safe progression)
echo "╔════════════════════════════════════════════╗"
echo "║  PHASE 1: State Machine Scaling            ║"
echo "╚════════════════════════════════════════════╝"
echo ""

run_test "State Machine" 10 10 "./02_state_machine/safe_bench 10 10"
run_test "State Machine" 100 10 "./02_state_machine/safe_bench 100 10"
run_test "State Machine" 1000 10 "./02_state_machine/safe_bench 1000 10"
run_test "State Machine" 10000 10 "./02_state_machine/safe_bench 10000 10"

echo "Press Enter to continue to larger tests (or Ctrl+C to stop)"
read

run_test "State Machine" 100000 10 "./02_state_machine/safe_bench 100000 10"

# Phase 2: Pthread (Careful progression)
echo ""
echo "╔════════════════════════════════════════════╗"
echo "║  PHASE 2: Pthread Baseline (CAREFUL)       ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "⚠️  WARNING: Pthread tests use heavy OS threads"
echo "⚠️  Monitor your system resources!"
echo ""

run_test "Pthread" 10 10 "./01_pthread_baseline/pthread_bench 10 10"
run_test "Pthread" 100 10 "./01_pthread_baseline/pthread_bench 100 10"

echo ""
echo "⚠️  Next test: 1,000 pthreads (~1-8GB RAM)"
echo "⚠️  Only proceed if you have sufficient RAM"
echo "Press Enter to continue (or Ctrl+C to skip)"
read

run_test "Pthread" 1000 10 "./01_pthread_baseline/pthread_bench 1000 10"

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║  EVIDENCE COLLECTED                        ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "Results saved to: $RESULTS"
echo ""
cat $RESULTS
echo ""
echo "Next steps:"
echo "1. Review EVIDENCE.csv"
echo "2. Create EVIDENCE.md summary"
echo "3. Decide on implementation approach"
