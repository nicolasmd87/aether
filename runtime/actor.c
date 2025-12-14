#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "aether_runtime.h"

// Windows MinGW/Winpthreads doesn't always provide clock_gettime().
// Provide a small shim so we can still use pthread_cond_timedwait().
static void aether_clock_realtime(struct timespec* ts) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    // FILETIME is 100ns ticks since 1601-01-01.
    // Convert to Unix epoch (1970-01-01).
    const unsigned long long EPOCH_DIFF_100NS = 116444736000000000ULL;
    unsigned long long t = uli.QuadPart - EPOCH_DIFF_100NS;

    ts->tv_sec = (time_t)(t / 10000000ULL);
    ts->tv_nsec = (long)((t % 10000000ULL) * 100ULL);
#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif
}

// Global scheduler instance
Scheduler* g_scheduler = NULL;

// Actor ID counter
static atomic_uint_fast64_t next_actor_id = 1;

// Actor lifecycle functions
Actor* create_actor(const char* name, void (*receive_func)(Actor*, Message*), void* user_data) {
    Actor* actor = malloc(sizeof(Actor));
    if (!actor) return NULL;
    
    actor->id = atomic_fetch_add(&next_actor_id, 1);
    actor->name = strdup(name);
    actor->state = ACTOR_STATE_CREATED;
    actor->receive_func = receive_func;
    actor->cleanup_func = NULL;
    actor->user_data = user_data;
    actor->should_stop = 0;
    
    // Create mailbox
    actor->mailbox = malloc(sizeof(Mailbox));
    if (!actor->mailbox) {
        free(actor->name);
        free(actor);
        return NULL;
    }
    
    actor->mailbox->head = NULL;
    actor->mailbox->tail = NULL;
    pthread_mutex_init(&actor->mailbox->mutex, NULL);
    pthread_cond_init(&actor->mailbox->condition, NULL);
    atomic_init(&actor->mailbox->message_count, 0);
    
    return actor;
}

void destroy_actor(Actor* actor) {
    if (!actor) return;
    
    // Stop the actor
    atomic_store(&actor->should_stop, 1);
    
    // Wait for thread to finish
    if (actor->thread) {
        pthread_join(actor->thread, NULL);
    }
    
    // Cleanup mailbox
    if (actor->mailbox) {
        pthread_mutex_lock(&actor->mailbox->mutex);
        
        Message* current = actor->mailbox->head;
        while (current) {
            Message* next = current->next;
            aether_free_message(current);
            current = next;
        }
        
        pthread_mutex_unlock(&actor->mailbox->mutex);
        pthread_mutex_destroy(&actor->mailbox->mutex);
        pthread_cond_destroy(&actor->mailbox->condition);
        free(actor->mailbox);
    }
    
    // Call user cleanup function
    if (actor->cleanup_func) {
        actor->cleanup_func(actor);
    }
    
    free(actor->name);
    free(actor);
}

// Actor thread function
void* actor_thread_func(void* arg) {
    Actor* actor = (Actor*)arg;
    actor->state = ACTOR_STATE_RUNNING;
    
    printf("Actor %s (ID: %llu) started\n", actor->name, (unsigned long long)actor->id);
    
    while (!atomic_load(&actor->should_stop)) {
        Message* msg = aether_receive_message(actor);
        if (msg) {
            if (actor->receive_func) {
                actor->receive_func(actor, msg);
            }
            aether_free_message(msg);
        } else {
            // No message, yield briefly
            usleep(1000); // 1ms
        }
    }
    
    actor->state = ACTOR_STATE_TERMINATED;
    printf("Actor %s (ID: %llu) terminated\n", actor->name, (unsigned long long)actor->id);
    
    return NULL;
}

// ActorRef functions
ActorRef* create_actor_ref(Actor* actor) {
    ActorRef* ref = malloc(sizeof(ActorRef));
    if (!ref) return NULL;
    
    ref->actor = actor;
    ref->actor_id = actor->id;
    atomic_init(&ref->ref_count, 1);
    
    return ref;
}

void aether_actor_ref_retain(ActorRef* ref) {
    if (ref) {
        atomic_fetch_add(&ref->ref_count, 1);
    }
}

void aether_actor_ref_release(ActorRef* ref) {
    if (!ref) return;
    
    int count = atomic_fetch_sub(&ref->ref_count, 1);
    if (count == 1) {
        // Last reference, destroy actor
        if (ref->actor) {
            destroy_actor(ref->actor);
        }
        free(ref);
    }
}

// Message functions
Message* create_message(void* data, size_t size, ActorRef* sender) {
    Message* msg = malloc(sizeof(Message));
    if (!msg) return NULL;
    
    msg->data = malloc(size);
    if (!msg->data) {
        free(msg);
        return NULL;
    }
    
    memcpy(msg->data, data, size);
    msg->size = size;
    msg->sender = sender;
    if (sender) {
        aether_actor_ref_retain(sender);
    }
    msg->next = NULL;
    
    return msg;
}

