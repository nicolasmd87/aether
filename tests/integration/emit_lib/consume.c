/*
 * consume.c — Minimal C host for the --emit=lib round-trip test.
 *
 * dlopens the shared library built from config.ae, resolves the aether_*
 * symbols, and calls them. Prints OK on success, FAIL with context on
 * any mismatch.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>

typedef int32_t (*aether_sum_fn)(int32_t, int32_t);
typedef const char* (*aether_greet_fn)(const char*);
typedef int32_t (*aether_multiply_by_two_fn)(int32_t);

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <path-to-libconfig.so>\n", argv[0]);
        return 2;
    }

    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) {
        fprintf(stderr, "FAIL: dlopen(%s): %s\n", argv[1], dlerror());
        return 1;
    }

    aether_sum_fn sum = (aether_sum_fn)dlsym(h, "aether_sum");
    aether_greet_fn greet = (aether_greet_fn)dlsym(h, "aether_greet");
    aether_multiply_by_two_fn mul2 = (aether_multiply_by_two_fn)dlsym(h, "aether_multiply_by_two");

    if (!sum)   { fprintf(stderr, "FAIL: aether_sum not found: %s\n", dlerror()); return 1; }
    if (!greet) { fprintf(stderr, "FAIL: aether_greet not found: %s\n", dlerror()); return 1; }
    if (!mul2)  { fprintf(stderr, "FAIL: aether_multiply_by_two not found: %s\n", dlerror()); return 1; }

    int32_t s = sum(7, 35);
    if (s != 42) {
        fprintf(stderr, "FAIL: aether_sum(7, 35) = %d, expected 42\n", s);
        return 1;
    }

    const char* g = greet("hello");
    if (!g || strcmp(g, "hello") != 0) {
        fprintf(stderr, "FAIL: aether_greet(\"hello\") = %s, expected \"hello\"\n", g ? g : "(null)");
        return 1;
    }

    int32_t m = mul2(21);
    if (m != 42) {
        fprintf(stderr, "FAIL: aether_multiply_by_two(21) = %d, expected 42\n", m);
        return 1;
    }

    dlclose(h);
    printf("OK: all three aether_* entry points round-tripped\n");
    return 0;
}
