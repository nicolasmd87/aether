/*
 * Experiment 01: Pthread Baseline Benchmark
 * 
 * Traditional 1:1 actor model using POSIX threads.
 * Each actor runs on its own OS thread with blocking message receives.
 * 
 * Usage: gcc pthread_bench.c -O2 -lpthread -o pthread_bench && ./pthread_bench <actors> <msgs_per_actor>
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

// Configuration
#define MAX_QUEUE_SIZE 64

// Message structure
typedef struct {
    int value;
} Message;

// Simple ring buffer queue
typedef struct {
    Message messages[MAX_QUEUE_SIZE];
    int head, tail, count;
} MessageQueue;

// Actor structure
typedef struct {
    int id;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    MessageQueue queue;
    int counter;
    volatile int running;
    int expected_messages;
} PthreadActor;

// Queue operations
void queue_init(MessageQueue* q) {
    q->head = q->tail = q->count = 0;
}

int queue_empty(MessageQueue* q) {
    return q->count == 0;
}

int queue_full(MessageQueue* q) {
    return q->count >= MAX_QUEUE_SIZE;
}

void queue_enqueue(MessageQueue* q, Message msg) {
    if (queue_full(q)) {
        fprintf(stderr, "Queue overflow!\n");
        return;
    }
    q->messages[q->tail] = msg;
    q->tail = (q->tail + 1) % MAX_QUEUE_SIZE;
    q->count++;
}

Message queue_dequeue(MessageQueue* q) {
    Message msg = q->messages[q->head];
    q->head = (q->head + 1) % MAX_QUEUE_SIZE;
    q->count--;
    return msg;
}

// Send message to actor
void send_message(PthreadActor* target, Message msg) {
    pthread_mutex_lock(&target->lock);
    queue_enqueue(&target->queue, msg);
    pthread_cond_signal(&target->cond);
    pthread_mutex_unlock(&target->lock);
}

// Actor thread function
void* actor_loop(void* arg) {
    PthreadActor* actor = (PthreadActor*)arg;
    int processed = 0;
    
    while (actor->running || processed < actor->expected_messages) {
        pthread_mutex_lock(&actor->lock);
        
        // Wait for message
        while (queue_empty(&actor->queue) && (actor->running || processed < actor->expected_messages)) {
            struct timespec timeout;
            #ifdef _WIN32
            // Windows workaround
            timeout.tv_sec = 0;
            timeout.tv_nsec = 100000000; // 100ms
            pthread_cond_timedwait(&actor->cond, &actor->lock, &timeout);
            #else
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 1;
            pthread_cond_timedwait(&actor->cond, &actor->lock, &timeout);
            #endif
            
            if (!actor->running && queue_empty(&actor->queue)) {
                pthread_mutex_unlock(&actor->lock);
                return NULL;
            }
        }
        
        if (queue_empty(&actor->queue)) {
            pthread_mutex_unlock(&actor->lock);
            break;
        }
        
        // Process message
        Message msg = queue_dequeue(&actor->queue);
        pthread_mutex_unlock(&actor->lock);
        
        actor->counter += msg.value;
        processed++;
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    int actor_count = 1000;
    int messages_per_actor = 10;
    
    if (argc >= 2) actor_count = atoi(argv[1]);
    if (argc >= 3) messages_per_actor = atoi(argv[2]);
    
    if (actor_count > 10000) {
        printf("Warning: %d actors may exhaust system resources!\n", actor_count);
        printf("Typical limit: 1,000-10,000 actors\n");
        printf("Memory required: ~%d MB - %d GB\n", actor_count / 1000, actor_count * 8 / 1000);
    }
    
    printf("Allocating %d actors (pthread model)...\n", actor_count);
    PthreadActor* actors = malloc(sizeof(PthreadActor) * actor_count);
    
    // Initialize actors
    for (int i = 0; i < actor_count; i++) {
        actors[i].id = i;
        actors[i].counter = 0;
        actors[i].running = 1;
        actors[i].expected_messages = messages_per_actor;
        queue_init(&actors[i].queue);
        pthread_mutex_init(&actors[i].lock, NULL);
        pthread_cond_init(&actors[i].cond, NULL);
    }
    
    // Create threads
    printf("Creating %d threads...\n", actor_count);
    for (int i = 0; i < actor_count; i++) {
        pthread_create(&actors[i].thread, NULL, actor_loop, &actors[i]);
    }
    
    printf("Starting benchmark: Sending %d messages...\n", actor_count * messages_per_actor);
    clock_t start = clock();
    
    // Send messages
    for (int i = 0; i < actor_count; i++) {
        for (int m = 0; m < messages_per_actor; m++) {
            Message msg = { .value = 1 };
            send_message(&actors[i], msg);
        }
    }
    
    // Signal completion and wait for all threads
    printf("Waiting for actors to finish...\n");
    for (int i = 0; i < actor_count; i++) {
        actors[i].running = 0;
        pthread_cond_signal(&actors[i].cond);
    }
    
    for (int i = 0; i < actor_count; i++) {
        pthread_join(actors[i].thread, NULL);
    }
    
    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    
    // Verify results
    long long total = 0;
    for (int i = 0; i < actor_count; i++) {
        total += actors[i].counter;
    }
    
    printf("Done.\n");
    printf("Processed %lld messages in %.4f seconds.\n", total, time_spent);
    printf("Throughput: %.0f messages/sec\n", total / time_spent);
    printf("Total State Value: %lld (Expected: %d)\n", total, actor_count * messages_per_actor);
    printf("Memory used: ~%d MB (approx, based on 1MB/thread)\n", actor_count);
    
    // Cleanup
    for (int i = 0; i < actor_count; i++) {
        pthread_mutex_destroy(&actors[i].lock);
        pthread_cond_destroy(&actors[i].cond);
    }
    free(actors);
    
    return 0;
}
