#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <immintrin.h>  // AVX2/AVX-512 intrinsics

// Configuration
#define ALIGNMENT 32  // AVX2 requires 32-byte alignment

// Actor structure (scalar)
typedef struct {
    int counter;
    int active;
} Actor;

// Actor structure (SoA - Structure of Arrays)
typedef struct {
    int* counters;
    int* active_flags;
    int count;
} ActorsSoA;

// Initialize scalar actors
Actor* create_actors_scalar(int count) {
    Actor* actors = (Actor*)_aligned_malloc(sizeof(Actor) * count, ALIGNMENT);
    for (int i = 0; i < count; i++) {
        actors[i].counter = 0;
        actors[i].active = 1;
    }
    return actors;
}

// Initialize SoA actors
ActorsSoA create_actors_soa(int count) {
    ActorsSoA actors;
    actors.count = count;
    actors.counters = (int*)_aligned_malloc(sizeof(int) * count, ALIGNMENT);
    actors.active_flags = (int*)_aligned_malloc(sizeof(int) * count, ALIGNMENT);
    
    for (int i = 0; i < count; i++) {
        actors.counters[i] = 0;
        actors.active_flags[i] = 1;
    }
    
    return actors;
}

// Free SoA actors
void free_actors_soa(ActorsSoA* actors) {
    _aligned_free(actors->counters);
    _aligned_free(actors->active_flags);
}

// Scalar baseline: Process actors one-at-a-time
void process_scalar(Actor* actors, int count, int messages_per_actor) {
    for (int m = 0; m < messages_per_actor; m++) {
        for (int i = 0; i < count; i++) {
            actors[i].counter += 1;
        }
    }
}

// AVX2: Process 8 actors at once
void process_avx2(ActorsSoA* actors, int messages_per_actor) {
    int count = actors->count;
    int* counters = actors->counters;
    
    // SIMD constant (payload = 1)
    __m256i payload = _mm256_set1_epi32(1);
    
    // Process 8 actors at a time
    int simd_count = (count / 8) * 8;
    
    for (int m = 0; m < messages_per_actor; m++) {
        for (int i = 0; i < simd_count; i += 8) {
            // Load 8 counters
            __m256i vec = _mm256_load_si256((__m256i*)&counters[i]);
            
            // Add payload to all 8 counters
            vec = _mm256_add_epi32(vec, payload);
            
            // Store result
            _mm256_store_si256((__m256i*)&counters[i], vec);
        }
    }
    
    // Handle remainder (not multiple of 8)
    for (int i = simd_count; i < count; i++) {
        for (int m = 0; m < messages_per_actor; m++) {
            counters[i] += 1;
        }
    }
}

#ifdef __AVX512F__
// AVX-512: Process 16 actors at once
void process_avx512(ActorsSoA* actors, int messages_per_actor) {
    int count = actors->count;
    int* counters = actors->counters;
    
    // SIMD constant (payload = 1)
    __m512i payload = _mm512_set1_epi32(1);
    
    // Process 16 actors at a time
    int simd_count = (count / 16) * 16;
    
    for (int m = 0; m < messages_per_actor; m++) {
        for (int i = 0; i < simd_count; i += 16) {
            // Load 16 counters
            __m512i vec = _mm512_load_si512((__m512i*)&counters[i]);
            
            // Add payload to all 16 counters
            vec = _mm512_add_epi32(vec, payload);
            
            // Store result
            _mm512_store_si512((__m512i*)&counters[i], vec);
        }
    }
    
    // Handle remainder
    for (int i = simd_count; i < count; i++) {
        for (int m = 0; m < messages_per_actor; m++) {
            counters[i] += 1;
        }
    }
}
#endif

// Verify correctness
int verify_scalar(Actor* actors, int count, int expected) {
    for (int i = 0; i < count; i++) {
        if (actors[i].counter != expected) {
            return 0;
        }
    }
    return 1;
}

