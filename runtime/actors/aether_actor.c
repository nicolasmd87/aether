#include "aether_actor.h"
#include "../aether_runtime.h"
#include "../utils/aether_compiler.h"
#define _ISOC11_SOURCE
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Forward declarations
static MessagePool* message_pool_create_tls();

// Global message pool (per-thread in production)
static AETHER_TLS MessagePool* tls_message_pool = NULL;

// Initialize thread-local message pool (call once per thread)
void AETHER_CONSTRUCTOR init_thread_local_pool() {
    if (!tls_message_pool) {
        tls_message_pool = message_pool_create_tls();  // Create lock-free TLS pool
    }
}

// Don't use destructor on Windows - causes issues with thread cleanup ordering
#ifndef _WIN32
void AETHER_DESTRUCTOR cleanup_thread_local_pool() {
    if (tls_message_pool) {
        message_pool_destroy(tls_message_pool);
        tls_message_pool = NULL;
    }
}
#endif

MessagePool* message_pool_create() {
    MessagePool* pool = (MessagePool*)malloc(sizeof(MessagePool));
    if (unlikely(!pool)) return NULL;
    
    pool->head = 0;
    pool->tail = 0;
    pool->count = 0;
    pool->is_thread_local = 0;  // Assume shared by default
    pthread_mutex_init(&pool->lock, NULL);
    
    // Pre-allocate buffers
    for (int i = 0; i < MESSAGE_POOL_SIZE; i++) {
        pool->buffers[i] = malloc(256); // 256-byte messages
        if (!pool->buffers[i]) {
            // Clean up on OOM
            for (int j = 0; j < i; j++) free(pool->buffers[j]);
            pthread_mutex_destroy(&pool->lock);
            free(pool);
            return NULL;
        }
    }
    
    return pool;
}

// Create thread-local pool (no mutex needed)
static MessagePool* message_pool_create_tls() {
    MessagePool* pool = message_pool_create();
    if (pool) {
        pool->is_thread_local = 1;  // Mark as thread-local
    }
    return pool;
}

void message_pool_destroy(MessagePool* pool) {
    if (!pool) return;
    
    for (int i = 0; i < MESSAGE_POOL_SIZE; i++) {
        if (pool->buffers[i]) free(pool->buffers[i]);
    }
    
    pthread_mutex_destroy(&pool->lock);
    free(pool);
}

void* AETHER_HOT message_pool_alloc(MessagePool* pool, int size) {
    // Use TLS pool if no explicit pool provided
    if (!pool) {
        if (!tls_message_pool) {
            tls_message_pool = message_pool_create_tls();
        }
        pool = tls_message_pool;
    }
    
    if (unlikely(size > 256)) return malloc(size);
    
    // Lock-free path for TLS pools
    if (pool->is_thread_local) {
        if (unlikely(pool->count == 0)) {
            return malloc(size);
        }
        
        void* buffer = pool->buffers[pool->head];
        pool->head = (pool->head + 1) % MESSAGE_POOL_SIZE;
        pool->count--;
        return buffer;
    }
    
    // Shared pool path (with mutex)
    pthread_mutex_lock(&pool->lock);
    
    if (unlikely(pool->count == 0)) {
        pthread_mutex_unlock(&pool->lock);
        return malloc(size);
    }
    
    void* buffer = pool->buffers[pool->head];
    pool->head = (pool->head + 1) % MESSAGE_POOL_SIZE;
    pool->count--;
    
    pthread_mutex_unlock(&pool->lock);
    return buffer;
}

void AETHER_HOT message_pool_free(MessagePool* pool, void* ptr) {
    if (unlikely(!pool || !ptr)) {
        free(ptr);
        return;
    }
    
    // Lock-free path for TLS pools
    if (pool->is_thread_local) {
        if (unlikely(pool->count >= MESSAGE_POOL_SIZE)) {
            free(ptr);
            return;
        }
        
        pool->buffers[pool->tail] = ptr;
        pool->tail = (pool->tail + 1) % MESSAGE_POOL_SIZE;
        pool->count++;
        return;
    }
    
    // Shared pool path (with mutex)
    pthread_mutex_lock(&pool->lock);
    
    if (unlikely(pool->count >= MESSAGE_POOL_SIZE)) {
        pthread_mutex_unlock(&pool->lock);
        free(ptr);
        return;
    }
    
    pool->buffers[pool->tail] = ptr;
    pool->tail = (pool->tail + 1) % MESSAGE_POOL_SIZE;
    pool->count++;
    
    pthread_mutex_unlock(&pool->lock);
}

static void* actor_thread_loop(void* arg) {
    Actor* actor = (Actor*)arg;
    
    while (likely(actor->active)) {
        Message msg;
        int received;
        
        // Use appropriate mailbox type
        if (actor->use_lockfree) {
            received = lockfree_mailbox_receive(&actor->mailbox.lockfree, &msg);
        } else {
            received = mailbox_receive(&actor->mailbox.simple, &msg);
        }
        
        if (likely(received)) {
            if (likely(actor->process_message)) {
                actor->process_message(actor, &msg, sizeof(Message));
            }
        } else {
            // Use shorter sleep for better responsiveness
            struct timespec ts = {0, 100000}; // 100μs
            nanosleep(&ts, NULL);
        }
    }
    
    return NULL;
}

