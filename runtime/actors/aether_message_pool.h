#ifndef AETHER_MESSAGE_POOL_H
#define AETHER_MESSAGE_POOL_H

#include "actor_state_machine.h"
#include "../config/aether_optimization_config.h"
#include <stdatomic.h>

/**
 * Message Pool - Zero-Copy Message Passing via Pointer Indirection
 *
 * Instead of copying message structs (40 bytes each), we:
 * 1. Allocate messages from thread-local pool
 * 2. Pass message *pointers* through mailboxes (8 bytes)
 * 3. Receiver claims ownership and returns to pool
 *
 * Pool size is configured via:
 * - AETHER_PROFILE env var (micro=256, small=1024, medium=4096, large=16384)
 * - AETHER_MSG_POOL_SIZE env var (explicit override)
 *
 * Benefits:
 * - 5x less data copied (8 bytes vs 40 bytes)
 * - Better cache utilization
 * - Enables true zero-copy for all messages
 * - Memory scales with workload (micro profile uses 90% less RAM)
 */

#define MSG_POOL_MAX_SIZE 16384  // Maximum possible pool size
#define MSG_POOL_BATCH 32

typedef struct {
    Message* messages;           // Dynamically allocated based on profile
    int* free_list;              // Dynamically allocated
    int capacity;                // Actual pool size
    atomic_int head;
    atomic_int count;
} MessagePool;

// Thread-local message pool
extern AETHER_TLS MessagePool* g_msg_pool;

// Initialize thread-local message pool
static inline void message_pool_init_thread(void) {
    if (g_msg_pool) return;

    // Get pool size from config (set by aether_init_from_env)
    int pool_size = g_aether_config.msg_pool_size;
    if (pool_size <= 0 || pool_size > MSG_POOL_MAX_SIZE) {
        pool_size = AETHER_PROFILE_MEDIUM_MSG_POOL;
    }

    g_msg_pool = malloc(sizeof(MessagePool));
    g_msg_pool->messages = malloc(pool_size * sizeof(Message));
    g_msg_pool->free_list = malloc(pool_size * sizeof(int));
    g_msg_pool->capacity = pool_size;
    atomic_store(&g_msg_pool->head, 0);
    atomic_store(&g_msg_pool->count, pool_size);

    // Initialize free list
    for (int i = 0; i < pool_size; i++) {
        g_msg_pool->free_list[i] = i;
    }
}

// Allocate message from pool
static inline Message* message_pool_alloc(void) {
    if (!g_msg_pool) message_pool_init_thread();

    int count = atomic_load_explicit(&g_msg_pool->count, memory_order_relaxed);
    if (count == 0) {
        // Pool exhausted - allocate from heap
        return malloc(sizeof(Message));
    }

    // Pop from free list
    int cap = g_msg_pool->capacity;
    int head = atomic_fetch_add(&g_msg_pool->head, 1) % cap;
    atomic_fetch_sub(&g_msg_pool->count, 1);

    int idx = g_msg_pool->free_list[head];
    return &g_msg_pool->messages[idx];
}

// Return message to pool
static inline void message_pool_free(Message* msg) {
    if (!g_msg_pool) return;

    int cap = g_msg_pool->capacity;

    // Check if message is from pool
    if (msg < &g_msg_pool->messages[0] ||
        msg >= &g_msg_pool->messages[cap]) {
        // Not from pool - free normally
        free(msg);
        return;
    }

    // Return to free list
    int idx = msg - &g_msg_pool->messages[0];
    int tail = (atomic_load(&g_msg_pool->head) + atomic_load(&g_msg_pool->count)) % cap;
    g_msg_pool->free_list[tail] = idx;
    atomic_fetch_add(&g_msg_pool->count, 1);
}

// Create message from pool (convenience)
static inline Message* message_pool_create(int type, int sender_id, int payload) {
    Message* msg = message_pool_alloc();
    msg->type = type;
    msg->sender_id = sender_id;
    msg->payload_int = payload;
    msg->payload_ptr = NULL;
    msg->zerocopy.data = NULL;
    msg->zerocopy.size = 0;
    msg->zerocopy.owned = 0;
    return msg;
}

#endif // AETHER_MESSAGE_POOL_H
