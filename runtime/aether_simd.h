#ifndef AETHER_SIMD_H
#define AETHER_SIMD_H

#include <stdint.h>

// Structure-of-Arrays layout for SIMD processing
typedef struct {
    int32_t* counters;
    int32_t* states;
    uint8_t* active_flags;
    int capacity;
    int count;
} ActorSoA;

// CPU feature detection
int cpu_supports_avx2(void);

// Actor processing functions
void actor_step_simd(ActorSoA* actors, int start_idx, int count);
void actor_step_scalar(ActorSoA* actors, int start_idx, int count);
void actor_step_auto(ActorSoA* actors, int start_idx, int count);

// SoA management
ActorSoA* actor_soa_create(int capacity);
void actor_soa_destroy(ActorSoA* soa);

// Info
void print_simd_info(void);

#endif // AETHER_SIMD_H
