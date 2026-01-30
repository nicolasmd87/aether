// Baseline benchmark for actor operations
// Measures unoptimized performance for comparison

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include "../../runtime/actors/actor_state_machine.h"

#define ITERATIONS 10000000
#define NUM_ACTORS 100

// Simple counter actor
typedef struct {
    Mailbox mailbox;
    int counter;
    int active;
} CounterActor;

void counter_step(CounterActor* actor) {
    Message msg;
    
    while (mailbox_receive(&actor->mailbox, &msg)) {
        if (msg.type == 1) {  // INCREMENT
            actor->counter++;
        } else if (msg.type == 2) {  // GET
            // Send response (simulate)
        }
    }
}

CounterActor* create_counter_actor() {
    CounterActor* actor = malloc(sizeof(CounterActor));
    memset(actor, 0, sizeof(CounterActor));  // Zero all fields
    mailbox_init(&actor->mailbox);
    actor->counter = 0;
    actor->active = 1;
    return actor;
}

void destroy_counter_actor(CounterActor* actor) {
    free(actor);
}

// Benchmark 1: Actor creation/destruction
double bench_actor_lifecycle() {
    clock_t start = clock();
    
    for (int i = 0; i < ITERATIONS; i++) {
        CounterActor* actor = create_counter_actor();
        destroy_counter_actor(actor);
    }
    
    clock_t end = clock();
    return (double)(end - start) / CLOCKS_PER_SEC;
}

// Benchmark 2: Message sending
double bench_message_send() {
    CounterActor* actors[NUM_ACTORS];
    for (int i = 0; i < NUM_ACTORS; i++) {
        actors[i] = create_counter_actor();
    }
    
    clock_t start = clock();
    
    for (int i = 0; i < ITERATIONS; i++) {
        CounterActor* target = actors[i % NUM_ACTORS];
        Message msg = {1, 0, 0, NULL};  // INCREMENT
        mailbox_send(&target->mailbox, msg);
    }
    
    clock_t end = clock();
    
    for (int i = 0; i < NUM_ACTORS; i++) {
        destroy_counter_actor(actors[i]);
    }
    
    return (double)(end - start) / CLOCKS_PER_SEC;
}

// Benchmark 3: Message processing
double bench_message_processing() {
    CounterActor* actor = create_counter_actor();
    
    // Fill mailbox
    for (int i = 0; i < MAILBOX_SIZE / 2; i++) {
        Message msg = {1, 0, 0, NULL};
        mailbox_send(&actor->mailbox, msg);
    }
    
    clock_t start = clock();
    
    for (int i = 0; i < ITERATIONS; i++) {
        // Keep mailbox half-full
        Message msg = {1, 0, 0, NULL};
        mailbox_send(&actor->mailbox, msg);
        counter_step(actor);
    }
    
    clock_t end = clock();
    
    destroy_counter_actor(actor);
    return (double)(end - start) / CLOCKS_PER_SEC;
}

// Benchmark 4: Cross-actor communication
double bench_cross_actor_comm() {
    CounterActor* actors[NUM_ACTORS];
    for (int i = 0; i < NUM_ACTORS; i++) {
        actors[i] = create_counter_actor();
    }
    
    clock_t start = clock();
    
    for (int i = 0; i < ITERATIONS; i++) {
        int sender = i % NUM_ACTORS;
        int receiver = (i + 1) % NUM_ACTORS;
        Message msg = {1, sender, 0, NULL};
        mailbox_send(&actors[receiver]->mailbox, msg);
        counter_step(actors[receiver]);
    }
    
    clock_t end = clock();
    
    for (int i = 0; i < NUM_ACTORS; i++) {
        destroy_counter_actor(actors[i]);
    }
    
    return (double)(end - start) / CLOCKS_PER_SEC;
}

int main() {
    printf("=== Baseline Actor Performance ===\n\n");
    
    printf("Test Configuration:\n");
    printf("  Iterations: %d\n", ITERATIONS);
    printf("  Actors: %d\n", NUM_ACTORS);
    printf("  Mailbox size: %d\n\n", MAILBOX_SIZE);
    
    double time;
    
    printf("1. Actor Lifecycle (create/destroy):\n");
    // Warmup
    for (int i = 0; i < 1000; i++) {
        CounterActor* actor = create_counter_actor();
        destroy_counter_actor(actor);
    }
    time = bench_actor_lifecycle();
    printf("   Time: %.3f seconds\n", time);
    if (time > 0.001) {
        printf("   Rate: %.0f ops/sec\n\n", ITERATIONS / time);
    } else {
        printf("   Rate: >10B ops/sec (too fast to measure)\n\n");
    }
    
    printf("2. Message Send:\n");
    time = bench_message_send();
    printf("   Time: %.3f seconds\n", time);
    if (time > 0.001) {
        printf("   Throughput: %.0f msg/sec\n\n", ITERATIONS / time);
    } else {
        printf("   Throughput: >10B msg/sec\n\n");
    }
    
    printf("3. Message Processing:\n");
    time = bench_message_processing();
    printf("   Time: %.3f seconds\n", time);
    if (time > 0.001) {
        printf("   Throughput: %.0f msg/sec\n\n", ITERATIONS / time);
    } else {
        printf("   Throughput: >10B msg/sec\n\n");
    }
    
    printf("4. Cross-Actor Communication:\n");
    time = bench_cross_actor_comm();
    printf("   Time: %.3f seconds\n", time);
    if (time > 0.001) {
        printf("   Throughput: %.0f msg/sec\n\n", ITERATIONS / time);
    } else {
        printf("   Throughput: >10B msg/sec\n\n");
    }
    
    return 0;
}
