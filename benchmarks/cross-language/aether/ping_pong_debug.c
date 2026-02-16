#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <unistd.h>

void aether_args_init(int argc, char** argv);

#include <stdatomic.h>

// Aether runtime libraries
#include "actor_state_machine.h"
#include "aether_send_message.h"
#include "aether_actor_thread.h"
#include "multicore_scheduler.h"
#include "aether_cpu_detect.h"
#include "aether_optimization_config.h"
#include "aether_supervision.h"
#include "aether_tracing.h"
#include "aether_bounds_check.h"
#include "aether_runtime_types.h"

extern __thread int current_core_id;

// Benchmark timing function
static inline uint64_t rdtsc() {
#if defined(__x86_64__) || defined(__i386__)
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__) || defined(__arm__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#else
    return 0;
#endif
}

// Message: Ping (1 fields)
typedef struct Ping {
    int _message_id;
    int count;
} Ping;

// Type-specific memory pool for Ping
// DECLARE_TYPE_POOL(Ping)
// DECLARE_TLS_POOL(Ping)

// Message: Pong (1 fields)
typedef struct Pong {
    int _message_id;
    int count;
} Pong;

// Type-specific memory pool for Pong
// DECLARE_TYPE_POOL(Pong)
// DECLARE_TLS_POOL(Pong)

typedef struct __attribute__((aligned(64))) PingActor {
    int active;              // Hot: checked every loop iteration
    int id;                  // Hot: used for identification
    Mailbox mailbox;         // Hot: message queue
    void (*step)(void*);     // Hot: message handler
    pthread_t thread;        // Warm: thread handle
    int auto_process;        // Warm: auto-processing flag
    int assigned_core;       // Cold: core assignment
    SPSCQueue spsc_queue;    // Lock-free same-core messaging
    
    void* pong_ref;
    atomic_int received;
    atomic_int target;
    atomic_int errors;
} PingActor;

static __attribute__((hot)) void PingActor_handle_Pong(PingActor* self, void* _msg_data) {
    Pong* _pattern = (Pong*)_msg_data;
    int count = _pattern->count;
if (count != self->received) {
        {
self->errors = (self->errors + 1);
        }
    }
self->received = (self->received + 1);
if (self->received < self->target) {
        {
{ Message _imsg = {0, 0, self->received, NULL, {NULL, 0, 0}}; if (current_core_id >= 0 && current_core_id == ((ActorBase*)(self->pong_ref))->assigned_core) { scheduler_send_local((ActorBase*)(self->pong_ref), _imsg); } else { scheduler_send_remote((ActorBase*)(self->pong_ref), _imsg, current_core_id); } };
        }
    }
}

typedef void (*PingActor_MessageHandler)(PingActor*, void*);
static PingActor_MessageHandler PingActor_handlers[256] = {0};
static int PingActor_handlers_initialized = 0;

static void PingActor_init_handlers(PingActor* self) {
    if (PingActor_handlers_initialized) return;
    PingActor_handlers[1] = PingActor_handle_Pong;
    PingActor_handlers_initialized = 1;
}

void PingActor_step(PingActor* self) {
    Message msg;
    
    // Likely path: mailbox has message
    if (__builtin_expect(!mailbox_receive(&self->mailbox, &msg), 0)) {
        self->active = 0;
        return;
    }
    
    // COMPUTED GOTO DISPATCH - 15-30% faster than function pointers
    // Used by CPython, LuaJIT for ultra-fast message dispatch
    void* _msg_data = msg.payload_ptr;
    int _msg_id = msg.type;  // Already set by aether_send_message, avoids pointer dereference
    
    // Dispatch table: direct jumps to labels (no indirect call overhead)
    static void* dispatch_table[256] = {
        [1] = &&handle_Pong,
    };
    
    // Bounds check with likely hint (message IDs are usually valid)
    if (__builtin_expect(_msg_id >= 0 && _msg_id < 256 && dispatch_table[_msg_id], 1)) {
        goto *dispatch_table[_msg_id];  // Direct jump - zero overhead
    }
    return;  // Unknown message type
    
    handle_Pong:
        if (_msg_data) {
            PingActor_handle_Pong(self, _msg_data);
            aether_free_message(_msg_data);
        } else {
            Pong _inline = { ._message_id = msg.type, .count = msg.payload_int };
            PingActor_handle_Pong(self, &_inline);
        }
        return;
    
}

