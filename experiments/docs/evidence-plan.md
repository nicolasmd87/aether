# Evidence Gathering Plan - Safe Incremental Tests

## Goal

Collect empirical evidence that state machine actors are viable **before** implementing compiler codegen. We need to prove:

1. Memory efficiency at different scales
2. Throughput characteristics  
3. Where the model breaks down (limits)
4. Multi-core scaling (with work-stealing)

## Safety First

**⚠️ IMPORTANT**: Start small, monitor resources, scale gradually.

### System Resource Limits

Before running any test, check:
```bash
# Windows: Open Task Manager, watch CPU/Memory
# Linux: Run `htop` or `top` in another terminal

# If memory usage > 80% or CPU > 90% for >10 seconds, STOP THE TEST
```

## Phase 1: Incremental Scaling (SAFE)

Start tiny, double each time, observe where things break.

### Test 1.1: Baseline (Tiny - 10 actors)
**Purpose**: Verify benchmarks work, establish baseline

```bash
cd experiments/02_state_machine
./state_machine_bench   # Modify: ACTOR_COUNT = 10
```

**Expected**:
- Memory: <1KB
- Time: Instant
- Throughput: Still very high (fewer messages)

### Test 1.2: Small Scale (100 actors)
```bash
# Modify source: ACTOR_COUNT = 100
```

**Expected**:
- Memory: ~13KB
- Time: <1ms
- Throughput: ~100M msg/s

### Test 1.3: Medium Scale (1,000 actors)
```bash
# Modify source: ACTOR_COUNT = 1000
```

**Expected**:
- Memory: ~128KB
- Time: <10ms
- Throughput: ~120M msg/s

### Test 1.4: Large Scale (10,000 actors)
```bash
# Modify source: ACTOR_COUNT = 10000
```

**Expected**:
- Memory: ~1.3MB
- Time: <100ms
- Throughput: ~125M msg/s

**⚠️ CHECKPOINT**: If this runs smoothly, continue. Otherwise, STOP here.

### Test 1.5: Very Large (100,000 actors)
```bash
# Modify source: ACTOR_COUNT = 100000
```

**Expected**:
- Memory: ~12.8MB
- Time: <1 second
- Throughput: ~125M msg/s

**Only run if Test 1.4 was instant.**

## Phase 2: Pthread Baseline (CAREFUL)

Pthreads are heavy. Start TINY.

### Test 2.1: Tiny Pthread (10 actors)
```bash
cd experiments/01_pthread_baseline
./pthread_bench 10 10
```

**Expected**:
- Memory: ~10-80MB (1-8MB per thread)
- Time: <1 second
- Throughput: ~100K-1M msg/s

### Test 2.2: Small Pthread (100 actors)
```bash
./pthread_bench 100 10
```

**Expected**:
- Memory: ~100-800MB
- Time: 1-5 seconds
- Throughput: ~100K-1M msg/s

**⚠️ STOP HERE for pthread tests on systems with <8GB RAM**

### Test 2.3: Medium Pthread (1,000 actors) - DANGEROUS
```bash
# Only if you have 16GB+ RAM
./pthread_bench 1000 10
```

**Expected**:
- Memory: ~1-8GB
- Time: 10-60 seconds
- May freeze system if resources exhausted

**DO NOT GO HIGHER THAN 1,000 actors with pthreads.**

## Phase 3: Comparative Evidence

Run both models at the same scale and compare.

### Safe Comparison Matrix

| Actors | State Machine | Pthread | Winner |
|--------|---------------|---------|--------|
| 10 | ✅ Run | ✅ Run | Compare |
| 100 | ✅ Run | ✅ Run | Compare |
| 1,000 | ✅ Run | ⚠️ Run if RAM OK | Compare |
| 10,000 | ✅ Run | ❌ SKIP (too heavy) | State wins |
| 100,000 | ✅ Run | ❌ SKIP (too heavy) | State wins |

### Metrics to Collect

For each test, record:
1. **Time to completion** (seconds)
2. **Peak memory usage** (MB)
3. **Throughput** (messages/second)
4. **CPU usage** (%)
5. **System stability** (did it freeze?)

### Data Collection Script

```bash
# Create a CSV log
echo "Model,Actors,Messages,Time,Memory,Throughput" > results.csv

# Run test and append
# (Manual for now, automate later)
```

## Phase 4: Identify Limits

Find where each model breaks.

### State Machine Limits

Test with increasing actor counts until:
- Memory exhausted (>80% RAM)
- Time exceeds reasonable threshold (>10 seconds)
- System becomes unresponsive

### Pthread Limits

Test with increasing thread counts until:
- OS thread limit reached
- Memory exhausted
- System freezes

## Phase 5: Multi-core Potential (Work-Stealing)

**Status**: Not yet implemented

**What to test**:
- Single-threaded state machine (baseline)
- 2-thread work-stealing
- 4-thread work-stealing
- N-thread (CPU cores)

**Expected**: Linear or near-linear scaling up to CPU count.

## Evidence Document

After running tests, create `experiments/EVIDENCE.md`:

### Section 1: Throughput Evidence
```
State Machine vs Pthread at comparable scales:

10 actors:
- State: X msg/s, Y ms
- Pthread: A msg/s, B ms
- Speedup: Z x

100 actors:
- State: ...
- Pthread: ...
```

### Section 2: Memory Evidence
```
Memory scaling:

State Machine:
- 10 actors: X MB
- 100 actors: Y MB
- 1K actors: Z MB
- 10K actors: A MB
- 100K actors: B MB
- Growth rate: Linear, ~128 bytes/actor

Pthread:
- 10 actors: X MB
- 100 actors: Y MB
- 1K actors: Z MB (if tested)
- Growth rate: Linear, ~1-8MB/actor
```

### Section 3: Scalability Evidence
```
Maximum tested:
- State Machine: X actors without issue
- Pthread: Y actors before slowdown
- Limit ratio: X/Y = Z x more actors
```

### Section 4: Failure Points
```
State Machine breaks at:
- N actors (memory limit)
- OR: No limit found up to M actors

Pthread breaks at:
- K actors (thread limit / memory)
```

## Decision Criteria

**Proceed with state machine implementation if**:
- ✅ 100x+ memory improvement proven
- ✅ 10x+ throughput improvement proven  
- ✅ Can scale to 10K+ actors smoothly
- ✅ No system crashes during tests

**Reconsider if**:
- ❌ State machine slower than pthread
- ❌ Memory not significantly better
- ❌ System instability

## Timeline

- **Week 1**: Run Phase 1-2 tests (incremental, safe)
- **Week 2**: Analyze data, create EVIDENCE.md
- **Week 3**: Implement work-stealing prototype (Experiment 03)
- **Week 4**: Re-test with multi-core, finalize decision

## Next Steps After Evidence

If evidence is solid:
1. Implement basic actor syntax (structs, pattern matching)
2. Build state machine codegen (simple case first)
3. Test generated code against benchmarks
4. Iterate

If evidence is weak:
1. Investigate why (wrong model? implementation issues?)
2. Try alternative approaches
3. Re-benchmark

---

**Remember**: The goal is evidence, not perfection. If state machines are 10x better in the common case, that's enough to proceed. We can optimize later.
