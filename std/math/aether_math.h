#ifndef AETHER_MATH_H
#define AETHER_MATH_H

// Basic math operations
int abs_int(int x);
float abs_float(float x);
int min_int(int a, int b);
int max_int(int a, int b);
float min_float(float a, float b);
float max_float(float a, float b);
int clamp_int(int x, int min, int max);
float clamp_float(float x, float min, float max);

// Advanced math
float math_sqrt(float x);
float math_pow(float base, float exp);
float math_sin(float x);
float math_cos(float x);
float math_tan(float x);
float math_asin(float x);
float math_acos(float x);
float math_atan(float x);
float math_atan2(float y, float x);
float math_floor(float x);
float math_ceil(float x);
float math_round(float x);
float math_log(float x);
float math_log10(float x);
float math_exp(float x);

// Random numbers
void random_seed(unsigned int seed);
int random_int(int min, int max);
float random_float(void);

// Constants
#define PI 3.14159265358979323846
#define E 2.71828182845904523536

#endif // AETHER_MATH_H
