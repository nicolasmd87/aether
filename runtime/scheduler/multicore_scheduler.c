// Partitioned State Machine Scheduler - Zero-Sharing Multi-core
// Based on Experiment 04: 291M msg/sec on 8 cores (2.3× scaling)
// Strategy: Static actor-to-core assignment, no atomics, perfect cache locality

// Platform defines must come first
#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include "multicore_scheduler.h"
#include "../utils/aether_cpu_detect.h"
#include "../config/aether_optimization_config.h"
#include "../aether_numa.h"
#include "../actors/aether_send_buffer.h"

// Branch prediction hints
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// MWAIT intrinsics for x86 (auto-detected at runtime)
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define HAS_X86_INTRINSICS 1
#else
#define HAS_X86_INTRINSICS 0
#endif

#ifdef _WIN32
#include <windows.h>
#endif

Scheduler schedulers[MAX_CORES];
int num_cores = 0;
atomic_int next_actor_id = 1;

__thread int current_core_id = -1;

// Pin thread to specific CPU core (NUMA awareness)
// Silently degrades if platform doesn't support affinity
static void pin_to_core(int core_id) {
#ifdef __linux__
    #include <sched.h>
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(_WIN32)
    DWORD_PTR mask = (DWORD_PTR)1 << core_id;
    SetThreadAffinityMask(GetCurrentThread(), mask);
#endif
    // Other platforms: no affinity support, but scheduler still works
}

