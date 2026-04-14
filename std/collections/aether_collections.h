#ifndef AETHER_COLLECTIONS_H
#define AETHER_COLLECTIONS_H

#include "../string/aether_string.h"
#include <stddef.h>

typedef struct ArrayList ArrayList;
typedef struct HashMap HashMap;

ArrayList* list_new();
int list_add_raw(ArrayList* list, void* item);
void* list_get(ArrayList* list, int index);
void list_set(ArrayList* list, int index, void* item);
int list_size(ArrayList* list);
void list_remove(ArrayList* list, int index);
void list_clear(ArrayList* list);
void list_free(ArrayList* list);

HashMap* map_new();
int map_put_raw(HashMap* map, const char* key, void* value);
void* map_get(HashMap* map, const char* key);
int map_has(HashMap* map, const char* key);
void map_remove(HashMap* map, const char* key);
int map_size(HashMap* map);
void map_clear(HashMap* map);
void map_free(HashMap* map);

typedef struct {
    AetherString** keys;
    int count;
} MapKeys;

MapKeys* map_keys(HashMap* map);
void map_keys_free(MapKeys* keys);

#endif