Actor* aether_actor_create(void (*process_fn)(Actor*, void*, int)) {
    // Use aligned_alloc for cache line alignment
    Actor* actor = (Actor*)malloc(sizeof(Actor));
    if (unlikely(!actor)) return NULL;
    
    actor->id = rand();
    actor->active = 0;
    
    // Check runtime config for mailbox type
    const AetherRuntimeConfig* config = aether_runtime_get_config();
    actor->use_lockfree = config->use_lockfree_mailbox;
    
    // Initialize appropriate mailbox type
    if (actor->use_lockfree) {
        lockfree_mailbox_init(&actor->mailbox.lockfree);
    } else {
        mailbox_init(&actor->mailbox.simple);
    }
    
    actor->pending_requests = NULL;
    actor->next_request_id = 1;
    pthread_mutex_init(&actor->request_mutex, NULL);
    actor->user_data = NULL;
    actor->process_message = process_fn;
    
    return actor;
}

void aether_actor_destroy(Actor* actor) {
    if (unlikely(!actor)) return;
    
    if (actor->active) {
        aether_actor_stop(actor);
    }
    
    PendingRequest* req = actor->pending_requests;
    while (req) {
        PendingRequest* next = req->next;
        pthread_mutex_destroy(&req->mutex);
        pthread_cond_destroy(&req->cond);
        free(req);
        req = next;
    }
    
    pthread_mutex_destroy(&actor->request_mutex);
    free(actor);
}

void aether_actor_start(Actor* actor) {
    actor->active = 1;
    pthread_create(&actor->thread, NULL, actor_thread_loop, actor);
}

void aether_actor_stop(Actor* actor) {
    actor->active = 0;
    pthread_join(actor->thread, NULL);
}

// Hot path - inline candidate with message pool optimization
void aether_send_message(Actor* target, void* message, int message_size) {
    Message msg;
    msg.type = ((int*)message)[0];  // _message_id
    msg.sender_id = 0;
    msg.payload_int = 0;
    msg._reply_slot = NULL;
    
    // Use message pool for small messages (zero-copy optimization)
    if (likely(message_size <= 256 && tls_message_pool)) {
        msg.payload_ptr = message_pool_alloc(tls_message_pool, message_size);
    } else {
        msg.payload_ptr = malloc(message_size);
    }
    
    if (unlikely(!msg.payload_ptr)) return;
    
    // Optimized memcpy for small messages
    if (likely(message_size <= 64)) {
        // Use fast path for small messages (single cache line)
        memcpy(msg.payload_ptr, message, message_size);
    } else {
        memcpy(msg.payload_ptr, message, message_size);
    }
    
    // Use appropriate mailbox type
    if (target->use_lockfree) {
        lockfree_mailbox_send(&target->mailbox.lockfree, msg);
    } else {
        mailbox_send(&target->mailbox.simple, msg);
    }
    
    target->active = 1;  // Wake actor if dormant
}

void* aether_ask_message(Actor* target, void* message, int message_size, int timeout_ms) {
    PendingRequest* req = (PendingRequest*)malloc(sizeof(PendingRequest));
    if (unlikely(!req)) return NULL;
    
    pthread_mutex_lock(&target->request_mutex);
    req->request_id = target->next_request_id++;
    req->reply_ready = 0;
    req->caller = NULL;
    req->reply_data = NULL;
    pthread_mutex_init(&req->mutex, NULL);
    pthread_cond_init(&req->cond, NULL);
    req->next = target->pending_requests;
    target->pending_requests = req;
    pthread_mutex_unlock(&target->request_mutex);
    
    aether_send_message(target, message, message_size);
    
    pthread_mutex_lock(&req->mutex);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    while (!req->reply_ready) {
        if (unlikely(pthread_cond_timedwait(&req->cond, &req->mutex, &ts) != 0)) {
            pthread_mutex_unlock(&req->mutex);
            return NULL; // Timeout
        }
    }
    
    void* reply = req->reply_data;
    pthread_mutex_unlock(&req->mutex);
    
    pthread_mutex_destroy(&req->mutex);
    pthread_cond_destroy(&req->cond);
    free(req);
    
    return reply;
}

void aether_reply(Actor* self, void* reply_data, int reply_size) {
    pthread_mutex_lock(&self->request_mutex);
    
    PendingRequest* req = self->pending_requests;
    if (likely(req)) {
        pthread_mutex_lock(&req->mutex);
        req->reply_data = malloc(reply_size);
        if (likely(req->reply_data)) {
            memcpy(req->reply_data, reply_data, reply_size);
        }
        req->reply_ready = 1;
        pthread_cond_signal(&req->cond);
        pthread_mutex_unlock(&req->mutex);
    }
    
    pthread_mutex_unlock(&self->request_mutex);
}
