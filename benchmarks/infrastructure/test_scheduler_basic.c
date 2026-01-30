// Simple test to verify scheduler basics work

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include "../../runtime/actors/actor_state_machine.h"
#include "../../runtime/scheduler/multicore_scheduler.h"

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

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
    // Test-specific fields below
    atomic_int count;
} TestActor;

void test_step(TestActor* self) {
    Message msg;
    if (!mailbox_receive(&self->mailbox, &msg)) {
        return;
    }
    atomic_fetch_add(&self->count, 1);
    printf("Actor %d processed message %d\n", self->id, atomic_load(&self->count));
}

int main() {
    printf("Testing Aether scheduler...\n");
    
    // Init
    printf("1. Initializing scheduler with 2 cores...\n");
    scheduler_init(2);
    
    // Create actor
    printf("2. Creating test actor...\n");
    TestActor* actor = malloc(sizeof(TestActor));
    memset(actor, 0, sizeof(TestActor));  // Zero all fields including pthread_t
    actor->id = 1;
    actor->active = 1;
    actor->auto_process = 0;
    actor->assigned_core = -1;
    actor->step = (void (*)(void*))test_step;
    atomic_store(&actor->count, 0);
    mailbox_init(&actor->mailbox);
    
    // Register
    printf("3. Registering actor...\n");
    scheduler_register_actor((ActorBase*)actor, 0);
    
    // Start
    printf("4. Starting scheduler...\n");
    scheduler_start();
    
    // Send messages
    printf("5. Sending 10 messages...\n");
    for (int i = 0; i < 10; i++) {
        Message msg = {1, 0, i, NULL};
        scheduler_send_local((ActorBase*)actor, msg);
        sleep_ms(100);
    }
    
    // Wait
    printf("6. Waiting for processing...\n");
    sleep_ms(2000);
    
    printf("7. Final count: %d\n", atomic_load(&actor->count));
    
    // Stop
    printf("8. Stopping scheduler...\n");
    scheduler_stop();
    scheduler_wait();
    
    printf("Test complete!\n");
    free(actor);
    
    return 0;
}
