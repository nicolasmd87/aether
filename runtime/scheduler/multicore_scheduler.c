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
#include "../utils/aether_thread.h"
#ifndef _WIN32
#include <sched.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <time.h>
#include "multicore_scheduler.h"
#include "../utils/aether_cpu_detect.h"
#include "../utils/aether_compiler.h"
#include "../config/aether_optimization_config.h"
#include "../aether_numa.h"
#include "../actors/aether_send_buffer.h"

// Forward declaration to avoid header cycle with aether_send_message.h
extern void aether_send_message(void* actor_ptr, void* message_data, size_t message_size);

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

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

// Per-core message tracking for wait_for_idle() - no atomic contention!
// Messages sent from main thread (non-scheduler threads) use this atomic counter
// This is rare (just initial messages), so the atomic overhead is negligible
static atomic_uint_fast64_t main_thread_sent = 0;

// Guard: scheduler_wait() joins threads exactly once even if called multiple times.
// Without this, calling wait_for_idle() followed by the shutdown sequence would
// pthread_join already-joined threads — undefined behaviour (crash on Linux glibc).
static atomic_int g_threads_joined = 0;

AETHER_TLS int current_core_id = -1;

// Pin thread to specific CPU core (NUMA awareness)
// Full support on Linux, macOS, and Windows
static void pin_to_core(int core_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(__APPLE__)
    // macOS uses thread affinity tags - threads with same tag tend to run on same core
    // This is a hint to the scheduler, not a hard binding (macOS design philosophy)
    thread_affinity_policy_data_t policy = { core_id + 1 };  // Tag must be > 0
    thread_policy_set(pthread_mach_thread_np(pthread_self()),
                      THREAD_AFFINITY_POLICY,
                      (thread_policy_t)&policy,
                      THREAD_AFFINITY_POLICY_COUNT);

    // Set high QoS to encourage P-core usage on Apple Silicon
    // QOS_CLASS_USER_INTERACTIVE has highest priority and prefers performance cores
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#elif defined(_WIN32)
    DWORD_PTR mask = (DWORD_PTR)1 << core_id;
    SetThreadAffinityMask(GetCurrentThread(), mask);
#endif
}