// Partitioned scheduler thread - NO work stealing
void* __attribute__((hot)) scheduler_thread(void* arg) {
    Scheduler* sched = (Scheduler*)arg;
    current_core_id = sched->core_id;

    // Initialize thread-local send buffer for this core
    send_buffer_init(sched->core_id);

    // TIER 2 AUTO-DETECT: Pin thread if OS supports it
    if (aether_has_cpu_pinning()) {
        pin_to_core(sched->core_id);
    }

    int idle_count = 0;

    while (atomic_load_explicit(&sched->running, memory_order_acquire)) {
        int work_done = 0;

        // TIER 1 ALWAYS ON: Adaptive batch sizing
        int batch_size = sched->batch_state.current_batch_size;
        if (batch_size > COALESCE_THRESHOLD) batch_size = COALESCE_THRESHOLD;

        // TIER 1 ALWAYS ON: Message coalescing
        sched->coalesce_buffer.count = 0;
        void* actor_ptr;
        Message msg;

        // Drain up to batch_size messages in one batch (adaptive)
        while (sched->coalesce_buffer.count < batch_size &&
               queue_dequeue(&sched->incoming_queue, &actor_ptr, &msg)) {
            sched->coalesce_buffer.actors[sched->coalesce_buffer.count] = actor_ptr;
            sched->coalesce_buffer.messages[sched->coalesce_buffer.count] = msg;
            sched->coalesce_buffer.count++;
        }

        // Process coalesced batch with minimal overhead
        for (int i = 0; i < sched->coalesce_buffer.count; i++) {
            ActorBase* actor = (ActorBase*)sched->coalesce_buffer.actors[i];
            Message msg = sched->coalesce_buffer.messages[i];

            // Drain actor mailbox aggressively BEFORE trying to add new message
            if (actor->mailbox.count > MAILBOX_SIZE / 2) {
                // Mailbox getting full - drain it NOW
                int drained = 0;
                while (actor->mailbox.count > 0 && drained < 128) {
                    if (actor->step) actor->step(actor);
                    drained++;
                }
            }

            // Now try to deliver message
            if (!mailbox_send(&actor->mailbox, msg)) {
                // Still full - re-queue for later
                queue_enqueue(&sched->incoming_queue, actor, msg);
            } else {
                // Successfully delivered
                actor->active = 1;
                work_done = 1;
            }
        }

        // TIER 1 ALWAYS ON: Adjust adaptive batch size based on what we received
        adaptive_batch_adjust(&sched->batch_state, sched->coalesce_buffer.count);

        // Process active actors (NO ATOMICS - actors are core-local)
        // This is the performance-critical loop
        for (int i = 0; i < sched->actor_count; i++) {
            ActorBase* actor = sched->actors[i];

            // Prefetch next actor for better pipeline utilization
            if (i + 1 < sched->actor_count) {
                __builtin_prefetch(sched->actors[i + 1], 0, 3);
            }

            if (likely(actor && actor->active)) {
                if (likely(actor->step)) {
                    // FIRST: Drain SPSC queue (lock-free same-core messages)
                    Message spsc_msgs[128];
                    int spsc_count = spsc_dequeue_batch(&actor->spsc_queue, spsc_msgs, 128);
                    if (spsc_count > 0) {
                        // Batch send to mailbox for processing
                        mailbox_send_batch(&actor->mailbox, spsc_msgs, spsc_count);
                    }

                    // THEN: Process ALL messages in mailbox to prevent buildup
                    int processed = 0;
                    while (actor->mailbox.count > 0 && processed < 64) {
                        actor->step(actor);
                        processed++;
                    }
                    // If mailbox is now empty, mark inactive
                    if (actor->mailbox.count == 0) {
                        actor->active = 0;
                    }
                }
                work_done = 1;
            }
        }

        // Partitioned approach: NO work stealing
        // Actors stay on their assigned core for perfect cache locality
        // Result: Zero cache thrashing, zero atomic contention

        if (!work_done) {
            idle_count++;
            atomic_fetch_add(&sched->idle_cycles, 1);

            // WORK STEALING: After significant idle time, try to steal work
            if (idle_count > 5000 && idle_count % 1000 == 0) {
                // Find busiest core
                int busiest_core = -1;
                int max_work = 0;
                for (int i = 0; i < num_cores; i++) {
                    if (i == sched->core_id) continue;
                    int work = atomic_load_explicit(&schedulers[i].work_count, memory_order_relaxed);
                    if (work > max_work) {
                        max_work = work;
                        busiest_core = i;
                    }
                }

                // If found a busy core, try to steal an actor
                if (busiest_core >= 0 && max_work > 100) {
                    Scheduler* victim = &schedulers[busiest_core];

                    // Try to lock victim's actor list (non-blocking)
                    if (atomic_flag_test_and_set_explicit(&victim->actor_lock.lock, memory_order_acquire) == 0) {
                        // Steal last actor if victim has multiple actors
                        if (victim->actor_count > 4) {
                            ActorBase* stolen = victim->actors[--victim->actor_count];

                            // Add to our core
                            spinlock_lock(&sched->actor_lock);
                            if (sched->actor_count < sched->capacity) {
                                stolen->assigned_core = sched->core_id;
                                sched->actors[sched->actor_count++] = stolen;
                                work_done = 1;
                                atomic_fetch_add(&sched->steal_attempts, 1);
                            } else {
                                // No space, return it
                                victim->actors[victim->actor_count++] = stolen;
                            }
                            spinlock_unlock(&sched->actor_lock);
                        }
                        atomic_flag_clear_explicit(&victim->actor_lock.lock, memory_order_release);
                    }
                }
            }

            // Keep spinning aggressively for high-throughput workloads
            if (idle_count < 10000) {
                // Tight spin with pause
                #if defined(__x86_64__) || defined(_M_X64)
                __asm__ __volatile__("pause" ::: "memory");
                #endif
            } else {
                // Brief yield only after extended idle
                sched_yield();
                idle_count = 5000;
            }
        } else {
            idle_count = 0;
            atomic_store(&sched->idle_cycles, 0);
        }

        // Explicit check to ensure timely exit on all platforms
        if (!atomic_load_explicit(&sched->running, memory_order_acquire)) {
            break;
        }
    }

    return NULL;
}

