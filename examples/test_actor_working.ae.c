#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "actor_state_machine.h"

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

int main() {
    {
printf("Actor simple assignment test\n");
    }
    return 0;
}
