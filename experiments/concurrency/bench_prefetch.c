// Benchmark: Manual Prefetch Hints vs No Prefetch
//
// Tests the performance impact of adding __builtin_prefetch to mailbox operations.
// Measures message throughput with and without prefetch hints.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define MAILBOX_SIZE 256
#define ITERATIONS 100000000  // 100M operations

// Message structure (same as runtime)
typedef struct {
    int id;
    void* data;
} Message;

// ====================================
// VERSION 1: No Prefetch
// ====================================
typedef struct {
    Message messages[MAILBOX_SIZE];
    int head;
    int tail;
} Mailbox_NoPrefetch;

static inline int mailbox_send_no_prefetch(Mailbox_NoPrefetch* mbox, Message msg) {
    int next_tail = (mbox->tail + 1) % MAILBOX_SIZE;
    if (next_tail == mbox->head) {
        return 0;  // Full
    }
    mbox->messages[mbox->tail] = msg;
    mbox->tail = next_tail;
    return 1;
}

static inline int mailbox_receive_no_prefetch(Mailbox_NoPrefetch* mbox, Message* out_msg) {
    if (mbox->head == mbox->tail) {
        return 0;  // Empty
    }
    *out_msg = mbox->messages[mbox->head];
    mbox->head = (mbox->head + 1) % MAILBOX_SIZE;
    return 1;
}

// ====================================
// VERSION 2: With Prefetch
// ====================================
typedef struct {
    Message messages[MAILBOX_SIZE];
    int head;
    int tail;
} Mailbox_WithPrefetch;

static inline int mailbox_send_with_prefetch(Mailbox_WithPrefetch* mbox, Message msg) {
    int next_tail = (mbox->tail + 1) % MAILBOX_SIZE;
    
    // Prefetch next slot for write
    __builtin_prefetch(&mbox->messages[next_tail], 1, 1);
    
    if (next_tail == mbox->head) {
        return 0;  // Full
    }
    mbox->messages[mbox->tail] = msg;
    mbox->tail = next_tail;
    return 1;
}

static inline int mailbox_receive_with_prefetch(Mailbox_WithPrefetch* mbox, Message* out_msg) {
    if (mbox->head == mbox->tail) {
        return 0;  // Empty
    }
    
    // Prefetch next message
    int next_head = (mbox->head + 1) % MAILBOX_SIZE;
    __builtin_prefetch(&mbox->messages[next_head], 0, 1);
    
    *out_msg = mbox->messages[mbox->head];
    mbox->head = next_head;
    return 1;
}

// ====================================
// Timing Utilities
// ====================================
static inline uint64_t get_nanos() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// ====================================
// Benchmarks
// ====================================

// Benchmark: No Prefetch
double bench_no_prefetch() {
    Mailbox_NoPrefetch mbox = {0};
    Message msg = {.id = 42, .data = NULL};
    Message out_msg;
    
    // Warmup
    for (int i = 0; i < 100000; i++) {
        mailbox_send_no_prefetch(&mbox, msg);
        mailbox_receive_no_prefetch(&mbox, &out_msg);
    }
    
    // Reset
    mbox.head = 0;
    mbox.tail = 0;
    
    // Benchmark: alternating send/receive
    uint64_t start = get_nanos();
    for (int i = 0; i < ITERATIONS; i++) {
        mailbox_send_no_prefetch(&mbox, msg);
        mailbox_receive_no_prefetch(&mbox, &out_msg);
    }
    uint64_t end = get_nanos();
    
    double elapsed = (end - start) / 1e9;
    return ITERATIONS / elapsed / 1e6;  // M ops/sec
}

// Benchmark: With Prefetch
double bench_with_prefetch() {
    Mailbox_WithPrefetch mbox = {0};
    Message msg = {.id = 42, .data = NULL};
    Message out_msg;
    
    // Warmup
    for (int i = 0; i < 100000; i++) {
        mailbox_send_with_prefetch(&mbox, msg);
        mailbox_receive_with_prefetch(&mbox, &out_msg);
    }
    
    // Reset
    mbox.head = 0;
    mbox.tail = 0;
    
    // Benchmark: alternating send/receive
    uint64_t start = get_nanos();
    for (int i = 0; i < ITERATIONS; i++) {
        mailbox_send_with_prefetch(&mbox, msg);
        mailbox_receive_with_prefetch(&mbox, &out_msg);
    }
    uint64_t end = get_nanos();
    
    double elapsed = (end - start) / 1e9;
    return ITERATIONS / elapsed / 1e6;  // M ops/sec
}

// ====================================
// Batch Benchmark (Better Cache Behavior Test)
// ====================================

