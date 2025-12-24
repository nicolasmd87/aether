// Manual runtime test - compile and run separately
// This is NOT part of the automated test suite
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../runtime/actor_state_machine.h"

typedef struct Counter {
    int id;
    int active;
    Mailbox mailbox;
    int count;
} Counter;

void Counter_step(Counter* self) {
    Message msg;
    if (!mailbox_receive(&self->mailbox, &msg)) {
        self->active = 0;
        return;
    }
    (self->count = (self->count + 1));
}

Counter* spawn_Counter() {
    Counter* actor = malloc(sizeof(Counter));
    actor->id = 1;
    actor->active = 1;
    mailbox_init(&actor->mailbox);
    actor->count = 0;
    return actor;
}

void send_message(void* actor_ptr, int type, int payload) {
    Counter* actor = (Counter*)actor_ptr;
    Message msg = {type, 0, payload, NULL};
    mailbox_send(&actor->mailbox, msg);
    actor->active = 1;
}

int main() {
    Counter* c = spawn_Counter();
    
    send_message(c, 1, 0);
    Counter_step(c);
    
    printf("Counter value: %d\n", c->count);
    free(c);
    return 0;
}

