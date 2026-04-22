#ifndef AETHER_COLLECTIONS_H
#define AETHER_COLLECTIONS_H

#include "../string/aether_string.h"
#include <stddef.h>

typedef struct ArrayList ArrayList;
typedef struct HashMap HashMap;

ArrayList* list_new();
int list_add_raw(ArrayList* list, void* item);
// Return element at `index`, or NULL for out-of-bounds / null list. The
// Aether wrapper `list.get` in std/collections/module.ae turns these into
// Go-style `(value, err)` returns.
void* list_get_raw(ArrayList* list, int index);
void list_set(ArrayList* list, int index, void* item);
int list_size(ArrayList* list);
void list_remove(ArrayList* list, int index);
void list_clear(ArrayList* list);
void list_free(ArrayList* list);

HashMap* map_new();
int map_put_raw(HashMap* map, const char* key, void* value);
// Return value for `key`, or NULL for absent / null-input. The Aether
// wrapper `map.get` distinguishes "absent" (null, "") from wrong-input
// (null, "null map").
void* map_get_raw(HashMap* map, const char* key);
int map_has(HashMap* map, const char* key);
void map_remove(HashMap* map, const char* key);
int map_size(HashMap* map);
void map_clear(HashMap* map);
void map_free(HashMap* map);

typedef struct {
    AetherString** keys;
    int count;
} MapKeys;

// Allocates a MapKeys snapshot; caller frees with map_keys_free. Returns
// NULL on allocation failure or null map. Aether wrapper `map.keys` gives
// a Go-style `(keys, err)` tuple.
MapKeys* map_keys_raw(HashMap* map);
void map_keys_free(MapKeys* keys);

#endif
