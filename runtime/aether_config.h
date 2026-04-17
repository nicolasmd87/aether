/*
 * aether_config.h — Public C ABI for consuming Aether-built shared libraries.
 *
 * Include this header on the host (C, C++, or via FFI from Java/Python/etc.)
 * when calling into a shared library produced by `aetherc --emit=lib`.
 *
 * The exported Aether functions are named `aether_<name>`; their signatures
 * are the ones emitted by the compiler from the user's .ae source. Primitive
 * types (int, int64, float, bool, string) cross the boundary directly.
 * Composite types (ptr, list, map) come back as opaque AetherValue* handles
 * and MUST be walked using the accessor functions declared below.
 *
 * SWIG: this header is consumed by aether_config.i. Keep declarations simple
 * (no inline, no macros, no function pointers) so SWIG can generate wrappers
 * for Java, Python, Ruby, Go, etc. without additional typemaps.
 *
 * Ownership convention in comments:
 *   Owned    — caller MUST call aether_config_free() on this handle.
 *   Borrowed — points into a parent tree; valid until the tree's root is freed.
 */

#ifndef AETHER_CONFIG_H
#define AETHER_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque handle for Aether composite values (map, list, or generic ptr).
 * The internal representation is an implementation detail — never dereference
 * or sizeof() this directly.
 */
typedef struct AetherValue AetherValue;

/* ------------------------------------------------------------------
 * Map accessors
 *
 * Maps in Aether are string-keyed and untyped — values are stored as
 * void* pointers with no runtime type tag. The accessors reinterpret the
 * stored value as the requested type; the caller must know what type the
 * .ae script placed at each key.
 *
 * Contract: if a key is missing, the typed getters return the supplied
 * default_value (or NULL for string/map/list returns). If the key is
 * present but the stored value wasn't of the expected type, behavior is
 * undefined (typical: garbage in/garbage out, or segfault for strings
 * reinterpreted from integers).
 *
 * For type safety, prefer returning a concrete primitive from the Aether
 * function rather than burying it in a map.
 * ------------------------------------------------------------------ */

/* Borrowed: the returned string lives in the parent map until it's freed.
 * Returns NULL if the key is missing. */
const char* aether_config_get_string(AetherValue* root, const char* key);

/* Returns default_value if the key is missing. */
int32_t     aether_config_get_int(AetherValue* root, const char* key, int32_t default_value);
int64_t     aether_config_get_int64(AetherValue* root, const char* key, int64_t default_value);

/* Returns default_value if the key is missing. */
float       aether_config_get_float(AetherValue* root, const char* key, float default_value);

/* Returns 0/1 for false/true; returns default_value if the key is missing. */
int32_t     aether_config_get_bool(AetherValue* root, const char* key, int32_t default_value);

/* Borrowed: nested map handle tied to the parent's lifetime.
 * Returns NULL if the key is missing. */
AetherValue* aether_config_get_map(AetherValue* root, const char* key);

/* Borrowed: nested list handle tied to the parent's lifetime.
 * Returns NULL if the key is missing. */
AetherValue* aether_config_get_list(AetherValue* root, const char* key);

/* Returns 1 if the key is present in the map, 0 otherwise. */
int32_t     aether_config_has(AetherValue* root, const char* key);

/* ------------------------------------------------------------------
 * List accessors
 *
 * Lists are 0-indexed. Like maps, they're untyped internally — the caller
 * must know what each element's actual type is. Out-of-range access
 * returns the appropriate default (NULL / 0 / default_value).
 * ------------------------------------------------------------------ */

/* Returns the number of elements in the list, or 0 if list is NULL. */
int32_t     aether_config_list_size(AetherValue* list);

/* Borrowed: returns a handle valid until the list's root is freed.
 * Returns NULL if index is out of range. */
AetherValue* aether_config_list_get(AetherValue* list, int32_t index);

/* Typed convenience getters for list elements. Semantics match the map
 * getters: the value is reinterpret-cast to the requested type without
 * a runtime type check. */
const char* aether_config_list_get_string(AetherValue* list, int32_t index);
int32_t     aether_config_list_get_int(AetherValue* list, int32_t index, int32_t default_value);
int64_t     aether_config_list_get_int64(AetherValue* list, int32_t index, int64_t default_value);
float       aether_config_list_get_float(AetherValue* list, int32_t index, float default_value);
int32_t     aether_config_list_get_bool(AetherValue* list, int32_t index, int32_t default_value);

/* ------------------------------------------------------------------
 * Lifetime
 *
 * The root handle returned by a top-level `aether_<name>(...)` call that
 * produces a map or list is Owned. Free it with aether_config_free().
 *
 * Handles obtained via aether_config_get_map, _get_list, or _list_get
 * are Borrowed — do NOT free them individually. They become invalid the
 * moment the root is freed.
 * ------------------------------------------------------------------ */

/* Frees the tree rooted at `root`. Passing NULL is a no-op. */
void aether_config_free(AetherValue* root);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* AETHER_CONFIG_H */
