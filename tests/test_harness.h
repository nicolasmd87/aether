#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test result structure
typedef struct {
    const char* name;
    bool passed;
    const char* file;
    int line;
    const char* message;
} TestResult;

// Test function pointer
typedef void (*TestFunction)(void);

// Test registration
void register_test(const char* name, TestFunction func);
void run_all_tests(void);
int get_test_count(void);
int get_passed_count(void);
int get_failed_count(void);

// Assertion macros
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            fprintf(stderr, "ASSERT_FALSE failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "ASSERT_EQ failed at %s:%d: expected %ld, got %ld\n", \
                    __FILE__, __LINE__, (long)(expected), (long)(actual)); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NE(expected, actual) \
    do { \
        if ((expected) == (actual)) { \
            fprintf(stderr, "ASSERT_NE failed at %s:%d: both values are %ld\n", \
                    __FILE__, __LINE__, (long)(expected)); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_STREQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            fprintf(stderr, "ASSERT_STREQ failed at %s:%d: expected '%s', got '%s'\n", \
                    __FILE__, __LINE__, (expected), (actual)); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_STRNE(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) == 0) { \
            fprintf(stderr, "ASSERT_STRNE failed at %s:%d: both strings are '%s'\n", \
                    __FILE__, __LINE__, (expected)); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "ASSERT_NULL failed at %s:%d: pointer is not NULL\n", \
                    __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "ASSERT_NOT_NULL failed at %s:%d: pointer is NULL\n", \
                    __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

// Test registration macro
#define TEST(name) \
    static void test_##name(void); \
    static void __attribute__((constructor)) register_##name(void) { \
        register_test(#name, test_##name); \
    } \
    static void test_##name(void)

#endif // TEST_HARNESS_H
