#ifndef AETHER_ACTOR_H
#define AETHER_ACTOR_H

#include "actor_state_machine.h"
#include "lockfree_mailbox.h"
#include "../utils/aether_thread.h"
#include <stdint.h>
#include "../utils/aether_compiler.h"

// Cache line size for alignment (typically 64 bytes)
#define CACHE_LINE_SIZE 64

// Unified mailbox type - can be either simple or lock-free
typedef union {
    Mailbox simple;
    LockFreeMailbox lockfree;
} UnifiedMailbox;

// Message pool for zero-copy optimization
// When used as TLS (thread-local storage), no mutex needed
#define MESSAGE_POOL_SIZE 1024
typedef struct MessagePool {
    void* buffers[MESSAGE_POOL_SIZE];
    int head;
    int tail;
    int count;
    int is_thread_local;  // If true, no mutex needed
    pthread_mutex_t lock; // Only used for shared pools
} AETHER_ALIGNED(CACHE_LINE_SIZE) MessagePool;

typedef struct Actor Actor;
typedef void* (*ActorReply)(void*);

// Align to cache line to prevent false sharing (GCC attribute syntax)
typedef struct PendingRequest {
    int request_id;
    int reply_ready;              // Group ints together for better packing
    Actor* caller;                // 8 bytes (pointer)
    void* reply_data;             // 8 bytes (pointer)
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct PendingRequest* next;
} AETHER_ALIGNED(CACHE_LINE_SIZE) PendingRequest;

// Optimize field order: frequent access first, align to cache line
typedef struct Actor {
    // Hot fields (accessed every message) - first cache line
    int active;                   // 4 bytes
    int id;                       // 4 bytes
    int use_lockfree;             // 4 bytes - which mailbox type to use
    UnifiedMailbox mailbox;       // union of simple/lockfree mailbox
    void (*process_message)(Actor* self, void* message, int message_size);
    
    // Warm fields (accessed occasionally) - second cache line
    pthread_t thread;             // 8 bytes
    void* user_data;              // 8 bytes
    
    // Cold fields (rarely accessed) - third cache line  
    PendingRequest* pending_requests;
    int next_request_id;
    pthread_mutex_t request_mutex;
} AETHER_ALIGNED(CACHE_LINE_SIZE) Actor;

Actor* aether_actor_create(void (*process_fn)(Actor*, void*, int));
void aether_actor_destroy(Actor* actor);
void aether_actor_start(Actor* actor);
void aether_actor_stop(Actor* actor);

// Inline hints for hot path functions
void aether_send_message(Actor* target, void* message, int message_size);
void* aether_ask_message(Actor* target, void* message, int message_size, int timeout_ms);
void aether_reply(Actor* self, void* reply_data, int reply_size);

// Message pool operations (zero-copy optimization)
MessagePool* message_pool_create();
void message_pool_destroy(MessagePool* pool);
void* message_pool_alloc(MessagePool* pool, int size);
void message_pool_free(MessagePool* pool, void* ptr);

#endif
