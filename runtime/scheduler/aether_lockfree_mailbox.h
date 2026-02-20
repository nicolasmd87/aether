// Lock-Free SPSC Queue - Single-Producer Single-Consumer
// Optimized for actor mailboxes (one writer, one reader)
// Uses atomic operations instead of mutexes for 1.5-2x speedup

#ifndef AETHER_LOCKFREE_MAILBOX_H
#define AETHER_LOCKFREE_MAILBOX_H

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "actor_state_machine.h"  // Use existing Message type

// Lock-free SPSC queue (Single-Producer, Single-Consumer)
#define LOCKFREE_MAILBOX_SIZE 64  // Power of 2 for fast modulo

typedef struct {
    // Producer fields (written by sender, read by receiver)
    _Atomic uint32_t head;      // Write position
    char pad1[60];              // Cache line padding

    // Consumer fields (written by receiver, read by sender)
    _Atomic uint32_t tail;      // Read position
    char pad2[60];              // Cache line padding

    // Shared data (read by both)
    Message messages[LOCKFREE_MAILBOX_SIZE];
} AETHER_ALIGNED(64) LockFreeMailbox;

// Initialize mailbox
static inline void lockfree_mailbox_init(LockFreeMailbox* mbox) {
    atomic_store_explicit(&mbox->head, 0, memory_order_relaxed);
    atomic_store_explicit(&mbox->tail, 0, memory_order_relaxed);
}

// Check if empty (consumer side)
static inline bool lockfree_mailbox_is_empty(LockFreeMailbox* mbox) {
    uint32_t head = atomic_load_explicit(&mbox->head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&mbox->tail, memory_order_relaxed);
    return head == tail;
}

// Check if full (producer side)
static inline bool lockfree_mailbox_is_full(LockFreeMailbox* mbox) {
    uint32_t head = atomic_load_explicit(&mbox->head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&mbox->tail, memory_order_acquire);
    return ((head + 1) & (LOCKFREE_MAILBOX_SIZE - 1)) == tail;
}

// Send message (producer side)
static inline bool AETHER_HOT lockfree_mailbox_send(
    LockFreeMailbox* AETHER_RESTRICT mbox,
    Message msg)
{
    uint32_t head = atomic_load_explicit(&mbox->head, memory_order_relaxed);
    uint32_t next = (head + 1) & (LOCKFREE_MAILBOX_SIZE - 1);
    uint32_t tail = atomic_load_explicit(&mbox->tail, memory_order_acquire);
    
    if (unlikely(next == tail)) {
        return false;  // Full
    }
    
    // Write message
    mbox->messages[head] = msg;
    
    // Publish write (release semantics)
    atomic_store_explicit(&mbox->head, next, memory_order_release);
    return true;
}

// Receive message (consumer side)
static inline bool AETHER_HOT lockfree_mailbox_receive(
    LockFreeMailbox* AETHER_RESTRICT mbox,
    Message* AETHER_RESTRICT out_msg)
{
    uint32_t tail = atomic_load_explicit(&mbox->tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&mbox->head, memory_order_acquire);
    
    if (unlikely(tail == head)) {
        return false;  // Empty
    }
    
    // Read message
    *out_msg = mbox->messages[tail];
    
    // Publish read (release semantics)
    uint32_t next = (tail + 1) & (LOCKFREE_MAILBOX_SIZE - 1);
    atomic_store_explicit(&mbox->tail, next, memory_order_release);
    return true;
}

// Batch receive (up to max_count messages)
static inline int AETHER_HOT lockfree_mailbox_receive_batch(
    LockFreeMailbox* AETHER_RESTRICT mbox,
    Message* AETHER_RESTRICT out_msgs,
    int max_count)
{
    int received = 0;
    uint32_t tail = atomic_load_explicit(&mbox->tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&mbox->head, memory_order_acquire);
    
    while (received < max_count && tail != head) {
        out_msgs[received++] = mbox->messages[tail];
        tail = (tail + 1) & (LOCKFREE_MAILBOX_SIZE - 1);
    }
    
    if (received > 0) {
        atomic_store_explicit(&mbox->tail, tail, memory_order_release);
    }
    
    return received;
}

// Get approximate count (not exact due to concurrent access)
static inline int lockfree_mailbox_count(LockFreeMailbox* mbox) {
    uint32_t head = atomic_load_explicit(&mbox->head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&mbox->tail, memory_order_acquire);
    return (head - tail) & (LOCKFREE_MAILBOX_SIZE - 1);
}

#endif // AETHER_LOCKFREE_MAILBOX_H
