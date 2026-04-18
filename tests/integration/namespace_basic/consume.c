/*
 * consume.c — End-to-end namespace round-trip test.
 *
 * Verifies that `ae build --namespace` produces a .so where:
 *   - aether_describe() reports the correct manifest
 *   - exported aether_<name>() functions are callable
 *   - registered event handlers receive notify() calls
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "aether_host.h"

typedef const AetherNamespaceManifest* (*describe_fn)(void);
typedef const char* (*say_hi_fn)(const char*);

#define FAIL(fmt, ...) do { \
    fprintf(stderr, "FAIL (line %d): " fmt "\n", __LINE__, ##__VA_ARGS__); \
    return 1; \
} while (0)

static int64_t g_last_id = -1;
static void on_greeted(int64_t id) { g_last_id = id; }

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <lib>\n", argv[0]); return 2; }

    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) FAIL("dlopen: %s", dlerror());

    describe_fn describe = (describe_fn)dlsym(h, "aether_describe");
    say_hi_fn say_hi    = (say_hi_fn)dlsym(h, "aether_say_hi");
    if (!describe) FAIL("aether_describe missing: %s", dlerror());
    if (!say_hi)   FAIL("aether_say_hi missing: %s", dlerror());

    /* aether_describe matches what manifest.ae declared. */
    const AetherNamespaceManifest* m = describe();
    if (!m) FAIL("aether_describe returned NULL");
    if (!m->namespace_name || strcmp(m->namespace_name, "greet") != 0)
        FAIL("namespace = %s, expected \"greet\"", m->namespace_name ? m->namespace_name : "(null)");
    if (m->input_count != 1) FAIL("input_count = %d, expected 1", m->input_count);
    if (strcmp(m->inputs[0].name, "greeting") != 0)
        FAIL("input[0].name = %s", m->inputs[0].name);
    if (strcmp(m->inputs[0].type_signature, "string") != 0)
        FAIL("input[0].type = %s", m->inputs[0].type_signature);
    if (m->event_count != 1) FAIL("event_count = %d, expected 1", m->event_count);
    if (strcmp(m->events[0].name, "Greeted") != 0)
        FAIL("event[0].name = %s", m->events[0].name);
    if (strcmp(m->java.package_name, "com.example.greet") != 0)
        FAIL("java.package = %s", m->java.package_name);
    if (strcmp(m->java.class_name, "Greeter") != 0)
        FAIL("java.class = %s", m->java.class_name);

    /* Round-trip: registered handler fires, exported function returns. */
    aether_event_register("Greeted", on_greeted);
    const char* r = say_hi("alice");
    if (!r || strcmp(r, "alice") != 0) FAIL("say_hi returned %s", r ? r : "(null)");
    if (g_last_id != 42) FAIL("Greeted handler last_id = %lld, expected 42", (long long)g_last_id);

    dlclose(h);
    printf("OK: namespace_basic — describe, downcall, notify\n");
    return 0;
}