// Batch: No Prefetch
double bench_batch_no_prefetch() {
    Mailbox_NoPrefetch mbox = {0};
    Message msg = {.id = 42, .data = NULL};
    Message out_msgs[128];
    
    // Warmup
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 128; j++) {
            mailbox_send_no_prefetch(&mbox, msg);
        }
        for (int j = 0; j < 128; j++) {
            mailbox_receive_no_prefetch(&mbox, &out_msgs[j]);
        }
    }
    
    // Reset
    mbox.head = 0;
    mbox.tail = 0;
    
    // Benchmark: batch operations
    uint64_t start = get_nanos();
    int iterations = ITERATIONS / 128;
    for (int i = 0; i < iterations; i++) {
        // Send batch
        for (int j = 0; j < 128; j++) {
            mailbox_send_no_prefetch(&mbox, msg);
        }
        // Receive batch
        for (int j = 0; j < 128; j++) {
            mailbox_receive_no_prefetch(&mbox, &out_msgs[j]);
        }
    }
    uint64_t end = get_nanos();
    
    double elapsed = (end - start) / 1e9;
    return (iterations * 128) / elapsed / 1e6;  // M ops/sec
}

// Batch: With Prefetch
double bench_batch_with_prefetch() {
    Mailbox_WithPrefetch mbox = {0};
    Message msg = {.id = 42, .data = NULL};
    Message out_msgs[128];
    
    // Warmup
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 128; j++) {
            mailbox_send_with_prefetch(&mbox, msg);
        }
        for (int j = 0; j < 128; j++) {
            mailbox_receive_with_prefetch(&mbox, &out_msgs[j]);
        }
    }
    
    // Reset
    mbox.head = 0;
    mbox.tail = 0;
    
    // Benchmark: batch operations
    uint64_t start = get_nanos();
    int iterations = ITERATIONS / 128;
    for (int i = 0; i < iterations; i++) {
        // Send batch
        for (int j = 0; j < 128; j++) {
            mailbox_send_with_prefetch(&mbox, msg);
        }
        // Receive batch
        for (int j = 0; j < 128; j++) {
            mailbox_receive_with_prefetch(&mbox, &out_msgs[j]);
        }
    }
    uint64_t end = get_nanos();
    
    double elapsed = (end - start) / 1e9;
    return (iterations * 128) / elapsed / 1e6;  // M ops/sec
}

// ====================================
// Main
// ====================================
int main() {
    printf("===========================================\n");
    printf("PREFETCH HINT BENCHMARK\n");
    printf("===========================================\n");
    printf("Testing: __builtin_prefetch() impact\n");
    printf("Operations: %d M message sends + receives\n", ITERATIONS / 1000000);
    printf("Compiler: GCC -O3\n");
    printf("\n");
    
    // Run benchmarks
    printf("Running benchmarks...\n\n");
    
    printf("Test 1: Alternating Send/Receive\n");
    printf("-------------------------------------------\n");
    double no_prefetch = bench_no_prefetch();
    printf("No Prefetch:   %.2f M ops/sec\n", no_prefetch);
    
    double with_prefetch = bench_with_prefetch();
    printf("With Prefetch: %.2f M ops/sec\n", with_prefetch);
    
    double speedup = with_prefetch / no_prefetch;
    printf("Speedup:       %.2fx (%.1f%% faster)\n", speedup, (speedup - 1.0) * 100);
    printf("\n");
    
    printf("Test 2: Batch Operations (128 msg batches)\n");
    printf("-------------------------------------------\n");
    double batch_no_prefetch = bench_batch_no_prefetch();
    printf("No Prefetch:   %.2f M ops/sec\n", batch_no_prefetch);
    
    double batch_with_prefetch = bench_batch_with_prefetch();
    printf("With Prefetch: %.2f M ops/sec\n", batch_with_prefetch);
    
    double batch_speedup = batch_with_prefetch / batch_no_prefetch;
    printf("Speedup:       %.2fx (%.1f%% faster)\n", batch_speedup, (batch_speedup - 1.0) * 100);
    printf("\n");
    
    printf("===========================================\n");
    printf("RESULTS SUMMARY\n");
    printf("===========================================\n");
    printf("Single ops improvement: %.1f%%\n", (speedup - 1.0) * 100);
    printf("Batch ops improvement:  %.1f%%\n", (batch_speedup - 1.0) * 100);
    
    if (speedup > 1.02) {
        printf("\n✅ OPTIMIZATION EFFECTIVE\n");
        printf("Prefetch hints provide measurable improvement!\n");
    } else if (speedup > 0.98) {
        printf("\n⚠️  NEUTRAL IMPACT\n");
        printf("Prefetch hints neither help nor hurt.\n");
    } else {
        printf("\n❌ NEGATIVE IMPACT\n");
        printf("Prefetch hints hurt performance (possibly too aggressive).\n");
    }
    
    return 0;
}
