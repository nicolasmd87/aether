// Test suite for the thread-local send buffer (runtime/actors/aether_send_buffer.c).
//
// Coverage motivation: gcov on `make ci-coverage` reported
// runtime/actors/aether_send_buffer.c at 0% (0 of 34 executable lines hit)
// — the public surface (`send_buffer_flush`, `send_buffered`,
// `send_buffer_force_flush`) is called only from runtime/examples/*.c
// bench programs, which aren't part of the test suite. The codegen never
// emits these calls. So the whole batched-send path was a CI dead zone
// — any regression would have shipped silently.
//
// What this exercises:
//   1. Empty-buffer flush (early return when count == 0)
//   2. NULL-target flush (early return when target == NULL)
//   3. Single-message same-core flush (SPSC fast path)
//   4. Multi-message same-core flush (SPSC batch)
//   5. send_buffered helper accumulates without flushing
//   6. send_buffered triggers flush on target change
//   7. send_buffered triggers flush at SEND_BUFFER_SIZE
//   8. send_buffer_force_flush drains a partial buffer
//   9. send_buffer_pending reports the pending count
//
// Cross-core path is left untested here because it requires Scheduler
// state (`schedulers[]` array) — that's a separate set of tests
// already covered by test_scheduler.c. The same-core path covered
// here is the more common scenario in real Aether programs anyway.

#include "test_harness.h"
#include "../../runtime/actors/aether_send_buffer.h"
#include "../../runtime/actors/actor_state_machine.h"
#include "../../runtime/actors/aether_spsc_queue.h"
#include "../../runtime/scheduler/multicore_scheduler.h"
#include <stdlib.h>
#include <string.h>

// Helper: build a minimal ActorBase that send_buffer_flush can target
// for the same-core fast path. Caller owns the returned pointer and
// must `free` it (plus any spsc_queue it lazily acquires).
static ActorBase* make_same_core_actor(int core_id) {
    ActorBase* actor = calloc(1, sizeof(ActorBase));
    if (!actor) return NULL;
    actor->id = 1;
    mailbox_init(&actor->mailbox);
    atomic_store(&actor->assigned_core, core_id);
    atomic_store(&actor->active, 0);
    actor->spsc_queue = NULL;  // lazy-allocated by send_buffer_flush
    return actor;
}

static void free_actor(ActorBase* actor) {
    if (!actor) return;
    if (actor->spsc_queue) free(actor->spsc_queue);
    free(actor);
}

// Helper: reset thread-local send buffer to a known state with
// core_id = 0 so the same-core path is the one we exercise.
static void reset_send_buffer(int core_id) {
    g_send_buffer.target = NULL;
    g_send_buffer.count = 0;
    g_send_buffer.core_id = core_id;
}

// Test 1: send_buffer_flush is a no-op when the buffer is empty.
TEST_CATEGORY(send_buffer_flush_empty_is_noop, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    // No actor target, no messages — must not crash, must not write
    // anything anywhere.
    send_buffer_flush();
    ASSERT_EQ(0, g_send_buffer.count);
    ASSERT_TRUE(g_send_buffer.target == NULL);
}

// Test 2: send_buffer_flush is a no-op when target is NULL even with
// count > 0 (defensive — shouldn't normally happen in practice).
TEST_CATEGORY(send_buffer_flush_null_target_is_noop, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    g_send_buffer.target = NULL;
    g_send_buffer.count = 3;
    g_send_buffer.buffer[0] = message_create_simple(1, 0, 100);
    send_buffer_flush();
    // count is left as-is — early return doesn't reset.
    ASSERT_EQ(3, g_send_buffer.count);
}

// Test 3: single-message flush via the same-core SPSC path.
TEST_CATEGORY(send_buffer_flush_single_message_spsc, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    ActorBase* actor = make_same_core_actor(0);
    ASSERT_NOT_NULL(actor);

    g_send_buffer.target = actor;
    g_send_buffer.count = 1;
    g_send_buffer.buffer[0] = message_create_simple(7, 0, 42);

    send_buffer_flush();

    // Successful SPSC enqueue clears the buffer and marks actor active.
    ASSERT_EQ(0, g_send_buffer.count);
    ASSERT_EQ(1, atomic_load(&actor->active));
    ASSERT_NOT_NULL(actor->spsc_queue);  // lazy-allocated on first flush

    free_actor(actor);
}

// Test 4: multi-message batch flush.
TEST_CATEGORY(send_buffer_flush_batch_messages, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    ActorBase* actor = make_same_core_actor(0);
    ASSERT_NOT_NULL(actor);

    g_send_buffer.target = actor;
    g_send_buffer.count = 5;
    for (int i = 0; i < 5; i++) {
        g_send_buffer.buffer[i] = message_create_simple(1, 0, i * 10);
    }

    send_buffer_flush();

    ASSERT_EQ(0, g_send_buffer.count);
    ASSERT_EQ(1, atomic_load(&actor->active));

    free_actor(actor);
}

// Test 5: send_buffered accumulates messages without flushing while
// the target is the same and count is below SEND_BUFFER_SIZE.
TEST_CATEGORY(send_buffered_accumulates, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    ActorBase* actor = make_same_core_actor(0);
    ASSERT_NOT_NULL(actor);

    for (int i = 0; i < 10; i++) {
        send_buffered((struct ActorBase*)actor, message_create_simple(1, 0, i));
    }

    // Should sit in buffer; no flush yet.
    ASSERT_EQ(10, g_send_buffer.count);
    ASSERT_EQ(10, send_buffer_pending());
    ASSERT_TRUE(g_send_buffer.target == actor);
    ASSERT_EQ(0, atomic_load(&actor->active));  // not flushed → not activated

    free_actor(actor);
    reset_send_buffer(0);  // don't leak the dead actor pointer to next test
}

