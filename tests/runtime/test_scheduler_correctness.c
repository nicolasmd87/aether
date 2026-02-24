// Comprehensive scheduler correctness tests
// Tests: message delivery, ordering, throughput, cross-core communication

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#include "../../runtime/actors/actor_state_machine.h"
#include "../../runtime/scheduler/multicore_scheduler.h"

// Test status
int tests_passed = 0;
int tests_failed = 0;

#define TEST_ASSERT(condition, msg) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
        return 0; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    printf("PASS: %s\n", name); \
    tests_passed++; \
    return 1; \
} while(0)

// ============================================
// Test Actor Types
// ============================================

typedef struct {
    int id;
    int active;
    atomic_int assigned_core;
    Mailbox mailbox;
    SPSCQueue spsc_queue;  // REQUIRED - must match ActorBase layout
    void (*step)(void*);
    atomic_int count;
    atomic_int last_value;
} CounterActor;

typedef struct {
    int id;
    int active;
    atomic_int assigned_core;
    Mailbox mailbox;
    SPSCQueue spsc_queue;  // REQUIRED - must match ActorBase layout
    void (*step)(void*);
    atomic_int received[100];  // Track received message IDs
    atomic_int count;
} OrderActor;

typedef struct {
    int id;
    int active;
    atomic_int assigned_core;
    Mailbox mailbox;
    SPSCQueue spsc_queue;  // REQUIRED - must match ActorBase layout
    void (*step)(void*);
    atomic_int pings_received;
    atomic_int pongs_sent;
} PingPongActor;

// ============================================
// Actor Step Functions
// ============================================

void counter_step(CounterActor* self) {
    Message msg;
    while (mailbox_receive(&self->mailbox, &msg)) {
        atomic_fetch_add(&self->count, 1);
        atomic_store(&self->last_value, msg.payload_int);
    }
    // Keep active if there might be more messages
    self->active = (self->mailbox.count > 0);
}

void order_step(OrderActor* self) {
    Message msg;
    while (mailbox_receive(&self->mailbox, &msg)) {
        int idx = atomic_load(&self->count);
        if (idx < 100) {
            atomic_store(&self->received[idx], msg.payload_int);
        }
        atomic_fetch_add(&self->count, 1);
    }
    self->active = (self->mailbox.count > 0);
}

void pingpong_step(PingPongActor* self) {
    Message msg;
    while (mailbox_receive(&self->mailbox, &msg)) {
        atomic_fetch_add(&self->pings_received, 1);
        
        // Send pong back if there's a reply target
        if (msg.payload_ptr) {
            PingPongActor* target = (PingPongActor*)msg.payload_ptr;
            Message pong = {2, self->id, 0, NULL};
            mailbox_send(&target->mailbox, pong);
            target->active = 1;
            atomic_fetch_add(&self->pongs_sent, 1);
        }
    }
    self->active = (self->mailbox.count > 0);
}

// ============================================
// Test Cases
// ============================================

int test_basic_message_delivery() {
    printf("\n--- Test 1: Basic Message Delivery ---\n");
    
    scheduler_init(2);
    scheduler_start();
    
    // Create actor
    CounterActor* actor = malloc(sizeof(CounterActor));
    actor->id = 1;
    actor->active = 0;
    actor->step = (void (*)(void*))counter_step;
    atomic_store(&actor->count, 0);
    atomic_store(&actor->last_value, -1);
    mailbox_init(&actor->mailbox);
    
    scheduler_register_actor((ActorBase*)actor, 0);
    
    // Send messages
    for (int i = 0; i < 10; i++) {
        Message msg = {1, 0, i, NULL};
        scheduler_send_local((ActorBase*)actor, msg);
    }
    
    // Wait for processing
    sleep_ms(100);
    
    int count = atomic_load(&actor->count);
    int last = atomic_load(&actor->last_value);
    
    scheduler_stop();
    scheduler_wait();
    free(actor);
    
    TEST_ASSERT(count == 10, "Should receive all 10 messages");
    TEST_ASSERT(last == 9, "Last value should be 9");
    
    TEST_PASS("Basic message delivery");
}

int test_message_ordering() {
    printf("\n--- Test 2: Message Ordering ---\n");
    
    scheduler_init(2);
    scheduler_start();
    
    OrderActor* actor = malloc(sizeof(OrderActor));
    actor->id = 1;
    actor->active = 0;
    actor->step = (void (*)(void*))order_step;
    atomic_store(&actor->count, 0);
    mailbox_init(&actor->mailbox);
    
    for (int i = 0; i < 100; i++) {
        atomic_store(&actor->received[i], -1);
    }
    
    scheduler_register_actor((ActorBase*)actor, 0);
    
    // Send ordered messages
    for (int i = 0; i < 50; i++) {
        Message msg = {1, 0, i, NULL};
        scheduler_send_local((ActorBase*)actor, msg);
    }
    
    sleep_ms(100);
    
    int count = atomic_load(&actor->count);
    TEST_ASSERT(count == 50, "Should receive 50 messages");
    
    // Check ordering
    int ordered = 1;
    for (int i = 0; i < 50; i++) {
        if (atomic_load(&actor->received[i]) != i) {
            ordered = 0;
            fprintf(stderr, "Order violation at %d: expected %d, got %d\n", 
                    i, i, atomic_load(&actor->received[i]));
            break;
        }
    }
    
    scheduler_stop();
    scheduler_wait();
    free(actor);
    
    TEST_ASSERT(ordered, "Messages should be received in order");
    
    TEST_PASS("Message ordering");
}

