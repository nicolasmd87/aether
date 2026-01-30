// Performance Benchmark: Baseline vs Optimized Scheduler
// Measures throughput improvement from scheduler optimizations

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include "../../runtime/scheduler/multicore_scheduler.h"
#include "../../runtime/scheduler/scheduler_optimizations.h"

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
static inline uint64_t get_nanos() {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000000ULL) / freq.QuadPart;
}
#else
#include <unistd.h>
#include <sys/time.h>
#define sleep_ms(ms) usleep((ms) * 1000)
static inline uint64_t get_nanos() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
#endif

#define NUM_MESSAGES 1000000
#define NUM_ACTORS 100

// Baseline actor (no optimizations)
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
    atomic_int counter;
} BaselineActor;

void baseline_actor_step(BaselineActor* self) {
    Message msg;
    int processed = 0;
    while (mailbox_receive(&self->mailbox, &msg) && processed < 16) {
        atomic_fetch_add(&self->counter, 1);
        processed++;
    }
    self->active = (self->mailbox.count > 0) || (processed > 0);
}

// Optimized actor handler
void optimized_actor_step(OptimizedActor* self) {
    Message buffer[64];
    int received = optimized_receive_messages(self, buffer, 64);
    
    for (int i = 0; i < received; i++) {
        atomic_fetch_add(&((atomic_int*)buffer[i].payload_ptr), 1);
    }
}

// Benchmark baseline scheduler
double benchmark_baseline() {
    scheduler_init(4);
    
    BaselineActor* actors[NUM_ACTORS];
    for (int i = 0; i < NUM_ACTORS; i++) {
        actors[i] = malloc(sizeof(BaselineActor));
        memset(actors[i], 0, sizeof(BaselineActor));  // Zero all fields including pthread_t
        actors[i]->id = i;
        actors[i]->active = 1;
        actors[i]->auto_process = 0;
        actors[i]->assigned_core = i % 4;
        mailbox_init(&actors[i]->mailbox);
        actors[i]->step = (void(*)(void*))baseline_actor_step;
        atomic_store(&actors[i]->counter, 0);

        scheduler_register_actor((ActorBase*)actors[i]);
    }
    
    scheduler_start();
    
    uint64_t start = get_nanos();
    
    // Send messages
    for (int i = 0; i < NUM_MESSAGES; i++) {
        Message msg = message_create_simple(1, 0, i);
        scheduler_send_message((ActorBase*)actors[i % NUM_ACTORS], msg);
    }
    
    // Wait for processing
    sleep_ms(500);
    
    uint64_t end = get_nanos();
    
    scheduler_stop();
    
    // Check results
    int total = 0;
    for (int i = 0; i < NUM_ACTORS; i++) {
        total += atomic_load(&actors[i]->counter);
        free(actors[i]);
    }
    
    double seconds = (end - start) / 1e9;
    double throughput = NUM_MESSAGES / seconds;
    
    printf("Baseline: %.2f M msg/sec (%d messages processed)\n", 
           throughput / 1e6, total);
    
    return throughput;
}

// Benchmark optimized scheduler
double benchmark_optimized() {
    scheduler_init(4);
    scheduler_opts_init();
    
    OptimizedActor* actors[NUM_ACTORS];
    atomic_int counters[NUM_ACTORS];
    
    for (int i = 0; i < NUM_ACTORS; i++) {
        actors[i] = malloc(sizeof(OptimizedActor));
        optimized_actor_init(actors[i], i % 4, optimized_actor_step);
        atomic_store(&counters[i], 0);
        
        scheduler_register_actor((ActorBase*)&actors[i]->base);
    }
    
    scheduler_start();
    
    uint64_t start = get_nanos();
    
    // Send messages (with direct send optimization)
    for (int i = 0; i < NUM_MESSAGES; i++) {
        Message msg = message_create_simple(1, 0, i);
        msg.payload_ptr = &counters[i % NUM_ACTORS];
        
        int sender_idx = i % NUM_ACTORS;
        int receiver_idx = (i + 1) % NUM_ACTORS;
        
        optimized_send_message(actors[sender_idx], actors[receiver_idx], msg);
    }
    
    // Wait for processing
    sleep_ms(500);
    
    uint64_t end = get_nanos();
    
    scheduler_stop();
    scheduler_opts_print_stats();
    
    // Check results
    int total = 0;
    for (int i = 0; i < NUM_ACTORS; i++) {
        total += atomic_load(&counters[i]);
        free(actors[i]);
    }
    
    double seconds = (end - start) / 1e9;
    double throughput = NUM_MESSAGES / seconds;
    
    printf("Optimized: %.2f M msg/sec (%d messages processed)\n", 
           throughput / 1e6, total);
    
    return throughput;
}

int main() {
    printf("==============================================\n");
    printf("  Scheduler Optimization Performance Benchmark\n");
    printf("  Messages: %d, Actors: %d, Cores: 4\n", NUM_MESSAGES, NUM_ACTORS);
    printf("==============================================\n\n");
    
    printf("Running baseline benchmark...\n");
    double baseline_throughput = benchmark_baseline();
    
    printf("\nRunning optimized benchmark...\n");
    double optimized_throughput = benchmark_optimized();
    
    printf("\n==============================================\n");
    printf("Results:\n");
    printf("  Baseline:   %.2f M msg/sec\n", baseline_throughput / 1e6);
    printf("  Optimized:  %.2f M msg/sec\n", optimized_throughput / 1e6);
    printf("  Speedup:    %.2fx\n", optimized_throughput / baseline_throughput);
    printf("==============================================\n");
    
    // Success if optimized is faster
    return (optimized_throughput > baseline_throughput * 1.1) ? 0 : 1;
}
