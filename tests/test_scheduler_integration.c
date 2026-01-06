// Integration Test: Partitioned Multi-core Scheduler
// Tests the actual scheduler implementation with real actors

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include "../runtime/multicore_scheduler.h"

// Simple test actor
typedef struct {
    ActorBase base;
    int counter;
    int messages_received;
} TestActor;

void test_actor_step(void* actor_ptr) {
    TestActor* actor = (TestActor*)actor_ptr;
    
    Message msg;
    while (mailbox_receive(&actor->base.mailbox, &msg)) {
        actor->counter += msg.payload_int;
        actor->messages_received++;
        actor->base.active = 0;  // Deactivate after processing
    }
}

TestActor* create_test_actor(int id) {
    TestActor* actor = malloc(sizeof(TestActor));
    actor->base.id = id;
    actor->base.active = 0;
    actor->base.assigned_core = -1;
    actor->base.step = test_actor_step;
    mailbox_init(&actor->base.mailbox);
    actor->counter = 0;
    actor->messages_received = 0;
    return actor;
}

int main() {
    printf("===========================================\n");
    printf("Partitioned Scheduler Integration Test\n");
    printf("===========================================\n\n");
    
    int num_cores = 8;
    int num_actors = 10000;
    int messages_per_actor = 100;
    
    printf("Configuration:\n");
    printf("  Cores: %d\n", num_cores);
    printf("  Actors: %d\n", num_actors);
    printf("  Messages per actor: %d\n\n", messages_per_actor);
    
    // Initialize scheduler
    printf("Initializing scheduler...\n");
    scheduler_init(num_cores);
    
    // Create and register actors
    printf("Creating %d actors...\n", num_actors);
    TestActor** actors = malloc(sizeof(TestActor*) * num_actors);
    
    for (int i = 0; i < num_actors; i++) {
        actors[i] = create_test_actor(i);
        int core = scheduler_register_actor(&actors[i]->base, -1);  // Auto-assign
        if (core < 0) {
            printf("Failed to register actor %d\n", i);
            return 1;
        }
    }
    
    printf("Actors distributed across cores:\n");
    int actors_per_core[MAX_CORES] = {0};
    for (int i = 0; i < num_actors; i++) {
        actors_per_core[actors[i]->base.assigned_core]++;
    }
    for (int i = 0; i < num_cores; i++) {
        printf("  Core %d: %d actors\n", i, actors_per_core[i]);
    }
    printf("\n");
    
    // Start scheduler
    printf("Starting scheduler threads...\n");
    scheduler_start();
    
    // Send messages
    printf("Sending %d messages...\n", num_actors * messages_per_actor);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < num_actors; i++) {
        for (int m = 0; m < messages_per_actor; m++) {
            Message msg = {1, 0, 1, NULL};
            
            if (actors[i]->base.assigned_core == -1) {
                scheduler_send_local(&actors[i]->base, msg);
            } else {
                scheduler_send_remote(&actors[i]->base, msg, 0);
            }
        }
    }
    
    // Wait for processing
    printf("Waiting for processing...\n");
    #ifdef _WIN32
    _sleep(2000);  // Windows: _sleep in milliseconds
    #else
    sleep(2);      // Unix: sleep in seconds
    #endif
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Stop scheduler
    printf("Stopping scheduler...\n");
    scheduler_stop();
    scheduler_wait();
    
    // Verify results
    printf("\nVerifying results...\n");
    int total_messages = 0;
    int total_counter = 0;
    int actors_ok = 0;
    
    for (int i = 0; i < num_actors; i++) {
        total_messages += actors[i]->messages_received;
        total_counter += actors[i]->counter;
        
        if (actors[i]->messages_received == messages_per_actor && 
            actors[i]->counter == messages_per_actor) {
            actors_ok++;
        }
    }
    
    printf("  Expected messages: %d\n", num_actors * messages_per_actor);
    printf("  Received messages: %d\n", total_messages);
    printf("  Expected counter: %d\n", num_actors * messages_per_actor);
    printf("  Actual counter: %d\n", total_counter);
    printf("  Actors correct: %d/%d\n\n", actors_ok, num_actors);
    
    // Performance
    long long total_msgs = (long long)num_actors * messages_per_actor;
    double throughput = total_msgs / time_taken / 1e6;
    
    printf("Performance:\n");
    printf("  Time: %.4f seconds\n", time_taken);
    printf("  Throughput: %.2f M msg/sec\n\n", throughput);
    
    // Cleanup
    for (int i = 0; i < num_actors; i++) {
        free(actors[i]);
    }
    free(actors);
    
    // Final verdict
    if (actors_ok == num_actors) {
        printf("===========================================\n");
        printf("✓ ALL TESTS PASSED\n");
        printf("===========================================\n");
        return 0;
    } else {
        printf("===========================================\n");
        printf("✗ TEST FAILED: %d/%d actors incorrect\n", num_actors - actors_ok, num_actors);
        printf("===========================================\n");
        return 1;
    }
}
