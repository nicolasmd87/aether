#include "aether_collections.h"
#include <stdlib.h>
#include <string.h>

struct ArrayList {
    void** items;
    int size;
    int capacity;
};

ArrayList* list_new() {
    ArrayList* list = (ArrayList*)malloc(sizeof(ArrayList));
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
    return list;
}

// list_add_raw returns 1 on success, 0 on failure (realloc error or
// null list). The Aether wrapper `list.add` in std.list/std.collections
// turns the 0 into an error string.
int list_add_raw(ArrayList* list, void* item) {
    if (!list) return 0;

    if (list->size >= list->capacity) {
        int new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        void** new_items = (void**)realloc(list->items, new_capacity * sizeof(void*));
        if (!new_items) return 0;
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->size++] = item;
    return 1;
}

void* list_get(ArrayList* list, int index) {
    if (!list || index < 0 || index >= list->size) return NULL;
    return list->items[index];
}

void list_set(ArrayList* list, int index, void* item) {
    if (!list || index < 0 || index >= list->size) return;
    list->items[index] = item;
}

int list_size(ArrayList* list) {
    return list ? list->size : 0;
}

void list_remove(ArrayList* list, int index) {
    if (!list || index < 0 || index >= list->size) return;

    for (int i = index; i < list->size - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    list->size--;
}

void list_clear(ArrayList* list) {
    if (!list) return;
    list->size = 0;
}

void list_free(ArrayList* list) {
    if (!list) return;
    if (list->items) free(list->items);
    free(list);
}

#define HASHMAP_INITIAL_CAPACITY 16
#define HASHMAP_LOAD_FACTOR 0.75

typedef struct HashMapEntry {
    AetherString* key;
    void* value;
    struct HashMapEntry* next;
} HashMapEntry;

struct HashMap {
    HashMapEntry** buckets;
    int capacity;
    int size;
};

static unsigned int hash_cstr(const char* key) {
    unsigned int hash = 5381;
    for (const char* p = key; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }
    return hash;
}

static int key_equals(AetherString* a, const char* b) {
    if (!a || !b) return 0;
    return strcmp(a->data, b) == 0;
}

HashMap* map_new() {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    map->capacity = HASHMAP_INITIAL_CAPACITY;
    map->size = 0;
    map->buckets = (HashMapEntry**)calloc(map->capacity, sizeof(HashMapEntry*));
    return map;
}

static void hashmap_resize(HashMap* map) {
    int old_capacity = map->capacity;
    HashMapEntry** old_buckets = map->buckets;
    int new_capacity = map->capacity * 2;

    HashMapEntry** new_buckets = (HashMapEntry**)calloc(new_capacity, sizeof(HashMapEntry*));
    if (!new_buckets) return;  // Keep existing map on alloc failure

    map->capacity = new_capacity;
    map->buckets = new_buckets;
    map->size = 0;

    for (int i = 0; i < old_capacity; i++) {
        HashMapEntry* entry = old_buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;

            unsigned int index = hash_cstr(entry->key->data) % map->capacity;
            entry->next = map->buckets[index];
            map->buckets[index] = entry;
            map->size++;

            entry = next;
        }
    }

    free(old_buckets);
}

// map_put_raw returns 1 on success, 0 on failure (null map/key or OOM).
// The Aether wrapper `map.put` in std.map/std.collections turns the 0
// into an error string.
int map_put_raw(HashMap* map, const char* key, void* value) {
    if (!map || !key) return 0;

    if ((float)map->size / map->capacity > HASHMAP_LOAD_FACTOR) {
        hashmap_resize(map);
    }

    unsigned int index = hash_cstr(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (key_equals(entry->key, key)) {
            entry->value = value;
            return 1;
        }
        entry = entry->next;
    }

    HashMapEntry* new_entry = (HashMapEntry*)malloc(sizeof(HashMapEntry));
    if (!new_entry) return 0;
    new_entry->key = string_new(key);
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->size++;
    return 1;
}

void* map_get(HashMap* map, const char* key) {
    if (!map || !key) return NULL;

    unsigned int index = hash_cstr(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (key_equals(entry->key, key)) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

int map_has(HashMap* map, const char* key) {
    return map_get(map, key) != NULL;
}

void map_remove(HashMap* map, const char* key) {
    if (!map || !key) return;

    unsigned int index = hash_cstr(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];
    HashMapEntry* prev = NULL;

    while (entry) {
        if (key_equals(entry->key, key)) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }

            string_release(entry->key);
            free(entry);
            map->size--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

int map_size(HashMap* map) {
    return map ? map->size : 0;
}

void map_clear(HashMap* map) {
    if (!map) return;

    for (int i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            string_release(entry->key);
            free(entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }
    map->size = 0;
}

void map_free(HashMap* map) {
    if (!map) return;
    map_clear(map);
    free(map->buckets);
    free(map);
}

MapKeys* map_keys(HashMap* map) {
    if (!map) return NULL;

    MapKeys* keys = (MapKeys*)malloc(sizeof(MapKeys));
    if (!keys) return NULL;
    keys->count = 0;
    if (map->size == 0) {
        keys->keys = NULL;
        return keys;
    }
    keys->keys = (AetherString**)malloc(map->size * sizeof(AetherString*));
    if (!keys->keys) { free(keys); return NULL; }

    for (int i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            keys->keys[keys->count++] = entry->key;
            entry = entry->next;
        }
    }

    return keys;
}

void map_keys_free(MapKeys* keys) {
    if (!keys) return;
    free(keys->keys);
    free(keys);
}