PingActor* spawn_PingActor() {
    int core = atomic_fetch_add(&next_actor_id, 1) % num_cores;
    PingActor* actor = (PingActor*)scheduler_spawn_pooled(core, (void (*)(void*))PingActor_step, sizeof(PingActor));
    if (!actor) {
        // Fallback to aligned allocation if pool exhausted
        actor = aligned_alloc(64, sizeof(PingActor));
        if (!actor) return NULL;
        actor->id = atomic_fetch_add(&next_actor_id, 1);
        actor->assigned_core = -1;
        actor->step = (void (*)(void*))PingActor_step;
        mailbox_init(&actor->mailbox);
        scheduler_register_actor((ActorBase*)actor, -1);
    }
    actor->active = 1;
    actor->auto_process = 0;
    
    actor->pong_ref = 0;
    actor->received = 0;
    actor->target = 0;
    actor->errors = 0;
    
    if (actor->auto_process) {
        pthread_create(&actor->thread, NULL, (void*(*)(void*))aether_actor_thread, actor);
    }
    
    return actor;
}

void send_PingActor(PingActor* actor, int type, int payload) {
    Message msg = {type, 0, payload, NULL};
    if (actor->assigned_core == current_core_id) {
        scheduler_send_local((ActorBase*)actor, msg);
    } else {
        scheduler_send_remote((ActorBase*)actor, msg, current_core_id);
    }
}

typedef struct __attribute__((aligned(64))) PongActor {
    int active;              // Hot: checked every loop iteration
    int id;                  // Hot: used for identification
    Mailbox mailbox;         // Hot: message queue
    void (*step)(void*);     // Hot: message handler
    pthread_t thread;        // Warm: thread handle
    int auto_process;        // Warm: auto-processing flag
    int assigned_core;       // Cold: core assignment
    SPSCQueue spsc_queue;    // Lock-free same-core messaging
    
    void* ping_ref;
    atomic_int expected;
} PongActor;

static __attribute__((hot)) void PongActor_handle_Ping(PongActor* self, void* _msg_data) {
    Ping* _pattern = (Ping*)_msg_data;
    int count = _pattern->count;
if (count != self->expected) {
        {
        }
    }
self->expected = (self->expected + 1);
{ Message _imsg = {1, 0, count, NULL, {NULL, 0, 0}}; if (current_core_id >= 0 && current_core_id == ((ActorBase*)(self->ping_ref))->assigned_core) { scheduler_send_local((ActorBase*)(self->ping_ref), _imsg); } else { scheduler_send_remote((ActorBase*)(self->ping_ref), _imsg, current_core_id); } };
}

typedef void (*PongActor_MessageHandler)(PongActor*, void*);
static PongActor_MessageHandler PongActor_handlers[256] = {0};
static int PongActor_handlers_initialized = 0;

static void PongActor_init_handlers(PongActor* self) {
    if (PongActor_handlers_initialized) return;
    PongActor_handlers[0] = PongActor_handle_Ping;
    PongActor_handlers_initialized = 1;
}

void PongActor_step(PongActor* self) {
    Message msg;
    
    // Likely path: mailbox has message
    if (__builtin_expect(!mailbox_receive(&self->mailbox, &msg), 0)) {
        self->active = 0;
        return;
    }
    
    // COMPUTED GOTO DISPATCH - 15-30% faster than function pointers
    // Used by CPython, LuaJIT for ultra-fast message dispatch
    void* _msg_data = msg.payload_ptr;
    int _msg_id = msg.type;  // Already set by aether_send_message, avoids pointer dereference
    
    // Dispatch table: direct jumps to labels (no indirect call overhead)
    static void* dispatch_table[256] = {
        [0] = &&handle_Ping,
    };
    
    // Bounds check with likely hint (message IDs are usually valid)
    if (__builtin_expect(_msg_id >= 0 && _msg_id < 256 && dispatch_table[_msg_id], 1)) {
        goto *dispatch_table[_msg_id];  // Direct jump - zero overhead
    }
    return;  // Unknown message type
    
    handle_Ping:
        if (_msg_data) {
            PongActor_handle_Ping(self, _msg_data);
            aether_free_message(_msg_data);
        } else {
            Ping _inline = { ._message_id = msg.type, .count = msg.payload_int };
            PongActor_handle_Ping(self, &_inline);
        }
        return;
    
}

PongActor* spawn_PongActor() {
    int core = atomic_fetch_add(&next_actor_id, 1) % num_cores;
    PongActor* actor = (PongActor*)scheduler_spawn_pooled(core, (void (*)(void*))PongActor_step, sizeof(PongActor));
    if (!actor) {
        // Fallback to aligned allocation if pool exhausted
        actor = aligned_alloc(64, sizeof(PongActor));
        if (!actor) return NULL;
        actor->id = atomic_fetch_add(&next_actor_id, 1);
        actor->assigned_core = -1;
        actor->step = (void (*)(void*))PongActor_step;
        mailbox_init(&actor->mailbox);
        scheduler_register_actor((ActorBase*)actor, -1);
    }
    actor->active = 1;
    actor->auto_process = 0;
    
    actor->ping_ref = 0;
    actor->expected = 0;
    
    if (actor->auto_process) {
        pthread_create(&actor->thread, NULL, (void*(*)(void*))aether_actor_thread, actor);
    }
    
    return actor;
}

