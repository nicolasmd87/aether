// Minimal C test framework for Aether UI backend tests.
//
// Each test file defines a `run_tests(void)` entry point that calls AE_CASE
// for each assertion. The runner in run_all.c calls run_tests() and reports.
//
// Runtime tests that create a real window use AE_WIN_RUN(timeout_ms) to
// auto-close via a WM_QUIT timer, so the CI doesn't hang.

#ifndef AETHER_UI_TEST_FRAMEWORK_H
#define AETHER_UI_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int ae_test_pass;
extern int ae_test_fail;
extern const char* ae_test_current;

#define AE_CASE(cond, msg)                                                   \
    do {                                                                     \
        if (cond) {                                                          \
            ae_test_pass++;                                                  \
            printf("  [PASS] %s: %s\n", ae_test_current, msg);               \
        } else {                                                             \
            ae_test_fail++;                                                  \
            printf("  [FAIL] %s: %s (at %s:%d)\n",                           \
                   ae_test_current, msg, __FILE__, __LINE__);                \
        }                                                                    \
    } while (0)

#define AE_TEST(name) \
    ae_test_current = #name; \
    printf("%s\n", #name);

#endif