int verify_soa(ActorsSoA* actors, int expected) {
    for (int i = 0; i < actors->count; i++) {
        if (actors->counters[i] != expected) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char* argv[]) {
    int num_actors = 10000;
    int messages_per_actor = 1000;  // Increased for better timing
    
    if (argc >= 2) num_actors = atoi(argv[1]);
    if (argc >= 3) messages_per_actor = atoi(argv[2]);
    
    // Round up to multiple of 16 for clean SIMD
    num_actors = ((num_actors + 15) / 16) * 16;
    
    printf("=== SIMD Vectorization Benchmark ===\n");
    printf("Actors: %d\n", num_actors);
    printf("Messages per actor: %d\n", messages_per_actor);
    printf("Total messages: %d\n\n", num_actors * messages_per_actor);
    
    long long total_messages = (long long)num_actors * messages_per_actor;
    
    // Test 1: Scalar baseline
    printf("Test 1: Scalar (baseline)\n");
    Actor* actors_scalar = create_actors_scalar(num_actors);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    process_scalar(actors_scalar, num_actors, messages_per_actor);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_scalar = (end.tv_sec - start.tv_sec) + 
                         (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_scalar = total_messages / time_scalar / 1e6;
    
    if (!verify_scalar(actors_scalar, num_actors, messages_per_actor)) {
        printf("  ERROR: Verification failed!\n");
    } else {
        printf("  Time:       %.4f seconds\n", time_scalar);
        printf("  Throughput: %.2f M msg/sec\n", throughput_scalar);
        printf("  Status:     PASS\n\n");
    }
    
    _aligned_free(actors_scalar);
    
    // Test 2: AVX2 (256-bit, 8 elements)
    printf("Test 2: AVX2 (8 actors per instruction)\n");
    ActorsSoA actors_avx2 = create_actors_soa(num_actors);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    process_avx2(&actors_avx2, messages_per_actor);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_avx2 = (end.tv_sec - start.tv_sec) + 
                       (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_avx2 = total_messages / time_avx2 / 1e6;
    double speedup_avx2 = time_scalar / time_avx2;
    
    if (!verify_soa(&actors_avx2, messages_per_actor)) {
        printf("  ERROR: Verification failed!\n");
    } else {
        printf("  Time:       %.4f seconds\n", time_avx2);
        printf("  Throughput: %.2f M msg/sec\n", throughput_avx2);
        printf("  Speedup:    %.2fx vs scalar\n", speedup_avx2);
        printf("  Status:     PASS\n\n");
    }
    
    free_actors_soa(&actors_avx2);
    
#ifdef __AVX512F__
    // Test 3: AVX-512 (512-bit, 16 elements)
    printf("Test 3: AVX-512 (16 actors per instruction)\n");
    ActorsSoA actors_avx512 = create_actors_soa(num_actors);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    process_avx512(&actors_avx512, messages_per_actor);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_avx512 = (end.tv_sec - start.tv_sec) + 
                         (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_avx512 = total_messages / time_avx512 / 1e6;
    double speedup_avx512 = time_scalar / time_avx512;
    
    if (!verify_soa(&actors_avx512, messages_per_actor)) {
        printf("  ERROR: Verification failed!\n");
    } else {
        printf("  Time:       %.4f seconds\n", time_avx512);
        printf("  Throughput: %.2f M msg/sec\n", throughput_avx512);
        printf("  Speedup:    %.2fx vs scalar\n", speedup_avx512);
        printf("  Status:     PASS\n\n");
    }
    
    free_actors_soa(&actors_avx512);
#else
    printf("Test 3: AVX-512 not available (CPU doesn't support it)\n\n");
#endif
    
    // Summary
    printf("=== Summary ===\n");
    printf("Scalar:  %.2f M msg/sec (baseline)\n", throughput_scalar);
    printf("AVX2:    %.2f M msg/sec (%.2fx faster)\n", throughput_avx2, speedup_avx2);
#ifdef __AVX512F__
    printf("AVX-512: %.2f M msg/sec (%.2fx faster)\n", throughput_avx512, speedup_avx512);
#endif
    
    return 0;
}
