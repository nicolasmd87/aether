#include "aether_collections.h"
#include <stdlib.h>
#include <string.h>

struct ArrayList {
    void** items;
    int size;
    int capacity;
};

ArrayList* aether_list_new() {
    ArrayList* list = (ArrayList*)malloc(sizeof(ArrayList));
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
    return list;
}

void aether_list_add(ArrayList* list, void* item) {
    if (!list) return;
    
    if (list->size >= list->capacity) {
        int new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = (void**)realloc(list->items, new_capacity * sizeof(void*));
        list->capacity = new_capacity;
    }
    
    list->items[list->size++] = item;
}

void* aether_list_get(ArrayList* list, int index) {
    if (!list || index < 0 || index >= list->size) return NULL;
    return list->items[index];
}

void aether_list_set(ArrayList* list, int index, void* item) {
    if (!list || index < 0 || index >= list->size) return;
    list->items[index] = item;
}

int aether_list_size(ArrayList* list) {
    return list ? list->size : 0;
}

void aether_list_remove(ArrayList* list, int index) {
    if (!list || index < 0 || index >= list->size) return;
    
    for (int i = index; i < list->size - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    list->size--;
}

void aether_list_clear(ArrayList* list) {
    if (!list) return;
    list->size = 0;
}

void aether_list_free(ArrayList* list) {
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

static unsigned int hash_string(AetherString* key) {
    unsigned int hash = 5381;
    for (size_t i = 0; i < key->length; i++) {
        hash = ((hash << 5) + hash) + key->data[i];
    }
    return hash;
}

HashMap* aether_map_new() {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    map->capacity = HASHMAP_INITIAL_CAPACITY;
    map->size = 0;
    map->buckets = (HashMapEntry**)calloc(map->capacity, sizeof(HashMapEntry*));
    return map;
}

static void hashmap_resize(HashMap* map) {
    int old_capacity = map->capacity;
    HashMapEntry** old_buckets = map->buckets;
    
    map->capacity *= 2;
    map->buckets = (HashMapEntry**)calloc(map->capacity, sizeof(HashMapEntry*));
    map->size = 0;
    
    for (int i = 0; i < old_capacity; i++) {
        HashMapEntry* entry = old_buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            
            unsigned int index = hash_string(entry->key) % map->capacity;
            entry->next = map->buckets[index];
            map->buckets[index] = entry;
            map->size++;
            
            entry = next;
        }
    }
    
    free(old_buckets);
}

void aether_map_put(HashMap* map, AetherString* key, void* value) {
    if (!map || !key) return;
    
    if ((float)map->size / map->capacity > HASHMAP_LOAD_FACTOR) {
        hashmap_resize(map);
    }
    
    unsigned int index = hash_string(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];
    
    while (entry) {
        if (aether_string_equals(entry->key, key)) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    HashMapEntry* new_entry = (HashMapEntry*)malloc(sizeof(HashMapEntry));
    new_entry->key = key;
    aether_string_retain(key);
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->size++;
}

void* aether_map_get(HashMap* map, AetherString* key) {
    if (!map || !key) return NULL;
    
    unsigned int index = hash_string(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];
    
    while (entry) {
        if (aether_string_equals(entry->key, key)) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    return NULL;
}

int aether_map_has(HashMap* map, AetherString* key) {
    return aether_map_get(map, key) != NULL;
}

void aether_map_remove(HashMap* map, AetherString* key) {
    if (!map || !key) return;
    
    unsigned int index = hash_string(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];
    HashMapEntry* prev = NULL;
    
    while (entry) {
        if (aether_string_equals(entry->key, key)) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }
            
            aether_string_release(entry->key);
            free(entry);
            map->size--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

int aether_map_size(HashMap* map) {
    return map ? map->size : 0;
}

void aether_map_clear(HashMap* map) {
    if (!map) return;
    
    for (int i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            aether_string_release(entry->key);
            free(entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }
    map->size = 0;
}

void aether_map_free(HashMap* map) {
    if (!map) return;
    aether_map_clear(map);
    free(map->buckets);
    free(map);
}

MapKeys* aether_map_keys(HashMap* map) {
    if (!map) return NULL;
    
    MapKeys* keys = (MapKeys*)malloc(sizeof(MapKeys));
    keys->keys = (AetherString**)malloc(map->size * sizeof(AetherString*));
    keys->count = 0;
    
    for (int i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            keys->keys[keys->count++] = entry->key;
            entry = entry->next;
        }
    }
    
    return keys;
}

void aether_map_keys_free(MapKeys* keys) {
    if (!keys) return;
    free(keys->keys);
    free(keys);
}

