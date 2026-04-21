/**
 * Type-Specific Actor Pools
 * Pre-allocated actor arrays with O(1) allocation/deallocation.
 * Avoids the malloc/free path for batched actor lifecycles.
 */

#ifndef AETHER_ACTOR_POOL_H
#define AETHER_ACTOR_POOL_H

#include "actor_state_machine.h"
#include <stdlib.h>
#include <string.h>

#define POOL_CAPACITY 1024  // Power of 2 for fast masking

// Generic actor pool structure
typedef struct {
    void* actors;           // Pre-allocated actor array
    int* free_list;         // Stack of free indices
    int free_count;         // Number of free slots
    int capacity;           // Total pool capacity
    size_t actor_size;      // Size of each actor
    void (*init_func)(void*); // Actor initialization function
} ActorPool;

// Initialize a pool for a specific actor type
static inline void actor_pool_init(ActorPool* pool, size_t actor_size, 
                                   void (*init_func)(void*), int capacity) {
    pool->capacity = capacity;
    pool->actor_size = actor_size;
    pool->init_func = init_func;
    pool->free_count = capacity;
    
    // Allocate actor array
    pool->actors = malloc(actor_size * capacity);
    
    // Initialize free list with all indices
    pool->free_list = malloc(sizeof(int) * capacity);
    for (int i = 0; i < capacity; i++) {
        pool->free_list[i] = i;
    }
}

// Allocate actor from pool (O(1))
static inline void* actor_pool_alloc(ActorPool* pool) {
    if (pool->free_count == 0) {
        return NULL;  // Pool exhausted
    }
    
    // Pop from free list
    int index = pool->free_list[--pool->free_count];
    void* actor = (char*)pool->actors + (index * pool->actor_size);
    
    // Initialize actor if init function provided
    if (pool->init_func) {
        pool->init_func(actor);
    }
    
    return actor;
}

// Deallocate actor back to pool (O(1))
static inline void actor_pool_free(ActorPool* pool, void* actor) {
    if (!actor) return;
    
    // Calculate index from pointer
    ptrdiff_t offset = (char*)actor - (char*)pool->actors;
    int index = (int)(offset / pool->actor_size);
    
    // Push to free list
    if (pool->free_count < pool->capacity) {
        pool->free_list[pool->free_count++] = index;
    }
}

// Cleanup pool
static inline void actor_pool_destroy(ActorPool* pool) {
    free(pool->actors);
    free(pool->free_list);
    pool->actors = NULL;
    pool->free_list = NULL;
    pool->free_count = 0;
}

// Batch allocation (even faster for multiple actors)
static inline int actor_pool_alloc_batch(ActorPool* pool, void** out_actors, int count) {
    int allocated = 0;
    
    for (int i = 0; i < count && pool->free_count > 0; i++) {
        out_actors[i] = actor_pool_alloc(pool);
        if (out_actors[i]) {
            allocated++;
        }
    }
    
    return allocated;
}

// Batch deallocation
static inline void actor_pool_free_batch(ActorPool* pool, void** actors, int count) {
    for (int i = 0; i < count; i++) {
        actor_pool_free(pool, actors[i]);
    }
}

#endif // AETHER_ACTOR_POOL_H
