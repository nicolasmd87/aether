#ifndef AETHER_MATH_H
#define AETHER_MATH_H

// Basic math operations
int aether_abs_int(int x);
float aether_abs_float(float x);
int aether_min_int(int a, int b);
int aether_max_int(int a, int b);
float aether_min_float(float a, float b);
float aether_max_float(float a, float b);

// Advanced math
float aether_sqrt(float x);
float aether_pow(float base, float exp);
float aether_sin(float x);
float aether_cos(float x);
float aether_tan(float x);
float aether_floor(float x);
float aether_ceil(float x);
float aether_round(float x);

// Random numbers
void aether_random_seed(unsigned int seed);
int aether_random_int(int min, int max);
float aether_random_float();

// Constants
#define AETHER_PI 3.14159265358979323846
#define AETHER_E 2.71828182845904523536

#endif // AETHER_MATH_H

