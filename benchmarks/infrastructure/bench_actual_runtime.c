// Benchmark actual Aether actor concurrency
// Tests real runtime performance to find bottlenecks

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdatomic.h>
#include <unistd.h>
#include "../../runtime/actors/actor_state_machine.h"
#include "../../runtime/scheduler/multicore_scheduler.h"
#include "../../runtime/scheduler/lockfree_queue.h"

#ifdef _WIN32
#include <windows.h>
#define usleep(us) Sleep((us) / 1000)
static int get_num_processors() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}
#else
static int get_num_processors() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}
#endif

#define TEST_ACTORS 1000
#define MESSAGES_PER_ACTOR 10000

// Simple ping actor
typedef struct {
    // MUST match ActorBase layout exactly!
    int active;
    int id;
    Mailbox mailbox;
    void (*step)(void*);
    pthread_t thread;
    int auto_process;
    int assigned_core;
    SPSCQueue spsc_queue;
    // Benchmark-specific fields below
    atomic_int messages_processed;
    int target_count;
} PingActor;

void ping_step(PingActor* self) {
    Message msg;
    
    if (!mailbox_receive(&self->mailbox, &msg)) {
        if (atomic_load(&self->messages_processed) >= self->target_count) {
            self->active = 0;
        }
        return;
    }
    
    atomic_fetch_add(&self->messages_processed, 1);
}

PingActor* spawn_ping_actor(int target_count) {
    PingActor* actor = (PingActor*)malloc(sizeof(PingActor));
    if (!actor) return NULL;

    memset(actor, 0, sizeof(PingActor));  // Zero all fields including pthread_t

    actor->id = atomic_fetch_add(&next_actor_id, 1);
    actor->active = 1;
    actor->assigned_core = -1;
    actor->step = (void (*)(void*))ping_step;
    actor->auto_process = 0;  // Manual processing for benchmark
    mailbox_init(&actor->mailbox);

    atomic_store(&actor->messages_processed, 0);
    actor->target_count = target_count;

    return actor;
}

// Benchmark 1: Single-core message throughput
void bench_single_core() {
    printf("=== Benchmark 1: Single-Core Message Throughput ===\n");
    
    // Initialize single-core scheduler
    scheduler_init(1);
    scheduler_start();
    
    // Create actors
    PingActor* actors[100];
    for (int i = 0; i < 100; i++) {
        actors[i] = spawn_ping_actor(MESSAGES_PER_ACTOR);
        scheduler_register_actor((ActorBase*)actors[i], 0);
    }
    
    // Send messages
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int round = 0; round < MESSAGES_PER_ACTOR; round++) {
        for (int i = 0; i < 100; i++) {
            Message msg = {1, 0, round, NULL};
            scheduler_send_local((ActorBase*)actors[i], msg);
        }
    }
    
    // Wait for completion
    int all_done = 0;
    while (!all_done) {
        all_done = 1;
        for (int i = 0; i < 100; i++) {
            if (atomic_load(&actors[i]->messages_processed) < MESSAGES_PER_ACTOR) {
                all_done = 0;
                break;
            }
        }
        usleep(100);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    int total_messages = 100 * MESSAGES_PER_ACTOR;
    double throughput = total_messages / elapsed;
    
    printf("  Actors: 100\n");
    printf("  Messages: %d\n", total_messages);
    printf("  Time: %.3f seconds\n", elapsed);
    printf("  Throughput: %.0f msg/sec\n\n", throughput);
    
    scheduler_stop();
    scheduler_wait();
    
    for (int i = 0; i < 100; i++) {
        free(actors[i]);
    }
}

// Benchmark 2: Multi-core scaling
void bench_multicore_scaling() {
    printf("=== Benchmark 2: Multi-Core Scaling ===\n");
    
    int core_counts[] = {1, 2, 4, 8};
    
    for (int c = 0; c < 4; c++) {
        int cores = core_counts[c];
        if (cores > get_num_processors()) continue;
        
        scheduler_init(cores);
        scheduler_start();
        
        // Create actors spread across cores
        PingActor* actors[TEST_ACTORS];
        for (int i = 0; i < TEST_ACTORS; i++) {
            actors[i] = spawn_ping_actor(100);
            scheduler_register_actor((ActorBase*)actors[i], i % cores);
        }
        
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        // Send messages
        for (int round = 0; round < 100; round++) {
            for (int i = 0; i < TEST_ACTORS; i++) {
                Message msg = {1, 0, round, NULL};
                if (actors[i]->assigned_core == 0) {
                    scheduler_send_local((ActorBase*)actors[i], msg);
                } else {
                    scheduler_send_remote((ActorBase*)actors[i], msg, 0);
                }
            }
        }
        
        // Wait for completion
        int all_done = 0;
        while (!all_done) {
            all_done = 1;
            for (int i = 0; i < TEST_ACTORS; i++) {
                if (atomic_load(&actors[i]->messages_processed) < 100) {
                    all_done = 0;
                    break;
                }
            }
            usleep(100);
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double elapsed = (end.tv_sec - start.tv_sec) + 
                         (end.tv_nsec - start.tv_nsec) / 1e9;
        
        int total_messages = TEST_ACTORS * 100;
        double throughput = total_messages / elapsed;
        
        printf("  %d cores: %.0f msg/sec (%.2fx scaling)\n", 
               cores, throughput, throughput / (100000.0 / 1));
        
        scheduler_stop();
        scheduler_wait();
        
        for (int i = 0; i < TEST_ACTORS; i++) {
            free(actors[i]);
        }
    }
    
    printf("\n");
}

// Benchmark 3: Cross-core message latency
void bench_cross_core_latency() {
    printf("=== Benchmark 3: Cross-Core Message Latency ===\n");
    
    scheduler_init(4);
    scheduler_start();
    
    PingActor* actor1 = spawn_ping_actor(10000);
    PingActor* actor2 = spawn_ping_actor(10000);
    
    scheduler_register_actor((ActorBase*)actor1, 0);
    scheduler_register_actor((ActorBase*)actor2, 3);  // Different core
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < 10000; i++) {
        Message msg = {1, 0, i, NULL};
        scheduler_send_remote((ActorBase*)actor2, msg, 0);
    }
    
    while (atomic_load(&actor2->messages_processed) < 10000) {
        usleep(10);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    double avg_latency_us = (elapsed / 10000.0) * 1000000.0;
    
    printf("  Cross-core messages: 10000\n");
    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Average latency: %.2f μs\n\n", avg_latency_us);
    
    scheduler_stop();
    scheduler_wait();
    
    free(actor1);
    free(actor2);
}

int main() {
    printf("=== Aether Runtime Concurrency Benchmark ===\n\n");
    
    bench_single_core();
    bench_multicore_scaling();
    bench_cross_core_latency();
    
    printf("Benchmark complete.\n");
    return 0;
}
