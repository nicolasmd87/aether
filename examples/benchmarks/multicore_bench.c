#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdatomic.h>
#include "actor_state_machine.h"
#include "multicore_scheduler.h"

#define NUM_ACTORS 4000
#define MESSAGES_PER_ACTOR 1000

typedef struct Node {
    int id;
    int active;
    int assigned_core;
    Mailbox mailbox;
    void (*step)(void*);
    int next_id;
    int count;
} Node;

extern __thread int current_core_id;

void Node_step(Node* self) {
    Message msg;
    if (!mailbox_receive(&self->mailbox, &msg)) {
        self->active = 0;
        return;
    }
    self->count++;
}

Node* spawn_Node(int next_id) {
    Node* actor = malloc(sizeof(Node));
    actor->id = atomic_fetch_add(&next_actor_id, 1);
    actor->active = 1;
    actor->assigned_core = -1;
    actor->step = (void (*)(void*))Node_step;
    mailbox_init(&actor->mailbox);
    actor->next_id = next_id;
    actor->count = 0;
    scheduler_register_actor((ActorBase*)actor, -1);
    return actor;
}

void send_Node(Node* actor, int type) {
    Message msg = {type, 0, 0, NULL};
    if (actor->assigned_core == current_core_id) {
        scheduler_send_local((ActorBase*)actor, msg);
    } else {
        scheduler_send_remote((ActorBase*)actor, msg, current_core_id);
    }
}

int main() {
    int cores = 4;
    scheduler_init(cores);
    
    Node** actors = malloc(NUM_ACTORS * sizeof(Node*));
    for (int i = 0; i < NUM_ACTORS; i++) {
        actors[i] = spawn_Node((i + 1) % NUM_ACTORS);
    }
    
    printf("Created %d actors on %d cores\n", NUM_ACTORS, cores);
    printf("Core assignment:\n");
    for (int c = 0; c < cores; c++) {
        int count = 0;
        for (int i = 0; i < NUM_ACTORS; i++) {
            if (actors[i]->assigned_core == c) count++;
        }
        printf("  Core %d: %d actors\n", c, count);
    }
    
    scheduler_start();
    
    int total_messages = NUM_ACTORS * MESSAGES_PER_ACTOR;
    
    clock_t start = clock();
    for (int i = 0; i < total_messages; i++) {
        int actor_idx = i % NUM_ACTORS;
        send_Node(actors[actor_idx], 1);
    }
    
    while (1) {
        int total_processed = 0;
        for (int i = 0; i < NUM_ACTORS; i++) {
            total_processed += actors[i]->count;
        }
        if (total_processed >= total_messages) break;
        usleep(1000);
    }
    clock_t end = clock();
    
    scheduler_stop();
    scheduler_wait();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double msg_per_sec = total_messages / elapsed;
    
    int total_processed = 0;
    for (int i = 0; i < NUM_ACTORS; i++) {
        total_processed += actors[i]->count;
    }
    
    printf("\nMulti-core benchmark results:\n");
    printf("Cores: %d\n", cores);
    printf("Actors: %d\n", NUM_ACTORS);
    printf("Messages sent: %d\n", total_messages);
    printf("Messages processed: %d\n", total_processed);
    printf("Time: %.3f seconds\n", elapsed);
    printf("Throughput: %.0f msg/sec\n", msg_per_sec);
    printf("Throughput: %.1f M msg/sec\n", msg_per_sec / 1000000.0);
    printf("Per-core: %.1f M msg/sec\n", (msg_per_sec / cores) / 1000000.0);
    
    for (int i = 0; i < NUM_ACTORS; i++) {
        free(actors[i]);
    }
    free(actors);
    
    return 0;
}
