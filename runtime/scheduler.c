#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "aether_runtime.h"

// Forward declaration
void destroy_actor(Actor* actor);

// Scheduler functions
int aether_runtime_init(int worker_threads) {
    if (g_scheduler) {
        return AETHER_ERROR_RUNTIME_NOT_INITIALIZED; // Already initialized
    }
    
    g_scheduler = malloc(sizeof(Scheduler));
    if (!g_scheduler) {
        return AETHER_ERROR_OUT_OF_MEMORY;
    }
    
    g_scheduler->max_actors = 1000; // Default max actors
    g_scheduler->actor_count = 0;
    g_scheduler->worker_count = worker_threads > 0 ? worker_threads : 4;
    atomic_init(&g_scheduler->running, 1);
    
    // Allocate actor array
    g_scheduler->actors = malloc(sizeof(Actor*) * g_scheduler->max_actors);
    if (!g_scheduler->actors) {
        free(g_scheduler);
        g_scheduler = NULL;
        return AETHER_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_scheduler->actors_mutex, NULL) != 0) {
        free(g_scheduler->actors);
        free(g_scheduler);
        g_scheduler = NULL;
        return AETHER_ERROR_OUT_OF_MEMORY;
    }
    
    // Allocate worker threads (currently unused - reserved for future work-stealing scheduler)
    // Note: Worker threads are not started here as the current implementation uses
    // per-actor threads instead of a work-stealing pool
    g_scheduler->worker_threads = malloc(sizeof(pthread_t) * g_scheduler->worker_count);
    if (!g_scheduler->worker_threads) {
        pthread_mutex_destroy(&g_scheduler->actors_mutex);
        free(g_scheduler->actors);
        free(g_scheduler);
        g_scheduler = NULL;
        return AETHER_ERROR_OUT_OF_MEMORY;
    }
    
    printf("Aether runtime initialized with %d worker threads (reserved for future use)\n", g_scheduler->worker_count);
    return AETHER_SUCCESS;
}

void aether_runtime_shutdown(void) {
    if (!g_scheduler) {
        printf("[DEBUG] Shutdown: No scheduler to shutdown\n");
        return;
    }
    
    printf("[DEBUG] Shutting down Aether runtime...\n");
    printf("[DEBUG] Actor count: %d\n", g_scheduler->actor_count);
    
    // Stop all actors
    atomic_store(&g_scheduler->running, 0);
    printf("[DEBUG] Set running flag to 0\n");
    
    pthread_mutex_lock(&g_scheduler->actors_mutex);
    printf("[DEBUG] Locked actors mutex\n");
    
    for (int i = 0; i < g_scheduler->actor_count; i++) {
        Actor* actor = g_scheduler->actors[i];
        if (actor) {
            printf("[DEBUG] Stopping actor %s (ID: %llu)\n", actor->name, (unsigned long long)actor->id);
            atomic_store(&actor->should_stop, 1);
        }
    }
    
    pthread_mutex_unlock(&g_scheduler->actors_mutex);
    printf("[DEBUG] Unlocked actors mutex\n");
    
    // Wait for all actors to finish
    printf("[DEBUG] Waiting for %d actors to finish...\n", g_scheduler->actor_count);
    for (int i = 0; i < g_scheduler->actor_count; i++) {
        Actor* actor = g_scheduler->actors[i];
        if (actor && actor->thread) {
            printf("[DEBUG] Joining actor thread %d...\n", i);
            pthread_join(actor->thread, NULL);
            printf("[DEBUG] Actor thread %d joined\n", i);
            destroy_actor(actor);
        }
    }
    printf("[DEBUG] All actor threads finished\n");
    
    // Cleanup scheduler
    printf("[DEBUG] Cleaning up scheduler resources...\n");
    pthread_mutex_destroy(&g_scheduler->actors_mutex);
    free(g_scheduler->actors);
    free(g_scheduler->worker_threads);
    free(g_scheduler);
    g_scheduler = NULL;
    
    printf("Aether runtime shutdown complete\n");
}

// Actor cleanup function
void cleanup_actor_from_scheduler(Actor* actor) {
    if (!g_scheduler || !actor) return;
    
    pthread_mutex_lock(&g_scheduler->actors_mutex);
    
    for (int i = 0; i < g_scheduler->actor_count; i++) {
        if (g_scheduler->actors[i] == actor) {
            // Remove actor from array
            for (int j = i; j < g_scheduler->actor_count - 1; j++) {
                g_scheduler->actors[j] = g_scheduler->actors[j + 1];
            }
            g_scheduler->actor_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_scheduler->actors_mutex);
}

// Runtime statistics
typedef struct {
    int total_actors;
    int running_actors;
    int suspended_actors;
    int terminated_actors;
    int total_messages;
} RuntimeStats;

RuntimeStats aether_get_runtime_stats(void) {
    RuntimeStats stats = {0};
    
    if (!g_scheduler) return stats;
    
    pthread_mutex_lock(&g_scheduler->actors_mutex);
    
    stats.total_actors = g_scheduler->actor_count;
    
    for (int i = 0; i < g_scheduler->actor_count; i++) {
        Actor* actor = g_scheduler->actors[i];
        if (actor) {
            switch (actor->state) {
                case ACTOR_STATE_RUNNING:
                    stats.running_actors++;
                    break;
                case ACTOR_STATE_SUSPENDED:
                    stats.suspended_actors++;
                    break;
                case ACTOR_STATE_TERMINATED:
                    stats.terminated_actors++;
                    break;
                default:
                    break;
            }
            
            if (actor->mailbox) {
                stats.total_messages += atomic_load(&actor->mailbox->message_count);
            }
        }
    }
    
    pthread_mutex_unlock(&g_scheduler->actors_mutex);
    
    return stats;
}

void aether_print_runtime_stats(void) {
    RuntimeStats stats = aether_get_runtime_stats();
    
    printf("\n=== Aether Runtime Statistics ===\n");
    printf("Total actors: %d\n", stats.total_actors);
    printf("Running actors: %d\n", stats.running_actors);
    printf("Suspended actors: %d\n", stats.suspended_actors);
    printf("Terminated actors: %d\n", stats.terminated_actors);
    printf("Total messages in queues: %d\n", stats.total_messages);
    printf("================================\n\n");
}
