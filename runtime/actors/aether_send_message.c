#include "actor_state_machine.h"
#include "../scheduler/multicore_scheduler.h"
#include "../config/aether_optimization_config.h"
#include "../utils/aether_compiler.h"
#include <string.h>
#include <stdlib.h>
#include "../utils/aether_thread.h"
#include <stdatomic.h>

// Thread-local core ID for optimization
extern AETHER_TLS int current_core_id;

// ActorBase is defined in multicore_scheduler.h

// ==============================================================================
// TIER 1 OPTIMIZATION: Thread-Local Message Payload Pool
// ==============================================================================
// Instead of malloc/free for every message (20M operations for 10M ping-pong),
// use thread-local pool of small buffers. Expected: 3-6x throughput improvement.

#define MSG_PAYLOAD_POOL_SIZE 256      // 256 buffers per thread
#define MSG_PAYLOAD_MAX_SIZE 256       // Max pooled message size (Ping/Pong ~16 bytes)

typedef struct {
    char buffer[MSG_PAYLOAD_MAX_SIZE];
    int in_use;  // Thread-local: no atomics needed
} PooledPayload;

typedef struct {
    PooledPayload payloads[MSG_PAYLOAD_POOL_SIZE];
    int next_index;  // Thread-local: no atomics needed
    int initialized;
} PayloadPool;

// Thread-local payload pool
static AETHER_TLS PayloadPool* g_payload_pool = NULL;

// Global statistics (atomic, shared across all threads)
static _Atomic uint64_t g_pool_hits = 0;
static _Atomic uint64_t g_pool_misses = 0;
static _Atomic uint64_t g_too_large = 0;

// Get pool statistics (for debugging/profiling)
void aether_message_pool_stats(uint64_t* hits, uint64_t* misses, uint64_t* large) {
    *hits = atomic_load_explicit(&g_pool_hits, memory_order_relaxed);
    *misses = atomic_load_explicit(&g_pool_misses, memory_order_relaxed);
    *large = atomic_load_explicit(&g_too_large, memory_order_relaxed);
}

// Initialize thread-local payload pool
static inline void payload_pool_init_thread(void) {
    if (g_payload_pool) return;

    g_payload_pool = calloc(1, sizeof(PayloadPool));
    if (!g_payload_pool) return;

    g_payload_pool->next_index = 0;
    for (int i = 0; i < MSG_PAYLOAD_POOL_SIZE; i++) {
        g_payload_pool->payloads[i].in_use = 0;
    }
    g_payload_pool->initialized = 1;
}

// Allocate from payload pool (lock-free for single thread)
static inline void* payload_pool_acquire(size_t size) {
    // Too large for pool
    if (size > MSG_PAYLOAD_MAX_SIZE) {
        atomic_fetch_add_explicit(&g_too_large, 1, memory_order_relaxed);
        return NULL;
    }

    // Initialize pool if needed
    if (!g_payload_pool || !g_payload_pool->initialized) {
        payload_pool_init_thread();
        if (!g_payload_pool) return NULL;
    }

    // Try to find free slot (round-robin, thread-local so no CAS needed)
    for (int attempts = 0; attempts < MSG_PAYLOAD_POOL_SIZE; attempts++) {
        int idx = g_payload_pool->next_index++ & (MSG_PAYLOAD_POOL_SIZE - 1);
        PooledPayload* slot = &g_payload_pool->payloads[idx];

        if (!slot->in_use) {
            slot->in_use = 1;
            atomic_fetch_add_explicit(&g_pool_hits, 1, memory_order_relaxed);
            return slot->buffer;
        }
    }

    // Pool exhausted
    atomic_fetch_add_explicit(&g_pool_misses, 1, memory_order_relaxed);
    return NULL;
}

// Return payload to pool
static inline int payload_pool_release(void* ptr) {
    if (!g_payload_pool || !g_payload_pool->initialized) {
        return 0;  // Not from pool
    }

    // Check if pointer is within pool memory
    char* pool_start = (char*)g_payload_pool->payloads;
    char* pool_end = pool_start + sizeof(g_payload_pool->payloads);

    if ((char*)ptr < pool_start || (char*)ptr >= pool_end) {
        return 0;  // Not from pool
    }

    // Find which slot this is
    size_t offset = (char*)ptr - pool_start;
    size_t slot_index = offset / sizeof(PooledPayload);

    if (slot_index >= MSG_PAYLOAD_POOL_SIZE) {
        return 0;  // Invalid
    }

    // Mark as free (thread-local: plain store)
    g_payload_pool->payloads[slot_index].in_use = 0;
    return 1;  // Successfully returned to pool
}

