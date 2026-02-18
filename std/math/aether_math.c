#include "aether_math.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

// Basic math operations
int abs_int(int x) {
    return x < 0 ? -x : x;
}

float abs_float(float x) {
    return fabsf(x);
}

int min_int(int a, int b) {
    return a < b ? a : b;
}

int max_int(int a, int b) {
    return a > b ? a : b;
}

float min_float(float a, float b) {
    return a < b ? a : b;
}

float max_float(float a, float b) {
    return a > b ? a : b;
}

int clamp_int(int x, int min, int max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

float clamp_float(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

// Advanced math
float math_sqrt(float x) {
    return sqrtf(x);
}

float math_pow(float base, float exp) {
    return powf(base, exp);
}

float math_sin(float x) {
    return sinf(x);
}

float math_cos(float x) {
    return cosf(x);
}

float math_tan(float x) {
    return tanf(x);
}

float math_asin(float x) {
    return asinf(x);
}

float math_acos(float x) {
    return acosf(x);
}

float math_atan(float x) {
    return atanf(x);
}

float math_atan2(float y, float x) {
    return atan2f(y, x);
}

float math_floor(float x) {
    return floorf(x);
}

float math_ceil(float x) {
    return ceilf(x);
}

float math_round(float x) {
    return roundf(x);
}

float math_log(float x) {
    return logf(x);
}

float math_log10(float x) {
    return log10f(x);
}

float math_exp(float x) {
    return expf(x);
}

// Random numbers
static int random_initialized = 0;

void random_seed(unsigned int seed) {
    srand(seed);
    random_initialized = 1;
}

int random_int(int min, int max) {
    if (!random_initialized) {
        random_seed((unsigned int)time(NULL));
    }
    if (min >= max) return min;
    return min + (rand() % (max - min + 1));
}

float random_float(void) {
    if (!random_initialized) {
        random_seed((unsigned int)time(NULL));
    }
    return (float)rand() / (float)RAND_MAX;
}
