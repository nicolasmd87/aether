#include "aether_collections.h"
#include "../../runtime/aether_resource_caps.h"
#include <stdlib.h>
#include <string.h>

struct ArrayList {
    void** items;
    int size;
    int capacity;
};

ArrayList* list_new() {
    /* #343: cap-aware. Items array is allocated lazily on first
     * add — only the struct is accounted here. */
    ArrayList* list = (ArrayList*)aether_caps_malloc(sizeof(ArrayList));
    if (!list) return NULL;
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
        size_t old_bytes = (size_t)list->capacity * sizeof(void*);
        size_t new_bytes = (size_t)new_capacity * sizeof(void*);
        void** new_items = (void**)aether_caps_realloc(list->items,
                                                       old_bytes, new_bytes);
        if (!new_items) return 0;
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->size++] = item;
    return 1;
}

void* list_get_raw(ArrayList* list, int index) {
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
    /* Items array's allocated bytes = capacity × sizeof(void*).
     * Struct is sizeof(ArrayList). Both pair with the alloc-side
     * accounting in list_new + list_add_raw. */
    if (list->items) {
        aether_caps_free(list->items,
                         (size_t)list->capacity * sizeof(void*));
    }
    aether_caps_free(list, sizeof(ArrayList));
}

#define HASHMAP_INITIAL_CAPACITY 16
// Load factor = 3/4; expressed as an integer comparison below so we
// don't do FP arithmetic on every put.
#define HASHMAP_LOAD_NUMERATOR   3
#define HASHMAP_LOAD_DENOMINATOR 4

typedef struct HashMapEntry {
    AetherString*         key;
    void*                 value;
    struct HashMapEntry*  next;
    unsigned int          hash;    // cached so resize doesn't recompute
    unsigned int          key_len; // cached so key_equals can prefilter
} HashMapEntry;

struct HashMap {
    HashMapEntry** buckets;
    int capacity;
    int size;
};

// djb2. Also returns length via an out-param so callers don't walk the
// key twice (once to hash, once to strlen).
static unsigned int hash_cstr_len(const char* key, unsigned int* out_len) {
    unsigned int hash = 5381;
    const char* p = key;
    while (*p) {
        hash = ((hash << 5) + hash) + (unsigned char)*p;
        p++;
    }
    if (out_len) *out_len = (unsigned int)(p - key);
    return hash;
}

static unsigned int hash_cstr(const char* key) {
    return hash_cstr_len(key, NULL);
}

// Fast equality: cheap length compare first, memcmp only on match.
static int key_equals(const HashMapEntry* e, const char* b, unsigned int b_len) {
    if (!e || !e->key || !b) return 0;
    if (e->key_len != b_len) return 0;
    return memcmp(e->key->data, b, b_len) == 0;
}

HashMap* map_new() {
    HashMap* map = (HashMap*)aether_caps_malloc(sizeof(HashMap));
    if (!map) return NULL;
    map->capacity = HASHMAP_INITIAL_CAPACITY;
    map->size = 0;
    map->buckets = (HashMapEntry**)aether_caps_calloc(
        (size_t)map->capacity, sizeof(HashMapEntry*));
    if (!map->buckets) { aether_caps_free(map, sizeof(HashMap)); return NULL; }
    return map;
}

static void hashmap_resize(HashMap* map) {
    int old_capacity = map->capacity;
    HashMapEntry** old_buckets = map->buckets;
    int new_capacity = map->capacity * 2;

    HashMapEntry** new_buckets = (HashMapEntry**)aether_caps_calloc(
        (size_t)new_capacity, sizeof(HashMapEntry*));
    if (!new_buckets) return;  // Keep existing map on alloc failure

    map->capacity = new_capacity;
    map->buckets = new_buckets;
    // map->size stays the same — we're moving the same entries into
    // new buckets, not adding new ones.

    for (int i = 0; i < old_capacity; i++) {
        HashMapEntry* entry = old_buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            // Use cached hash; avoid walking the key string again.
            unsigned int index = entry->hash % (unsigned int)map->capacity;
            entry->next = map->buckets[index];
            map->buckets[index] = entry;
            entry = next;
        }
    }

    aether_caps_free(old_buckets,
                     (size_t)old_capacity * sizeof(HashMapEntry*));
}

// map_put_raw returns 1 on success, 0 on failure (null map/key or OOM).
// The Aether wrapper `map.put` in std.map/std.collections turns the 0
// into an error string.
int map_put_raw(HashMap* map, const char* key, void* value) {
    if (!map || !key) return 0;

    // Integer load-factor check: resize when size/capacity > 3/4.
    if ((long)map->size * HASHMAP_LOAD_DENOMINATOR >
        (long)map->capacity * HASHMAP_LOAD_NUMERATOR) {
        hashmap_resize(map);
    }

    unsigned int key_len = 0;
    unsigned int hash = hash_cstr_len(key, &key_len);
    unsigned int index = hash % (unsigned int)map->capacity;
    HashMapEntry* entry = map->buckets[index];

    while (entry) {
        // Hash check is a cheap prefilter before key_equals (which
        // still runs memcmp for false hash collisions).
        if (entry->hash == hash && key_equals(entry, key, key_len)) {
            entry->value = value;
            return 1;
        }
        entry = entry->next;
    }

    HashMapEntry* new_entry = (HashMapEntry*)aether_caps_malloc(sizeof(HashMapEntry));
    if (!new_entry) return 0;
    new_entry->key = string_new(key);
    if (!new_entry->key) { aether_caps_free(new_entry, sizeof(HashMapEntry)); return 0; }
    new_entry->value   = value;
    new_entry->hash    = hash;
    new_entry->key_len = key_len;
    new_entry->next    = map->buckets[index];
    map->buckets[index] = new_entry;
    map->size++;
    return 1;
}

void* map_get_raw(HashMap* map, const char* key) {
    if (!map || !key) return NULL;

    unsigned int key_len = 0;
    unsigned int hash = hash_cstr_len(key, &key_len);
    unsigned int index = hash % (unsigned int)map->capacity;
    HashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (entry->hash == hash && key_equals(entry, key, key_len)) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

int map_has(HashMap* map, const char* key) {
    return map_get_raw(map, key) != NULL;
}

void map_remove(HashMap* map, const char* key) {
    if (!map || !key) return;

    unsigned int key_len = 0;
    unsigned int hash = hash_cstr_len(key, &key_len);
    unsigned int index = hash % (unsigned int)map->capacity;
    HashMapEntry* entry = map->buckets[index];
    HashMapEntry* prev = NULL;

    while (entry) {
        if (entry->hash == hash && key_equals(entry, key, key_len)) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }

            string_release(entry->key);
            aether_caps_free(entry, sizeof(HashMapEntry));
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
            aether_caps_free(entry, sizeof(HashMapEntry));
            entry = next;
        }
        map->buckets[i] = NULL;
    }
    map->size = 0;
}

void map_free(HashMap* map) {
    if (!map) return;
    map_clear(map);
    aether_caps_free(map->buckets,
                     (size_t)map->capacity * sizeof(HashMapEntry*));
    aether_caps_free(map, sizeof(HashMap));
}

MapKeys* map_keys_raw(HashMap* map) {
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

