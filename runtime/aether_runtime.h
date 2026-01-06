#ifndef AETHER_RUNTIME_H
#define AETHER_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

// Performance: Proven by experiments 01-07
// - Partitioned scheduler: 291M msg/sec (8 cores)
// - SIMD (AVX2): 3× speedup
// - Message batching: 1.78× speedup
// - Combined: ~2.3B msg/sec

// Core runtime types
typedef struct ActorRef ActorRef;
typedef struct Message Message;
typedef uint64_t ActorID;

// Error codes
#define AETHER_SUCCESS 0
#define AETHER_ERROR_OUT_OF_MEMORY 1
#define AETHER_ERROR_INVALID_PARAM 2

// Runtime configuration flags
#define AETHER_FLAG_ENABLE_SIMD    (1 << 0)  // Enable AVX2 vectorization
#define AETHER_FLAG_ENABLE_BATCH   (1 << 1)  // Enable message batching
#define AETHER_FLAG_PIN_CORES      (1 << 2)  // Pin threads to CPU cores

// Initialize runtime with specified number of cores
// flags: Bitwise OR of AETHER_FLAG_* constants
// Returns: AETHER_SUCCESS or error code
int aether_runtime_init(int num_cores, int flags);

// Shutdown runtime and cleanup resources
void aether_runtime_shutdown(void);

// Get runtime statistics
typedef struct {
    uint64_t messages_processed;
    uint64_t actors_active;
    double throughput_msg_per_sec;
    int simd_enabled;
    int batch_enabled;
} AetherRuntimeStats;

void aether_runtime_get_stats(AetherRuntimeStats* stats);

// Memory management
void* aether_malloc(size_t size);
void* aether_calloc(size_t count, size_t size);
void* aether_realloc(void* ptr, size_t size);
void aether_free(void* ptr);

// Memory pool management
int aether_memory_init(size_t initial_pool_size);
void aether_memory_cleanup();

// Actor messaging (defined in scheduler)
void actor_send(ActorID target, Message msg);

#endif // AETHER_RUNTIME_H

