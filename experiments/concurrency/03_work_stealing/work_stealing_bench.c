/*
 * Experiment 03: Work-Stealing Multi-Core Actor Benchmark
 * 
 * This implements M:N threading where M lightweight actor tasks are scheduled
 * across N OS worker threads with work-stealing for load balancing.
 * 
 * Goal: Prove multi-core scalability and achieve 10-50M msg/sec throughput
 * across multiple cores with near-linear scaling.
 * 
 * Architecture:
 * - N worker threads (one per CPU core)
 * - Each worker has a lock-free work-stealing deque
 * - Actors are lightweight tasks (struct + state machine)
 * - Steal from random victims when idle
 * 
 * Compilation (Windows/MinGW):
 *   gcc -O3 -march=native work_stealing_bench.c -o work_stealing_bench -lpthread
 * 
 * Usage:
 *   ./work_stealing_bench [num_workers] [num_actors] [messages_per_actor]
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#else
#include <unistd.h>
#endif

// Configuration
#define MAX_WORKERS 64
#define DEQUE_SIZE 131072  // Increased to handle 100K+ actors
#define MAILBOX_SIZE 256   // Increased from 32 to handle more messages
#define CACHE_LINE 64
#define STEAL_ATTEMPTS 3

// Message types
typedef enum {
    MSG_INCREMENT,
    MSG_PING,
    MSG_TERMINATE
} MsgType;

typedef struct {
    MsgType type;
    int payload;
} Message;

// Lock-free circular mailbox
typedef struct {
    Message messages[MAILBOX_SIZE];
    atomic_int head;
    atomic_int tail;
    char padding[CACHE_LINE];
} Mailbox;

// Lightweight Actor (state machine)
typedef struct {
    int id;
    atomic_int active;
    int counter_value;
    int target_messages;
    int processed_messages;
    Mailbox mailbox;
    char padding[CACHE_LINE];
} Actor;

// Work-stealing deque (per-worker)
// Uses Chase-Lev algorithm: local end uses regular ops, thieves use CAS
typedef struct {
    Actor** tasks;
    atomic_int top;    // Owner side (LIFO)
    atomic_int bottom; // Steal side (FIFO)
    int size;
    char padding[CACHE_LINE];
} WorkDeque;

// Worker thread data
typedef struct {
    int id;
    pthread_t thread;
    WorkDeque deque;
    atomic_int running;
    atomic_long tasks_processed;
    atomic_long steal_attempts;
    atomic_long steals_succeeded;
    char padding[CACHE_LINE];
} Worker;

// Global state
Actor* all_actors = NULL;
Worker workers[MAX_WORKERS];
int num_workers = 0;
int num_actors = 0;
int messages_per_actor = 0;
atomic_long total_messages_sent;
atomic_long total_messages_processed;

// Mailbox operations
static inline int mailbox_send(Mailbox* mb, Message msg) {
    int tail = atomic_load_explicit(&mb->tail, memory_order_relaxed);
    int next = (tail + 1) % MAILBOX_SIZE;
    int head = atomic_load_explicit(&mb->head, memory_order_acquire);
    
    if (next == head) return 0; // Full
    
    mb->messages[tail] = msg;
    atomic_store_explicit(&mb->tail, next, memory_order_release);
    return 1;
}

static inline int mailbox_receive(Mailbox* mb, Message* out) {
    int head = atomic_load_explicit(&mb->head, memory_order_relaxed);
    int tail = atomic_load_explicit(&mb->tail, memory_order_acquire);
    
    if (head == tail) return 0; // Empty
    
    *out = mb->messages[head];
    atomic_store_explicit(&mb->head, (head + 1) % MAILBOX_SIZE, memory_order_release);
    return 1;
}

// Deque operations (Chase-Lev algorithm)
static void deque_init(WorkDeque* dq, int size) {
    // Simple malloc for Windows compatibility
    dq->tasks = (Actor**)malloc(sizeof(Actor*) * size);
    atomic_init(&dq->top, 0);
    atomic_init(&dq->bottom, 0);
    dq->size = size;
}

static inline void deque_push(WorkDeque* dq, Actor* task) {
    int bottom = atomic_load_explicit(&dq->bottom, memory_order_relaxed);
    dq->tasks[bottom % dq->size] = task;
    atomic_store_explicit(&dq->bottom, bottom + 1, memory_order_release);
}

static inline Actor* deque_pop(WorkDeque* dq) {
    int bottom = atomic_load_explicit(&dq->bottom, memory_order_relaxed) - 1;
    atomic_store_explicit(&dq->bottom, bottom, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    
    int top = atomic_load_explicit(&dq->top, memory_order_relaxed);
    
    if (top <= bottom) {
        // Non-empty queue
        Actor* task = dq->tasks[bottom % dq->size];
        
        if (top == bottom) {
            // Last element, race with thieves
            if (!atomic_compare_exchange_strong_explicit(
                    &dq->top, &top, top + 1,
                    memory_order_seq_cst, memory_order_relaxed)) {
                // Lost race
                task = NULL;
            }
            atomic_store_explicit(&dq->bottom, bottom + 1, memory_order_relaxed);
        }
        return task;
    } else {
        // Empty queue
        atomic_store_explicit(&dq->bottom, bottom + 1, memory_order_relaxed);
        return NULL;
    }
}

static inline Actor* deque_steal(WorkDeque* dq) {
    int top = atomic_load_explicit(&dq->top, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    int bottom = atomic_load_explicit(&dq->bottom, memory_order_acquire);
    
    if (top < bottom) {
        // Non-empty
        Actor* task = dq->tasks[top % dq->size];
        
        if (atomic_compare_exchange_strong_explicit(
                &dq->top, &top, top + 1,
                memory_order_seq_cst, memory_order_relaxed)) {
            return task;
        }
    }
    return NULL;
}

// Actor step function (state machine)
static int actor_step(Actor* actor) {
    Message msg;
    
    if (!mailbox_receive(&actor->mailbox, &msg)) {
        atomic_store(&actor->active, 0);
        return 0;  // No message processed
    }
    
    // Process message
    actor->processed_messages++;
    actor->counter_value++;
    
    // CRITICAL: Increment global counter
    atomic_fetch_add(&total_messages_processed, 1);
    
    if (actor->processed_messages >= actor->target_messages) {
        atomic_store(&actor->active, 0);
    }
    
    return 1;  // Message processed
}

// Try to steal work from a random victim
static Actor* try_steal_work(Worker* thief) {
    for (int attempt = 0; attempt < STEAL_ATTEMPTS; attempt++) {
        int victim_id = rand() % num_workers;
        
        if (victim_id == thief->id) continue;
        
        Worker* victim = &workers[victim_id];
        atomic_fetch_add(&thief->steal_attempts, 1);
        
        Actor* stolen = deque_steal(&victim->deque);
        if (stolen) {
            atomic_fetch_add(&thief->steals_succeeded, 1);
            return stolen;
        }
    }
    return NULL;
}

// Worker thread main loop
void* worker_main(void* arg) {
    Worker* self = (Worker*)arg;
    int consecutive_idle = 0;
    
    // Pin to CPU core for better cache locality
#ifdef _WIN32
    DWORD_PTR mask = (DWORD_PTR)1 << self->id;
    SetThreadAffinityMask(GetCurrentThread(), mask);
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(self->id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    
    while (atomic_load(&self->running)) {
        Actor* task = deque_pop(&self->deque);
        
        if (!task) {
            task = try_steal_work(self);
        }
        
        if (task) {
            // Process the actor
            if (actor_step(task)) {
                atomic_fetch_add(&self->tasks_processed, 1);
            }
            
            // Re-enqueue if actor still has work
            int head = atomic_load_explicit(&task->mailbox.head, memory_order_acquire);
            int tail = atomic_load_explicit(&task->mailbox.tail, memory_order_acquire);
            if (head != tail) {
                deque_push(&self->deque, task);
            }
            
            consecutive_idle = 0;
        } else {
            // No work found
            consecutive_idle++;
            
            if (consecutive_idle > 10) {
                long processed = atomic_load(&total_messages_processed);
                long sent = atomic_load(&total_messages_sent);
                
                if (processed >= sent && sent > 0) {
                    break;
                }
            }
            
            if (consecutive_idle > 100) {
                usleep(100);
            }
        }
    }
    
    return NULL;
}

// Initialize system
void init_system(int workers_count, int actors_count, int msgs_per_actor) {
    num_workers = workers_count;
    num_actors = actors_count;
    messages_per_actor = msgs_per_actor;
    
    atomic_init(&total_messages_sent, 0);
    atomic_init(&total_messages_processed, 0);
    
    // Allocate actors
    all_actors = (Actor*)malloc(sizeof(Actor) * num_actors);
    memset(all_actors, 0, sizeof(Actor) * num_actors);
    
    for (int i = 0; i < num_actors; i++) {
        all_actors[i].id = i;
        atomic_init(&all_actors[i].active, 1);
        all_actors[i].counter_value = 0;
        all_actors[i].target_messages = messages_per_actor;
        all_actors[i].processed_messages = 0;
        atomic_init(&all_actors[i].mailbox.head, 0);
        atomic_init(&all_actors[i].mailbox.tail, 0);
    }
    
    // Initialize workers
    for (int i = 0; i < num_workers; i++) {
        workers[i].id = i;
        deque_init(&workers[i].deque, DEQUE_SIZE);
        atomic_init(&workers[i].running, 0);
        atomic_init(&workers[i].tasks_processed, 0);
        atomic_init(&workers[i].steal_attempts, 0);
        atomic_init(&workers[i].steals_succeeded, 0);
    }
    
    // Distribute actors across workers
    for (int i = 0; i < num_actors; i++) {
        int worker_id = i % num_workers;
        deque_push(&workers[worker_id].deque, &all_actors[i]);
    }
}

// Send all messages (pre-load before starting workers)
void send_messages() {
    for (int i = 0; i < num_actors; i++) {
        for (int m = 0; m < messages_per_actor; m++) {
            Message msg = { .type = MSG_INCREMENT, .payload = 1 };
            
            if (!mailbox_send(&all_actors[i].mailbox, msg)) {
                fprintf(stderr, "\nERROR: Mailbox full for actor %d at message %d (increase MAILBOX_SIZE from %d)\n", 
                        i, m, MAILBOX_SIZE);
                exit(1);
            }
            
            atomic_fetch_add(&total_messages_sent, 1);
        }
        atomic_store(&all_actors[i].active, 1);
    }
}

// Start all workers
void start_workers() {
    for (int i = 0; i < num_workers; i++) {
        atomic_store(&workers[i].running, 1);
        pthread_create(&workers[i].thread, NULL, worker_main, &workers[i]);
    }
}

// Wait for completion
void wait_workers() {
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i].thread, NULL);
    }
}

void print_stats(double elapsed_sec) {
    long total_sent = atomic_load(&total_messages_sent);
    long total_proc = atomic_load(&total_messages_processed);
    
    printf("\n=== Performance Results ===\n");
    printf("Workers:            %d\n", num_workers);
    printf("Actors:             %d\n", num_actors);
    printf("Messages sent:      %ld\n", total_sent);
    printf("Messages processed: %ld\n", total_proc);
    printf("Time:               %.4f seconds\n", elapsed_sec);
    printf("Throughput:         %.2f M msg/sec\n", 
           total_proc / elapsed_sec / 1e6);
    printf("Latency (avg):      %.0f ns/msg\n", 
           elapsed_sec * 1e9 / total_proc);
    
    printf("\n=== Worker Statistics ===\n");
    long total_steals = 0, total_attempts = 0;
    for (int i = 0; i < num_workers; i++) {
        long processed = atomic_load(&workers[i].tasks_processed);
        long attempts = atomic_load(&workers[i].steal_attempts);
        long succeeded = atomic_load(&workers[i].steals_succeeded);
        total_steals += succeeded;
        total_attempts += attempts;
        
        printf("Worker %d: %ld tasks (%.1f%%), %ld steals (%ld attempts, %.1f%% success)\n",
               i, processed, 100.0 * processed / total_proc,
               succeeded, attempts, 
               attempts > 0 ? 100.0 * succeeded / attempts : 0.0);
    }
    
    printf("\nTotal steals: %ld (%.1f%% of messages)\n", 
           total_steals, 100.0 * total_steals / total_proc);
    
    // Verify correctness
    long long total_value = 0;
    for (int i = 0; i < num_actors; i++) {
        total_value += all_actors[i].counter_value;
    }
    printf("\n=== Verification ===\n");
    printf("Total counter value: %lld\n", total_value);
    printf("Expected:            %lld\n", (long long)num_actors * messages_per_actor);
    printf("Status:              %s\n", 
           total_value == (long long)num_actors * messages_per_actor ? 
           "✓ PASS" : "✗ FAIL");
}

int main(int argc, char* argv[]) {
    // Parse arguments
    int workers_count = 4;
    int actors_count = 10000;
    int msgs = 100;
    
    if (argc >= 2) workers_count = atoi(argv[1]);
    if (argc >= 3) actors_count = atoi(argv[2]);
    if (argc >= 4) msgs = atoi(argv[3]);
    
    // Validate
    if (workers_count <= 0 || workers_count > MAX_WORKERS) {
        fprintf(stderr, "Invalid worker count (1-%d)\n", MAX_WORKERS);
        return 1;
    }
    if (actors_count <= 0 || actors_count > 10000000) {
        fprintf(stderr, "Invalid actor count (1-10000000)\n");
        return 1;
    }
    
    printf("=== Aether Work-Stealing Multi-Core Benchmark ===\n");
    printf("Configuration: %d workers, %d actors, %d msg/actor\n",
           workers_count, actors_count, msgs);
    printf("Total messages: %d\n\n", actors_count * msgs);
    
    // Initialize
    init_system(workers_count, actors_count, msgs);
    
    // Pre-load all messages
    send_messages();
    
    // Start benchmark timer
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Start workers
    start_workers();
    
    // Wait for completion
    wait_workers();
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate elapsed time
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Print results
    print_stats(elapsed);
    
    // Cleanup
    free(all_actors);
    for (int i = 0; i < num_workers; i++) {
        free(workers[i].deque.tasks);
    }
    
    return 0;
}
