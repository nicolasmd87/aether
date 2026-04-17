/*
 * consume.c — List-return tests for --emit=lib.
 *
 * Verifies: int list, string list, empty list, out-of-range access.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>

#include "aether_config.h"

typedef AetherValue* (*list_fn)(void);

#define FAIL(fmt, ...) do { \
    fprintf(stderr, "FAIL (line %d): " fmt "\n", __LINE__, ##__VA_ARGS__); \
    return 1; \
} while (0)

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) FAIL("dlopen: %s", dlerror());

    list_fn primes   = (list_fn)dlsym(h, "aether_primes_up_to_20");
    list_fn weekdays = (list_fn)dlsym(h, "aether_weekdays");
    list_fn empty    = (list_fn)dlsym(h, "aether_empty_list");
    if (!primes || !weekdays || !empty) FAIL("symbol lookup failed: %s", dlerror());

    /* Int list */
    AetherValue* p = primes();
    if (!p) FAIL("primes_up_to_20 returned NULL");
    if (aether_config_list_size(p) != 8) FAIL("primes size = %d, expected 8", aether_config_list_size(p));
    int32_t expected[8] = {2, 3, 5, 7, 11, 13, 17, 19};
    for (int i = 0; i < 8; i++) {
        int32_t got = aether_config_list_get_int(p, i, -1);
        if (got != expected[i]) FAIL("primes[%d] = %d, expected %d", i, got, expected[i]);
    }

    /* String list */
    AetherValue* w = weekdays();
    if (!w) FAIL("weekdays returned NULL");
    if (aether_config_list_size(w) != 5) FAIL("weekdays size = %d, expected 5", aether_config_list_size(w));
    const char* days[5] = {"Mon", "Tue", "Wed", "Thu", "Fri"};
    for (int i = 0; i < 5; i++) {
        const char* s = aether_config_list_get_string(w, i);
        if (!s || strcmp(s, days[i]) != 0) FAIL("weekdays[%d] = %s, expected %s", i, s ? s : "(null)", days[i]);
    }

    /* Empty list */
    AetherValue* e = empty();
    if (!e) FAIL("empty_list returned NULL");
    if (aether_config_list_size(e) != 0) FAIL("empty list size = %d, expected 0", aether_config_list_size(e));
    if (aether_config_list_get_string(e, 0) != NULL) FAIL("get from empty list didn't return NULL");

    /* Out-of-range */
    if (aether_config_list_get_int(p, 99, 777) != 777) FAIL("OOR int didn't return default");
    if (aether_config_list_get_string(w, 99) != NULL)  FAIL("OOR string didn't return NULL");
    if (aether_config_list_get_int(p, -1, 42) != 42)   FAIL("negative index didn't return default");

    dlclose(h);
    printf("OK: list returns round-tripped\n");
    return 0;
}
