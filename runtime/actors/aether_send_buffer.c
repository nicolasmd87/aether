#include "aether_send_buffer.h"
#include "aether_spsc_queue.h"
#include "../scheduler/multicore_scheduler.h"
#include "../scheduler/lockfree_queue.h"
#include <string.h>

// Thread-local send buffer
AETHER_TLS SendBuffer g_send_buffer = {NULL, {{0}}, 0, -1};

// Forward declare to access schedulers
extern Scheduler schedulers[];

// Flush accumulated messages to target actor
void send_buffer_flush(void) {
    if (g_send_buffer.count == 0 || !g_send_buffer.target) {
        return;
    }
    
    ActorBase* actor = (ActorBase*)g_send_buffer.target;
    int target_core = actor->assigned_core;
    
    // Fast path: same core, use lock-free SPSC queue
    if (g_send_buffer.core_id == target_core && g_send_buffer.core_id >= 0) {
        // Try SPSC queue first (lock-free, fastest path)
        int sent = spsc_enqueue_batch(&actor->spsc_queue, g_send_buffer.buffer, g_send_buffer.count);
        if (sent == g_send_buffer.count) {
            actor->active = 1;
            g_send_buffer.count = 0;
            return;
        }
        
        // SPSC queue full, fall back to mailbox
        sent = mailbox_send_batch(&actor->mailbox, g_send_buffer.buffer, g_send_buffer.count);
        if (sent == g_send_buffer.count) {
            actor->active = 1;
            g_send_buffer.count = 0;
            return;
        }
        
        // Partial send - move unsent messages to front
        int unsent = g_send_buffer.count - sent;
        memmove(g_send_buffer.buffer, g_send_buffer.buffer + sent, unsent * sizeof(Message));
        g_send_buffer.count = unsent;
        return;
    }
    
    // Slow path: cross-core batch enqueue (KEY OPTIMIZATION)
    // Batch all messages to same actor in ONE atomic operation
    void* actors[SEND_BUFFER_SIZE];
    for (int i = 0; i < g_send_buffer.count; i++) {
        actors[i] = actor;  // All messages to same actor
    }
    
    if (queue_enqueue_batch(&schedulers[target_core].incoming_queue, 
                            actors, g_send_buffer.buffer, g_send_buffer.count)) {
        // Success - update work count once for entire batch
        atomic_fetch_add(&schedulers[target_core].work_count, g_send_buffer.count);
        g_send_buffer.count = 0;
    } else {
        // Queue full - fall back to individual sends with backoff
        for (int i = 0; i < g_send_buffer.count; i++) {
            scheduler_send_remote(actor, g_send_buffer.buffer[i], g_send_buffer.core_id);
        }
        g_send_buffer.count = 0;
    }
}
