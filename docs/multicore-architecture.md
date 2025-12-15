# Multi-Core Actor Architecture

## Design Philosophy

Single-core performance: 166.7 M msg/sec. Instead of complex work-stealing with context switching overhead, use static partitioning for simplicity and predictable performance.

## Architecture: Fixed Core Partitioning

### Core Concept
- N cores = N independent schedulers
- Actors assigned to cores at spawn time (hash-based or explicit)
- Each core runs its own scheduler loop at full speed
- No work stealing, no context switching, no locks within scheduler
- Cross-core communication via lock-free channels

### Benefits
1. **Simplicity**: Each core is independent single-threaded scheduler
2. **Predictable**: No stealing means predictable latency
3. **Cache locality**: Actors stay on same core
4. **Linear scaling**: N cores = N × 166M msg/sec theoretical max
5. **No synchronization overhead**: Only when sending cross-core

### Implementation

```c
// One scheduler per core
typedef struct Scheduler {
    int core_id;
    Actor** actors;
    int actor_count;
    LockFreeQueue* incoming_queue;  // Messages from other cores
} Scheduler;

// Global state
Scheduler schedulers[NUM_CORES];

// Main loop per core
void run_scheduler(int core_id) {
    Scheduler* sched = &schedulers[core_id];
    
    while (running) {
        // Process incoming cross-core messages
        process_incoming(sched);
        
        // Run local actors (166M msg/sec)
        for (int i = 0; i < sched->actor_count; i++) {
            if (sched->actors[i]->active) {
                actor_step(sched->actors[i]);
            }
        }
    }
}

// Actor assignment (hash-based partitioning)
int get_core_for_actor(int actor_id) {
    return actor_id % NUM_CORES;
}

// Cross-core send
void send_message(Actor* target, Message msg) {
    int target_core = target->assigned_core;
    
    if (target_core == current_core) {
        // Local: fast path, direct mailbox
        mailbox_send(&target->mailbox, msg);
    } else {
        // Remote: lock-free queue
        lockfree_enqueue(&schedulers[target_core].incoming_queue, target, msg);
    }
}
```

## Performance Analysis

### Single Core (Current)
- Throughput: 166.7 M msg/sec
- Latency: ~6 ns per message
- Memory: 264 bytes per actor

### Multi-Core (Projected)
**Best case (all local messages)**:
- 4 cores: 666 M msg/sec
- 8 cores: 1.3 B msg/sec
- 16 cores: 2.6 B msg/sec

**Realistic (10% cross-core)**:
- Lock-free queue overhead: ~50-100 ns per cross-core message
- Still achieves 500+ M msg/sec on 4 cores

### Comparison to Work-Stealing
- Work-stealing: Complex, unpredictable, context switch overhead
- Fixed partitioning: Simple, predictable, no overhead
- Trade-off: Work-stealing handles imbalanced load better

## When to Use Each

### Fixed Partitioning (Recommended)
- Actors are roughly balanced
- Message patterns are known
- Predictable latency required
- Maximum throughput needed

### Work-Stealing (Future)
- Highly dynamic workloads
- Unknown actor patterns
- Some actors much busier than others

## Implementation Strategy

### Phase 1: Basic Multi-Core
1. One pthread per core
2. Hash-based actor assignment
3. Lock-free queue for cross-core
4. Test with ring benchmark across cores

### Phase 2: Optimizations
1. NUMA-aware allocation
2. Core pinning
3. Batch cross-core messages
4. Adaptive load balancing (optional)

### Phase 3: Advanced
1. Actor migration (if needed)
2. Hybrid with work-stealing
3. Distributed actors (network)

## Code Generation Changes

Minimal changes needed:

```aether
// User code unchanged
actor Counter {
    state int count = 0;
    receive(msg) {
        count = count + 1;
    }
}

main() {
    // Explicit core assignment (optional)
    Counter c1 = spawn_Counter_on_core(0);
    Counter c2 = spawn_Counter_on_core(1);
    
    // Or automatic hash-based
    Counter c3 = spawn_Counter();  // Assigned by hash
}
```

Generated C adds core field:
```c
typedef struct Counter {
    int id;
    int active;
    int assigned_core;  // NEW
    Mailbox mailbox;
    int count;
} Counter;
```

## Next Steps

1. Implement lock-free queue (use existing algorithms)
2. Thread pool with core pinning
3. Update spawn to assign cores
4. Benchmark cross-core message passing
5. Measure scaling on real hardware

## Expected Results

Conservative estimate:
- 4 cores: 400-500 M msg/sec (75-80% efficiency)
- 8 cores: 800-1000 M msg/sec (70-75% efficiency)
- Cross-core overhead: 10-20% throughput loss

This beats work-stealing for most workloads and is much simpler to implement and reason about.
