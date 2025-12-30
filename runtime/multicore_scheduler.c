#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "multicore_scheduler.h"

Scheduler schedulers[MAX_CORES];
int num_cores = 0;
atomic_int next_actor_id = 1;

__thread int current_core_id = -1;

void* scheduler_thread(void* arg) {
    Scheduler* sched = (Scheduler*)arg;
    current_core_id = sched->core_id;
    
    while (atomic_load(&sched->running)) {
        void* actor_ptr;
        Message msg;
        
        while (queue_dequeue(&sched->incoming_queue, &actor_ptr, &msg)) {
            ActorBase* actor = (ActorBase*)actor_ptr;
            mailbox_send(&actor->mailbox, msg);
            actor->active = 1;
        }
        
        for (int i = 0; i < sched->actor_count; i++) {
            ActorBase* actor = sched->actors[i];
            if (actor && actor->active) {
                if (actor->step) {
                    actor->step(actor);
                }
            }
        }
    }
    
    return NULL;
}

void scheduler_init(int cores) {
    if (cores <= 0 || cores > MAX_CORES) {
        cores = 4;
    }
    num_cores = cores;
    
    for (int i = 0; i < num_cores; i++) {
        schedulers[i].core_id = i;
        schedulers[i].actors = malloc(MAX_ACTORS_PER_CORE * sizeof(ActorBase*));
        schedulers[i].actor_count = 0;
        schedulers[i].capacity = MAX_ACTORS_PER_CORE;
        queue_init(&schedulers[i].incoming_queue);
        atomic_store(&schedulers[i].running, 0);
    }
}

void scheduler_start() {
    for (int i = 0; i < num_cores; i++) {
        atomic_store(&schedulers[i].running, 1);
        pthread_create(&schedulers[i].thread, NULL, scheduler_thread, &schedulers[i]);
    }
}

void scheduler_stop() {
    for (int i = 0; i < num_cores; i++) {
        atomic_store(&schedulers[i].running, 0);
    }
}

void scheduler_wait() {
    for (int i = 0; i < num_cores; i++) {
        pthread_join(schedulers[i].thread, NULL);
    }
}

int scheduler_register_actor(ActorBase* actor, int preferred_core) {
    if (preferred_core < 0) {
        preferred_core = actor->id % num_cores;
    }
    
    Scheduler* sched = &schedulers[preferred_core];
    
    if (sched->actor_count >= sched->capacity) {
        return -1;
    }
    
    actor->assigned_core = preferred_core;
    sched->actors[sched->actor_count++] = actor;
    
    return preferred_core;
}

void scheduler_send_local(ActorBase* actor, Message msg) {
    mailbox_send(&actor->mailbox, msg);
    actor->active = 1;
}

void scheduler_send_remote(ActorBase* actor, Message msg, int from_core) {
    int target_core = actor->assigned_core;
    queue_enqueue(&schedulers[target_core].incoming_queue, actor, msg);
}