// Partitioned scheduler thread with work-stealing fallback for idle cores
void* AETHER_HOT scheduler_thread(void* arg) {
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
        
        // TIER 1 ALWAYS ON: Message coalescing (batch dequeue reduces atomics from N to 1)
        sched->coalesce_buffer.count = queue_dequeue_batch(
            &sched->incoming_queue,
            sched->coalesce_buffer.actors,
            sched->coalesce_buffer.messages,
            batch_size);
        
        // Process coalesced batch with minimal overhead
        for (int i = 0; i < sched->coalesce_buffer.count; i++) {
            ActorBase* actor = (ActorBase*)sched->coalesce_buffer.actors[i];
            Message msg = sched->coalesce_buffer.messages[i];

            // Actor may have been migrated to another core after this message
            // was enqueued.  Forward to the actor's current core rather than
            // delivering here (which would race with the new core's processing).
            if (unlikely(actor->assigned_core != sched->core_id)) {
                int new_core = actor->assigned_core;
                if (new_core >= 0 && new_core < num_cores) {
                    // Spin-retry: must not drop messages during redirect
                    int retries = 0;
                    while (!queue_enqueue(&schedulers[new_core].incoming_queue, actor, msg)) {
                        // Actor may have migrated again — re-read destination
                        int cur = actor->assigned_core;
                        if (cur >= 0 && cur < num_cores) new_core = cur;
                        if (++retries % 1000 == 0) sched_yield();
                    }
                }
                work_done = 1;
                continue;
            }

            // auto_process actors own their mailbox from their thread;
            // deliver via SPSC queue (thread-safe) instead of mailbox.
            if (unlikely(actor->auto_process)) {
                if (!spsc_enqueue(&actor->spsc_queue, msg)) {
                    // SPSC full - re-queue for next iteration
                    queue_enqueue(&sched->incoming_queue, actor, msg);
                } else {
                    work_done = 1;
                }
                continue;
            }

            // For actors processed on main thread, just deliver to mailbox (don't process)
            if (unlikely(atomic_load_explicit(&actor->main_thread_only, memory_order_acquire))) {
                mailbox_send(&actor->mailbox, msg);
                work_done = 1;
                continue;
            }

            // Drain actor mailbox aggressively BEFORE trying to add new message
            if (actor->mailbox.count > MAILBOX_SIZE / 2) {
                // Mailbox getting full - drain it NOW
                int drained = 0;
                while (actor->mailbox.count > 0 && drained < 128) {
                    if (actor->step) actor->step(actor);
                    drained++;
                }
                sched->messages_processed += drained;  // Batch update!
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
                AETHER_PREFETCH(sched->actors[i + 1], 0, 3);
            }

            if (unlikely(!actor)) continue;

            // Skip actors processed on main thread
            if (unlikely(atomic_load_explicit(&actor->main_thread_only, memory_order_acquire))) continue;

            // auto_process actors own their mailbox and SPSC from their
            // own thread.  Skip entirely — touching either here would
            // race with aether_actor_thread.  Messages arrive via the
            // coalescing path above (spsc_enqueue).
            if (unlikely(actor->auto_process)) {
                work_done = 1;
                continue;
            }

            // FIRST: Process actor to ensure progress even during migration
            if (likely(actor->active)) {
                if (likely(actor->step)) {
                    // Drain SPSC queue (lock-free same-core messages)
                    Message spsc_msgs[128];
                    int spsc_count = spsc_dequeue_batch(&actor->spsc_queue, spsc_msgs, 128);
                    if (spsc_count > 0) {
                        mailbox_send_batch(&actor->mailbox, spsc_msgs, spsc_count);
                    }

                    // Process messages in mailbox
                    int processed = 0;
                    while (actor->mailbox.count > 0 && processed < 64) {
                        actor->step(actor);
                        processed++;
                    }
                    // Batch counter update - single write per batch instead of per message!
                    sched->messages_processed += processed;
                    if (processed > 0 && actor->mailbox.count == 0) {
                        actor->active = 0;
                    }
                }
                work_done = 1;
            }

            // THEN: Message-driven migration — move actor to the core that
            // communicates with it most.  Processing first ensures the actor
            // makes progress even under constant migration pressure.
            if (unlikely(actor->migrate_to >= 0 &&
                         actor->migrate_to != sched->core_id &&
                         actor->migrate_to < num_cores)) {
                int dst_core = actor->migrate_to;
                Scheduler* dst = &schedulers[dst_core];

                // Lock in ascending core-id order to prevent deadlock
                OptimizedSpinlock* first_lock  = (sched->core_id < dst_core) ? &sched->actor_lock : &dst->actor_lock;
                OptimizedSpinlock* second_lock = (sched->core_id < dst_core) ? &dst->actor_lock : &sched->actor_lock;

                if (!atomic_flag_test_and_set_explicit(
                        &first_lock->lock, memory_order_acquire)) {
                    if (!atomic_flag_test_and_set_explicit(
                            &second_lock->lock, memory_order_acquire)) {
                        if (dst->actor_count < dst->capacity) {
                            sched->actors[i] = sched->actors[--sched->actor_count];
                            actor->assigned_core = dst_core;
                            actor->migrate_to = -1;
                            dst->actors[dst->actor_count++] = actor;

                            atomic_flag_clear_explicit(&second_lock->lock, memory_order_release);
                            atomic_flag_clear_explicit(&first_lock->lock, memory_order_release);

                            i--;  // Re-examine this slot (now holds a different actor)
                            continue;
                        }
                        atomic_flag_clear_explicit(&second_lock->lock, memory_order_release);
                    }
                    atomic_flag_clear_explicit(&first_lock->lock, memory_order_release);
                }
            }
        }
        
        // Primary: partitioned assignment (actors stay on assigned core for cache locality)
        // Fallback: work stealing kicks in after extended idle to balance load
        
        if (!work_done) {
            idle_count++;
            atomic_fetch_add_explicit(&sched->idle_cycles, 1, memory_order_relaxed);
            
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
                
                // If found a busy core, try to steal an actor.
                // Lock in ascending core-id order to prevent deadlock
                // (same convention as migration).
                if (busiest_core >= 0 && max_work > 100) {
                    Scheduler* victim = &schedulers[busiest_core];
                    OptimizedSpinlock* first_lock  = (sched->core_id < busiest_core) ? &sched->actor_lock : &victim->actor_lock;
                    OptimizedSpinlock* second_lock = (sched->core_id < busiest_core) ? &victim->actor_lock : &sched->actor_lock;

                    if (!atomic_flag_test_and_set_explicit(&first_lock->lock, memory_order_acquire)) {
                        if (!atomic_flag_test_and_set_explicit(&second_lock->lock, memory_order_acquire)) {
                            if (victim->actor_count > 4 && sched->actor_count < sched->capacity) {
                                ActorBase* stolen = victim->actors[--victim->actor_count];
                                stolen->assigned_core = sched->core_id;
                                stolen->migrate_to = -1;
                                sched->actors[sched->actor_count++] = stolen;
                                work_done = 1;
                                atomic_fetch_add(&sched->steal_attempts, 1);
                            }
                            atomic_flag_clear_explicit(&second_lock->lock, memory_order_release);
                        }
                        atomic_flag_clear_explicit(&first_lock->lock, memory_order_release);
                    }
                }
            }
            
            // Keep spinning aggressively for high-throughput workloads
            if (idle_count < 10000) {
                // Tight spin with architecture-specific pause
                #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
                __asm__ __volatile__("pause" ::: "memory");
                #elif defined(__aarch64__) || defined(__arm64__) || defined(__arm__)
                __asm__ __volatile__("yield" ::: "memory");
                #endif
            } else {
                // Brief yield only after extended idle
                sched_yield();
                idle_count = 5000;
            }
        } else {
            idle_count = 0;
            atomic_store_explicit(&sched->idle_cycles, 0, memory_order_relaxed);
        }
        
        // Explicit check to ensure timely exit on all platforms
        if (!atomic_load_explicit(&sched->running, memory_order_acquire)) {
            break;
        }
    }
    
    return NULL;
}

