#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#else
#include <unistd.h>
#endif

// Configuration
#define MAX_CORES 16
#define MAILBOX_SIZE 256

// Message structure
typedef enum {
    MSG_INCREMENT = 1,
    MSG_TERMINATE = 2
} MessageType;

typedef struct {
    MessageType type;
    int payload;
} Message;

// Mailbox (ring buffer)
typedef struct {
    Message buffer[MAILBOX_SIZE];
    int head;
    int tail;
    int count;
} Mailbox;

// Actor structure
typedef struct Actor {
    int id;
    int active;
    Mailbox mailbox;
    void (*step)(struct Actor*);
    
    // Counter actor state
    long long counter_value;
} Actor;

// Partition (one per core)
typedef struct {
    int core_id;
    pthread_t thread;
    
    Actor* actors;
    int actor_count;
    
    long long messages_processed;
    long long messages_expected;
    
    int running;
} Partition;

// Global state
Partition partitions[MAX_CORES];
int num_cores = 1;
int total_actors = 0;
int messages_per_actor = 0;

// Mailbox operations (non-atomic - each core owns its actors)
static inline int mailbox_send(Mailbox* mb, Message msg) {
    if (mb->count >= MAILBOX_SIZE) {
        return 0;  // Full
    }
    mb->buffer[mb->tail] = msg;
    mb->tail = (mb->tail + 1) % MAILBOX_SIZE;
    mb->count++;
    return 1;
}

static inline int mailbox_receive(Mailbox* mb, Message* msg) {
    if (mb->count == 0) {
        return 0;  // Empty
    }
    *msg = mb->buffer[mb->head];
    mb->head = (mb->head + 1) % MAILBOX_SIZE;
    mb->count--;
    return 1;
}

// Actor step function (counter actor)
void counter_step(Actor* self) {
    Message msg;
    
    if (!mailbox_receive(&self->mailbox, &msg)) {
        self->active = 0;
        return;
    }
    
    if (msg.type == MSG_INCREMENT) {
        self->counter_value += msg.payload;
    }
    
    // Keep actor active if more messages
    self->active = (self->mailbox.count > 0);
}