// TLS flag: when set, skip freeing (used for sync mode zero-copy)
extern AETHER_TLS int g_skip_free;

// Free message payload - try pool first, then fall back to free()
void aether_free_message(void* msg_data) {
    if (!msg_data) return;

    // SYNC MODE: Skip free when processing messages with zero-copy stack pointers
    if (unlikely(g_skip_free)) {
        return;  // Caller's stack memory - don't free!
    }

    // Try to return to pool
    if (payload_pool_release(msg_data)) {
        return;  // Returned to pool successfully
    }

    // Was malloc'd (large message or pool exhausted), use regular free
    free(msg_data);
}

// ==============================================================================
// Message Sending with Pool Optimization
// ==============================================================================

// ==============================================================================
// MAIN THREAD MODE: Synchronous Message Processing
// ==============================================================================
// When main thread mode is active:
// - Single actor, all sends from main(), no scheduler threads
// - Messages processed synchronously in sender's context
// - Zero queue overhead: 50M+ msg/sec possible
//
// This is the fastest path for single-actor programs like the counting benchmark.

// TLS flag: when set, aether_free_message skips freeing (used for sync mode)
// Declared here, accessed by aether_free_message below
AETHER_TLS int g_skip_free = 0;

static inline void AETHER_HOT aether_send_message_sync(ActorBase* actor, void* message_data, size_t message_size) {
    // ULTRA-FAST PATH: Pass original message directly without copying
    // Since processing is synchronous, caller's stack memory is still valid
    // We mark g_skip_free so step() doesn't try to free the caller's stack

    // Create mailbox message pointing to original data
    Message msg;
    msg.type = *(int*)message_data;  // First field is _message_id
    msg.sender_id = 0;
    msg.payload_int = 0;
    msg.payload_ptr = message_data;  // Direct pointer - no copy!
    msg.zerocopy.data = NULL;
    msg.zerocopy.size = 0;
    msg.zerocopy.owned = 0;

    // Direct mailbox insert + synchronous step (no scheduler involvement)
    mailbox_send(&actor->mailbox, msg);

    // Tell aether_free_message to skip freeing during this step()
    g_skip_free = 1;
    actor->step(actor);
    g_skip_free = 0;

    // Track stats
    AETHER_STAT_INC(inline_sends);
}

// Send a typed message to an actor using optimized scheduler paths
// Uses SPSC queues, direct sends, and other optimizations automatically
void aether_send_message(void* actor_ptr, void* message_data, size_t message_size) {
    ActorBase* actor = (ActorBase*)actor_ptr;

    // ==============================================================================
    // FAST PATH: Main thread mode (single actor, synchronous processing)
    // ==============================================================================
    // For single-actor programs (like counting benchmark), process immediately.
    // No scheduler threads, no queues, no atomics - pure function call.
    if (aether_main_thread_mode_active()) {
        aether_send_message_sync(actor, message_data, message_size);
        return;
    }

    // ==============================================================================
    // STANDARD PATH: Multi-actor scheduler-based processing
    // ==============================================================================
    // Always use heap allocation for type-safe messages.
    // TLS pools have thread affinity issues when sender/receiver are on different threads.
    // The inline message path (Message._imsg with payload_int) is still optimized for
    // single-int payloads and avoids allocation entirely.
    void* msg_copy = malloc(message_size);
    if (!msg_copy) return;

    memcpy(msg_copy, message_data, message_size);

    // Create mailbox message with the data pointer
    Message msg;
    msg.type = *(int*)message_data;  // First field is _message_id
    msg.sender_id = 0;
    msg.payload_int = 0;
    msg.payload_ptr = msg_copy;  // Use payload_ptr for the message data
    msg.zerocopy.data = NULL;
    msg.zerocopy.size = 0;
    msg.zerocopy.owned = 0;

    // Use optimized scheduler send paths:
    // - Same-core: direct mailbox send (no queue overhead)
    // - Cross-core: lock-free queue with batching
    // NOTE: Inline mode disabled for debugging
    if (current_core_id >= 0 && current_core_id == actor->assigned_core) {
        // Same-core: direct mailbox send
        scheduler_send_local(actor, msg);
    } else {
        // Cross-core or main thread: use queue
        scheduler_send_remote(actor, msg, current_core_id);
    }
}
