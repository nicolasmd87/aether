# Actor Runtime Optimizations

This directory contains the core actor runtime with performance optimizations.

## Core Components

### actor_state_machine.h
Base actor implementation with lightweight message passing.

**Key Features:**
- Ring buffer mailboxes (power-of-2 for fast masking)
- Branch prediction hints for hot paths
- Batch message processing
- Zero-copy message transfer

### Performance Optimizations

#### 1. Actor Pooling (aether_actor_pool.h)
Reuses actor instances instead of repeated allocation/deallocation.

**Implementation:**
- Pool of 256 pre-allocated actors per type
- Lock-free acquisition using atomic CAS
- Custom reset callbacks for state cleanup
- Falls back to malloc when pool exhausted

**Use Case:** High actor churn workloads where actors are frequently created and destroyed.

#### 2. Direct Actor Bypass (aether_direct_send.h)
Skips mailbox for same-core actors by directly invoking message handlers.

**Implementation:**
- Thread-local scheduler tracking
- Same-core detection via actor metadata
- Direct function call replaces mailbox enqueue/dequeue
- Queue depth heuristic prevents overwhelming busy actors

**Use Case:** Request-response patterns where actors communicate primarily within a core.

**Expected Improvement:** Eliminates mailbox latency for intra-core communication.

#### 3. Message Deduplication (aether_message_dedup.h)
Detects and skips redundant messages using a rolling window.

**Implementation:**
- 16-message rolling window of fingerprints
- Fast hash-based fingerprinting (Knuth multiplicative)
- O(1) duplicate detection with 16-slot search
- Automatic eviction of oldest entries

**Use Case:** Workloads with redundant state updates or repeated notifications.

**Expected Improvement:** Reduces processing overhead for duplicate-heavy patterns.

#### 4. Compile-Time Message Specialization (aether_message_specialize.h)
Generates optimized send/receive functions for specific message types.

**Implementation:**
- Macro-based code generation per message type
- Eliminates generic message construction overhead
- Type-safe at compile time
- Zero-overhead abstraction

**Use Case:** Performance-critical message types with known structure.

**Expected Improvement:** Eliminates branching and unused field initialization.

#### 5. Adaptive Batch Processing (aether_adaptive_batch.h)
Dynamically adjusts batch size based on message queue utilization.

**Implementation:**
- Tracks consecutive full/partial batches
- Increases batch size (4→8→16→32→64) when consistently full
- Decreases batch size when consistently partial
- Bounded by MIN_BATCH_SIZE (4) and MAX_BATCH_SIZE (64)

**Use Case:** Variable-load scenarios where message arrival rate fluctuates.

**Expected Improvement:** Balances throughput and latency based on load.

## Usage

### Basic Actor Pooling

```c
#include "aether_actor_pool.h"

ActorPool pool;
actor_pool_init(&pool);

// Acquire from pool
PooledActor* actor = actor_pool_acquire(&pool);
if (!actor) {
    // Pool exhausted, allocate manually
    actor = malloc(sizeof(PooledActor));
}

// Use actor...

// Return to pool
actor_pool_release(&pool, actor);
```

### Direct Send Optimization

```c
#include "aether_direct_send.h"

// Set current scheduler
current_scheduler_id = 0;

// Try direct send
if (!direct_send(&sender_meta, &receiver_meta, msg)) {
    // Fall back to normal mailbox send
    mailbox_send(&receiver->mailbox, msg);
}
```

### Message Deduplication

```c
#include "aether_message_dedup.h"

DedupWindow window = {0};

// Check before sending
if (!is_duplicate(&window, &msg)) {
    mailbox_send(&actor->mailbox, msg);
    record_message(&window, &msg);
}
```

### Specialized Messages

```c
#include "aether_message_specialize.h"

// Define specialized message type
DEFINE_SPECIALIZED_SEND(MSG_INCREMENT, increment)

// Use specialized send
send_increment(&actor->mailbox, sender_id);
```

### Adaptive Batching

```c
#include "aether_adaptive_batch.h"

AdaptiveBatchState state;
adaptive_batch_init(&state);

// Receive with adaptive batch size
Message msgs[64];
int count = adaptive_batch_receive(&state, &mbox, msgs, 64);
```

## Benchmarking

Run baseline benchmark:
```bash
cd benchmarks/optimizations
gcc -O3 bench_actor_baseline.c -o bench_baseline
./bench_baseline
```

Run optimized benchmark:
```bash
gcc -O3 bench_actor_optimized.c -o bench_optimized
./bench_optimized
```

## Testing

Run unit tests:
```bash
cd tests/runtime
gcc -O2 test_actor_optimizations.c -o test_optimizations
./test_optimizations
```

## Performance Notes

- All optimizations are optional and can be used independently or combined
- Benchmarks should be run on target hardware to validate improvements
- Some optimizations benefit specific workload patterns more than others
- Profiling recommended to identify which optimizations apply to your use case

## Implementation Status

- All optimizations are header-only for easy integration
- No external dependencies beyond standard C library
- Thread-safe where applicable (noted in documentation)
- Tested on x86-64, cross-platform compatible