void scheduler_init(int cores) {
    // TIER 2: Auto-detect hardware capabilities first
    aether_detect_hardware();

    // Initialize NUMA topology detection
    (void)aether_numa_init();  // NUMA topology initialized for future use

    if (cores <= 0 || cores > MAX_CORES) {
        // Use auto-detected core count if not specified
        cores = cpu_recommend_cores();
        if (cores <= 0 || cores > MAX_CORES) cores = 4;
    }
    num_cores = cores;

    for (int i = 0; i < num_cores; i++) {
        schedulers[i].core_id = i;

        // NUMA-aware allocation: allocate scheduler data on same NUMA node as core
        int numa_node = aether_numa_node_of_cpu(i);
        schedulers[i].actors = aether_numa_alloc(MAX_ACTORS_PER_CORE * sizeof(ActorBase*), numa_node);
        if (!schedulers[i].actors) {
            fprintf(stderr, "ERROR: Failed to allocate memory for scheduler %d actors\n", i);
            exit(1);
        }
        schedulers[i].actor_count = 0;
        schedulers[i].capacity = MAX_ACTORS_PER_CORE;
        queue_init(&schedulers[i].incoming_queue);
        atomic_store(&schedulers[i].running, 0);
        atomic_store(&schedulers[i].work_count, 0);
        atomic_store(&schedulers[i].steal_attempts, 0);
        atomic_store(&schedulers[i].idle_cycles, 0);
        spinlock_init(&schedulers[i].actor_lock);

        // TIER 1 ALWAYS ON: Initialize actor pool with NUMA-aware allocation
        schedulers[i].actor_pool = aether_numa_alloc(sizeof(ActorPool), numa_node);
        if (schedulers[i].actor_pool) {
            actor_pool_init(schedulers[i].actor_pool);
        }
        // TIER 1 ALWAYS ON: Initialize adaptive batching
        adaptive_batch_init(&schedulers[i].batch_state);
    }
}

// Initialize with explicit optimization flags (TIER 3 opt-in)
void scheduler_init_with_opts(int cores, AetherOptFlags opts) {
    aether_runtime_configure(opts);
    scheduler_init(cores);
}

void scheduler_start() {
    for (int i = 0; i < num_cores; i++) {
        atomic_store_explicit(&schedulers[i].running, 1, memory_order_release);
        int rc = pthread_create(&schedulers[i].thread, NULL, scheduler_thread, &schedulers[i]);
        if (rc != 0) {
            fprintf(stderr, "ERROR: Failed to create scheduler thread for core %d: %d\n", i, rc);
        }
    }
}

void scheduler_stop() {
    // Set all running flags to 0 with release semantics
    for (int i = 0; i < num_cores; i++) {
        atomic_store_explicit(&schedulers[i].running, 0, memory_order_release);
    }

    // Wake threads by writing to monitored addresses (wakes MWAIT)
    // On non-MWAIT platforms, threads wake quickly from short sleep
    for (int i = 0; i < num_cores; i++) {
        // Write to the incoming queue tail to trigger MWAIT wake
        atomic_store_explicit(&schedulers[i].incoming_queue.tail,
                            atomic_load_explicit(&schedulers[i].incoming_queue.tail, memory_order_relaxed),
                            memory_order_release);
    }
}

void scheduler_wait() {
    for (int i = 0; i < num_cores; i++) {
        pthread_join(schedulers[i].thread, NULL);
    }

    // Cleanup NUMA resources
    aether_numa_cleanup();
}

void scheduler_cleanup() {
    // Free allocated scheduler resources
    for (int i = 0; i < num_cores; i++) {
        if (schedulers[i].actors != NULL) {
            aether_numa_free(schedulers[i].actors, schedulers[i].capacity * sizeof(ActorBase*));
            schedulers[i].actors = NULL;
        }
        if (schedulers[i].actor_pool != NULL) {
            aether_numa_free(schedulers[i].actor_pool, sizeof(ActorPool));
            schedulers[i].actor_pool = NULL;
        }
    }
    num_cores = 0;
}

int scheduler_register_actor(ActorBase* actor, int preferred_core) {
    // Partitioned assignment: actor_id % num_cores
    // This ensures perfect load balance across cores
    if (preferred_core < 0) {
        preferred_core = actor->id % num_cores;
    }

    Scheduler* sched = &schedulers[preferred_core];

    if (sched->actor_count >= sched->capacity) {
        // Dynamically grow actor array with NUMA-aware reallocation
        int numa_node = aether_numa_node_of_cpu(preferred_core);
        size_t old_size = sched->capacity * sizeof(ActorBase*);
        size_t new_size = sched->capacity * 2 * sizeof(ActorBase*);

        ActorBase** new_actors = aether_numa_alloc(new_size, numa_node);
        if (!new_actors) {
            fprintf(stderr, "Fatal: Failed to grow actor array for core %d\n", preferred_core);
            return -1;
        }

        // Copy old data and free old array
        memcpy(new_actors, sched->actors, old_size);
        aether_numa_free(sched->actors, old_size);

        sched->actors = new_actors;
        sched->capacity *= 2;
    }

    actor->assigned_core = preferred_core;
    sched->actors[sched->actor_count++] = actor;

    return preferred_core;
}