int test_cross_core_messaging() {
    printf("\n--- Test 3: Cross-Core Messaging ---\n");
    
    scheduler_init(4);
    scheduler_start();
    
    // Create actors on different cores
    CounterActor* actor0 = malloc(sizeof(CounterActor));
    CounterActor* actor1 = malloc(sizeof(CounterActor));
    
    actor0->id = 1;
    actor0->active = 0;
    actor0->step = (void (*)(void*))counter_step;
    atomic_store(&actor0->count, 0);
    mailbox_init(&actor0->mailbox);
    
    actor1->id = 2;
    actor1->active = 0;
    actor1->step = (void (*)(void*))counter_step;
    atomic_store(&actor1->count, 0);
    mailbox_init(&actor1->mailbox);
    
    scheduler_register_actor((ActorBase*)actor0, 0);
    scheduler_register_actor((ActorBase*)actor1, 1);
    
    // Send cross-core messages
    for (int i = 0; i < 100; i++) {
        Message msg = {1, 0, i, NULL};
        scheduler_send_remote((ActorBase*)actor1, msg, 0);
    }
    
    sleep_ms(200);
    
    int count0 = atomic_load(&actor0->count);
    int count1 = atomic_load(&actor1->count);
    
    scheduler_stop();
    scheduler_wait();
    free(actor0);
    free(actor1);
    
    TEST_ASSERT(count1 == 100, "Actor1 should receive 100 messages");
    TEST_ASSERT(count0 == 0, "Actor0 should receive no messages");
    
    TEST_PASS("Cross-core messaging");
}

int test_high_throughput() {
    printf("\n--- Test 4: High Throughput ---\n");
    
    scheduler_init(4);
    scheduler_start();
    
    CounterActor* actor = malloc(sizeof(CounterActor));
    actor->id = 1;
    actor->active = 0;
    actor->step = (void (*)(void*))counter_step;
    atomic_store(&actor->count, 0);
    mailbox_init(&actor->mailbox);
    
    scheduler_register_actor((ActorBase*)actor, 0);
    
    // Send many messages
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int total_msgs = 100000;
    for (int i = 0; i < total_msgs; i++) {
        Message msg = {1, 0, i, NULL};
        mailbox_send(&actor->mailbox, msg);
        actor->active = 1;
    }
    
    // Wait for processing
    while (atomic_load(&actor->count) < total_msgs) {
        sleep_ms(1);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    double msgs_per_sec = total_msgs / elapsed;
    
    printf("Processed %d messages in %.3f seconds\n", total_msgs, elapsed);
    printf("Throughput: %.0f messages/sec\n", msgs_per_sec);
    
    scheduler_stop();
    scheduler_wait();
    free(actor);
    
    TEST_ASSERT(atomic_load(&actor->count) == total_msgs, "All messages processed");
    
    TEST_PASS("High throughput");
}

int test_bidirectional_communication() {
    printf("\n--- Test 5: Bidirectional Communication ---\n");
    
    scheduler_init(2);
    scheduler_start();
    
    PingPongActor* actor1 = malloc(sizeof(PingPongActor));
    PingPongActor* actor2 = malloc(sizeof(PingPongActor));
    
    actor1->id = 1;
    actor1->active = 0;
    actor1->step = (void (*)(void*))pingpong_step;
    atomic_store(&actor1->pings_received, 0);
    atomic_store(&actor1->pongs_sent, 0);
    mailbox_init(&actor1->mailbox);
    
    actor2->id = 2;
    actor2->active = 0;
    actor2->step = (void (*)(void*))pingpong_step;
    atomic_store(&actor2->pings_received, 0);
    atomic_store(&actor2->pongs_sent, 0);
    mailbox_init(&actor2->mailbox);
    
    scheduler_register_actor((ActorBase*)actor1, 0);
    scheduler_register_actor((ActorBase*)actor2, 0);
    
    // Send pings from actor1 to actor2
    for (int i = 0; i < 50; i++) {
        Message msg = {1, actor1->id, 0, actor1};  // Reply to actor1
        mailbox_send(&actor2->mailbox, msg);
        actor2->active = 1;
    }
    
    sleep_ms(200);
    
    int pings2 = atomic_load(&actor2->pings_received);
    int pongs2 = atomic_load(&actor2->pongs_sent);
    int pings1 = atomic_load(&actor1->pings_received);
    
    printf("Actor2 received %d pings, sent %d pongs\n", pings2, pongs2);
    printf("Actor1 received %d pongs back\n", pings1);
    
    scheduler_stop();
    scheduler_wait();
    free(actor1);
    free(actor2);
    
    TEST_ASSERT(pings2 == 50, "Actor2 should receive 50 pings");
    TEST_ASSERT(pongs2 == 50, "Actor2 should send 50 pongs");
    TEST_ASSERT(pings1 == 50, "Actor1 should receive 50 pongs back");
    
    TEST_PASS("Bidirectional communication");
}

// ============================================
// Main Test Runner
// ============================================

int main() {
    printf("========================================\n");
    printf("Aether Scheduler Correctness Test Suite\n");
    printf("========================================\n");
    
    test_basic_message_delivery();
    test_message_ordering();
    test_cross_core_messaging();
    test_high_throughput();
    test_bidirectional_communication();
    
    printf("\n========================================\n");
    printf("Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
