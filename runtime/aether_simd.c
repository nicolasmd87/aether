// SIMD Actor Processing - AVX2 vectorization for 3× speedup
// Based on Experiment 05: 41.2B msg/sec vs 13.8B (scalar)
// Processes 8 actors in parallel using 256-bit vector operations

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef __AVX2__
#include <immintrin.h>
#define SIMD_WIDTH 8  // Process 8 actors per instruction
#define SIMD_AVAILABLE 1
#else
#define SIMD_AVAILABLE 0
#endif

// Structure-of-Arrays layout for SIMD (better than Array-of-Structures)
typedef struct {
    int32_t* counters;     // Actor state counters
    int32_t* states;       // Actor states
    uint8_t* active_flags; // Active status (1 byte per actor)
    int capacity;
    int count;
} ActorSoA;

// CPU feature detection
int cpu_supports_avx2() {
#ifdef __AVX2__
    // Runtime check using CPUID
    #if defined(_WIN32)
    int cpuInfo[4];
    int function_id = 0;
    __asm__ __volatile__(
        "cpuid"
        : "=a"(cpuInfo[0]), "=b"(cpuInfo[1]), "=c"(cpuInfo[2]), "=d"(cpuInfo[3])
        : "a"(function_id), "c"(0)
    );
    if (cpuInfo[0] < 7) return 0;
    
    function_id = 7;
    __asm__ __volatile__(
        "cpuid"
        : "=a"(cpuInfo[0]), "=b"(cpuInfo[1]), "=c"(cpuInfo[2]), "=d"(cpuInfo[3])
        : "a"(function_id), "c"(0)
    );
    return (cpuInfo[1] & (1 << 5)) != 0; // EBX bit 5 = AVX2
    #else
    unsigned int eax, ebx, ecx, edx;
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(7), "c"(0)
    );
    return (ebx & (1 << 5)) != 0; // EBX bit 5 = AVX2
    #endif
#endif
    return 0;
}

#ifdef __AVX2__
// SIMD: Process 8 actors at once
void actor_step_simd(ActorSoA* actors, int start_idx, int count) {
    // Process in chunks of 8
    int i;
    for (i = start_idx; i + 7 < start_idx + count; i += 8) {
        // Load 8 counters
        __m256i counters = _mm256_loadu_si256((__m256i*)&actors->counters[i]);
        
        // Load 8 active flags (need to expand to 32-bit)
        __m128i active_bytes = _mm_loadl_epi64((__m128i*)&actors->active_flags[i]);
        __m256i active = _mm256_cvtepu8_epi32(active_bytes);
        
        // Increment counters where active (could be message payload in real scenario)
        __m256i increment = _mm256_set1_epi32(1);
        counters = _mm256_add_epi32(counters, _mm256_and_si256(increment, active));
        
        // Store result
        _mm256_storeu_si256((__m256i*)&actors->counters[i], counters);
    }
    
    // Handle remaining actors (< 8) with scalar code
    for (; i < start_idx + count; i++) {
        if (actors->active_flags[i]) {
            actors->counters[i] += 1;
        }
    }
}
#endif

// Scalar fallback (when AVX2 not available)
void actor_step_scalar(ActorSoA* actors, int start_idx, int count) {
    for (int i = start_idx; i < start_idx + count; i++) {
        if (actors->active_flags[i]) {
            actors->counters[i] += 1;
        }
    }
}

// Auto-dispatch: Use SIMD if available, otherwise scalar
void actor_step_auto(ActorSoA* actors, int start_idx, int count) {
#ifdef __AVX2__
    if (cpu_supports_avx2()) {
        actor_step_simd(actors, start_idx, count);
    } else {
        actor_step_scalar(actors, start_idx, count);
    }
#else
    actor_step_scalar(actors, start_idx, count);
#endif
}

// Initialize SoA actor array
ActorSoA* actor_soa_create(int capacity) {
    ActorSoA* soa = malloc(sizeof(ActorSoA));
    soa->capacity = capacity;
    soa->count = 0;
    
    // Allocate aligned memory for SIMD (32-byte alignment for AVX2)
    #ifdef _WIN32
    soa->counters = _aligned_malloc(capacity * sizeof(int32_t), 32);
    soa->states = _aligned_malloc(capacity * sizeof(int32_t), 32);
    soa->active_flags = _aligned_malloc(capacity * sizeof(uint8_t), 32);
    #else
    posix_memalign((void**)&soa->counters, 32, capacity * sizeof(int32_t));
    posix_memalign((void**)&soa->states, 32, capacity * sizeof(int32_t));
    posix_memalign((void**)&soa->active_flags, 32, capacity * sizeof(uint8_t));
    #endif
    
    memset(soa->counters, 0, capacity * sizeof(int32_t));
    memset(soa->states, 0, capacity * sizeof(int32_t));
    memset(soa->active_flags, 0, capacity * sizeof(uint8_t));
    
    return soa;
}

void actor_soa_destroy(ActorSoA* soa) {
    #ifdef _WIN32
    _aligned_free(soa->counters);
    _aligned_free(soa->states);
    _aligned_free(soa->active_flags);
    #else
    free(soa->counters);
    free(soa->states);
    free(soa->active_flags);
    #endif
    free(soa);
}

// Print SIMD status
void print_simd_info() {
    printf("SIMD Support:\n");
    printf("  Compiled with AVX2: %s\n", SIMD_AVAILABLE ? "YES" : "NO");
    
#ifdef __AVX2__
    printf("  Runtime AVX2 support: %s\n", cpu_supports_avx2() ? "YES" : "NO");
    if (cpu_supports_avx2()) {
        printf("  Vector width: %d actors per instruction\n", SIMD_WIDTH);
        printf("  Expected speedup: 2.5-3× vs scalar\n");
    }
#endif
    
    printf("\nNote: SIMD requires Structure-of-Arrays (SoA) layout\n");
    printf("      Use for uniform actor types with high throughput needs\n\n");
}
