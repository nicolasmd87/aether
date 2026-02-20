// Lock-Free SPSC (Single Producer Single Consumer) Mailbox
// Optimized for cross-core message passing with cache line alignment

#ifndef LOCKFREE_MAILBOX_H
#define LOCKFREE_MAILBOX_H

#include <stdatomic.h>
#include <stdint.h>
#include "actor_state_machine.h"

// Power-of-2 size for fast modulo
#define LOCKFREE_MAILBOX_SIZE 64
#define LOCKFREE_MAILBOX_MASK (LOCKFREE_MAILBOX_SIZE - 1)

// Lock-free mailbox with cache line padding to prevent false sharing
typedef struct {
    // Producer-owned (write-mostly)
    atomic_int tail;
    char padding1[60];  // Pad to 64 bytes
    
    // Consumer-owned (write-mostly)
    atomic_int head;
    char padding2[60];  // Pad to 64 bytes
    
    // Shared read-only (after initialization)
    Message messages[LOCKFREE_MAILBOX_SIZE];
} LockFreeMailbox;

// Initialize mailbox
static inline void lockfree_mailbox_init(LockFreeMailbox* mbox) {
    memset(mbox, 0, sizeof(*mbox));
    atomic_store_explicit(&mbox->head, 0, memory_order_relaxed);
    atomic_store_explicit(&mbox->tail, 0, memory_order_relaxed);
}

// Send message (producer side)
static inline int AETHER_HOT lockfree_mailbox_send(
    LockFreeMailbox* AETHER_RESTRICT mbox,
    Message msg)
{
    int tail = atomic_load_explicit(&mbox->tail, memory_order_relaxed);
    int next_tail = (tail + 1) & LOCKFREE_MAILBOX_MASK;
    
    // Note: No manual prefetch - benchmarks showed negative impact
    // Hardware prefetcher handles sequential access pattern efficiently
    
    // Check if full (need acquire to see consumer's head update)
    int head = atomic_load_explicit(&mbox->head, memory_order_acquire);
    if (unlikely(next_tail == head)) {
        return 0;  // Full
    }
    
    // Write message
    mbox->messages[tail] = msg;
    
    // Release write to make message visible to consumer
    atomic_store_explicit(&mbox->tail, next_tail, memory_order_release);
    return 1;
}

// Receive message (consumer side)
static inline int AETHER_HOT lockfree_mailbox_receive(
    LockFreeMailbox* AETHER_RESTRICT mbox,
    Message* AETHER_RESTRICT out_msg)
{
    int head = atomic_load_explicit(&mbox->head, memory_order_relaxed);
    
    // Note: No manual prefetch - sequential pattern already optimal
    
    // Check if empty (need acquire to see producer's tail update)
    int tail = atomic_load_explicit(&mbox->tail, memory_order_acquire);
    if (unlikely(head == tail)) {
        return 0;  // Empty
    }
    
    // Read message
    *out_msg = mbox->messages[head];
    
    // Release read to make space visible to producer
    int next_head = (head + 1) & LOCKFREE_MAILBOX_MASK;
    atomic_store_explicit(&mbox->head, next_head, memory_order_release);
    return 1;
}

// Batch receive (consumer side)
static inline int AETHER_HOT lockfree_mailbox_receive_batch(
    LockFreeMailbox* AETHER_RESTRICT mbox,
    Message* AETHER_RESTRICT out_msgs,
    int max_count)
{
    int head = atomic_load_explicit(&mbox->head, memory_order_relaxed);
    int tail = atomic_load_explicit(&mbox->tail, memory_order_acquire);
    
    // Calculate available messages
    int available = (tail - head) & LOCKFREE_MAILBOX_MASK;
    if (available == 0) return 0;
    
    int to_receive = (available < max_count) ? available : max_count;
    
    // Copy messages
    for (int i = 0; i < to_receive; i++) {
        out_msgs[i] = mbox->messages[(head + i) & LOCKFREE_MAILBOX_MASK];
    }
    
    // Update head
    int new_head = (head + to_receive) & LOCKFREE_MAILBOX_MASK;
    atomic_store_explicit(&mbox->head, new_head, memory_order_release);
    
    return to_receive;
}

// Batch send (producer side)
static inline int AETHER_HOT lockfree_mailbox_send_batch(
    LockFreeMailbox* AETHER_RESTRICT mbox,
    const Message* AETHER_RESTRICT msgs,
    int count)
{
    int tail = atomic_load_explicit(&mbox->tail, memory_order_relaxed);
    int head = atomic_load_explicit(&mbox->head, memory_order_acquire);
    
    // Calculate available space
    int available = (head - tail - 1) & LOCKFREE_MAILBOX_MASK;
    if (available == 0) return 0;
    
    int to_send = (available < count) ? available : count;
    
    // Copy messages
    for (int i = 0; i < to_send; i++) {
        mbox->messages[(tail + i) & LOCKFREE_MAILBOX_MASK] = msgs[i];
    }
    
    // Update tail
    int new_tail = (tail + to_send) & LOCKFREE_MAILBOX_MASK;
    atomic_store_explicit(&mbox->tail, new_tail, memory_order_release);
    
    return to_send;
}

// Check if mailbox is empty (consumer side)
static inline int lockfree_mailbox_is_empty(const LockFreeMailbox* mbox) {
    int head = atomic_load_explicit(&mbox->head, memory_order_relaxed);
    int tail = atomic_load_explicit(&mbox->tail, memory_order_acquire);
    return head == tail;
}

// Get message count (approximate, for debugging)
static inline int lockfree_mailbox_count(const LockFreeMailbox* mbox) {
    int head = atomic_load_explicit(&mbox->head, memory_order_relaxed);
    int tail = atomic_load_explicit(&mbox->tail, memory_order_acquire);
    return (tail - head) & LOCKFREE_MAILBOX_MASK;
}

#endif // LOCKFREE_MAILBOX_H
