#ifndef AETHER_TEST_H
#define AETHER_TEST_H

#include <stdio.h>
#include "aether_compiler.h"

// Test framework structures
typedef struct {
    const char* name;
    void (*test_func)(void);
} AetherTest;

typedef struct {
    int total;
    int passed;
    int failed;
    const char* current_test;
} AetherTestStats;

// Global test stats
extern AetherTestStats aether_test_stats;

// Test registration and running
void aether_register_test(const char* name, void (*test_func)(void));
void aether_run_tests(void);
void aether_test_summary(void);

// Assertion macros
#define AETHER_TEST(test_name) \
    void test_##test_name(void); \
    AETHER_CONSTRUCTOR void register_test_##test_name(void) { \
        aether_register_test(#test_name, test_##test_name); \
    } \
    void test_##test_name(void)

#define assert_true(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "  \033[31mFAIL\033[0m: %s:%d: Assertion failed: %s\n", \
                    __FILE__, __LINE__, #condition); \
            aether_test_stats.failed++; \
            return; \
        } \
    } while(0)

#define assert_false(condition) assert_true(!(condition))

#define assert_eq(a, b) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "  \033[31mFAIL\033[0m: %s:%d: Expected %s == %s\n", \
                    __FILE__, __LINE__, #a, #b); \
            fprintf(stderr, "    Left:  %d\n", (int)(a)); \
            fprintf(stderr, "    Right: %d\n", (int)(b)); \
            aether_test_stats.failed++; \
            return; \
        } \
    } while(0)

#define assert_ne(a, b) \
    do { \
        if ((a) == (b)) { \
            fprintf(stderr, "  \033[31mFAIL\033[0m: %s:%d: Expected %s != %s\n", \
                    __FILE__, __LINE__, #a, #b); \
            aether_test_stats.failed++; \
            return; \
        } \
    } while(0)

#define assert_lt(a, b) assert_true((a) < (b))
#define assert_le(a, b) assert_true((a) <= (b))
#define assert_gt(a, b) assert_true((a) > (b))
#define assert_ge(a, b) assert_true((a) >= (b))

#define assert_null(ptr) assert_true((ptr) == NULL)
#define assert_not_null(ptr) assert_true((ptr) != NULL)

// String assertions
#define assert_str_eq(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            fprintf(stderr, "  \033[31mFAIL\033[0m: %s:%d: String mismatch\n", \
                    __FILE__, __LINE__); \
            fprintf(stderr, "    Expected: \"%s\"\n", (b)); \
            fprintf(stderr, "    Got:      \"%s\"\n", (a)); \
            aether_test_stats.failed++; \
            return; \
        } \
    } while(0)

// Actor-specific assertions
#define assert_actor_alive(actor) assert_not_null(actor)

#endif // AETHER_TEST_H

