/*
 * aether_host.c — Implementation of the host-callback dispatch table.
 *
 * Single linear-search registry capped at AETHER_HOST_MAX_EVENTS. Linear
 * search keeps the data layout obvious and is fine for small N — most
 * namespaces declare 5-20 events, not hundreds. If a namespace ever
 * needs more, the cap can be raised; a hash table would be premature.
 */

#include "aether_host.h"
#include <string.h>

#ifndef AETHER_HOST_MAX_EVENTS
#define AETHER_HOST_MAX_EVENTS 64
#endif

typedef struct {
    /* event_name is a borrowed pointer — the caller (typically the host
     * Java/Python/Go SDK) owns the storage for the lifetime of the
     * registration. The generated host SDKs use string literals, so
     * lifetime is the program's lifetime in practice. */
    const char* event_name;
    aether_event_handler_t handler;
} EventEntry;

static EventEntry g_events[AETHER_HOST_MAX_EVENTS];
static int        g_event_count = 0;

static int find_event_index(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < g_event_count; i++) {
        if (g_events[i].event_name &&
            strcmp(g_events[i].event_name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int aether_event_register(const char* event_name, aether_event_handler_t handler) {
    if (!event_name || !handler) return -1;

    /* Replace existing entry if the name is already registered. */
    int existing = find_event_index(event_name);
    if (existing >= 0) {
        g_events[existing].handler = handler;
        return 0;
    }

    if (g_event_count >= AETHER_HOST_MAX_EVENTS) return -1;

    g_events[g_event_count].event_name = event_name;
    g_events[g_event_count].handler    = handler;
    g_event_count++;
    return 0;
}

int aether_event_unregister(const char* event_name) {
    int idx = find_event_index(event_name);
    if (idx < 0) return -1;

    /* Compact the array — preserves order, which we don't promise but
     * also don't deny. Cheap at small N. */
    for (int i = idx; i < g_event_count - 1; i++) {
        g_events[i] = g_events[i + 1];
    }
    g_event_count--;
    g_events[g_event_count].event_name = NULL;
    g_events[g_event_count].handler    = NULL;
    return 0;
}

void aether_event_clear(void) {
    for (int i = 0; i < g_event_count; i++) {
        g_events[i].event_name = NULL;
        g_events[i].handler    = NULL;
    }
    g_event_count = 0;
}

int notify(const char* event_name, int64_t id) {
    int idx = find_event_index(event_name);
    if (idx < 0) return 0;
    g_events[idx].handler(id);
    return 1;
}

/* ---------------------------------------------------------------------
 * Manifest registry
 *
 * String fields are borrowed pointers into Aether's interned-string
 * storage. The manifest .ae script lives for the lifetime of the
 * compile; the pipeline reads g_manifest before the script is freed.
 * --------------------------------------------------------------------- */

static AetherManifest g_manifest = {0};

/* Each builder ignores _ctx — the manifest registry is global state.
 * The _ctx parameter is here purely to satisfy the codegen's auto-
 * injection rule so the manifest DSL reads idiomatically inside a
 * trailing block: `abi() { describe("trading") { input(...) } }`. */

void describe(void* _ctx, const char* name) {
    (void)_ctx;
    g_manifest.namespace_name = name;
}

void input(void* _ctx, const char* name, const char* type_signature) {
    (void)_ctx;
    if (g_manifest.input_count >= AETHER_MANIFEST_MAX_INPUTS) return;
    g_manifest.inputs[g_manifest.input_count].name           = name;
    g_manifest.inputs[g_manifest.input_count].type_signature = type_signature;
    g_manifest.input_count++;
}

void event(void* _ctx, const char* name, const char* carries_type) {
    (void)_ctx;
    if (g_manifest.event_count >= AETHER_MANIFEST_MAX_EVENTS) return;
    g_manifest.events[g_manifest.event_count].name         = name;
    g_manifest.events[g_manifest.event_count].carries_type = carries_type;
    g_manifest.event_count++;
}

void bindings(void* _ctx) {
    (void)_ctx;
    /* Visual grouping; no state to mutate. */
}

void java(void* _ctx, const char* package_name, const char* class_name) {
    (void)_ctx;
    g_manifest.java.package_name = package_name;
    g_manifest.java.class_name   = class_name;
}

void python(void* _ctx, const char* module_name) {
    (void)_ctx;
    g_manifest.python.module_name = module_name;
}

void ruby(void* _ctx, const char* module_name) {
    (void)_ctx;
    g_manifest.ruby.module_name = module_name;
}

void go(void* _ctx, const char* package_name) {
    (void)_ctx;
    g_manifest.go.package_name = package_name;
}

AetherManifest* manifest_get(void) {
    /* Return NULL when nothing has been declared so callers can
     * distinguish "no manifest run" from "empty manifest". */
    if (!g_manifest.namespace_name) return NULL;
    return &g_manifest;
}

void manifest_clear(void) {
    /* memset is safe here: the strings we hold are borrowed pointers
     * we never owned. The Aether-side string storage takes care of
     * its own lifetime. */
    AetherManifest empty = {0};
    g_manifest = empty;
}
