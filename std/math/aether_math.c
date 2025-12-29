#include "aether_math.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

// Basic math operations
int aether_abs_int(int x) {
    return x < 0 ? -x : x;
}

float aether_abs_float(float x) {
    return fabsf(x);
}

int aether_min_int(int a, int b) {
    return a < b ? a : b;
}

int aether_max_int(int a, int b) {
    return a > b ? a : b;
}

float aether_min_float(float a, float b) {
    return a < b ? a : b;
}

float aether_max_float(float a, float b) {
    return a > b ? a : b;
}

// Advanced math
float aether_sqrt(float x) {
    return sqrtf(x);
}

float aether_pow(float base, float exp) {
    return powf(base, exp);
}

float aether_sin(float x) {
    return sinf(x);
}

float aether_cos(float x) {
    return cosf(x);
}

float aether_tan(float x) {
    return tanf(x);
}

float aether_floor(float x) {
    return floorf(x);
}

float aether_ceil(float x) {
    return ceilf(x);
}

float aether_round(float x) {
    return roundf(x);
}

// Random numbers
static int random_initialized = 0;

void aether_random_seed(unsigned int seed) {
    srand(seed);
    random_initialized = 1;
}

int aether_random_int(int min, int max) {
    if (!random_initialized) {
        aether_random_seed((unsigned int)time(NULL));
    }
    if (min >= max) return min;
    return min + (rand() % (max - min + 1));
}

float aether_random_float() {
    if (!random_initialized) {
        aether_random_seed((unsigned int)time(NULL));
    }
    return (float)rand() / (float)RAND_MAX;
}

