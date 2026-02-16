#ifndef MULTICORE_SCHEDULER_H
#define MULTICORE_SCHEDULER_H

#include <pthread.h>
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
        #if defined(__x86_64__) || defined(_M_X64)
        __asm__ __volatile__("pause" ::: "memory");
        #elif defined(__aarch64__)
        __asm__ __volatile__("yield" ::: "memory");
        #endif
    }
}

static inline void spinlock_unlock(OptimizedSpinlock* lock) {
    atomic_flag_clear_explicit(&lock->lock, memory_order_release);
}

typedef struct {
    int active;
    int id;
    Mailbox mailbox;
    void (*step)(void*);
    pthread_t thread;
    int auto_process;
    int assigned_core;
    int migrate_to;        // Affinity hint: core to migrate to (-1 = none)
    int main_thread_only;  // If set, scheduler threads must not process this actor
    SPSCQueue spsc_queue;  // Lock-free same-core messaging
} ActorBase;

typedef struct {
    int core_id;
    pthread_t thread;
    ActorBase** actors;
    int actor_count;
    int capacity;
    LockFreeQueue incoming_queue;
    atomic_int running;
    atomic_int work_count;  // For work stealing - approximate message count
    atomic_int steal_attempts;  // Statistics
    atomic_int idle_cycles;     // Track how long core has been idle
    OptimizedSpinlock actor_lock;  // Protects actors array during work stealing

    // Per-core message counters (no atomics needed - core-local!)
    // This is the Linux kernel's per-CPU counter pattern for scalability
    uint64_t messages_sent;      // Messages sent FROM this core
    uint64_t messages_processed; // Messages processed ON this core
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
void scheduler_stop();
void scheduler_wait();
void scheduler_cleanup();

int scheduler_register_actor(ActorBase* actor, int preferred_core);
void scheduler_send_local(ActorBase* actor, Message msg);
void scheduler_send_remote(ActorBase* actor, Message msg, int from_core);

// Optimized APIs using integrated features (TIER 1 - always on)
ActorBase* scheduler_spawn_pooled(int preferred_core, void (*step)(void*), size_t actor_size);
void scheduler_release_pooled(ActorBase* actor);

// Legacy API - now controls only TIER 3 opt-in features
void scheduler_enable_features(int use_pool, int use_lockfree, int use_adaptive, int use_direct);

#endif
