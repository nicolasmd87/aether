// Partitioned State Machine Scheduler - Zero-Sharing Multi-core
// Based on Experiment 04: 291M msg/sec on 8 cores (2.3× scaling)
// Strategy: Static actor-to-core assignment, no atomics, perfect cache locality

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "multicore_scheduler.h"

// Cross-platform includes
#ifdef __linux__
#define _GNU_SOURCE
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#define sleep_us(us) usleep(us)
#endif

#ifdef _WIN32
#include <windows.h>
#define sleep_us(us) Sleep((us) / 1000)
#endif

#ifdef __APPLE__
#include <unistd.h>
#define sleep_us(us) usleep(us)
#endif

Scheduler schedulers[MAX_CORES];
int num_cores = 0;
atomic_int next_actor_id = 1;

__thread int current_core_id = -1;

// Pin thread to specific CPU core (NUMA awareness)
static void pin_to_core(int core_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        fprintf(stderr, "Warning: Failed to pin thread to core %d: %s\n", 
                core_id, strerror(errno));
    }
#elif defined(_WIN32)
    // Windows: Use SetThreadAffinityMask
    DWORD_PTR mask = (DWORD_PTR)1 << core_id;
    if (SetThreadAffinityMask(GetCurrentThread(), mask) == 0) {
        fprintf(stderr, "Warning: Failed to pin thread to core %d\n", core_id);
    }
#endif
}

// Partitioned scheduler thread - NO work stealing
void* scheduler_thread(void* arg) {
    Scheduler* sched = (Scheduler*)arg;
    current_core_id = sched->core_id;
    
    // NUMA awareness: pin thread to core for cache affinity
    pin_to_core(sched->core_id);
    
    int idle_count = 0;
    
    while (atomic_load(&sched->running)) {
        int work_done = 0;
        void* actor_ptr;
        Message msg;
        
        // Process incoming cross-core messages (minimal atomic operations)
        // Note: This is the ONLY atomic operation in the hot path
        int batch_count = 0;
        while (batch_count < BATCH_SIZE && 
               queue_dequeue(&sched->incoming_queue, &actor_ptr, &msg)) {
            ActorBase* actor = (ActorBase*)actor_ptr;
            mailbox_send(&actor->mailbox, msg);
            actor->active = 1;
            batch_count++;
            work_done = 1;
        }
        
        // Process active actors (NO ATOMICS - actors are core-local)
        // This is the performance-critical loop
        for (int i = 0; i < sched->actor_count; i++) {
            ActorBase* actor = sched->actors[i];
            if (actor && actor->active) {
                if (actor->step) {
                    actor->step(actor);
                }
                work_done = 1;
            }
        }
        
        // Partitioned approach: NO work stealing
        // Actors stay on their assigned core for perfect cache locality
        // Result: Zero cache thrashing, zero atomic contention
        
        if (!work_done) {
            idle_count++;
            
            // Exponential backoff for idle cores
            if (idle_count > 100) {
                sleep_us(1);  // Brief sleep to reduce CPU usage
                idle_count = 50;
            }
        } else {
            idle_count = 0;
        }
    }
    
    return NULL;
}

void scheduler_init(int cores) {
    if (cores <= 0 || cores > MAX_CORES) {
        cores = 4;
    }
    num_cores = cores;
    
    for (int i = 0; i < num_cores; i++) {
        schedulers[i].core_id = i;
        schedulers[i].actors = malloc(MAX_ACTORS_PER_CORE * sizeof(ActorBase*));
        schedulers[i].actor_count = 0;
        schedulers[i].capacity = MAX_ACTORS_PER_CORE;
        queue_init(&schedulers[i].incoming_queue);
        atomic_store(&schedulers[i].running, 0);
        atomic_store(&schedulers[i].work_count, 0);
        atomic_store(&schedulers[i].steal_attempts, 0);
    }
}

void scheduler_start() {
    for (int i = 0; i < num_cores; i++) {
        atomic_store(&schedulers[i].running, 1);
        pthread_create(&schedulers[i].thread, NULL, scheduler_thread, &schedulers[i]);
    }
}

void scheduler_stop() {
    for (int i = 0; i < num_cores; i++) {
        atomic_store(&schedulers[i].running, 0);
    }
}

void scheduler_wait() {
    for (int i = 0; i < num_cores; i++) {
        pthread_join(schedulers[i].thread, NULL);
    }
}

int scheduler_register_actor(ActorBase* actor, int preferred_core) {
    // Partitioned assignment: actor_id % num_cores
    // This ensures perfect load balance across cores
    if (preferred_core < 0) {
        preferred_core = actor->id % num_cores;
    }
    
    Scheduler* sched = &schedulers[preferred_core];
    
    if (sched->actor_count >= sched->capacity) {
        // Dynamically grow actor array if needed
        sched->capacity *= 2;
        sched->actors = realloc(sched->actors, sched->capacity * sizeof(ActorBase*));
        if (!sched->actors) {
            fprintf(stderr, "Fatal: Failed to grow actor array for core %d\n", preferred_core);
            return -1;
        }
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
    queue_enqueue(&schedulers[target_core].incoming_queue, actor, msg);
    atomic_fetch_add(&schedulers[target_core].work_count, 1);
}