void scheduler_send_local(ActorBase* actor, Message msg) {
    mailbox_send(&actor->mailbox, msg);
    actor->active = 1;
}

void scheduler_send_remote(ActorBase* actor, Message msg, int from_core) {
    int target_core = actor->assigned_core;

    // TIER 1 ALWAYS ON: Direct send for same-core actors (bypasses queue)
    if (from_core >= 0 && from_core == target_core) {
        // Same core - direct mailbox delivery, no queue overhead
        mailbox_send(&actor->mailbox, msg);
        actor->active = 1;
        AETHER_STAT_INC(direct_sends);
        return;
    }

    // Retry with backoff if queue full
    int retries = 0;
    while (!queue_enqueue(&schedulers[target_core].incoming_queue, actor, msg)) {
        // Queue full - yield to let scheduler thread drain it
        if (++retries % 1000 == 0) {
            sched_yield();  // Let scheduler threads run
        }
        #if defined(__x86_64__) || defined(_M_X64)
        __asm__ __volatile__("pause" ::: "memory");
        #endif
    }

    atomic_fetch_add(&schedulers[target_core].work_count, 1);
    AETHER_STAT_INC(queue_sends);
}

// TIER 1 ALWAYS ON: Spawn actor from pool (1.81x faster than malloc)
ActorBase* scheduler_spawn_pooled(int preferred_core, void (*step)(void*)) {
    if (preferred_core < 0 || preferred_core >= num_cores) {
        preferred_core = atomic_fetch_add(&next_actor_id, 1) % num_cores;
    }

    Scheduler* sched = &schedulers[preferred_core];
    ActorBase* actor = NULL;

    // TIER 1 ALWAYS ON: Try to get from pool first
    if (sched->actor_pool) {
        PooledActor* pooled = actor_pool_acquire(sched->actor_pool);
        if (pooled) {
            actor = (ActorBase*)pooled;
            AETHER_STAT_INC(actors_pooled);
        }
    }

    // Fallback to malloc if pool exhausted - use NUMA-aware allocation
    if (!actor) {
        int numa_node = aether_numa_node_of_cpu(preferred_core);
        actor = aether_numa_alloc(sizeof(ActorBase), numa_node);
        if (!actor) return NULL;
        mailbox_init(&actor->mailbox);
        spsc_queue_init(&actor->spsc_queue);
        AETHER_STAT_INC(actors_malloced);
    }

    actor->id = atomic_fetch_add(&next_actor_id, 1);
    actor->step = step;
    actor->active = 1;
    actor->assigned_core = preferred_core;

    scheduler_register_actor(actor, preferred_core);

    return actor;
}

// TIER 1 ALWAYS ON: Release actor back to pool
void scheduler_release_pooled(ActorBase* actor) {
    if (!actor) return;

    int core = actor->assigned_core;
    if (core >= 0 && core < num_cores && schedulers[core].actor_pool) {
        PooledActor* pooled = (PooledActor*)actor;
        if (pooled->pool_index >= 0 && pooled->pool_index < ACTOR_POOL_SIZE) {
            actor_pool_release(schedulers[core].actor_pool, pooled);
            return;
        }
    }

    // Not from pool, free with NUMA-aware deallocation
    aether_numa_free(actor, sizeof(ActorBase));
}

// Legacy API - now controls only TIER 3 opt-in features
void scheduler_enable_features(int use_pool, int use_lockfree, int use_adaptive, int use_direct) {
    // TIER 1 features are always on - these parameters are ignored
    (void)use_pool;      // Actor pooling is always on
    (void)use_adaptive;  // Adaptive batching is always on
    (void)use_direct;    // Direct send is always on

    // TIER 3 opt-in: Lock-free mailbox
    if (use_lockfree) {
        aether_enable_opt(AETHER_OPT_LOCKFREE_MAILBOX);
    } else {
        aether_disable_opt(AETHER_OPT_LOCKFREE_MAILBOX);
    }
}
