# Runtime Optimizations

## Overview

The Aether runtime implements several performance optimizations targeting message-passing overhead, synchronization primitives, and memory access patterns. Each optimization is integrated into the scheduler and message delivery paths.

## Active Optimizations

### Thread-Local Message Payload Pools

**Implementation:** `runtime/actors/aether_send_message.c`

Per-thread pool of pre-allocated buffers eliminates dynamic allocation overhead in the message-passing hot path. Each pool contains 256 buffers of up to 256 bytes. Messages that fit within the pool buffer size are served from the pool; larger messages fall back to `malloc`. Global atomic counters track pool hit/miss rates across threads.

```c
#define MSG_PAYLOAD_POOL_SIZE 256
#define MSG_PAYLOAD_MAX_SIZE 256

typedef struct {
    char buffer[MSG_PAYLOAD_MAX_SIZE];
    int in_use;  // Thread-local: no atomics needed
} PooledPayload;

typedef struct {
    PooledPayload payloads[MSG_PAYLOAD_POOL_SIZE];
    int next_index;  // Thread-local: plain increment
    int initialized;
} PayloadPool;

static __thread PayloadPool* g_payload_pool = NULL;
```

Because each pool is thread-local, acquisition and release use plain loads and stores rather than atomic operations. The round-robin index uses bitwise AND masking for constant-time slot lookup.

### Message Coalescing with Batch Dequeue

**Implementation:** `runtime/scheduler/multicore_scheduler.c`, `runtime/scheduler/lockfree_queue.h`

The scheduler drains multiple messages from the lock-free incoming queue in a single batch dequeue operation. This reduces atomic operations from one-per-message to one-per-batch.

```c
#define COALESCE_THRESHOLD 512

// Single batch dequeue: 1 atomic store for entire batch
count = queue_dequeue_batch(&incoming_queue, buffer.actors, buffer.messages, batch_size);

for (int i = 0; i < count; i++) {
    // Redirect migrated actors, deliver to mailbox, or enqueue to SPSC
}
```

The batch dequeue reads `head` and `tail` once, copies all available messages, then advances `head` with a single `atomic_store`. This matches the existing batch enqueue pattern used by `queue_enqueue_batch`.

### Adaptive Batch Size

**Implementation:** `runtime/actors/aether_adaptive_batch.h`

The batch size adjusts dynamically based on queue utilization. Under sustained load the batch size increases (up to 1024) to amortize overhead. During idle periods it decreases (down to 64) to maintain responsiveness.

```c
#define MIN_BATCH_SIZE 64
#define MAX_BATCH_SIZE 1024
```

### Optimized Spinlock with Platform-Specific Yield

**Implementation:** `runtime/scheduler/multicore_scheduler.h`

Custom spinlock using `atomic_flag` with platform-specific CPU yield hints during spin-wait.

```c
static inline void spinlock_lock(OptimizedSpinlock* lock) {
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)) {
        #if defined(__x86_64__) || defined(_M_X64)
        __asm__ __volatile__("pause" ::: "memory");
        #elif defined(__aarch64__)
        __asm__ __volatile__("yield" ::: "memory");
        #endif
    }
}
```

The `PAUSE` instruction (x86) and `YIELD` instruction (ARM) reduce power consumption during spin-wait and signal the CPU that the thread is in a spin-loop, improving SMT scheduling.

### Lock-Free Cross-Core Queue

**Implementation:** `runtime/scheduler/lockfree_queue.h`

Single-producer, single-consumer ring buffer using atomic head/tail pointers with explicit memory ordering.

```c
typedef struct __attribute__((aligned(64))) {
    atomic_int head;
    char padding1[60];  // Cache line alignment
    atomic_int tail;
    char padding2[60];
    QueueItem items[QUEUE_SIZE];  // QUEUE_SIZE = 16384
} LockFreeQueue;
```

Cache line padding on `head` and `tail` prevents false sharing between producer and consumer cores. Power-of-2 sizing enables bitwise AND masking instead of modulo division.

### SPSC Queue for Same-Core Messaging

**Implementation:** `runtime/actors/aether_spsc_queue.h`

