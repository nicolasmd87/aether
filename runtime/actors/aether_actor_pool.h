// CRAZY OPTIMIZATION #1: Actor Pooling (Reuse instead of spawn)
// Expected: 30-50% improvement for high-churn workloads

#ifndef AETHER_ACTOR_POOL_H
#define AETHER_ACTOR_POOL_H

#include <stdatomic.h>
#include "actor_state_machine.h"

// Reduced from 256 to 64 - still enough for most workloads (saves ~20MB per core)
#define ACTOR_POOL_SIZE 64
#define ACTOR_POOL_MASK (ACTOR_POOL_SIZE - 1)

// Pooled actor with reset capability
typedef struct PooledActor {
    Mailbox mailbox;
    void (*step)(struct PooledActor*);  // Step function pointer
    int active;
    int pool_index;
    _Atomic int in_use;
    void (*reset_fn)(struct PooledActor*);  // Custom reset logic
    // User can extend this with additional fields
} PooledActor;

// Actor pool per type
typedef struct {
    PooledActor actors[ACTOR_POOL_SIZE];
    _Atomic int alloc_index;
    _Atomic int free_index;
    int initialized;
} ActorPool;

// Initialize actor pool
static inline void actor_pool_init(ActorPool* pool) {
    atomic_store(&pool->alloc_index, 0);
    atomic_store(&pool->free_index, 0);
    pool->initialized = 1;
    
    for (int i = 0; i < ACTOR_POOL_SIZE; i++) {
        mailbox_init(&pool->actors[i].mailbox);
        pool->actors[i].step = NULL;
        pool->actors[i].active = 0;
        pool->actors[i].pool_index = i;
        atomic_store(&pool->actors[i].in_use, 0);
        pool->actors[i].reset_fn = NULL;
    }
}

// Get actor from pool (or create if empty)
static inline PooledActor* actor_pool_acquire(ActorPool* pool) {
    // Fast path: try to reuse existing actor
    for (int attempts = 0; attempts < ACTOR_POOL_SIZE; attempts++) {
        int idx = atomic_fetch_add(&pool->alloc_index, 1) & ACTOR_POOL_MASK;
        PooledActor* actor = &pool->actors[idx];
        
        int expected = 0;
        if (atomic_compare_exchange_strong(&actor->in_use, &expected, 1)) {
            // Reset actor state
            mailbox_init(&actor->mailbox);
            actor->active = 1;
            if (actor->reset_fn) {
                actor->reset_fn(actor);
            }
            return actor;
        }
    }
    
    // Pool exhausted - fall back to malloc (slow path)
    return NULL;  // Caller handles allocation
}

// Return actor to pool
static inline void actor_pool_release(ActorPool* pool, PooledActor* actor) {
    if (actor->pool_index >= 0 && actor->pool_index < ACTOR_POOL_SIZE) {
        // Verify it's from this pool
        if (&pool->actors[actor->pool_index] == actor) {
            atomic_store(&actor->in_use, 0);
            return;
        }
    }
    
    // Not from pool, free it
    free(actor);
}

#endif // AETHER_ACTOR_POOL_H
