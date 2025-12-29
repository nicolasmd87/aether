#ifndef AETHER_COLLECTIONS_H
#define AETHER_COLLECTIONS_H

#include "../string/aether_string.h"
#include <stddef.h>

typedef struct ArrayList ArrayList;
typedef struct HashMap HashMap;

ArrayList* aether_list_new();
void aether_list_add(ArrayList* list, void* item);
void* aether_list_get(ArrayList* list, int index);
void aether_list_set(ArrayList* list, int index, void* item);
int aether_list_size(ArrayList* list);
void aether_list_remove(ArrayList* list, int index);
void aether_list_clear(ArrayList* list);
void aether_list_free(ArrayList* list);

HashMap* aether_map_new();
void aether_map_put(HashMap* map, AetherString* key, void* value);
void* aether_map_get(HashMap* map, AetherString* key);
int aether_map_has(HashMap* map, AetherString* key);
void aether_map_remove(HashMap* map, AetherString* key);
int aether_map_size(HashMap* map);
void aether_map_clear(HashMap* map);
void aether_map_free(HashMap* map);

typedef struct {
    AetherString** keys;
    int count;
} MapKeys;

MapKeys* aether_map_keys(HashMap* map);
void aether_map_keys_free(MapKeys* keys);

#endif

