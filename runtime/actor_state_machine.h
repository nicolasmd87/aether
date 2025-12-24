// Aether Actor Runtime - State Machine Implementation
// This header defines the lightweight message passing system

#ifndef AETHER_ACTOR_STATE_MACHINE_H
#define AETHER_ACTOR_STATE_MACHINE_H

#include <stdint.h>

// Message type enum (user-defined message types start at 100)
typedef enum {
    MSG_INCREMENT = 1,
    MSG_DECREMENT = 2,
    MSG_GET_VALUE = 3,
    MSG_SET_VALUE = 4,
    MSG_USER_START = 100
} MessageType;

// Generic message struct
typedef struct {
    int type;           // Message type
    int sender_id;      // ID of sending actor
    int payload_int;    // Integer payload
    void* payload_ptr;  // Pointer payload
} Message;

// Ring buffer mailbox for actors
#define MAILBOX_SIZE 16
typedef struct {
    Message messages[MAILBOX_SIZE];
    int head;
    int tail;
    int count;
} Mailbox;

// Mailbox operations
static inline int mailbox_send(Mailbox* mbox, Message msg) {
    if (mbox->count >= MAILBOX_SIZE) return 0; // Full
    
    mbox->messages[mbox->tail] = msg;
    mbox->tail = (mbox->tail + 1) % MAILBOX_SIZE;
    mbox->count++;
    return 1;
}

static inline int mailbox_receive(Mailbox* mbox, Message* out_msg) {
    if (mbox->count == 0) return 0; // Empty
    
    *out_msg = mbox->messages[mbox->head];
    mbox->head = (mbox->head + 1) % MAILBOX_SIZE;
    mbox->count--;
    return 1;
}

static inline void mailbox_init(Mailbox* mbox) {
    mbox->head = 0;
    mbox->tail = 0;
    mbox->count = 0;
}

#endif // AETHER_ACTOR_STATE_MACHINE_H
