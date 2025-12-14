#ifndef AETHER_RUNTIME_H
#define AETHER_RUNTIME_H

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

// Forward declarations
typedef struct Actor Actor;
typedef struct Message Message;
typedef struct Mailbox Mailbox;
typedef struct Scheduler Scheduler;
typedef struct ActorRef ActorRef;

// Message structure
typedef struct Message {
    void* data;
    size_t size;
    ActorRef* sender;
    struct Message* next;
} Message;

// Mailbox for actor message queue
typedef struct Mailbox {
    Message* head;
    Message* tail;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    atomic_int message_count;
} Mailbox;

// Actor reference
typedef struct ActorRef {
    uint64_t actor_id;
    Actor* actor;
    atomic_int ref_count;
} ActorRef;

// Actor state
typedef enum {
    ACTOR_STATE_CREATED,
    ACTOR_STATE_RUNNING,
    ACTOR_STATE_SUSPENDED,
    ACTOR_STATE_TERMINATED
} ActorState;

// Actor structure
typedef struct Actor {
    uint64_t id;
    char* name;
    ActorState state;
    Mailbox* mailbox;
    void* user_data;
    void (*receive_func)(Actor* self, Message* msg);
    void (*cleanup_func)(Actor* self);
    pthread_t thread;
    atomic_int should_stop;
} Actor;

// Scheduler for managing actors
typedef struct Scheduler {
    Actor** actors;
    int actor_count;
    int max_actors;
    pthread_t* worker_threads;
    int worker_count;
    atomic_int running;
    pthread_mutex_t actors_mutex;
} Scheduler;

// Runtime functions
int aether_runtime_init(int worker_threads);
void aether_runtime_shutdown(void);

// Actor management
ActorRef* aether_spawn_actor(const char* name, void (*receive_func)(Actor*, Message*), void* user_data);
void aether_actor_ref_retain(ActorRef* ref);
void aether_actor_ref_release(ActorRef* ref);
int aether_actor_terminate(ActorRef* ref);

// Message passing
int aether_send_message(ActorRef* to, ActorRef* from, void* data, size_t size);
Message* aether_receive_message(Actor* actor);
void aether_free_message(Message* msg);

// Actor utilities
ActorRef* aether_self(void);
int aether_actor_is_running(ActorRef* ref);

// Memory management
void* aether_alloc(size_t size);
void aether_free(void* ptr);

// Error handling
typedef enum {
    AETHER_SUCCESS = 0,
    AETHER_ERROR_INVALID_ARG,
    AETHER_ERROR_OUT_OF_MEMORY,
    AETHER_ERROR_ACTOR_NOT_FOUND,
    AETHER_ERROR_RUNTIME_NOT_INITIALIZED,
    AETHER_ERROR_ACTOR_TERMINATED
} AetherError;

const char* aether_error_string(AetherError error);

// Global runtime instance
extern Scheduler* g_scheduler;

#endif
