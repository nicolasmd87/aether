// Simple foundation test - test one thing at a time

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#include "../../runtime/actors/actor_state_machine.h"
#include "../../runtime/scheduler/multicore_scheduler.h"

typedef struct {
    int id;
    int active;
    atomic_int assigned_core;
    Mailbox mailbox;
    void (*step)(void*);
    atomic_int count;
    atomic_int last_value;
} CounterActor;

void counter_step(CounterActor* self) {
    Message msg;
    int processed = 0;
    while (mailbox_receive(&self->mailbox, &msg)) {
        atomic_fetch_add(&self->count, 1);
        atomic_store(&self->last_value, msg.payload_int);
        processed++;
    }
    // Keep active if there might be more messages or if we just processed some
    self->active = (self->mailbox.count > 0) || (processed > 0);
}

int main() {
    printf("========================================\n");
    printf("Aether Scheduler Foundation Test\n");
    printf("========================================\n\n");
    
    printf("Initializing scheduler with 2 cores...\n");
    scheduler_init(2);
    
    printf("Creating test actor...\n");
    CounterActor* actor = malloc(sizeof(CounterActor));
    actor->id = 1;
    actor->active = 0;
    actor->step = (void (*)(void*))counter_step;
    atomic_store(&actor->count, 0);
    atomic_store(&actor->last_value, -1);
    mailbox_init(&actor->mailbox);
    
    printf("Registering actor on core 0...\n");
    scheduler_register_actor((ActorBase*)actor, 0);
    
    printf("Starting scheduler threads...\n");
    scheduler_start();
    
    printf("Sending 100 messages (in batches to avoid overflow)...\n");
    fflush(stdout);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int sent = 0;
    while (sent < 100) {
        if (sent % 10 == 0) {
            fprintf(stderr, "[TEST] Sent %d messages so far\n", sent);
        }
        Message msg = {1, 0, sent, NULL};
        if (mailbox_send(&actor->mailbox, msg)) {
            actor->active = 1;
            sent++;
        } else {
            // Mailbox full, wait a bit
            sleep_ms(1);
        }
    }
    
    fprintf(stderr, "[TEST] All messages queued successfully\n");
    printf("All messages queued successfully\n");
    
    // Wait for processing
    printf("Waiting for processing...\n");
    int last_count = 0;
    int stuck_iterations = 0;
    for (int i = 0; i < 100; i++) {
        sleep_ms(10);
        int current_count = atomic_load(&actor->count);
        printf("  Progress: %d/100 messages processed\r", current_count);
        fflush(stdout);
        
        if (current_count == last_count) {
            stuck_iterations++;
            if (stuck_iterations > 50) {
                printf("\n");
                printf("ERROR: Stuck at %d messages - scheduler appears hung\n", current_count);
                break;
            }
        } else {
            stuck_iterations = 0;
        }
        last_count = current_count;
        
        if (current_count >= 100) {
            break;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("\n");
    int final_count = atomic_load(&actor->count);
    int final_last = atomic_load(&actor->last_value);
    
    printf("\n========================================\n");
    printf("Results:\n");
    printf("  Messages sent: 100\n");
    printf("  Messages processed: %d\n", final_count);
    printf("  Last value received: %d\n", final_last);
    printf("  Time: %.3f seconds\n", elapsed);
    if (final_count > 0) {
        printf("  Throughput: %.0f messages/sec\n", final_count / elapsed);
    }
    printf("========================================\n\n");
    
    printf("Stopping scheduler...\n");
    scheduler_stop();
    scheduler_wait();
    
    free(actor);
    
    if (final_count == 100 && final_last == 99) {
        printf("\n✓ TEST PASSED: All messages processed correctly\n");
        return 0;
    } else {
        printf("\n✗ TEST FAILED: Expected 100 messages with last value 99\n");
        return 1;
    }
}
