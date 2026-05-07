/* tests/runtime/test_libaether_caps.c — public surface (#343 part 2/3).
 *
 * Verifies the libaether.h symbols (aether_set_memory_cap,
 * aether_set_call_deadline, aether_deadline_tripped) forward to the
 * internal cap state correctly. Embedders link against this exact
 * surface — the test exercises it from a host's perspective.
 */

#include "test_harness.h"
#include "../../include/libaether.h"
#include "../../runtime/aether_resource_caps.h"

#include <time.h>

static void caps_reset_for_test(void) {
    aether_set_memory_cap(0);
    aether_set_call_deadline(0);
    uint64_t leftover = aether_caps_used_bytes();
    if (leftover > 0) aether_caps_account_free((size_t)leftover);
}

TEST_CATEGORY(libaether_set_memory_cap_blocks_oversize, TEST_CATEGORY_STDLIB) {
    caps_reset_for_test();
    aether_set_memory_cap(4096);
    /* Internal check_alloc surfaces the cap. */
    ASSERT_EQ(0, aether_caps_check_alloc(8192));
    aether_set_memory_cap(0);
}

TEST_CATEGORY(libaether_set_call_deadline_arms_tripwire, TEST_CATEGORY_STDLIB) {
    caps_reset_for_test();
    aether_set_call_deadline(15);
    ASSERT_EQ(0, aether_deadline_tripped());
    struct timespec ts = { 0, 40 * 1000 * 1000 };
    nanosleep(&ts, NULL);
    ASSERT_EQ(1, aether_deadline_tripped());
    aether_set_call_deadline(0);
    ASSERT_EQ(0, aether_deadline_tripped());
}

TEST_CATEGORY(libaether_zero_disables_cap_and_deadline, TEST_CATEGORY_STDLIB) {
    caps_reset_for_test();
    /* Cap-set-then-cleared: disabled state passes any size. */
    aether_set_memory_cap(1024);
    aether_set_memory_cap(0);
    ASSERT_EQ(1, aether_caps_check_alloc(1024 * 1024 * 1024));
    aether_caps_account_free(1024 * 1024 * 1024);

    /* Deadline-set-then-cleared: tripwire stays 0 even past time. */
    aether_set_call_deadline(0);
    ASSERT_EQ(0, aether_deadline_tripped());
}