// Test 6: send_buffered flushes when the target changes.
TEST_CATEGORY(send_buffered_flushes_on_target_change, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    ActorBase* actor_a = make_same_core_actor(0);
    ActorBase* actor_b = make_same_core_actor(0);
    ASSERT_NOT_NULL(actor_a);
    ASSERT_NOT_NULL(actor_b);

    // 3 messages to actor_a — accumulate.
    send_buffered((struct ActorBase*)actor_a, message_create_simple(1, 0, 1));
    send_buffered((struct ActorBase*)actor_a, message_create_simple(1, 0, 2));
    send_buffered((struct ActorBase*)actor_a, message_create_simple(1, 0, 3));
    ASSERT_EQ(3, g_send_buffer.count);

    // Switching target triggers flush of pending messages, then queues
    // the new one against actor_b.
    send_buffered((struct ActorBase*)actor_b, message_create_simple(2, 0, 99));

    ASSERT_EQ(1, g_send_buffer.count);                  // only the new message
    ASSERT_TRUE(g_send_buffer.target == actor_b);
    ASSERT_EQ(1, atomic_load(&actor_a->active));        // a got activated by flush
    ASSERT_EQ(0, atomic_load(&actor_b->active));        // b not flushed yet

    free_actor(actor_a);
    free_actor(actor_b);
    reset_send_buffer(0);
}

// Test 7: send_buffered flushes when buffer hits SEND_BUFFER_SIZE.
TEST_CATEGORY(send_buffered_flushes_on_capacity, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    ActorBase* actor = make_same_core_actor(0);
    ASSERT_NOT_NULL(actor);

    // Fill to capacity. Each call goes through the
    // count >= SEND_BUFFER_SIZE check (false until the 256th send,
    // since count starts at 0 and we increment after appending).
    for (int i = 0; i < SEND_BUFFER_SIZE; i++) {
        send_buffered((struct ActorBase*)actor, message_create_simple(1, 0, i));
    }
    ASSERT_EQ(SEND_BUFFER_SIZE, g_send_buffer.count);

    // The next send_buffered triggers the count >= SEND_BUFFER_SIZE
    // branch and calls send_buffer_flush(). The flush drains as many
    // as the SPSC queue (~63 slots) + the mailbox (~31 slots) can
    // hold, then memmoves the rest to the front (partial-send path).
    // Then the new message is appended. Exact count after this is
    // implementation-defined; the invariant we care about is "the
    // flush ran" — actor is now active, and count is < the 256+1
    // we'd see if no flush had happened.
    int before = g_send_buffer.count;
    send_buffered((struct ActorBase*)actor, message_create_simple(1, 0, 999));
    int after = g_send_buffer.count;

    // The flush was triggered: count went down from SEND_BUFFER_SIZE
    // and is now < before+1 (which would be the no-flush case where
    // the new message just got appended to a full buffer). The exact
    // post-count is implementation-defined: depends on how many slots
    // SPSC + mailbox had free, and on which path inside flush ran.
    ASSERT_TRUE(after < before + 1);                   // flush ran
    ASSERT_TRUE(after > 0);                            // partial drain — buffer wasn't fully empty
    ASSERT_TRUE(g_send_buffer.target == actor);        // target preserved
    ASSERT_NOT_NULL(actor->spsc_queue);                // SPSC was lazily allocated

    free_actor(actor);
    reset_send_buffer(0);
}

// Test 8: send_buffer_force_flush drains a partial buffer and resets target.
TEST_CATEGORY(send_buffer_force_flush_drains_partial, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    ActorBase* actor = make_same_core_actor(0);
    ASSERT_NOT_NULL(actor);

    send_buffered((struct ActorBase*)actor, message_create_simple(1, 0, 10));
    send_buffered((struct ActorBase*)actor, message_create_simple(1, 0, 20));
    ASSERT_EQ(2, g_send_buffer.count);

    send_buffer_force_flush();

    ASSERT_EQ(0, g_send_buffer.count);
    ASSERT_TRUE(g_send_buffer.target == NULL);
    ASSERT_EQ(1, atomic_load(&actor->active));

    free_actor(actor);
}

// Test 9: send_buffer_force_flush is a no-op when buffer is empty.
TEST_CATEGORY(send_buffer_force_flush_empty_is_noop, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    g_send_buffer.target = NULL;
    g_send_buffer.count = 0;

    send_buffer_force_flush();

    ASSERT_EQ(0, g_send_buffer.count);
    ASSERT_TRUE(g_send_buffer.target == NULL);
}

// Test 10: send_buffer_pending reports the current count.
TEST_CATEGORY(send_buffer_pending_reports_count, TEST_CATEGORY_RUNTIME) {
    reset_send_buffer(0);
    ActorBase* actor = make_same_core_actor(0);
    ASSERT_NOT_NULL(actor);

    ASSERT_EQ(0, send_buffer_pending());
    send_buffered((struct ActorBase*)actor, message_create_simple(1, 0, 1));
    ASSERT_EQ(1, send_buffer_pending());
    send_buffered((struct ActorBase*)actor, message_create_simple(1, 0, 2));
    send_buffered((struct ActorBase*)actor, message_create_simple(1, 0, 3));
    ASSERT_EQ(3, send_buffer_pending());

    send_buffer_force_flush();
    ASSERT_EQ(0, send_buffer_pending());

    free_actor(actor);
}

void register_send_buffer_tests(void) {
    // Empty - tests auto-register via TEST_CATEGORY constructor on POSIX.
}