Each actor has a dedicated SPSC (single-producer, single-consumer) queue for receiving messages from its owning scheduler thread or from other actor threads via `scheduler_send_local`. This separates same-core and cross-core message paths.

### Direct Mailbox Delivery

**Implementation:** `runtime/scheduler/multicore_scheduler.c` (`scheduler_send_remote`)

When the sender is on the same core as the target actor and is actually running on that core's scheduler thread (verified via `current_core_id`), messages bypass the incoming queue entirely and write directly to the actor's mailbox or SPSC queue.

```c
if (from_core >= 0 && from_core == target_core &&
    from_core == current_core_id) {
    if (unlikely(actor->auto_process)) {
        spsc_enqueue(&actor->spsc_queue, msg);
    } else {
        mailbox_send(&actor->mailbox, msg);
    }
    actor->active = 1;
    return;
}
```

The `current_core_id` guard prevents non-scheduler threads (such as the main thread) from writing to the mailbox concurrently with the scheduler thread, which would be a data race on the non-thread-safe ring buffer.

### Message-Driven Actor Migration

**Implementation:** `runtime/scheduler/multicore_scheduler.c` (`scheduler_send_remote`, scheduler actor loop)

When a cross-core send occurs, the sender sets a `migrate_to` hint on the target actor. The scheduler thread that owns the actor checks this hint after processing messages and, if set, migrates the actor to the hinted core. This co-locates actors with their most frequent communicators.

Migration uses ascending core-id lock ordering to prevent deadlock between concurrent migration and work-stealing operations. The actor is always processed before migration is attempted, ensuring progress even under constant migration pressure.

### Inline Single-Int Messages

**Implementation:** `compiler/backend/codegen.c`

The code generator detects messages with exactly one integer field and emits an inline fast path. Instead of allocating a pool buffer and copying the message struct, the message ID is stored in `msg.type` and the field value in `msg.payload_int`. The receiver reconstructs the struct on the stack. This eliminates pool allocation and deallocation for the most common message pattern.

### Computed Goto Dispatch

**Implementation:** `compiler/backend/codegen.c` (generated code)

The code generator emits a dispatch table with GCC computed goto (`goto *dispatch_table[msg_id]`) for message handler selection. This replaces indirect function calls or switch statements with direct label jumps. The message ID is read from `msg.type` rather than dereferencing the payload pointer.

### Progressive Backoff Strategy

**Implementation:** `runtime/scheduler/multicore_scheduler.c`

Three-phase idle strategy:

1. **Tight spin with PAUSE/YIELD:** Low latency, used for the first 10,000 idle iterations
2. **Work stealing:** After 5,000 idle cycles, scan for busy cores and steal actors
3. **OS yield:** After 10,000 idle iterations, call `sched_yield()` and partially reset the counter

```c
if (idle_count < 10000) {
    #if defined(__x86_64__) || defined(_M_X64)
    __asm__ __volatile__("pause" ::: "memory");
    #endif
} else {
    sched_yield();
    idle_count = 5000;  // Partial reset to stay responsive
}
```

### Cache Line Alignment

**Implementation:** Multiple components

Frequently-accessed shared data structures are aligned to 64-byte cache line boundaries to prevent false sharing.

```c
typedef struct __attribute__((aligned(64))) {
    atomic_flag lock;
    char padding[63];
} OptimizedSpinlock;

#define MAILBOX_SIZE 256  // 256 slots for L1 cache locality
```

### Power-of-2 Buffer Sizing

**Implementation:** All ring buffers

All ring buffers use power-of-2 sizes with bitwise AND masking instead of modulo division.

```c
#define QUEUE_SIZE 16384
#define QUEUE_MASK (QUEUE_SIZE - 1)
int index = (head + 1) & QUEUE_MASK;

// Also used in thread-local pool (plain increment, no atomics)
int idx = pool->next_index++ & (MSG_PAYLOAD_POOL_SIZE - 1);
```

### Relaxed Atomic Memory Ordering

**Implementation:** `runtime/actors/aether_send_message.c`, `runtime/scheduler/multicore_scheduler.c`

Non-critical atomic operations use `memory_order_relaxed` to avoid unnecessary memory barriers:

