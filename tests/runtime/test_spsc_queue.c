#include "test_harness.h"
#include "../../runtime/actors/aether_spsc_queue.h"
#include <string.h>
#include <stdbool.h>

// Test basic enqueue/dequeue
TEST_CATEGORY(spsc_basic_operations, TEST_CATEGORY_RUNTIME) {
    SPSCQueue q;
    spsc_queue_init(&q);
    
    Message msg = message_create_simple(1, 100, 42);
    ASSERT_TRUE(spsc_enqueue(&q, msg));
    
    Message out;
    ASSERT_TRUE(spsc_dequeue(&q, &out));
    ASSERT_EQ(out.type, 1);
    ASSERT_EQ(out.sender_id, 100);
    ASSERT_EQ(out.payload_int, 42);
}

// Test queue full condition
TEST_CATEGORY(spsc_queue_full, TEST_CATEGORY_RUNTIME) {
    SPSCQueue q;
    spsc_queue_init(&q);
    
    // Fill queue (SPSC_QUEUE_SIZE - 1 because of empty/full distinction)
    for (int i = 0; i < SPSC_QUEUE_SIZE - 1; i++) {
        Message msg = message_create_simple(1, 0, i);
        ASSERT_TRUE(spsc_enqueue(&q, msg));
    }
    
    // Next enqueue should fail (full)
    Message msg = message_create_simple(1, 0, 999);
    ASSERT_FALSE(spsc_enqueue(&q, msg));
}

// Test batch enqueue/dequeue
TEST_CATEGORY(spsc_batch_operations, TEST_CATEGORY_RUNTIME) {
    SPSCQueue q;
    spsc_queue_init(&q);
    
    // Enqueue batch
    Message msgs[10];
    for (int i = 0; i < 10; i++) {
        msgs[i] = message_create_simple(1, 0, i);
    }
    
    int sent = spsc_enqueue_batch(&q, msgs, 10);
    ASSERT_EQ(sent, 10);
    
    // Dequeue batch
    Message out[10];
    int received = spsc_dequeue_batch(&q, out, 10);
    ASSERT_EQ(received, 10);
    
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(out[i].payload_int, i);
    }
}

// Test alternating enqueue/dequeue (wrap around)
TEST_CATEGORY(spsc_wrap_around, TEST_CATEGORY_RUNTIME) {
    SPSCQueue q;
    spsc_queue_init(&q);
    
    // Enqueue and dequeue repeatedly to test wrap-around
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < 100; i++) {
            Message msg = message_create_simple(1, 0, i);
            ASSERT_TRUE(spsc_enqueue(&q, msg));
        }
        
        for (int i = 0; i < 100; i++) {
            Message out;
            ASSERT_TRUE(spsc_dequeue(&q, &out));
            ASSERT_EQ(out.payload_int, i);
        }
    }
}

// Test count approximation
TEST_CATEGORY(spsc_count_tracking, TEST_CATEGORY_RUNTIME) {
    SPSCQueue q;
    spsc_queue_init(&q);
    
    ASSERT_EQ(spsc_count(&q), 0);
    
    for (int i = 0; i < 50; i++) {
        Message msg = message_create_simple(1, 0, i);
        spsc_enqueue(&q, msg);
    }
    
    int count = spsc_count(&q);
    ASSERT_TRUE(count >= 49 && count <= 51);  // Approximate due to relaxed atomics
    
    Message out;
    for (int i = 0; i < 25; i++) {
        spsc_dequeue(&q, &out);
    }
    
    count = spsc_count(&q);
    ASSERT_TRUE(count >= 24 && count <= 26);
}
