# Experiment 04: Partitioned State Machines (Zero-Sharing Multi-Core)

## Hypothesis

**Work-stealing is unnecessary overhead for compute-bound actors.**

Instead of sharing actors across cores with atomics and stealing, partition actors statically:
- Each core runs its own state machine scheduler loop
- Actors pinned to cores at spawn time (actor_id % num_cores)
- No atomics, no sharing, no stealing
- Cross-core messages use lock-free queues (only for cross-core communication)

## Expected Performance

If state machine achieves 125M msg/sec on 1 core, and we have N cores with no sharing:

- **1 core**: 125M msg/sec (baseline)
- **2 cores**: 250M msg/sec (2× perfect scaling)
- **4 cores**: 500M msg/sec (4× perfect scaling)
- **8 cores**: 1000M msg/sec (8× perfect scaling)

**This assumes**:
- Most messages are local (same-core actor-to-actor)
- Cross-core messages are <10% of total
- No false sharing in cache lines

## Architecture

```
┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│  Core 0     │  │  Core 1     │  │  Core 2     │  │  Core 3     │
│             │  │             │  │             │  │             │
│ Actors:     │  │ Actors:     │  │ Actors:     │  │ Actors:     │
│ [0,4,8,12]  │  │ [1,5,9,13]  │  │ [2,6,10,14] │  │ [3,7,11,15] │
│             │  │             │  │             │  │             │
│ Scheduler:  │  │ Scheduler:  │  │ Scheduler:  │  │ Scheduler:  │
│ while(1) {  │  │ while(1) {  │  │ while(1) {  │  │ while(1) {  │
│   for actor │  │   for actor │  │   for actor │  │   for actor │
│     step()  │  │     step()  │  │     step()  │  │     step()  │
│ }           │  │ }           │  │ }           │  │ }           │
└─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘
       │                │                │                │
       └────────────────┴────────────────┴────────────────┘
                 Lock-Free Cross-Core Message Queue
```

## Implementation

### Phase 1: Local-Only (No Cross-Core)
- Each core owns subset of actors
- Messages only within same partition
- Measure scaling vs state machine baseline

### Phase 2: Cross-Core Messages
- Add lock-free queue for remote messages
- Measure cross-core message overhead
- Test 10%, 50%, 90% cross-core ratios

### Phase 3: NUMA Optimization
- Allocate actor memory on local NUMA node
- Pin threads with `SetThreadAffinityMask` (Windows) / `pthread_setaffinity_np` (Linux)
- Measure NUMA vs non-NUMA performance

## Comparison to Work-Stealing

| Metric | Work-Stealing (Exp 03) | Partitioned (Exp 04) |
|--------|------------------------|----------------------|
| **1 core** | 51M msg/sec | 125M msg/sec |
| **4 cores** | 43M msg/sec | ~500M msg/sec (predicted) |
| **Atomics** | Per message | Only cross-core |
| **Cache misses** | High (stealing) | Low (static partition) |
| **Load balance** | Dynamic | Static |
| **Code complexity** | High (Chase-Lev) | Low (simple loop) |

## When Work-Stealing Wins

Partitioned model fails when:
- **Unbalanced load**: One actor does 10,000× more work than others
- **Dynamic workload**: Actors created/destroyed at runtime
- **Blocking operations**: Actor blocks on I/O, wastes entire core

Work-stealing wins:
- Mixed compute + I/O workloads
- Unpredictable actor lifetimes
- Need to maximize CPU utilization

## Implementation Status

✅ **Phase 1**: Local-only partitioned (to implement)
- [ ] Partition actors across cores
- [ ] Independent scheduler per core
- [ ] Benchmark vs single-threaded baseline

⏳ **Phase 2**: Cross-core messaging
- [ ] Lock-free queue for remote messages
- [ ] Measure cross-core overhead

⏳ **Phase 3**: Hybrid with work-stealing
- [ ] Start partitioned
- [ ] Steal only when idle >100 cycles
- [ ] Best of both worlds?
