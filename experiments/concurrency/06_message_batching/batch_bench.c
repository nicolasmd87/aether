#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Configuration
#define MAILBOX_SIZE 512
#define MAX_BATCH_SIZE 256

// Message structure
typedef struct {
    int type;
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
typedef struct {
    int id;
    Mailbox mailbox;
    long long counter_value;
} Actor;

// Initialize mailbox
void mailbox_init(Mailbox* mb) {
    mb->head = 0;
    mb->tail = 0;
    mb->count = 0;
}

// Send single message (baseline)
int mailbox_send_single(Mailbox* mb, Message msg) {
    if (mb->count >= MAILBOX_SIZE) {
        return 0;  // Full
    }
    mb->buffer[mb->tail] = msg;
    mb->tail = (mb->tail + 1) % MAILBOX_SIZE;
    mb->count++;
    return 1;
}

// Send batch of messages (optimized)
int mailbox_send_batch(Mailbox* mb, Message* messages, int count) {
    if (mb->count + count > MAILBOX_SIZE) {
        return 0;  // Not enough space
    }
    
    // Fast path: Can copy contiguously
    int space_until_wrap = MAILBOX_SIZE - mb->tail;
    
    if (count <= space_until_wrap) {
        // Single memcpy
        memcpy(&mb->buffer[mb->tail], messages, count * sizeof(Message));
        mb->tail = (mb->tail + count) % MAILBOX_SIZE;
    } else {
        // Two memcpy operations (wrap around)
        memcpy(&mb->buffer[mb->tail], messages, space_until_wrap * sizeof(Message));
        memcpy(&mb->buffer[0], &messages[space_until_wrap], 
               (count - space_until_wrap) * sizeof(Message));
        mb->tail = count - space_until_wrap;
    }
    
    mb->count += count;
    return 1;
}

// Receive message
int mailbox_receive(Mailbox* mb, Message* msg) {
    if (mb->count == 0) {
        return 0;  // Empty
    }
    *msg = mb->buffer[mb->head];
    mb->head = (mb->head + 1) % MAILBOX_SIZE;
    mb->count--;
    return 1;
}

// Process messages
void process_messages(Actor* actor) {
    Message msg;
    while (mailbox_receive(&actor->mailbox, &msg)) {
        actor->counter_value += msg.payload;
    }
}

// Benchmark single-message sending
double benchmark_single(int num_actors, int messages_per_actor) {
    Actor* actors = (Actor*)malloc(sizeof(Actor) * num_actors);
    
    for (int i = 0; i < num_actors; i++) {
        actors[i].id = i;
        actors[i].counter_value = 0;
        mailbox_init(&actors[i].mailbox);
    }
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Send messages one-at-a-time
    for (int i = 0; i < num_actors; i++) {
        for (int m = 0; m < messages_per_actor; m++) {
            Message msg = { 1, 1 };
            mailbox_send_single(&actors[i].mailbox, msg);
        }
    }
    
    // Process all messages
    for (int i = 0; i < num_actors; i++) {
        process_messages(&actors[i]);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Verify correctness
    long long expected = (long long)num_actors * messages_per_actor;
    long long actual = 0;
    for (int i = 0; i < num_actors; i++) {
        actual += actors[i].counter_value;
    }
    
    if (actual != expected) {
        printf("ERROR: Expected %lld, got %lld\n", expected, actual);
    }
    
    free(actors);
    return elapsed;
}

// Benchmark batch sending
double benchmark_batch(int num_actors, int messages_per_actor, int batch_size) {
    Actor* actors = (Actor*)malloc(sizeof(Actor) * num_actors);
    
    for (int i = 0; i < num_actors; i++) {
        actors[i].id = i;
        actors[i].counter_value = 0;
        mailbox_init(&actors[i].mailbox);
    }
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Send messages in batches
    Message batch[MAX_BATCH_SIZE];
    for (int m = 0; m < batch_size; m++) {
        batch[m].type = 1;
        batch[m].payload = 1;
    }
    
    for (int i = 0; i < num_actors; i++) {
        int remaining = messages_per_actor;
        while (remaining > 0) {
            int to_send = (remaining < batch_size) ? remaining : batch_size;
            mailbox_send_batch(&actors[i].mailbox, batch, to_send);
            remaining -= to_send;
        }
    }
    
    // Process all messages
    for (int i = 0; i < num_actors; i++) {
        process_messages(&actors[i]);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Verify correctness
    long long expected = (long long)num_actors * messages_per_actor;
    long long actual = 0;
    for (int i = 0; i < num_actors; i++) {
        actual += actors[i].counter_value;
    }
    
    if (actual != expected) {
        printf("ERROR: Expected %lld, got %lld\n", expected, actual);
    }
    
    free(actors);
    return elapsed;
}

int main(int argc, char* argv[]) {
    int num_actors = 10000;
    int messages_per_actor = 100;
    
    if (argc >= 2) num_actors = atoi(argv[1]);
    if (argc >= 3) messages_per_actor = atoi(argv[2]);
    
    printf("=== Message Batching Benchmark ===\n");
    printf("Actors: %d\n", num_actors);
    printf("Messages per actor: %d\n", messages_per_actor);
    printf("Total messages: %d\n\n", num_actors * messages_per_actor);
    
    // Baseline: Single-message sending
    double time_single = benchmark_single(num_actors, messages_per_actor);
    double throughput_single = (num_actors * messages_per_actor) / time_single / 1e6;
    
    printf("Baseline (single-message):\n");
    printf("  Time:       %.4f seconds\n", time_single);
    printf("  Throughput: %.2f M msg/sec\n\n", throughput_single);
    
    // Batch sizes to test
    int batch_sizes[] = { 2, 4, 8, 16, 32, 64, 128, 256 };
    int num_tests = sizeof(batch_sizes) / sizeof(batch_sizes[0]);
    
    printf("Batched sending results:\n");
    printf("%-12s %-12s %-15s %-10s\n", "Batch Size", "Time (s)", "Throughput", "Speedup");
    printf("%-12s %-12s %-15s %-10s\n", "----------", "--------", "-----------", "-------");
    
    for (int i = 0; i < num_tests; i++) {
        int batch_size = batch_sizes[i];
        double time_batch = benchmark_batch(num_actors, messages_per_actor, batch_size);
        double throughput_batch = (num_actors * messages_per_actor) / time_batch / 1e6;
        double speedup = time_single / time_batch;
        
        printf("%-12d %-12.4f %-15.2f %.2fx\n", 
               batch_size, time_batch, throughput_batch, speedup);
    }
    
    return 0;
}
