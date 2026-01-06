/*
 * SAFE Incremental Benchmark - State Machine Actors
 * 
 * This version starts small and lets you scale gradually.
 * Prevents system crashes by starting with tiny actor counts.
 * 
 * Usage: 
 *   gcc safe_bench.c -O2 -o safe_bench
 *   ./safe_bench <actor_count> <msgs_per_actor>
 * 
 * Safe starting points:
 *   ./safe_bench 10 10        # Tiny test
 *   ./safe_bench 100 10       # Small test
 *   ./safe_bench 1000 10      # Medium test
 *   ./safe_bench 10000 10     # Large test (check RAM first!)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Safety limits
#define MAX_SAFE_ACTORS 1000000
#define MAILBOX_SIZE 16

typedef struct {
    int type;
    int payload;
} Message;

typedef struct {
    Message messages[MAILBOX_SIZE];
    int head, tail, count;
} Mailbox;

typedef struct Actor Actor;
typedef void (*ActorStepFunc)(Actor* self);

struct Actor {
    int id;
    int active;
    Mailbox mailbox;
    ActorStepFunc step;
    int counter_value;
};

// Mailbox operations
int send(Actor* target, Message msg) {
    if (target->mailbox.count >= MAILBOX_SIZE) return 0;
    
    target->mailbox.messages[target->mailbox.tail] = msg;
    target->mailbox.tail = (target->mailbox.tail + 1) % MAILBOX_SIZE;
    target->mailbox.count++;
    target->active = 1;
    return 1;
}

int receive(Actor* self, Message* out_msg) {
    if (self->mailbox.count == 0) return 0;
    
    *out_msg = self->mailbox.messages[self->mailbox.head];
    self->mailbox.head = (self->mailbox.head + 1) % MAILBOX_SIZE;
    self->mailbox.count--;
    return 1;
}

// Simple counter actor
void counter_step(Actor* self) {
    Message msg;
    if (!receive(self, &msg)) {
        self->active = 0;
        return;
    }
    self->counter_value += 1;
}

int main(int argc, char* argv[]) {
    int actor_count = 10;  // Safe default
    int msgs_per_actor = 10;
    
    if (argc >= 2) actor_count = atoi(argv[1]);
    if (argc >= 3) msgs_per_actor = atoi(argv[2]);
    
    // Safety check
    if (actor_count > MAX_SAFE_ACTORS) {
        printf("⚠️  WARNING: %d actors exceeds safety limit (%d)\n", 
               actor_count, MAX_SAFE_ACTORS);
        printf("⚠️  This may freeze your system!\n");
        printf("⚠️  Press Ctrl+C to cancel, or wait 5 seconds to continue...\n");
        
        #ifdef _WIN32
        Sleep(5000);
        #else
        sleep(5);
        #endif
    }
    
    // Memory estimate
    long memory_bytes = (long)actor_count * sizeof(Actor);
    double memory_mb = memory_bytes / (1024.0 * 1024.0);
    
    printf("═══════════════════════════════════════════\n");
    printf("State Machine Actor Benchmark (SAFE MODE)\n");
    printf("═══════════════════════════════════════════\n");
    printf("Actors: %d\n", actor_count);
    printf("Messages per actor: %d\n", msgs_per_actor);
    printf("Total messages: %d\n", actor_count * msgs_per_actor);
    printf("Estimated memory: %.2f MB\n", memory_mb);
    printf("═══════════════════════════════════════════\n\n");
    
    // Allocate
    printf("Allocating %d actors...\n", actor_count);
    Actor* actors = malloc(sizeof(Actor) * actor_count);
    if (!actors) {
        printf("❌ FAILED: Out of memory!\n");
        return 1;
    }
    
    // Initialize
    for (int i = 0; i < actor_count; i++) {
        actors[i].id = i;
        actors[i].active = 1;
        actors[i].step = counter_step;
        actors[i].counter_value = 0;
        actors[i].mailbox.head = 0;
        actors[i].mailbox.tail = 0;
        actors[i].mailbox.count = 0;
    }
    
    printf("✅ Allocation successful\n\n");
    
    // Seed messages
    printf("Sending %d messages...\n", actor_count * msgs_per_actor);
    for (int i = 0; i < actor_count; i++) {
        for (int m = 0; m < msgs_per_actor; m++) {
            Message msg = { .type = 0, .payload = 1 };
            send(&actors[i], msg);
        }
    }
    
    printf("Starting benchmark...\n");
    clock_t start = clock();
    
    // Scheduler loop
    int total_processed = 0;
    int target = actor_count * msgs_per_actor;
    
    while (total_processed < target) {
        int active_count = 0;
        for (int i = 0; i < actor_count; i++) {
            if (actors[i].active && actors[i].mailbox.count > 0) {
                actors[i].step(&actors[i]);
                total_processed++;
                active_count++;
            } else if (actors[i].mailbox.count == 0) {
                actors[i].active = 0;
            }
        }
        
        if (active_count == 0 && total_processed < target) {
            break;  // Deadlock detection
        }
    }
    
    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    
    // Verify
    long long total_value = 0;
    for (int i = 0; i < actor_count; i++) {
        total_value += actors[i].counter_value;
    }
    
    printf("\n═══════════════════════════════════════════\n");
    printf("RESULTS\n");
    printf("═══════════════════════════════════════════\n");
    printf("Processed: %d messages\n", total_processed);
    printf("Time: %.4f seconds\n", time_spent);
    printf("Throughput: %.0f messages/second\n", total_processed / time_spent);
    printf("Memory: %.2f MB\n", memory_mb);
    printf("Bytes per actor: %zu\n", sizeof(Actor));
    printf("Verification: %lld (expected: %d) %s\n", 
           total_value, target, 
           total_value == target ? "✅ PASS" : "❌ FAIL");
    printf("═══════════════════════════════════════════\n");
    
    free(actors);
    return 0;
}
