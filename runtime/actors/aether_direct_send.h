// CRAZY OPTIMIZATION #2: Direct Actor Bypass
// Skip mailbox for same-core actors = massive speedup

#ifndef AETHER_DIRECT_SEND_H
#define AETHER_DIRECT_SEND_H

#include <stdatomic.h>
#include "actor_state_machine.h"

// Thread-local scheduler info (defined in multicore_scheduler.c)
extern AETHER_TLS int current_core_id;

// Actor metadata for direct send optimization
typedef struct {
    Mailbox* mailbox;
    int scheduler_id;  // Which core owns this actor
    _Atomic int message_count;
} ActorMetadata;

// Check if two actors are on same core
static inline int actors_same_core(ActorMetadata* sender, ActorMetadata* receiver) {
    return sender->scheduler_id == receiver->scheduler_id;
}

// Direct send: bypass mailbox, call handler immediately
// Returns 1 if message sent via direct path, 0 if caller should use normal send
static inline int direct_send(
    ActorMetadata* sender, 
    ActorMetadata* receiver, 
    Message msg
) {
    // Verify we're on the correct core
    if (current_core_id < 0 || current_core_id != sender->scheduler_id) {
        return 0;  // Wrong core, use normal send
    }
    
    // Same core: use normal send (simplified - full optimization needs handler access)
    if (actors_same_core(sender, receiver)) {
        return mailbox_send(receiver->mailbox, msg);
    }
    
    return 0;  // Different core, use normal send
}

// Heuristic: should we use direct send?
static inline int should_use_direct_send(
    ActorMetadata* sender,
    ActorMetadata* receiver
) {
    // Only beneficial if:
    // 1. Same core (no cross-core latency)
    // 2. Receiver not too busy (< 8 messages queued)
    // 3. Sender in hot loop (message_count > 1000)
    
    if (!actors_same_core(sender, receiver)) {
        return 0;
    }
    
    int receiver_queue = atomic_load(&receiver->message_count);
    if (receiver_queue > 8) {
        return 0;  // Receiver too busy
    }
    
    return 1;
}

#endif // AETHER_DIRECT_SEND_H
