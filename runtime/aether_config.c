/*
 * aether_config.c — Implementation of the public C ABI accessors declared
 * in aether_config.h.
 *
 * AetherValue* is a thin alias for the internal void* pointer that Aether's
 * map/list implementation hands around. The accessors delegate to the
 * existing map_get / list_get / list_size functions and reinterpret the
 * stored void* as the requested type.
 *
 * Because Aether's collections are untyped at the runtime level, these
 * functions trust the caller: if the script stored an int at "port" and
 * the host asks for a string, the host gets garbage. Document this in the
 * header and move on.
 */

#include "aether_config.h"
#include "../std/collections/aether_collections.h"
#include <stdint.h>
#include <stddef.h>

/* -----------------------------------------------------------------
 * Map accessors
 * ----------------------------------------------------------------- */

const char* aether_config_get_string(AetherValue* root, const char* key) {
    if (!root || !key) return NULL;
    return (const char*)map_get((HashMap*)root, key);
}

int32_t aether_config_get_int(AetherValue* root, const char* key, int32_t default_value) {
    if (!root || !key) return default_value;
    if (!map_has((HashMap*)root, key)) return default_value;
    return (int32_t)(intptr_t)map_get((HashMap*)root, key);
}

int64_t aether_config_get_int64(AetherValue* root, const char* key, int64_t default_value) {
    if (!root || !key) return default_value;
    if (!map_has((HashMap*)root, key)) return default_value;
    /* Aether stores int64 in a box or directly as intptr_t depending on the
     * platform. On 64-bit systems intptr_t is 64 bits so this is safe.
     * On 32-bit platforms the int64 would have been boxed via malloc — but
     * Aether's target is 64-bit for lib mode, so we assume intptr_t == int64_t.
     */
    return (int64_t)(intptr_t)map_get((HashMap*)root, key);
}

float aether_config_get_float(AetherValue* root, const char* key, float default_value) {
    if (!root || !key) return default_value;
    if (!map_has((HashMap*)root, key)) return default_value;
    /* Aether boxes float values as malloc'd float* when storing in an
     * untyped map. If the .ae code stored raw bits the caller gets
     * undefined behavior — documented in the header. */
    void* v = map_get((HashMap*)root, key);
    if (!v) return default_value;
    return *(float*)v;
}

int32_t aether_config_get_bool(AetherValue* root, const char* key, int32_t default_value) {
    if (!root || !key) return default_value;
    if (!map_has((HashMap*)root, key)) return default_value;
    return (int32_t)(intptr_t)map_get((HashMap*)root, key) ? 1 : 0;
}

AetherValue* aether_config_get_map(AetherValue* root, const char* key) {
    if (!root || !key) return NULL;
    return (AetherValue*)map_get((HashMap*)root, key);
}

AetherValue* aether_config_get_list(AetherValue* root, const char* key) {
    if (!root || !key) return NULL;
    return (AetherValue*)map_get((HashMap*)root, key);
}

int32_t aether_config_has(AetherValue* root, const char* key) {
    if (!root || !key) return 0;
    return map_has((HashMap*)root, key) ? 1 : 0;
}

/* -----------------------------------------------------------------
 * List accessors
 * ----------------------------------------------------------------- */

int32_t aether_config_list_size(AetherValue* list) {
    if (!list) return 0;
    return (int32_t)list_size((ArrayList*)list);
}

AetherValue* aether_config_list_get(AetherValue* list, int32_t index) {
    if (!list) return NULL;
    if (index < 0 || index >= list_size((ArrayList*)list)) return NULL;
    return (AetherValue*)list_get((ArrayList*)list, index);
}

const char* aether_config_list_get_string(AetherValue* list, int32_t index) {
    if (!list) return NULL;
    if (index < 0 || index >= list_size((ArrayList*)list)) return NULL;
    return (const char*)list_get((ArrayList*)list, index);
}

int32_t aether_config_list_get_int(AetherValue* list, int32_t index, int32_t default_value) {
    if (!list) return default_value;
    if (index < 0 || index >= list_size((ArrayList*)list)) return default_value;
    return (int32_t)(intptr_t)list_get((ArrayList*)list, index);
}

int64_t aether_config_list_get_int64(AetherValue* list, int32_t index, int64_t default_value) {
    if (!list) return default_value;
    if (index < 0 || index >= list_size((ArrayList*)list)) return default_value;
    return (int64_t)(intptr_t)list_get((ArrayList*)list, index);
}

float aether_config_list_get_float(AetherValue* list, int32_t index, float default_value) {
    if (!list) return default_value;
    if (index < 0 || index >= list_size((ArrayList*)list)) return default_value;
    void* v = list_get((ArrayList*)list, index);
    if (!v) return default_value;
    return *(float*)v;
}

int32_t aether_config_list_get_bool(AetherValue* list, int32_t index, int32_t default_value) {
    if (!list) return default_value;
    if (index < 0 || index >= list_size((ArrayList*)list)) return default_value;
    return (int32_t)(intptr_t)list_get((ArrayList*)list, index) ? 1 : 0;
}

/* -----------------------------------------------------------------
 * Lifetime
 *
 * The v1 convention: the root returned by the .ae script's entry function
 * is an ArrayList or HashMap (or something that points to one). Both
 * structures have dedicated free functions. Walking the tree is the
 * caller's responsibility — we can't free nested maps/lists from here
 * without a type tag. In practice, most `ae config` scripts build a
 * single-owner tree and the whole thing is released on free.
 *
 * Ownership rule: nested maps/lists inside the root are not freed by
 * this function; only the root's own storage is. This matches how the
 * Aether runtime itself handles these structures — collection cleanup
 * is per-container, not deep-recursive. Callers that need deep cleanup
 * walk the tree themselves, or structure their scripts to return a
 * flat map.
 * ----------------------------------------------------------------- */

void aether_config_free(AetherValue* root) {
    if (!root) return;
    /* We can't distinguish map from list at this level without a type tag.
     * For v1, assume the root is a HashMap — that's what Aether uses when
     * a script returns a structured config via `map_new()`. If callers
     * need list-root cleanup they can call list_free directly. */
    map_free((HashMap*)root);
}
