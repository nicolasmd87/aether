#include "test_harness.h"
#include <string.h>

#define MAX_TESTS 1000

static struct {
    const char* name;
    TestFunction func;
} tests[MAX_TESTS];

static int test_count = 0;
static int passed_count = 0;
static int failed_count = 0;

void register_test(const char* name, TestFunction func) {
    if (test_count >= MAX_TESTS) {
        fprintf(stderr, "ERROR: Too many tests (max %d)\n", MAX_TESTS);
        exit(1);
    }
    tests[test_count].name = name;
    tests[test_count].func = func;
    test_count++;
}

void run_all_tests(void) {
    printf("Running %d test(s)...\n\n", test_count);
    
    for (int i = 0; i < test_count; i++) {
        printf("[%d/%d] Running test: %s\n", i + 1, test_count, tests[i].name);
        fflush(stdout);
        
        // Run test
        // Note: Assertions call exit(1) on failure, so if we reach here,
        // the test passed
        tests[i].func();
        
        passed_count++;
        printf("  ✓ PASSED\n");
    }
    
    printf("\n");
    printf("========================================\n");
    printf("Test Results: %d passed, %d failed, %d total\n", 
           passed_count, failed_count, test_count);
    printf("========================================\n");
    
    if (failed_count > 0) {
        exit(1);
    }
}

int get_test_count(void) {
    return test_count;
}

int get_passed_count(void) {
    return passed_count;
}

int get_failed_count(void) {
    return failed_count;
}
