# Aether Runtime Profiling Guide

## Quick Start

### 1. Compile with Profiling Enabled

```bash
gcc -O2 -march=native -DAETHER_PROFILE your_app.c \
    runtime/utils/aether_runtime_profile.c \
    -Iruntime -o your_app_profiled
```

### 2. Run and Get Reports

```bash
./your_app_profiled
```

Output includes:
- Per-core cycle counts for all operations
- Average cycles per operation
- Throughput estimates (at 3 GHz)
- CSV export for trend analysis

## Example Output

Sample output (actual values depend on hardware):

```
--- Core 0 ---
  Mailbox Send:         1000000 ops,    19.28 cycles/op
  Mailbox Receive:      1000000 ops,    20.03 cycles/op
  Batch Send:             50000 msgs,    18.50 cycles/msg
  SPSC Enqueue:          200000 ops,    12.34 cycles/op
  Actor Step:            100000 ops,   125.67 cycles/op

=== Performance Summary ===
Total Messages:   1000000
Avg Cycles/Msg:   19.28
Throughput:       155.60 M msg/sec (at 3 GHz)
```

## What Gets Measured

### Mailbox Operations
- `mailbox_send()` - Single message send
- `mailbox_receive()` - Single message receive
- `mailbox_send_batch()` - Batched sends
- `mailbox_receive_batch()` - Batched receives

### SPSC Queue (Same-Core Fast Path)
- `spsc_enqueue()` - Lock-free enqueue
- `spsc_dequeue()` - Lock-free dequeue

### Cross-Core Messaging
- `queue_enqueue()` - Cross-core message queue
- `queue_dequeue()` - Dequeue from shared queue

### Scheduler
- `actor_step()` - Actor processing time
- Idle cycles
- Atomic operation counts
- Lock contention tracking

## Production Use

### Zero Overhead in Production

Without `-DAETHER_PROFILE`, all profiling macros compile to `((void)0)` - **zero overhead**.

```bash
# Production build - no profiling
gcc -O2 -march=native your_app.c -o your_app_prod

# Development build - with profiling
gcc -O2 -march=native -DAETHER_PROFILE your_app.c \
    runtime/utils/aether_runtime_profile.c -o your_app_dev
```

### Continuous Integration

Use profiled builds in CI to detect performance regressions:

```bash
# Run profiled benchmark
./bench_profiled.exe

# Export to CSV
# Result: profile_results.csv

# Compare against baseline
python tools/compare_profiles.py baseline.csv profile_results.csv
```

## CSV Export Format

```csv
core,operation,count,total_cycles,avg_cycles
0,mailbox_send,1000000,19280000,19.28
0,mailbox_receive,1000000,20030000,20.03
0,batch_send,50000,925000,18.50
```

Use for:
- Trend analysis across commits
- Performance regression detection
- Bottleneck identification
- Optimization validation

## Integration with Existing Code

### Automatic in Hot Paths

Profiling is already integrated into:
- `actor_state_machine.h` - Mailbox operations
- (More to be added as needed)

### Manual Instrumentation

Add to custom hot paths:

```c
#include "runtime/utils/aether_runtime_profile.h"

void my_hot_function(int core_id) {
    PROFILE_START();
    
    // Your code here
    do_work();
    
    PROFILE_END_ACTOR_STEP(core_id);
}
```

Available macros:
- `PROFILE_START()` - Begin timing
- `PROFILE_END_MAILBOX_SEND(core_id)`
- `PROFILE_END_MAILBOX_RECEIVE(core_id)`
- `PROFILE_END_BATCH_SEND(core_id, count)`
- `PROFILE_END_BATCH_RECEIVE(core_id, count)`
- `PROFILE_END_SPSC_ENQUEUE(core_id)`
- `PROFILE_END_SPSC_DEQUEUE(core_id)`
- `PROFILE_END_QUEUE_ENQUEUE(core_id)`
- `PROFILE_END_QUEUE_DEQUEUE(core_id)`
- `PROFILE_END_ACTOR_STEP(core_id)`
- `PROFILE_ATOMIC_OP(core_id)`
- `PROFILE_LOCK_CONTENTION(core_id)`

## Use Cases

### 1. Validate Optimizations

```bash
# Baseline
gcc -O2 -DAETHER_PROFILE -o bench_before
./bench_before > before.txt

# Apply optimization
# ... make changes ...

# Measure improvement
gcc -O2 -DAETHER_PROFILE -o bench_after
./bench_after > after.txt

# Compare
diff before.txt after.txt
```

### 2. Find Bottlenecks

Look for operations with highest cycle counts or lowest throughput:

```
Actor Step: 125.67 cycles/op  ← SLOW! Investigate actor logic
SPSC Queue: 12.34 cycles/op   ← FAST! Lock-free working
Mailbox:    19.28 cycles/op   ← GOOD baseline
```

### 3. Monitor Production

Enable in staging environment to catch regressions before production:

```bash
# Staging with profiling
export AETHER_PROFILE=1
./run_staging_tests.sh

# Check for regressions
if [ $(awk '/Avg Cycles/{print $NF}' profile.txt) -gt 25 ]; then
    echo "Performance regression detected!"
    exit 1
fi
```

## Technical Details

### RDTSC Cycle Counting

Uses `__rdtsc()` intrinsic for cycle-accurate measurements:
- Resolution: 1 CPU cycle
- Overhead: ~20-30 cycles per measurement
- Accuracy: ±2% on modern CPUs

### Atomic Statistics

Stats use relaxed atomics for minimal overhead:
- Per-core stats (reduces contention)
- Atomic fetch-add for counters
- Final aggregation at report time

### Overhead Analysis

With profiling enabled:
- ~25 cycles per operation (2x overhead at 20 cycles/op)
- Acceptable for development/CI
- **Zero overhead in production** (macros compile out)

## Examples

See:
- `tests/runtime/bench_profiled.c` - Basic example
- `tests/runtime/bench_atomic_overhead.c` - Atomic operation analysis
- `tests/runtime/micro_profile.h` - Low-level utilities

## Best Practices

1. **Always compare before/after** - Don't guess, measure
2. **Use CSV exports** - Track trends over time
3. **Profile in CI** - Catch regressions early
4. **Disable in production** - Zero overhead by default
5. **Focus on hot paths** - Profile what matters

## Optimization Validation

Using this profiling system, you can measure:
- Atomic operation overhead in hot paths
- Mailbox operation cycles
- Message copy overhead
- Queue operation latency

Profile your code to identify bottlenecks and validate optimizations on your target hardware. Results vary by platform and workload.
