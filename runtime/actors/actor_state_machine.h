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

// Mailbox operations with optimization hints
// Note: Manual prefetch removed - benchmarks showed 16% performance loss
// Hardware prefetcher handles sequential ring buffer access more efficiently
static inline int __attribute__((hot)) mailbox_send(Mailbox* __restrict__ mbox, Message msg) {
    if (__builtin_expect(mbox->count >= MAILBOX_SIZE, 0)) return 0; // Full (unlikely)
    
    mbox->messages[mbox->tail] = msg;
    mbox->tail = (mbox->tail + 1) % MAILBOX_SIZE;
    mbox->count++;
    return 1;
}

static inline int __attribute__((hot)) mailbox_receive(Mailbox* __restrict__ mbox, Message* __restrict__ out_msg) {
    if (__builtin_expect(mbox->count == 0, 0)) return 0; // Empty (unlikely)
    
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

// Batch operations for high-throughput scenarios
static inline int __attribute__((hot)) mailbox_receive_batch(
    Mailbox* __restrict__ mbox, 
    Message* __restrict__ out_msgs, 
    int max_count) 
{
    int received = 0;
    while (received < max_count && mbox->count > 0) {
        out_msgs[received] = mbox->messages[mbox->head];
        mbox->head = (mbox->head + 1) % MAILBOX_SIZE;
        mbox->count--;
        received++;
    }
    return received;
}

static inline int __attribute__((hot)) mailbox_send_batch(
    Mailbox* __restrict__ mbox,
    const Message* __restrict__ msgs,
    int count)
{
    int sent = 0;
    while (sent < count && mbox->count < MAILBOX_SIZE) {
        mbox->messages[mbox->tail] = msgs[sent];
        mbox->tail = (mbox->tail + 1) % MAILBOX_SIZE;
        mbox->count++;
        sent++;
    }
    return sent;
}

#endif // AETHER_ACTOR_STATE_MACHINE_H
