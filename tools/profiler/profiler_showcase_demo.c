// Comprehensive Profiler Showcase in C
// Demonstrates memory tracking, actor simulation, and profiler integration

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "profiler_server.h"
#include "../../runtime/aether_memory_stats.h"
#include "../../runtime/aether_arena.h"

#ifdef _WIN32
    #include <windows.h>
    #define sleep(n) Sleep((n) * 1000)
    #define usleep(n) Sleep((n) / 1000)
#else
    #include <unistd.h>
#endif

// Simulate worker actors
typedef struct {
    int id;
    int tasks_processed;
    Arena* arena;
} Worker;

// Simulate coordinator
typedef struct {
    Worker* workers[10];
    int worker_count;
    int total_tasks_distributed;
} Coordinator;

// Create a worker
Worker* create_worker(int id) {
    Worker* w = (Worker*)malloc(sizeof(Worker));
    w->id = id;
    w->tasks_processed = 0;
    w->arena = arena_create(1024 * 16); // 16KB per worker
    
    memory_stats_record_alloc(sizeof(Worker));
    
    // Record spawn event
    ProfilerEvent event = {
        .type = PROF_EVENT_ACTOR_SPAWN,
        .timestamp_ms = profiler_get_time_ms(),
        .actor_id = id,
        .target_actor_id = 0,
        .message_type = 0,
        .message_payload = 0,
        .memory_bytes = sizeof(Worker),
        .thread_id = 0
    };
    snprintf(event.custom_data, sizeof(event.custom_data), 
            "Worker %d spawned", id);
    profiler_record_event(&event);
    
    printf("  [SPAWN] Worker %d created\n", id);
    return w;
}

// Process task in worker
void worker_process_task(Worker* w, int task_id) {
    w->tasks_processed++;
    
    // Simulate work with memory allocations
    size_t alloc_size = 64 + (task_id % 256);
    void* data = arena_alloc(w->arena, alloc_size);
    memory_stats_record_alloc(alloc_size);
    
    // Record message received
    ProfilerEvent recv_event = {
        .type = PROF_EVENT_ACTOR_MESSAGE_RECEIVED,
        .timestamp_ms = profiler_get_time_ms(),
        .actor_id = w->id,
        .target_actor_id = 0,
        .message_type = 1, // "process"
        .message_payload = task_id,
        .memory_bytes = 0,
        .thread_id = 0
    };
    profiler_record_event(&recv_event);
    
    // Simulate processing time
    usleep(10000 + (task_id % 20000)); // 10-30ms
    
    // Record message processed
    ProfilerEvent proc_event = {
        .type = PROF_EVENT_ACTOR_MESSAGE_PROCESSED,
        .timestamp_ms = profiler_get_time_ms(),
        .actor_id = w->id,
        .target_actor_id = 0,
        .message_type = 1,
        .message_payload = task_id,
        .memory_bytes = alloc_size,
        .thread_id = 0
    };
    profiler_record_event(&proc_event);
    
    printf("  [TASK] Worker %d processed task #%d (allocated %zu bytes)\n", 
           w->id, w->tasks_processed, alloc_size);
}

// Send message to worker
void send_to_worker(Coordinator* coord, int worker_idx, int msg_type, int payload) {
    if (worker_idx >= coord->worker_count) return;
    
    Worker* w = coord->workers[worker_idx];
    
    // Record message sent
    ProfilerEvent event = {
        .type = PROF_EVENT_ACTOR_MESSAGE_SENT,
        .timestamp_ms = profiler_get_time_ms(),
        .actor_id = 0, // Coordinator
        .target_actor_id = w->id,
        .message_type = msg_type,
        .message_payload = payload,
        .memory_bytes = 0,
        .thread_id = 0
    };
    profiler_record_event(&event);
    
    // Process message
    worker_process_task(w, payload);
}

// Main showcase
int main() {
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║   Aether Profiler Showcase - Live Demo        ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    // Initialize memory tracking
    memory_stats_init();
    
    // Initialize profiler
    ProfilerConfig config = {
        .enabled = 1,
        .port = 8081,
        .bind_address = "0.0.0.0",
        .max_events = 10000,
        .collection_interval_ms = 100
    };
    
    profiler_init(&config);
    profiler_start_server();
    
    printf("\n┌─────────────────────────────────────────────┐\n");
    printf("│ 🌐 OPEN YOUR BROWSER:                      │\n");
    printf("│    http://localhost:8081                    │\n");
    printf("│                                             │\n");
    printf("│ Watch real-time updates as the demo runs!  │\n");
    printf("└─────────────────────────────────────────────┘\n\n");
    
    sleep(3); // Give time to open browser
    
    // Create coordinator
    Coordinator coord = {
        .worker_count = 0,
        .total_tasks_distributed = 0
    };
    
    // Phase 1: Spawn workers
    printf("📦 Phase 1: Creating Workers\n");
    printf("─────────────────────────────────────\n");
    for (int i = 0; i < 5; i++) {
        coord.workers[coord.worker_count++] = create_worker(i + 1);
        usleep(200000); // 200ms between spawns
    }
    
    printf("\n✅ %d workers created\n", coord.worker_count);
    sleep(2);
    
    // Phase 2: Distribute work in rounds
    printf("\n📨 Phase 2: Work Distribution\n");
    printf("─────────────────────────────────────\n");
    for (int round = 0; round < 10; round++) {
        printf("\n  Round %d: Distributing tasks...\n", round + 1);
        
        for (int i = 0; i < coord.worker_count; i++) {
            int task_id = coord.total_tasks_distributed++;
            send_to_worker(&coord, i, 1, task_id);
        }
        
        printf("  Round %d: %d tasks dispatched\n", 
               round + 1, coord.worker_count);
        sleep(1); // 1 second between rounds
    }
    
    printf("\n✅ %d total tasks distributed\n", coord.total_tasks_distributed);
    sleep(2);
    
    // Phase 3: Status report
    printf("\n📊 Phase 3: Status Report\n");
    printf("─────────────────────────────────────\n");
    for (int i = 0; i < coord.worker_count; i++) {
        Worker* w = coord.workers[i];
        printf("  Worker %d: %d tasks processed\n", 
               w->id, w->tasks_processed);
    }
    
    // Show memory stats
    printf("\n💾 Memory Statistics:\n");
    memory_stats_print();
    
    // Keep running for observation
    printf("\n⏳ Keeping server running for 20 seconds...\n");
    printf("   Check the profiler dashboard for:\n");
    printf("   • Memory usage graphs\n");
    printf("   • Actor message flow\n");
    printf("   • Event timeline\n");
    printf("   • Performance metrics\n\n");
    
    for (int i = 20; i > 0; i--) {
        printf("\r   Time remaining: %2d seconds... ", i);
        fflush(stdout);
        sleep(1);
    }
    
    printf("\n\n");
    
    // Cleanup
    printf("🧹 Cleaning up...\n");
    for (int i = 0; i < coord.worker_count; i++) {
        Worker* w = coord.workers[i];
        arena_destroy(w->arena);
        memory_stats_record_free(sizeof(Worker));
        free(w);
    }
    
    profiler_shutdown();
    
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║   Demo Complete!                               ║\n");
    printf("║                                                ║\n");
    printf("║   Final Stats:                                 ║\n");
    printf("║   • Workers: %d                                 ║\n", coord.worker_count);
    printf("║   • Tasks: %d                                  ║\n", coord.total_tasks_distributed);
    printf("║   • Events Recorded: %d+                       ║\n", coord.total_tasks_distributed * 3);
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    return 0;
}