// Partition scheduler (runs on dedicated thread)
void* partition_scheduler(void* arg) {
    Partition* part = (Partition*)arg;
    
    // Pin to CPU core
#ifdef _WIN32
    DWORD_PTR mask = (DWORD_PTR)1 << part->core_id;
    SetThreadAffinityMask(GetCurrentThread(), mask);
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(part->core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    
    // State machine scheduler loop (same as Experiment 02)
    while (part->running) {
        int work_done = 0;
        
        for (int i = 0; i < part->actor_count; i++) {
            Actor* actor = &part->actors[i];
            
            if (actor->active && actor->mailbox.count > 0) {
                actor->step(actor);
                part->messages_processed++;
                work_done = 1;
            }
        }
        
        // Exit when all messages processed
        if (!work_done && part->messages_processed >= part->messages_expected) {
            break;
        }
        
        // Brief yield if no work (avoid busy-wait)
        if (!work_done) {
            usleep(10);
        }
    }
    
    return NULL;
}

// Initialize system
void init_system(int cores, int actors, int msgs_per_actor) {
    num_cores = cores;
    total_actors = actors;
    messages_per_actor = msgs_per_actor;
    
    // Partition actors across cores
    int actors_per_core = actors / cores;
    int remainder = actors % cores;
    
    int actor_offset = 0;
    
    for (int c = 0; c < cores; c++) {
        Partition* part = &partitions[c];
        part->core_id = c;
        
        // Distribute remainder actors to first few cores
        int count = actors_per_core + (c < remainder ? 1 : 0);
        part->actor_count = count;
        part->actors = (Actor*)malloc(sizeof(Actor) * count);
        
        // Initialize actors
        for (int i = 0; i < count; i++) {
            Actor* actor = &part->actors[i];
            actor->id = actor_offset + i;
            actor->active = 1;
            actor->step = counter_step;
            actor->counter_value = 0;
            actor->mailbox.head = 0;
            actor->mailbox.tail = 0;
            actor->mailbox.count = 0;
        }
        
        part->messages_processed = 0;
        part->messages_expected = count * msgs_per_actor;
        part->running = 1;
        
        actor_offset += count;
    }
}

// Pre-load messages (before starting threads)
void send_all_messages() {
    for (int c = 0; c < num_cores; c++) {
        Partition* part = &partitions[c];
        
        for (int i = 0; i < part->actor_count; i++) {
            Actor* actor = &part->actors[i];
            
            for (int m = 0; m < messages_per_actor; m++) {
                Message msg = { MSG_INCREMENT, 1 };
                
                if (!mailbox_send(&actor->mailbox, msg)) {
                    fprintf(stderr, "ERROR: Mailbox full for actor %d (increase MAILBOX_SIZE)\n", actor->id);
                    exit(1);
                }
            }
            
            actor->active = 1;
        }
    }
}

// Start all partition threads
void start_partitions() {
    for (int c = 0; c < num_cores; c++) {
        pthread_create(&partitions[c].thread, NULL, partition_scheduler, &partitions[c]);
    }
}

// Wait for all partitions to finish
void wait_partitions() {
    for (int c = 0; c < num_cores; c++) {
        pthread_join(partitions[c].thread, NULL);
    }
}

// Print statistics
void print_stats(double elapsed_sec) {
    long long total_processed = 0;
    long long total_expected = 0;
    
    for (int c = 0; c < num_cores; c++) {
        total_processed += partitions[c].messages_processed;
        total_expected += partitions[c].messages_expected;
    }
    
    double throughput_M = (double)total_processed / elapsed_sec / 1e6;
    double latency_ns = elapsed_sec * 1e9 / total_processed;
    
    printf("\n=== Performance Results ===\n");
    printf("Cores:              %d\n", num_cores);
    printf("Actors:             %d\n", total_actors);
    printf("Messages sent:      %lld\n", total_expected);
    printf("Messages processed: %lld\n", total_processed);
    printf("Time:               %.4f seconds\n", elapsed_sec);
    printf("Throughput:         %.2f M msg/sec\n", throughput_M);
    printf("Latency (avg):      %.0f ns/msg\n", latency_ns);
    
    printf("\n=== Core Statistics ===\n");
    for (int c = 0; c < num_cores; c++) {
        Partition* part = &partitions[c];
        double percent = 100.0 * part->messages_processed / total_processed;
        printf("Core %d: %d actors, %lld messages (%.1f%%)\n",
               c, part->actor_count, part->messages_processed, percent);
    }
    
    // Verify correctness
    long long total_value = 0;
    for (int c = 0; c < num_cores; c++) {
        for (int i = 0; i < partitions[c].actor_count; i++) {
            total_value += partitions[c].actors[i].counter_value;
        }
    }
    
    printf("\n=== Verification ===\n");
    printf("Total counter value: %lld\n", total_value);
    printf("Expected:            %lld\n", total_expected);
    printf("Status:              %s\n", 
           total_value == total_expected ? "✓ PASS" : "✗ FAIL");
}

// Cleanup
void cleanup() {
    for (int c = 0; c < num_cores; c++) {
        free(partitions[c].actors);
    }
}

int main(int argc, char* argv[]) {
    int cores = 4;
    int actors = 10000;
    int msgs = 100;
    
    if (argc >= 2) cores = atoi(argv[1]);
    if (argc >= 3) actors = atoi(argv[2]);
    if (argc >= 4) msgs = atoi(argv[3]);
    
    if (cores <= 0 || cores > MAX_CORES) {
        fprintf(stderr, "Invalid core count (1-%d)\n", MAX_CORES);
        return 1;
    }
    
    printf("=== Aether Partitioned State Machine Benchmark ===\n");
    printf("Configuration: %d cores, %d actors, %d msg/actor\n",
           cores, actors, msgs);
    printf("Total messages: %d\n\n", actors * msgs);
    
    // Initialize
    init_system(cores, actors, msgs);
    
    // Pre-load all messages
    send_all_messages();
    
    // Start timer
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Start partition threads
    start_partitions();
    
    // Wait for completion
    wait_partitions();
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Results
    print_stats(elapsed);
    
    // Cleanup
    cleanup();
    
    return 0;
}