void aether_free_message(Message* msg) {
    if (!msg) return;
    
    if (msg->data) {
        free(msg->data);
    }
    
    if (msg->sender) {
        aether_actor_ref_release(msg->sender);
    }
    
    free(msg);
}

// Mailbox functions
int aether_send_message(ActorRef* to, ActorRef* from, void* data, size_t size) {
    if (!to || !to->actor) return AETHER_ERROR_ACTOR_NOT_FOUND;
    
    Message* msg = create_message(data, size, from);
    if (!msg) return AETHER_ERROR_OUT_OF_MEMORY;
    
    Mailbox* mailbox = to->actor->mailbox;
    
    pthread_mutex_lock(&mailbox->mutex);
    
    if (mailbox->tail) {
        mailbox->tail->next = msg;
    } else {
        mailbox->head = msg;
    }
    mailbox->tail = msg;
    
    atomic_fetch_add(&mailbox->message_count, 1);
    
    pthread_cond_signal(&mailbox->condition);
    pthread_mutex_unlock(&mailbox->mutex);
    
    return AETHER_SUCCESS;
}

Message* aether_receive_message(Actor* actor) {
    if (!actor || !actor->mailbox) return NULL;
    
    Mailbox* mailbox = actor->mailbox;
    Message* msg = NULL;
    
    pthread_mutex_lock(&mailbox->mutex);
    
    // Wait for message with timeout
    struct timespec timeout;
    aether_clock_realtime(&timeout);
    timeout.tv_nsec += 10000000; // 10ms timeout
    if (timeout.tv_nsec >= 1000000000) {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000;
    }
    
    while (!mailbox->head && !atomic_load(&actor->should_stop)) {
        int result = pthread_cond_timedwait(&mailbox->condition, &mailbox->mutex, &timeout);
        if (result == ETIMEDOUT) {
            break;
        }
    }
    
    if (mailbox->head) {
        msg = mailbox->head;
        mailbox->head = msg->next;
        if (!mailbox->head) {
            mailbox->tail = NULL;
        }
        atomic_fetch_sub(&mailbox->message_count, 1);
    }
    
    pthread_mutex_unlock(&mailbox->mutex);
    
    return msg;
}

// Actor management functions
ActorRef* aether_spawn_actor(const char* name, void (*receive_func)(Actor*, Message*), void* user_data) {
    if (!g_scheduler) return NULL;
    
    Actor* actor = create_actor(name, receive_func, user_data);
    if (!actor) return NULL;
    
    // Add to scheduler
    pthread_mutex_lock(&g_scheduler->actors_mutex);
    
    if (g_scheduler->actor_count >= g_scheduler->max_actors) {
        pthread_mutex_unlock(&g_scheduler->actors_mutex);
        destroy_actor(actor);
        return NULL;
    }
    
    g_scheduler->actors[g_scheduler->actor_count] = actor;
    g_scheduler->actor_count++;
    
    pthread_mutex_unlock(&g_scheduler->actors_mutex);
    
    // Start actor thread
    if (pthread_create(&actor->thread, NULL, actor_thread_func, actor) != 0) {
        // Failed to create thread, remove from scheduler
        pthread_mutex_lock(&g_scheduler->actors_mutex);
        for (int i = 0; i < g_scheduler->actor_count; i++) {
            if (g_scheduler->actors[i] == actor) {
                for (int j = i; j < g_scheduler->actor_count - 1; j++) {
                    g_scheduler->actors[j] = g_scheduler->actors[j + 1];
                }
                g_scheduler->actor_count--;
                break;
            }
        }
        pthread_mutex_unlock(&g_scheduler->actors_mutex);
        
        destroy_actor(actor);
        return NULL;
    }
    
    return create_actor_ref(actor);
}

int aether_actor_terminate(ActorRef* ref) {
    if (!ref || !ref->actor) return AETHER_ERROR_ACTOR_NOT_FOUND;
    
    atomic_store(&ref->actor->should_stop, 1);
    return AETHER_SUCCESS;
}

ActorRef* aether_self(void) {
    // This would need to be implemented with thread-local storage
    // For now, return NULL as a placeholder
    return NULL;
}

int aether_actor_is_running(ActorRef* ref) {
    if (!ref || !ref->actor) return 0;
    return ref->actor->state == ACTOR_STATE_RUNNING;
}

// Memory management functions are defined in memory.c

// Error handling
const char* aether_error_string(AetherError error) {
    switch (error) {
        case AETHER_SUCCESS: return "Success";
        case AETHER_ERROR_INVALID_ARG: return "Invalid argument";
        case AETHER_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case AETHER_ERROR_ACTOR_NOT_FOUND: return "Actor not found";
        case AETHER_ERROR_RUNTIME_NOT_INITIALIZED: return "Runtime not initialized";
        case AETHER_ERROR_ACTOR_TERMINATED: return "Actor terminated";
        default: return "Unknown error";
    }
}