```c
// Statistics counters
atomic_fetch_add_explicit(&g_pool_hits, 1, memory_order_relaxed);

// Scheduler idle tracking
atomic_fetch_add_explicit(&sched->idle_cycles, 1, memory_order_relaxed);
atomic_store_explicit(&sched->idle_cycles, 0, memory_order_relaxed);
```

Statistics counters and approximate work counts do not require sequential consistency. Relaxed ordering provides atomicity without the overhead of full memory barriers.

### NUMA-Aware Allocation

**Implementation:** `runtime/aether_numa.c`, `runtime/aether_numa.h`

Actor structures are allocated on the NUMA node local to the assigned core. The topology is detected at scheduler initialization. On systems without NUMA support, allocation falls back to standard `malloc`.

- **Linux:** `numa_alloc_onnode` (requires libnuma)
- **Windows:** `VirtualAllocExNuma`
- **Single-node systems:** Graceful degradation to `malloc`

### Link-Time Optimization

**Implementation:** `benchmarks/cross-language/aether/Makefile`

The benchmark build uses `-flto` for both compilation and linking, enabling cross-translation-unit inlining and dead code elimination.

### CPU Detection Fallback

**Implementation:** `runtime/utils/aether_cpu_detect.c`

The `cpu_recommend_cores` function uses CPUID on x86 and falls back to OS-level APIs (`sysctl` on macOS, `sysconf` on Linux, `GetNativeSystemInfo` on Windows) when CPUID returns zero (ARM, virtualized environments).

### Apple Silicon P-Core Detection

**Implementation:** `runtime/utils/aether_cpu_detect.c`, `runtime/scheduler/multicore_scheduler.c`

On Apple Silicon (M1/M2/M3), the runtime detects and uses only Performance cores (P-cores) to ensure consistent throughput. Efficiency cores (E-cores) are excluded because they run at lower clock speeds and can cause variance in benchmarks.

Detection uses `sysctlbyname("hw.perflevel0.physicalcpu")` to query the P-core count. Combined with `QOS_CLASS_USER_INTERACTIVE` thread priority, this encourages macOS to schedule actor threads on P-cores.

**Platform-specific thread affinity:**

| Platform | Mechanism | Binding |
|----------|-----------|---------|
| Linux | `pthread_setaffinity_np` | Hard binding |
| macOS | `thread_policy_set` + QoS | Hint only |
| Windows | `SetThreadAffinityMask` | Hard binding |

Note: macOS does not support hard core pinning by design. Thread placement is advisory, which may cause occasional variance in microbenchmarks.

## Rejected Optimizations

### Manual Prefetching

Benchmarks showed hardware prefetchers handle sequential ring buffer access more effectively than manual `__builtin_prefetch` hints. Manual prefetch introduced pipeline stalls.

### SIMD Message Processing

Message processing is memory-bound. Vectorization overhead exceeded benefits for typical message sizes.

## Inactive Headers

The following headers exist in the source tree but are not called from the scheduler or message delivery paths:

- `runtime/actors/aether_direct_send.h` - Direct send logic (functionality integrated into `scheduler_send_remote` instead)
- `runtime/actors/aether_message_dedup.h` - Message deduplication
- `runtime/actors/aether_message_specialize.h` - Compile-time message specialization
- `runtime/actors/aether_simd_batch.h` - SIMD batch processing

These headers define interfaces that may be integrated in a future release.

## Benchmarking

### Cross-Language Benchmarks

```bash
cd benchmarks/cross-language
./run_benchmarks.sh
```

### Test Suite

```bash
make test  # Runs all 153 tests
```

### Methodology

- Compiler: GCC or Clang with `-O3 -march=native -flto`
- Multiple runs to account for variance
- Median values reported to avoid outlier bias
- Cold-start and warm-cache scenarios measured separately

## References

- Lock-Free Programming: Harris, "A Pragmatic Implementation of Non-Blocking Linked-Lists"
- Cache Coherency: Intel 64 and IA-32 Architectures Optimization Reference Manual
- Memory Ordering: C11 Atomic Operations and Memory Model
- Actor Model: Hewitt, Bishop, Steiger (1973)
