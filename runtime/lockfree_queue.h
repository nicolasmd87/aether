#ifndef LOCKFREE_QUEUE_H
#define LOCKFREE_QUEUE_H

#include <stdatomic.h>
#include <stdlib.h>
#include "actor_state_machine.h"

#define QUEUE_SIZE 1024

typedef struct {
    void* actor;
    Message msg;
} QueueItem;

typedef struct {
    QueueItem items[QUEUE_SIZE];
    atomic_int head;
    atomic_int tail;
    char padding[128];
} LockFreeQueue;

static inline void queue_init(LockFreeQueue* q) {
    atomic_store(&q->head, 0);
    atomic_store(&q->tail, 0);
}

static inline int queue_enqueue(LockFreeQueue* q, void* actor, Message msg) {
    int tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    int next_tail = (tail + 1) % QUEUE_SIZE;
    int head = atomic_load_explicit(&q->head, memory_order_acquire);
    
    if (next_tail == head) {
        return 0;
    }
    
    q->items[tail].actor = actor;
    q->items[tail].msg = msg;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return 1;
}

static inline int queue_dequeue(LockFreeQueue* q, void** actor, Message* msg) {
    int head = atomic_load_explicit(&q->head, memory_order_relaxed);
    int tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    
    if (head == tail) {
        return 0;
    }
    
    *actor = q->items[head].actor;
    *msg = q->items[head].msg;
    atomic_store_explicit(&q->head, (head + 1) % QUEUE_SIZE, memory_order_release);
    return 1;
}

#endif
