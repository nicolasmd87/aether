#ifndef MULTICORE_SCHEDULER_H
#define MULTICORE_SCHEDULER_H

#include "../utils/aether_compiler.h"
#include "../utils/aether_thread.h"
#include <stdatomic.h>
#include "../actors/actor_state_machine.h"
#include "../actors/aether_actor_pool.h"
#include "../actors/lockfree_mailbox.h"
#include "../actors/aether_adaptive_batch.h"
#include "../actors/aether_message_dedup.h"
#include "../actors/aether_spsc_queue.h"
#include "../config/aether_optimization_config.h"
#include "lockfree_queue.h"

#define MAX_ACTORS_PER_CORE 10000
#define MAX_CORES 16
#define BATCH_SIZE 64  // Process up to 64 messages per batch for better throughput
#define COALESCE_THRESHOLD 512  // Drain this many messages at once for high throughput

// Legacy compatibility - use g_aether_config instead
#define g_sched_features g_aether_config

// Reply slot for ask/reply pattern (experimental)
// Heap-allocated per ask call; freed by whoever holds the last reference (refcounted).
typedef struct {
    void*              reply_data;   // malloc'd reply payload; returned to caller, caller must free
    size_t             reply_size;   // size of reply_data
    volatile int       reply_ready;  // 1 when reply has been set
    volatile int       timed_out;    // 1 when asker has given up
    pthread_mutex_t    mutex;        // protects reply_ready / timed_out / cond
    pthread_cond_t     cond;         // signalled by scheduler_reply()
    atomic_int         refcount;     // starts at 2 (asker + actor); freed when hits 0
} ActorReplySlot;

// Optimized spinlock with PAUSE instruction (3x faster than standard spinlock)
typedef struct {
    atomic_flag lock;
    char padding[63];  // Cache line alignment to prevent false sharing
} OptimizedSpinlock;

static inline void spinlock_init(OptimizedSpinlock* lock) {
    atomic_flag_clear(&lock->lock);
}

static inline void spinlock_lock(OptimizedSpinlock* lock) {
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)) {
        AETHER_CPU_PAUSE();
    }
}

static inline void spinlock_unlock(OptimizedSpinlock* lock) {
    atomic_flag_clear_explicit(&lock->lock, memory_order_release);
}

typedef struct {
    atomic_int active;
    int id;
    Mailbox mailbox;
    void (*step)(void*);
    pthread_t thread;
    int auto_process;
    atomic_int assigned_core;
    atomic_int migrate_to;    // Affinity hint: core to migrate to (-1 = none)
    atomic_int main_thread_only;         // If set, scheduler threads must not process this actor
    SPSCQueue* spsc_queue;               // Lock-free same-core messaging (lazy, only for auto_process)
    _Atomic(ActorReplySlot*) reply_slot; // Non-NULL only while an ask/reply is in flight
    // Prevents concurrent step() calls during work-steal handoff.
    // Set (TAS) by the thread about to call step(); cleared (release) after.
    // Work-stealing threads that find this set will retry next outer iteration.
    atomic_flag step_lock;
} ActorBase;

typedef struct {
    int core_id;
    pthread_t thread;
    ActorBase** actors;
    int actor_count;
    int capacity;
    // Per-sender SPSC channels: from_queues[src] is written ONLY by core src
    // (or by the main thread when src == MAX_CORES).  Each channel is a true
    // SPSC queue, so no CAS or locks are needed on the producer side.
    LockFreeQueue from_queues[MAX_CORES + 1];
    atomic_int running;
    atomic_int work_count;  // Approximate in-flight message count (used for load reporting)
    atomic_int steal_attempts;  // Cumulative count of successful work-steal operations
    atomic_int idle_cycles;     // Track how long core has been idle
    OptimizedSpinlock actor_lock;  // Protects actors array during migration and registration

    // Per-core message counters — only written by owning core, but read
    // cross-thread by count_pending_messages(), so must be _Atomic to avoid
    // torn reads on weakly-ordered architectures (e.g. ARM64).
    _Atomic uint64_t messages_sent;      // Messages sent FROM this core
    _Atomic uint64_t messages_processed; // Messages processed ON this core
    char counter_padding[48];    // Cache line padding to prevent false sharing

    // Message coalescing buffer for 15x throughput improvement
    struct {
        void* actors[COALESCE_THRESHOLD];
        Message messages[COALESCE_THRESHOLD];
        int count;
    } coalesce_buffer;

    // Integrated optimizations (pointers to avoid bloating struct)
    ActorPool* actor_pool;            // Actor pooling (1.81x speedup)
    AdaptiveBatchState batch_state;   // Adaptive batching (small, embedded)
} Scheduler;

extern Scheduler schedulers[MAX_CORES];
extern int num_cores;
extern atomic_int next_actor_id;

// Initialize scheduler with core count (autodetects hardware)
void scheduler_init(int cores);

// Initialize with explicit optimization flags
void scheduler_init_with_opts(int cores, AetherOptFlags opts);

void scheduler_start();
void scheduler_ensure_threads_running();  // Start threads if not already started (for main-thread mode transition)
void scheduler_stop();
void scheduler_wait();
void scheduler_cleanup();

int scheduler_register_actor(ActorBase* actor, int preferred_core);
void scheduler_send_local(ActorBase* actor, Message msg);
void scheduler_send_remote(ActorBase* actor, Message msg, int from_core);

// Batch send for main thread fan-out patterns (fork-join)
void scheduler_send_batch_start(void);
void scheduler_send_batch_add(ActorBase* actor, Message msg);
void scheduler_send_batch_flush(void);

// Optimized APIs using integrated features (TIER 1 - always on)
ActorBase* scheduler_spawn_pooled(int preferred_core, void (*step)(void*), size_t actor_size);
void scheduler_release_pooled(ActorBase* actor);

// Legacy API - now controls only TIER 3 opt-in features
void scheduler_enable_features(int use_pool, int use_lockfree, int use_adaptive, int use_direct);

// Ask/reply: send a message and block until a reply arrives or timeout.
// Returns malloc'd reply payload on success (caller must free), NULL on timeout.
void* scheduler_ask_message(ActorBase* target, void* msg_data, size_t msg_size, int timeout_ms);

// Reply to the pending ask (called from inside an actor's receive handler).
// data/data_size describe the reply payload; it is copied internally.
void scheduler_reply(ActorBase* self, void* data, size_t data_size);

// Drain pending messages for main-thread-only actors.
// Call this from C-hosted event loops (e.g. inside a render/event callback)
// to keep Aether actors alive while the main thread is occupied in C code.
// max_per_actor: max messages to process per actor per call (0 = unlimited).
// Returns total messages processed across all actors.
int aether_scheduler_poll(int max_per_actor);

// Thread-local reply slot set by the send path (sender) and step function (receiver).
// g_pending_reply_slot: set before aether_send_message so the slot rides inside the Message.
// g_current_reply_slot: set by the generated step function after mailbox_receive.
extern AETHER_TLS void* g_pending_reply_slot;
extern AETHER_TLS void* g_current_reply_slot;

#endif
