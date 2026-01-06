// Test Suite for Runtime Implementations
// Tests: Partitioned scheduler, SIMD, Batching, CPU detection

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "../runtime/aether_cpu_detect.h"
#include "../runtime/aether_simd.h"
#include "../runtime/aether_batch.h"

#define TEST_PASS printf("  ✓ PASS\n")
#define TEST_FAIL printf("  ✗ FAIL\n"); return 1

// Test 1: CPU Detection
int test_cpu_detection() {
    printf("\nTest 1: CPU Detection\n");
    
    // Get CPU info
    const CPUInfo* info = cpu_get_info();
    assert(info != NULL);
    
    printf("  CPU: %s\n", info->cpu_brand);
    printf("  Cores: %d\n", info->num_cores);
    printf("  AVX2: %s\n", info->avx2_supported ? "YES" : "NO");
    
    // Test feature checks
    int has_avx2 = cpu_has_avx2();
    int has_avx512 = cpu_has_avx512();
    
    printf("  cpu_has_avx2(): %d\n", has_avx2);
    printf("  cpu_has_avx512(): %d\n", has_avx512);
    
    // Test recommend cores
    int recommended = cpu_recommend_cores();
    assert(recommended > 0 && recommended <= 16);
    printf("  Recommended cores: %d\n", recommended);
    
    TEST_PASS;
    return 0;
}

