#ifndef AETHER_SEND_BUFFER_H
#define AETHER_SEND_BUFFER_H

#include "actor_state_machine.h"
#include "../utils/aether_thread.h"

// Forward declarations to avoid circular dependencies
struct ActorBase;

/**
 * Sender-Side Message Batching
 * 
 * Reduces atomic operations by accumulating messages in thread-local buffers
 * before flushing to mailboxes in batches.
 * 
 * Performance impact:
 * - Before: 1000 messages = 1000 atomic operations
 * - After: 1000 messages = ~4 atomic operations (256-message batches)
 * - Expected: 2-3x throughput improvement
 */

#define SEND_BUFFER_SIZE 256

// Thread-local send buffer for batching messages to same target
typedef struct {
    void* target;               // Target actor (ActorBase* or similar)
    Message buffer[SEND_BUFFER_SIZE];
    int count;
    int core_id;                // Core ID for cross-core detection
} SendBuffer;

// Thread-local send buffer instance
extern AETHER_TLS SendBuffer g_send_buffer;

// Initialize send buffer for current thread
static inline void send_buffer_init(int core_id) {
    g_send_buffer.target = NULL;
    g_send_buffer.count = 0;
    g_send_buffer.core_id = core_id;
}

// Forward declare flush function (implemented in .c file)
void send_buffer_flush(void);

// Buffered send - accumulates messages before flushing
static inline void send_buffered(struct ActorBase* actor, Message msg) {
    // Flush if target changed or buffer full
    if (g_send_buffer.target != actor || g_send_buffer.count >= SEND_BUFFER_SIZE) {
        if (g_send_buffer.count > 0) {
            send_buffer_flush();
        }
        g_send_buffer.target = actor;
    }
    
    // Add to buffer
    g_send_buffer.buffer[g_send_buffer.count++] = msg;
}

// Force flush of pending messages (call before blocking operations)
static inline void send_buffer_force_flush(void) {
    if (g_send_buffer.count > 0) {
        send_buffer_flush();
        g_send_buffer.target = NULL;
        g_send_buffer.count = 0;
    }
}

// Get buffer stats for monitoring
static inline int send_buffer_pending(void) {
    return g_send_buffer.count;
}

#endif // AETHER_SEND_BUFFER_H