void scheduler_init(int cores) {
    // Reset join guard so the scheduler can be restarted (e.g. in tests).
    atomic_store_explicit(&g_threads_joined, 0, memory_order_relaxed);

    // TIER 2: Auto-detect hardware capabilities first
    aether_detect_hardware();

    // Initialize profile and inline mode from env vars
    aether_init_from_env();

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

        // Per-core message counters (no atomics needed - core-local)
        schedulers[i].messages_sent = 0;
        schedulers[i].messages_processed = 0;

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
    // MAIN THREAD MODE: Single-actor programs don't need scheduler threads
    // All message processing happens synchronously on the main thread
    if (aether_main_thread_mode_active()) {
        return;  // No threads to start
    }

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

// Count total pending messages across all queues, SPSC queues, and mailboxes
static inline int count_pending_messages(void) {
    int total = 0;
    for (int i = 0; i < num_cores; i++) {
        Scheduler* core = &schedulers[i];
        total += queue_size(&core->incoming_queue);
        for (int j = 0; j < core->actor_count; j++) {
            ActorBase* actor = core->actors[j];
            if (actor) {
                total += actor->mailbox.count;
                total += spsc_count(&actor->spsc_queue);
            }
        }
    }
    return total;
}

void scheduler_wait() {
    // MAIN THREAD MODE: All messages processed synchronously, nothing to wait for
    // This is the fastest path for single-actor programs (counting benchmark)
    if (aether_main_thread_mode_active()) {
        return;  // No scheduler threads, no waiting needed
    }

    // Check if scheduler is still running
    int still_running = 0;
    for (int i = 0; i < num_cores; i++) {
        if (atomic_load_explicit(&schedulers[i].running, memory_order_acquire)) {
            still_running = 1;
            break;
        }
    }

    // If still running, wait for all messages to be processed first
    if (still_running) {
        // Uses per-core counters (Linux kernel's per-CPU counter pattern)
        // No atomic contention on the hot path - only sum on read
        int stable_count = 0;
        while (stable_count < 3) {  // Require 3 consecutive stable reads
            // Memory barrier to ensure we see latest values from other cores
            atomic_thread_fence(memory_order_acquire);

            // Sum all sent counters
            uint64_t total_sent = atomic_load_explicit(&main_thread_sent, memory_order_relaxed);
            for (int i = 0; i < num_cores; i++) {
                total_sent += schedulers[i].messages_sent;
            }

            // Sum all processed counters
            uint64_t total_processed = 0;
            for (int i = 0; i < num_cores; i++) {
                total_processed += schedulers[i].messages_processed;
            }

            if (total_sent == total_processed) {
                stable_count++;
            } else {
                stable_count = 0;
            }

            // Brief spin between checks for memory visibility
            // Note: 500 iterations is enough for memory visibility while keeping latency low
            for (int spin = 0; spin < 500; spin++) {
                #if defined(__x86_64__) || defined(_M_X64)
                __asm__ __volatile__("pause" ::: "memory");
                #elif defined(__aarch64__)
                __asm__ __volatile__("yield" ::: "memory");
                #endif
            }
        }

        // Signal threads to stop
        scheduler_stop();
    }

    // Join threads — but only once. If wait_for_idle() already joined them,
    // a second call (from the generated shutdown sequence) must be a no-op.
    // pthread_join on an already-joined thread is undefined behaviour (crash on Linux).
    if (atomic_exchange_explicit(&g_threads_joined, 1, memory_order_acq_rel) == 0) {
        for (int i = 0; i < num_cores; i++) {
            int result = pthread_join(schedulers[i].thread, NULL);
            (void)result;  // Suppress unused warning
        }

        // Cleanup NUMA resources
        aether_numa_cleanup();
    }
}

void scheduler_cleanup() {
    // Free allocated scheduler resources
    for (int i = 0; i < num_cores; i++) {
        // Clean up thread resources
        schedulers[i].thread = 0;

        if (schedulers[i].actors != NULL) {
            // Free with the size we allocated, not current capacity
            aether_numa_free(schedulers[i].actors, MAX_ACTORS_PER_CORE * sizeof(ActorBase*));
            schedulers[i].actors = NULL;
        }
        if (schedulers[i].actor_pool != NULL) {
            aether_numa_free(schedulers[i].actor_pool, sizeof(ActorPool));
            schedulers[i].actor_pool = NULL;
        }

        // Reset counters
        schedulers[i].actor_count = 0;
        schedulers[i].capacity = 0;
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

    // Initialize SPSC queue for same-core messaging
    spsc_queue_init(&actor->spsc_queue);

    sched->actors[sched->actor_count++] = actor;

    return preferred_core;
}

// Thread-local recursion guard for work inlining (prevent stack overflow)
static AETHER_TLS int inline_depth = 0;
#define MAX_INLINE_DEPTH 16  // Limit recursion to prevent stack overflow

void scheduler_send_local(ActorBase* actor, Message msg) {
    // NOTE: Inline mode optimization is handled by the existing WORK INLINING
    // code below (lines 590+), which is safe because it checks for scheduler
    // thread context (current_core_id >= 0 && actor->assigned_core == current_core_id).
    //
    // The profile-based inline mode (aether_inline_mode_active) is NOT used here
    // because it would cause races when main thread sends to actors being
    // processed by scheduler threads.

    // Per-core sent counter - no atomic contention on hot path!
    if (likely(current_core_id >= 0)) {
        schedulers[current_core_id].messages_sent++;
    } else {
        // Main thread (rare) - use atomic
        atomic_fetch_add_explicit(&main_thread_sent, 1, memory_order_relaxed);
    }
    // auto_process actors own their mailbox; deliver via thread-safe SPSC.
    if (unlikely(actor->auto_process)) {
        spsc_enqueue(&actor->spsc_queue, msg);
    } else {
        mailbox_send(&actor->mailbox, msg);
    }
    actor->active = 1;

    // WORK INLINING: If actor is idle and we're not too deep, run it immediately.
    // This eliminates scheduler loop overhead for tight request-response patterns.
    if (likely(current_core_id >= 0) &&
        inline_depth < MAX_INLINE_DEPTH &&
        actor->assigned_core == current_core_id &&
        actor->mailbox.count == 1) {  // Just this message, actor was idle
        inline_depth++;
        actor->step(actor);
        schedulers[current_core_id].messages_processed++;
        if (actor->mailbox.count == 0) {
            actor->active = 0;
        }
        inline_depth--;
    }
}

void scheduler_send_remote(ActorBase* actor, Message msg, int from_core) {
    // INLINE MODE: For single-actor programs, process synchronously on the main thread.
    // scheduler_send_batch_add has the same check; keep them in sync.
    if (unlikely(aether_main_thread_mode_active())) {
        mailbox_send(&actor->mailbox, msg);
        actor->step(actor);
        AETHER_STAT_INC(inline_sends);
        return;
    }

    // Per-core sent counter - no atomic contention on hot path!
    if (likely(current_core_id >= 0)) {
        schedulers[current_core_id].messages_sent++;
    } else {
        // Main thread (rare) - use atomic
        atomic_fetch_add_explicit(&main_thread_sent, 1, memory_order_relaxed);
    }
    int target_core = actor->assigned_core;

    // Guard against uninitialized or invalid assigned_core
    if (unlikely(target_core < 0 || target_core >= num_cores)) {
        target_core = actor->id % num_cores;
    }

    // Already same-core AND caller is actually running on that core's
    // scheduler thread — deliver directly to mailbox (no queue overhead).
    // The current_core_id check prevents non-scheduler threads (e.g. main)
    // from racing with the scheduler thread on mailbox access.
    if (from_core >= 0 && from_core == target_core &&
        from_core == current_core_id) {
        if (unlikely(actor->auto_process)) {
            spsc_enqueue(&actor->spsc_queue, msg);
        } else {
            mailbox_send(&actor->mailbox, msg);
        }
        actor->active = 1;
        AETHER_STAT_INC(direct_sends);
        return;
    }

    // Cross-core send: set affinity hint so communicating actors converge.
    // STABLE CONVERGENCE: Always migrate to the LOWER core ID to prevent oscillation.
    // This ensures ping-pong actors eventually end up on the same core.
    if (from_core >= 0 && from_core == current_core_id && !actor->auto_process) {
        int target_migrate = (from_core < target_core) ? from_core : target_core;
        // Only set if it would move the actor to a lower core (stable direction)
        if (target_migrate < actor->assigned_core &&
            (actor->migrate_to < 0 || target_migrate < actor->migrate_to)) {
            actor->migrate_to = target_migrate;
        }
    }

    // Enqueue to target core's incoming queue
    int retries = 0;
    while (!queue_enqueue(&schedulers[target_core].incoming_queue, actor, msg)) {
        if (++retries % 1000 == 0) {
            sched_yield();
        }
        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        __asm__ __volatile__("pause" ::: "memory");
        #elif defined(__aarch64__) || defined(__arm64__) || defined(__arm__)
        __asm__ __volatile__("yield" ::: "memory");
        #endif
    }

    atomic_fetch_add_explicit(&schedulers[target_core].work_count, 1, memory_order_relaxed);
    AETHER_STAT_INC(queue_sends);
}

// ==============================================================================
// BATCH SEND OPTIMIZATION (for main thread fan-out patterns)
// ==============================================================================
// Reduces atomic operations from N to num_cores by grouping messages by target
// core and using queue_enqueue_batch for bulk insertion.
//
// Performance: For fork-join with 8 workers and 1M messages:
//   - Without batching: ~1M atomic ops (one per message)
//   - With batching: ~4K atomic ops (256 messages per batch, 8 cores)

#define BATCH_SEND_SIZE 256

typedef struct {
    ActorBase* actors[BATCH_SEND_SIZE];
    Message messages[BATCH_SEND_SIZE];
    int count;
    int by_core[MAX_CORES];  // Count per target core for efficient sorting
} BatchSendBuffer;

static AETHER_TLS BatchSendBuffer* g_batch_buffer = NULL;

void scheduler_send_batch_start(void) {
    if (!g_batch_buffer) {
        g_batch_buffer = calloc(1, sizeof(BatchSendBuffer));
    }
    g_batch_buffer->count = 0;
    memset(g_batch_buffer->by_core, 0, sizeof(g_batch_buffer->by_core));
}

void scheduler_send_batch_add(ActorBase* actor, Message msg) {
    // FAST PATH: Single-actor programs bypass batching entirely
    // Main Thread Mode = synchronous processing, zero queue overhead
    if (aether_main_thread_mode_active()) {
        mailbox_send(&actor->mailbox, msg);
        actor->step(actor);
        AETHER_STAT_INC(inline_sends);
        return;
    }

    // BATCH PATH: Multi-actor fan-out optimization
    if (!g_batch_buffer) {
        scheduler_send_batch_start();
    }

    // If buffer is full, flush first
    if (g_batch_buffer->count >= BATCH_SEND_SIZE) {
        scheduler_send_batch_flush();
    }

    int idx = g_batch_buffer->count++;
    g_batch_buffer->actors[idx] = actor;
    g_batch_buffer->messages[idx] = msg;

    // Track per-core counts for O(1) sorting later
    int target_core = actor->assigned_core;
    if (target_core >= 0 && target_core < num_cores) {
        g_batch_buffer->by_core[target_core]++;
    }
}

void scheduler_send_batch_flush(void) {
    if (!g_batch_buffer || g_batch_buffer->count == 0) return;

    // === PHASE 1: Compute offsets for radix sort by core ===
    int offsets[MAX_CORES];
    int offset = 0;
    for (int c = 0; c < num_cores; c++) {
        offsets[c] = offset;
        offset += g_batch_buffer->by_core[c];
    }

    // === PHASE 2: Sort messages into per-core buckets ===
    void* sorted_actors[BATCH_SEND_SIZE];
    Message sorted_msgs[BATCH_SEND_SIZE];
    int positions[MAX_CORES];
    memcpy(positions, offsets, sizeof(offsets));

    for (int i = 0; i < g_batch_buffer->count; i++) {
        ActorBase* actor = g_batch_buffer->actors[i];
        int target_core = actor->assigned_core;
        if (target_core < 0 || target_core >= num_cores) {
            target_core = actor->id % num_cores;
        }
        int pos = positions[target_core]++;
        sorted_actors[pos] = actor;
        sorted_msgs[pos] = g_batch_buffer->messages[i];
    }

    // === PHASE 3: Batch enqueue to each core (ONE atomic per core!) ===
    uint64_t total_sent = 0;
    for (int c = 0; c < num_cores; c++) {
        int start = offsets[c];
        int count = g_batch_buffer->by_core[c];
        if (count == 0) continue;

        // Use queue_enqueue_batch: single atomic_store for entire batch!
        int enqueued = queue_enqueue_batch(
            &schedulers[c].incoming_queue,
            &sorted_actors[start],
            &sorted_msgs[start],
            count
        );

        // Fallback for overflow (rare) - scheduler_send_remote handles its own counting
        for (int j = enqueued; j < count; j++) {
            scheduler_send_remote(sorted_actors[start + j], sorted_msgs[start + j], -1);
        }

        // Only count batch-enqueued messages here (fallback already counted by send_remote)
        total_sent += enqueued;
        atomic_fetch_add_explicit(&schedulers[c].work_count, enqueued, memory_order_relaxed);
    }

    // Single atomic update for batch-sent messages
    atomic_fetch_add_explicit(&main_thread_sent, total_sent, memory_order_relaxed);

    // Reset buffer
    g_batch_buffer->count = 0;
    memset(g_batch_buffer->by_core, 0, sizeof(g_batch_buffer->by_core));
}

// Spawn actor with NUMA-aware allocation.  actor_size must be >= sizeof(ActorBase)
// and cover the full derived-actor struct (e.g. sizeof(PingActor)).
ActorBase* scheduler_spawn_pooled(int preferred_core, void (*step)(void*), size_t actor_size) {
    if (preferred_core < 0 || preferred_core >= num_cores) {
        preferred_core = atomic_fetch_add(&next_actor_id, 1) % num_cores;
    }
    if (actor_size < sizeof(ActorBase)) actor_size = sizeof(ActorBase);

    ActorBase* actor = NULL;

    {
        int numa_node = aether_numa_node_of_cpu(preferred_core);
        actor = aether_numa_alloc(actor_size, numa_node);
        if (!actor) return NULL;
        mailbox_init(&actor->mailbox);
        spsc_queue_init(&actor->spsc_queue);
        AETHER_STAT_INC(actors_malloced);
    }
    
    actor->id = atomic_fetch_add(&next_actor_id, 1);
    actor->step = step;
    actor->active = 1;
    actor->thread = 0;
    actor->auto_process = 0;
    actor->assigned_core = preferred_core;
    actor->migrate_to = -1;
    atomic_init(&actor->main_thread_only, 0);
    atomic_init(&actor->reply_slot, NULL);

    // Track actor count for inline mode auto-detection
    // Get previous count and main_actor BEFORE aether_on_actor_spawn modifies them
    int prev_count = atomic_load_explicit(&g_aether_config.actor_count, memory_order_relaxed);
    ActorBase* prev_main_actor = (ActorBase*)g_aether_config.main_actor;
    aether_on_actor_spawn();

    // MAIN THREAD MODE handling
    if (prev_count == 0 && !atomic_load(&g_aether_config.inline_mode_disabled)) {
        // First actor: enable main thread mode for synchronous processing
        aether_enable_main_thread_mode(actor);
        atomic_store_explicit(&actor->main_thread_only, 1, memory_order_release);
    } else if (prev_count == 1 && prev_main_actor != NULL) {
        // Second actor: disable main thread mode on the first actor
        // so scheduler threads can process both actors normally
        // Use atomic store to prevent data race with scheduler thread reads
        atomic_store_explicit(&prev_main_actor->main_thread_only, 0, memory_order_release);
    }

    scheduler_register_actor(actor, preferred_core);

    return actor;
}

// TIER 1 ALWAYS ON: Release actor back to pool
void scheduler_release_pooled(ActorBase* actor) {
    if (!actor) return;

    // Track actor count for inline mode auto-detection
    aether_on_actor_terminate();

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

// ============================================================================
// Ask/Reply support (experimental)
// ============================================================================

// Decrement the slot refcount; free when it reaches zero.
// NOTE: reply_data is NOT freed here — it is transferred to the ask caller.
static void reply_slot_decref(ActorReplySlot* slot) {
    if (atomic_fetch_sub_explicit(&slot->refcount, 1, memory_order_acq_rel) == 1) {
        pthread_cond_destroy(&slot->cond);
        pthread_mutex_destroy(&slot->mutex);
        free(slot);
    }
}

// Send a message to target and block until a reply is received or timeout_ms expires.
// Returns a malloc'd pointer to the reply payload (caller must free), or NULL on timeout.
void* scheduler_ask_message(ActorBase* target, void* msg_data, size_t msg_size, int timeout_ms) {
    if (!target || !msg_data) return NULL;

    // Allocate and initialize the reply slot (refcount starts at 2: asker + actor).
    ActorReplySlot* slot = (ActorReplySlot*)calloc(1, sizeof(ActorReplySlot));
    if (!slot) return NULL;

    pthread_mutex_init(&slot->mutex, NULL);
    pthread_cond_init(&slot->cond, NULL);
    atomic_init(&slot->refcount, 2);

    // Publish the slot before sending so the actor can reply immediately.
    atomic_store_explicit(&target->reply_slot, slot, memory_order_seq_cst);

    // Deliver the ask message using the same path as fire-and-forget.
    aether_send_message((void*)target, msg_data, msg_size);

    // Build absolute timeout deadline.
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    // Wait for the reply (or timeout).
    pthread_mutex_lock(&slot->mutex);
    while (!slot->reply_ready) {
        if (pthread_cond_timedwait(&slot->cond, &slot->mutex, &ts) == ETIMEDOUT) break;
    }
    void* result = slot->reply_ready ? slot->reply_data : NULL;

    if (!slot->reply_ready) {
        // Timed out: mark slot so the actor won't try to signal it.
        slot->timed_out = 1;
        // Try to detach the slot from the actor atomically.
        // If the CAS succeeds:  actor hasn't exchanged yet; we take the actor's ref.
        // If the CAS fails:     actor already owns the slot and will decref its own ref.
        ActorReplySlot* expected = slot;
        if (atomic_compare_exchange_strong_explicit(&target->reply_slot, &expected, NULL,
                                                    memory_order_seq_cst, memory_order_seq_cst)) {
            reply_slot_decref(slot);  // release the actor's ref (actor won't use the slot)
        }
    }
    pthread_mutex_unlock(&slot->mutex);

    reply_slot_decref(slot);  // release the asker's ref
    return result;
}

// Called from within an actor's receive handler to reply to a pending ask.
// data/data_size are copied internally; the asker is responsible for freeing the result.
void scheduler_reply(ActorBase* self, void* data, size_t data_size) {
    // Atomically take the slot so concurrent timeout cannot race with us.
    ActorReplySlot* slot = atomic_exchange_explicit(&self->reply_slot, NULL, memory_order_seq_cst);
    if (!slot) return;  // no pending ask, or asker already timed out and cleared it

    pthread_mutex_lock(&slot->mutex);
    if (!slot->timed_out) {
        // Copy the reply payload for the waiting caller.
        if (data && data_size > 0) {
            slot->reply_data = malloc(data_size);
            if (slot->reply_data) {
                memcpy(slot->reply_data, data, data_size);
                slot->reply_size = data_size;
            }
        }
        slot->reply_ready = 1;
        pthread_cond_signal(&slot->cond);
    }
    pthread_mutex_unlock(&slot->mutex);

    reply_slot_decref(slot);  // release the actor's ref
}
