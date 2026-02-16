# Aether Scheduler - Quick Reference

## Main Thread Mode

Single-actor programs bypass the scheduler entirely. When only one actor exists and all messages originate from the main thread, the runtime processes messages synchronously without spawning scheduler threads.

**Activation:**
- Automatic: Enabled when the first actor spawns, disabled when a second actor spawns
- Manual disable: Set `AETHER_NO_INLINE=1` environment variable

**Mechanism:**
- `scheduler_start()` returns immediately
- `scheduler_wait()` returns immediately
- `aether_send_message` processes synchronously via `aether_send_message_sync`
- Zero-copy: message data passed directly from caller's stack

**Per-actor flag:**
The `main_thread_only` field on `ActorBase` signals scheduler threads to skip processing this actor. When a second actor spawns, the flag is cleared on the first actor.

---

## Scheduler Usage

```c
// Initialize with N cores (auto-detects hardware if 0)
scheduler_init(4);

// Register actors (assigned to preferred core)
scheduler_register_actor((ActorBase*)actor, preferred_core);

// Start processing
scheduler_start();

// Send messages (routed automatically)
for (int i = 0; i < 10000; i++) {
    Message msg = message_create_simple(1, 0, i);
    scheduler_send_remote(actor, msg, current_core_id);
}

// Wait for all messages to be processed, then stop
scheduler_wait();  // Blocks until idle, then stops and joins threads
```

## Actor Allocation

```c
// NUMA-aware allocation with correct derived-struct size
ActorBase* actor = scheduler_spawn_pooled(preferred_core, step_fn, sizeof(MyActor));
if (!actor) {
    // Fallback to manual allocation
    actor = aligned_alloc(64, sizeof(MyActor));
    memset(actor, 0, sizeof(MyActor));
    mailbox_init(&actor->mailbox);
    spsc_queue_init(&actor->spsc_queue);
}
```

## Message Flow

```
sender -> scheduler_send_remote -> lock-free queue -> batch dequeue
       -> coalesce buffer -> redirect/deliver -> mailbox -> actor step
```

**Same-core fast path:**
```
sender -> scheduler_send_local -> mailbox (direct) -> actor step
```

## Configuration

### Message Coalescing
```c
#define COALESCE_THRESHOLD 512  // In multicore_scheduler.h
```

### Adaptive Batch Size
```c
#define MIN_BATCH_SIZE 64    // In aether_adaptive_batch.h
#define MAX_BATCH_SIZE 1024
```

### Mailbox Size
```c
#define MAILBOX_SIZE 256  // In actor_state_machine.h (must be power of 2)
```

### Message Pool
```c
#define MSG_PAYLOAD_POOL_SIZE 256  // In aether_send_message.c
#define MSG_PAYLOAD_MAX_SIZE 256   // Max pooled message size
```

### Cross-Core Queue
```c
#define QUEUE_SIZE 16384  // In lockfree_queue.h
```

## Idle Detection

`scheduler_wait()` blocks until all messages have been processed. It uses per-core counters to detect idle state without atomic contention on the message-passing hot path:

```c
// Per-core counters in Scheduler struct
uint64_t messages_sent;      // Incremented on send (no atomic)
uint64_t messages_processed; // Incremented on process (no atomic)

// scheduler_wait() sums across cores
while (total_sent != total_processed) {
    // spin-wait
}
```

Messages sent from the main thread (before scheduler threads start) use a separate atomic counter. This path is infrequent.

## Core Assignment and Migration

Actors are assigned to cores at spawn time via round-robin (`actor_id % num_cores`). During operation, cross-core senders set a `migrate_to` hint. The owning scheduler migrates the actor after processing, co-locating it with its most frequent communicator.

Both migration and work stealing use ascending core-id lock ordering to prevent deadlock.

## Best Practices

**Do:**
- Let the scheduler handle core assignment and migration
- Use `scheduler_send_remote` for all cross-core messages
- Initialize `migrate_to = -1` when creating actors manually
- Ensure test actor structs match `ActorBase` field layout exactly

**Do not:**
- Access an actor's mailbox from a thread other than its owning scheduler thread
- Call `mailbox_send` directly on actors owned by other cores
- Omit `migrate_to` from custom actor structs (causes struct layout mismatch)

## Troubleshooting

### Low Throughput
- Verify compiler flags include `-O3 -march=native -flto`
- Check that message pool is active (pool stats via `aether_message_pool_stats`)
- Ensure batch size is appropriate for workload
- Verify core count matches available hardware

### High Latency
- Tune `COALESCE_THRESHOLD` for latency-sensitive workloads (lower = less batching delay)
- Check progressive backoff thresholds
- Avoid blocking operations in actor step functions

### Crashes
- Verify all actor structs include `migrate_to` field between `assigned_core` and `spsc_queue`
- Initialize `migrate_to = -1` after actor creation
- Use `scheduler_send_remote` instead of direct mailbox writes for cross-core messages
- Call `scheduler_stop` and `scheduler_wait` before process exit

## Files Reference

### Headers
- `runtime/actors/actor_state_machine.h` - Message struct, mailbox
- `runtime/scheduler/multicore_scheduler.h` - Scheduler API, ActorBase, spinlock
- `runtime/scheduler/lockfree_queue.h` - Cross-core message queue
- `runtime/actors/aether_spsc_queue.h` - Same-core SPSC queue
- `runtime/actors/aether_adaptive_batch.h` - Adaptive batch sizing

### Implementation
- `runtime/scheduler/multicore_scheduler.c` - Scheduler core, coalescing, migration, work stealing
- `runtime/actors/aether_send_message.c` - Message sending, thread-local pools
- `runtime/actors/aether_actor_thread.c` - Actor-owned thread loop
- `runtime/aether_numa.c` - NUMA topology detection and allocation

### Tests
- `tests/runtime/test_scheduler.c` - Scheduler correctness tests
- `tests/runtime/test_scheduler_stress.c` - Concurrency stress tests
- `tests/runtime/test_scheduler_optimizations.c` - Optimization validation
