#include "test_harness.h"
#include "../std/math/aether_math.h"
#include <math.h>

TEST(math_sqrt_positive) {
    double result = aether_sqrt(16.0);
    ASSERT_EQ(4, (int)result);
}

TEST(math_sqrt_zero) {
    double result = aether_sqrt(0.0);
    ASSERT_EQ(0, (int)result);
}

TEST(math_pow_positive) {
    double result = aether_pow(2.0, 3.0);
    ASSERT_EQ(8, (int)result);
}

TEST(math_pow_zero_exponent) {
    double result = aether_pow(5.0, 0.0);
    ASSERT_EQ(1, (int)result);
}

TEST(math_abs_positive) {
    int result = aether_abs(42);
    ASSERT_EQ(42, result);
}

TEST(math_abs_negative) {
    int result = aether_abs(-42);
    ASSERT_EQ(42, result);
}

TEST(math_abs_zero) {
    int result = aether_abs(0);
    ASSERT_EQ(0, result);
}

TEST(math_min_basic) {
    int result = aether_min(5, 10);
    ASSERT_EQ(5, result);
}

TEST(math_min_equal) {
    int result = aether_min(7, 7);
    ASSERT_EQ(7, result);
}

TEST(math_max_basic) {
    int result = aether_max(5, 10);
    ASSERT_EQ(10, result);
}

TEST(math_max_equal) {
    int result = aether_max(7, 7);
    ASSERT_EQ(7, result);
}

TEST(math_sin_zero) {
    double result = aether_sin(0.0);
    ASSERT_EQ(0, (int)result);
}

TEST(math_cos_zero) {
    double result = aether_cos(0.0);
    ASSERT_EQ(1, (int)result);
}

TEST(math_floor_positive) {
    double result = aether_floor(3.7);
    ASSERT_EQ(3, (int)result);
}

TEST(math_floor_negative) {
    double result = aether_floor(-3.7);
    ASSERT_EQ(-4, (int)result);
}

TEST(math_ceil_positive) {
    double result = aether_ceil(3.2);
    ASSERT_EQ(4, (int)result);
}

TEST(math_ceil_negative) {
    double result = aether_ceil(-3.2);
    ASSERT_EQ(-3, (int)result);
}

TEST(math_round_positive_up) {
    double result = aether_round(3.6);
    ASSERT_EQ(4, (int)result);
}

TEST(math_round_positive_down) {
    double result = aether_round(3.4);
    ASSERT_EQ(3, (int)result);
}

TEST(math_random_int_range) {
    // Test random integer generation
    for (int i = 0; i < 10; i++) {
        int result = aether_random_int(1, 10);
        ASSERT_TRUE(result >= 1 && result <= 10);
    }
}

TEST(math_random_int_single) {
    int result = aether_random_int(5, 5);
    ASSERT_EQ(5, result);
}