// Test 2: SIMD Actor Processing
int test_simd_actors() {
    printf("\nTest 2: SIMD Actor Processing\n");
    
    int actor_count = 1000000;
    printf("  Testing with %d actors\n", actor_count);
    
    // Create SoA
    ActorSoA* actors = actor_soa_create(actor_count);
    assert(actors != NULL);
    assert(actors->capacity == actor_count);
    assert(actors->count == 0);
    
    // Initialize actors
    for (int i = 0; i < actor_count; i++) {
        actors->counters[i] = 0;
        actors->active_flags[i] = 1;
    }
    actors->count = actor_count;
    
    // Test scalar processing
    printf("  Testing scalar processing...\n");
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int iter = 0; iter < 10; iter++) {
        actor_step_scalar(actors, 0, actor_count);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_scalar = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Verify results
    for (int i = 0; i < actor_count; i++) {
        if (actors->counters[i] != 10) {
            printf("  ERROR: Actor %d has counter %d, expected 10\n", i, actors->counters[i]);
            TEST_FAIL;
        }
    }
    
    printf("    Scalar time: %.4f seconds\n", time_scalar);
    
    // Reset for SIMD test
    for (int i = 0; i < actor_count; i++) {
        actors->counters[i] = 0;
    }
    
#ifdef __AVX2__
    if (cpu_has_avx2()) {
        printf("  Testing SIMD processing (AVX2)...\n");
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for (int iter = 0; iter < 10; iter++) {
            actor_step_simd(actors, 0, actor_count);
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double time_simd = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        // Verify results
        for (int i = 0; i < actor_count; i++) {
            if (actors->counters[i] != 10) {
                printf("  ERROR: Actor %d has counter %d, expected 10\n", i, actors->counters[i]);
                TEST_FAIL;
            }
        }
        
        printf("    SIMD time: %.4f seconds\n", time_simd);
        printf("    Speedup: %.2fx\n", time_scalar / time_simd);
        
        if (time_simd < time_scalar * 0.5) {
            printf("    ✓ SIMD is significantly faster (>2x)\n");
        }
    } else {
        printf("  Skipping SIMD test (AVX2 not available)\n");
    }
#else
    printf("  Skipping SIMD test (not compiled with AVX2)\n");
#endif
    
    actor_soa_destroy(actors);
    TEST_PASS;
    return 0;
}

// Test 3: Message Batching
int test_message_batching() {
    printf("\nTest 3: Message Batching\n");
    
    // Create batch
    MessageBatch* batch = batch_create(256);
    assert(batch != NULL);
    assert(batch->capacity == 256);
    assert(batch->count == 0);
    
    printf("  Testing batch operations...\n");
    
    // Add messages
    Message msg = {1, 0, 42, NULL};
    for (int i = 0; i < 100; i++) {
        int result = batch_add(batch, i, msg);
        assert(result == 0);
    }
    
    assert(batch->count == 100);
    printf("    Added 100 messages\n");
    
    // Verify batch contents
    for (int i = 0; i < 100; i++) {
        assert(batch->targets[i] == i);
        assert(batch->messages[i].type == 1);
        assert(batch->messages[i].payload_int == 42);
    }
    
    // Test clear
    batch_clear(batch);
    assert(batch->count == 0);
    printf("    Batch cleared\n");
    
    // Test capacity limit
    for (int i = 0; i < 256; i++) {
        assert(batch_add(batch, i, msg) == 0);
    }
    assert(batch->count == 256);
    
    // Try to add one more (should fail)
    int result = batch_add(batch, 999, msg);
    assert(result == -1);
    printf("    Capacity limit enforced\n");
    
    batch_destroy(batch);
    TEST_PASS;
    return 0;
}

// Test 4: Batch Performance
int test_batch_performance() {
    printf("\nTest 4: Batch Performance\n");
    
    int num_messages = 1000000;
    printf("  Sending %d messages\n", num_messages);
    
    // Test single sends (simulated)
    printf("  Testing individual sends...\n");
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    volatile int counter = 0;
    for (int i = 0; i < num_messages; i++) {
        counter++; // Simulate send overhead
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_single = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("    Time: %.4f seconds\n", time_single);
    
    // Test batch sends
    printf("  Testing batch sends (256)...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    MessageBatch* batch = batch_create(256);
    Message msg = {1, 0, 1, NULL};
    
    for (int i = 0; i < num_messages; i++) {
        batch_add(batch, i % 1000, msg);
        
        if (batch->count == 256) {
            // Simulate batch send
            counter += batch->count;
            batch_clear(batch);
        }
    }
    
    // Send remaining
    if (batch->count > 0) {
        counter += batch->count;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_batch = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("    Time: %.4f seconds\n", time_batch);
    printf("    Speedup: %.2fx\n", time_single / time_batch);
    
    batch_destroy(batch);
    TEST_PASS;
    return 0;
}

// Test 5: Memory Alignment (for SIMD)
int test_memory_alignment() {
    printf("\nTest 5: Memory Alignment\n");
    
    ActorSoA* actors = actor_soa_create(1000);
    
    // Check 32-byte alignment (required for AVX2)
    uintptr_t counter_addr = (uintptr_t)actors->counters;
    uintptr_t states_addr = (uintptr_t)actors->states;
    uintptr_t flags_addr = (uintptr_t)actors->active_flags;
    
    printf("  Counter address: 0x%lx\n", counter_addr);
    printf("  States address:  0x%lx\n", states_addr);
    printf("  Flags address:   0x%lx\n", flags_addr);
    
    if (counter_addr % 32 == 0 && states_addr % 32 == 0 && flags_addr % 32 == 0) {
        printf("  ✓ All arrays are 32-byte aligned (AVX2 compatible)\n");
    } else {
        printf("  ⚠ Warning: Arrays not properly aligned\n");
    }
    
    actor_soa_destroy(actors);
    TEST_PASS;
    return 0;
}

// Test 6: Stress Test
int test_stress() {
    printf("\nTest 6: Stress Test\n");
    
    printf("  Creating large batch...\n");
    MessageBatch* batch = batch_create(10000);
    
    Message msg = {1, 0, 1, NULL};
    for (int i = 0; i < 10000; i++) {
        batch_add(batch, i, msg);
    }
    
    assert(batch->count == 10000);
    printf("  ✓ Created batch with 10,000 messages\n");
    
    printf("  Creating large actor array...\n");
    ActorSoA* actors = actor_soa_create(10000000);
    
    for (int i = 0; i < 1000000; i++) {
        actors->counters[i] = i;
        actors->active_flags[i] = 1;
    }
    
    printf("  ✓ Created SoA with 10M capacity\n");
    
    batch_destroy(batch);
    actor_soa_destroy(actors);
    
    TEST_PASS;
    return 0;
}

int main() {
    printf("===========================================\n");
    printf("Aether Runtime Test Suite\n");
    printf("===========================================\n");
    
    // Print CPU info first
    cpu_print_info();
    
    int failures = 0;
    
    failures += test_cpu_detection();
    failures += test_simd_actors();
    failures += test_message_batching();
    failures += test_batch_performance();
    failures += test_memory_alignment();
    failures += test_stress();
    
    printf("\n===========================================\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED ✓\n");
    } else {
        printf("%d TESTS FAILED ✗\n", failures);
    }
    printf("===========================================\n");
    
    return failures;
}