void send_PongActor(PongActor* actor, int type, int payload) {
    Message msg = {type, 0, payload, NULL};
    if (actor->assigned_core == current_core_id) {
        scheduler_send_local((ActorBase*)actor, msg);
    } else {
        scheduler_send_remote((ActorBase*)actor, msg, current_core_id);
    }
}

int main(int argc, char** argv) {
    aether_args_init(argc, argv);
    
    uint64_t _bench_start = rdtsc();
    
    // Initialize Aether runtime with auto-detected optimizations
    // TIER 1 (always-on): Actor pooling, Direct send, Adaptive batching
    // TIER 2 (auto-detect): SIMD (if AVX2/NEON), MWAIT (if supported)
    int num_cores = cpu_recommend_cores();
    scheduler_init(num_cores);  // Auto-detects hardware capabilities
    
    #ifdef AETHER_VERBOSE
    aether_print_config();
    #endif
    
    scheduler_start();
    current_core_id = -1;  // Main thread is not a scheduler thread
    
    {
int total_messages = 100000;
const char* env_val = getenv("BENCHMARK_MESSAGES");
if (env_val != 0) {
            {
total_messages = atoi(env_val);
            }
        }
PingActor* ping = spawn_PingActor();
PongActor* pong = spawn_PongActor();
(ping->pong_ref = pong);
(ping->target = total_messages);
(pong->ping_ref = ping);
{ Message _imsg = {0, 0, 0, NULL, {NULL, 0, 0}}; if (current_core_id >= 0 && current_core_id == ((ActorBase*)(pong))->assigned_core) { scheduler_send_local((ActorBase*)(pong), _imsg); } else { scheduler_send_remote((ActorBase*)(pong), _imsg, current_core_id); } };
scheduler_wait();
if (atomic_load(&ping->errors) > 0) {
            {
printf("VALIDATION FAILED: ");
printf("%d", atomic_load(&ping->errors));
printf(" errors\n");
            }
        }
if (atomic_load(&ping->received) < total_messages) {
            {
printf("INCOMPLETE: ");
printf("%d", atomic_load(&ping->received));
printf("/");
printf("%d", total_messages);
printf(" messages\n");
            }
        }
    }
    
    // Wait for actors to complete and clean up
    scheduler_wait();
    
    // Output benchmark results
    uint64_t _bench_end = rdtsc();
    uint64_t _bench_cycles = _bench_end - _bench_start;
    #if defined(__x86_64__) || defined(__i386__)
    double cycles_per_msg = (double)_bench_cycles / (10000000 * 2);
    double cpu_freq_ghz = 3.0;  // Approximate CPU frequency
    double seconds = cycles_per_msg * (10000000 * 2) / (cpu_freq_ghz * 1e9);
    double throughput = (2.0 * 10000000) / seconds;
    printf("\nCycles/msg:     %.2f\n", cycles_per_msg);
    printf("Throughput:     %.2f M msg/sec\n", throughput / 1e6);
    #elif defined(__aarch64__) || defined(__arm__)
    double seconds = _bench_cycles / 1e9;
    double throughput = (2.0 * 10000000) / seconds;
    double cycles_per_msg = _bench_cycles / (10000000 * 2.0);
    printf("\nCycles/msg:     %.2f\n", cycles_per_msg);
    printf("Throughput:     %.2f M msg/sec\n", throughput / 1e6);
    #endif
    
    // Message pool statistics
    {
        uint64_t pool_hits = 0, pool_misses = 0, too_large = 0;
        aether_message_pool_stats(&pool_hits, &pool_misses, &too_large);
        if (pool_hits + pool_misses + too_large > 0) {
            printf("\n=== Message Pool Statistics ===\n");
            printf("Pool hits:      %llu\n", (unsigned long long)pool_hits);
            printf("Pool misses:    %llu (exhausted)\n", (unsigned long long)pool_misses);
            printf("Too large:      %llu (>256 bytes)\n", (unsigned long long)too_large);
            uint64_t total = pool_hits + pool_misses + too_large;
            double hit_rate = (double)pool_hits / total * 100.0;
            printf("Hit rate:       %.1f%%\n", hit_rate);
        }
    }
    
    return 0;
}
