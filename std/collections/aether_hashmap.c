#include "aether_hashmap.h"
#include "../../runtime/utils/aether_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_CAPACITY 16
#define MAX_LOAD_FACTOR 0.75
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// FNV-1a hash function for strings
uint64_t hashmap_hash_string(const void* key) {
    const char* str = (const char*)key;
    uint64_t hash = FNV_OFFSET;
    
    for (const char* p = str; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    
    return hash;
}

bool hashmap_equals_string(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b) == 0;
}

void hashmap_free_string(void* str) {
    free(str);
}

void* hashmap_clone_string(const void* str) {
    return strdup((const char*)str);
}

// Integer hash using identity function with mixing
uint64_t hashmap_hash_int(const void* key) {
    uint64_t x = (uint64_t)(uintptr_t)key;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

bool hashmap_equals_int(const void* a, const void* b) {
    return (uintptr_t)a == (uintptr_t)b;
}

void hashmap_free_int(void* ptr) {
    // Integers stored as values, no free needed
    (void)ptr;
}

void* hashmap_clone_int(const void* ptr) {
    return (void*)ptr;
}

// Create hashmap
HashMap* hashmap_create(size_t initial_capacity,
                       uint64_t (*hash_func)(const void*),
                       bool (*key_equals)(const void*, const void*),
                       void (*key_free)(void*),
                       void (*value_free)(void*),
                       void* (*key_clone)(const void*),
                       void* (*value_clone)(const void*)) {
    
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    if (!map) return NULL;
    
    if (initial_capacity < DEFAULT_CAPACITY) {
        initial_capacity = DEFAULT_CAPACITY;
    }
    
    map->entries = (HashMapEntry*)calloc(initial_capacity, sizeof(HashMapEntry));
    if (!map->entries) {
        free(map);
        return NULL;
    }
    
    map->capacity = initial_capacity;
    map->size = 0;
    map->load_factor = MAX_LOAD_FACTOR;
    map->hash_func = hash_func;
    map->key_equals = key_equals;
    map->key_free = key_free;
    map->value_free = value_free;
    map->key_clone = key_clone;
    map->value_clone = value_clone;
    
    return map;
}

void hashmap_free(HashMap* map) {
    if (!map) return;
    
    if (map->entries) {
        for (size_t i = 0; i < map->capacity; i++) {
            if (map->entries[i].occupied) {
                if (map->key_free) map->key_free(map->entries[i].key);
                if (map->value_free) map->value_free(map->entries[i].value);
            }
        }
        free(map->entries);
    }
    
    free(map);
}

// Resize hashmap
static void hashmap_resize(HashMap* map) {
    size_t old_capacity = map->capacity;
    HashMapEntry* old_entries = map->entries;
    
    map->capacity *= 2;
    map->entries = (HashMapEntry*)calloc(map->capacity, sizeof(HashMapEntry));
    if (!map->entries) {
        // Failed to allocate, restore old state
        map->capacity = old_capacity;
        map->entries = old_entries;
        return;
    }
    
    // Reset size counter for reinsertion
    map->size = 0;
    
    // Rehash all entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].occupied) {
            hashmap_insert(map, old_entries[i].key, old_entries[i].value);
        }
    }
    
    free(old_entries);
}

// Robin Hood hashing insert
bool hashmap_insert(HashMap* map, void* key, void* value) {
    if (!map || !key) return false;
    
    // Check load factor
    if ((float)map->size / map->capacity >= map->load_factor) {
        hashmap_resize(map);
    }
    
    uint64_t hash = map->hash_func(key);
    size_t index = hash % map->capacity;
    int32_t psl = 0;
    
    HashMapEntry entry = {
        .key = key,
        .value = value,
        .hash = hash,
        .psl = 0,
        .occupied = true
    };
    
    while (true) {
        HashMapEntry* current = &map->entries[index];
        
        // Empty slot found
        if (!current->occupied) {
            entry.psl = psl;
            *current = entry;
            map->size++;
            return true;
        }
        
        // Key already exists, update value
        if (current->hash == hash && map->key_equals(current->key, key)) {
            if (map->value_free) map->value_free(current->value);
            if (map->key_free && current->key != key) map->key_free(current->key);
            current->value = value;
            current->key = key;
            return true;
        }
        
        // Robin Hood: if our PSL is greater than current, swap
        if (psl > current->psl) {
            HashMapEntry temp = *current;
            entry.psl = psl;
            *current = entry;
            entry = temp;
            psl = temp.psl;
        }
        
        psl++;
        index = (index + 1) % map->capacity;
    }
}

void* hashmap_get(HashMap* map, const void* key) {
    if (!map || !key) return NULL;
    
    uint64_t hash = map->hash_func(key);
    size_t index = hash % map->capacity;
    int32_t psl = 0;
    
    while (true) {
        HashMapEntry* entry = &map->entries[index];
        
        // Empty slot or PSL too high means key doesn't exist
        if (!entry->occupied || psl > entry->psl) {
            return NULL;
        }
        
        // Found the key
        if (entry->hash == hash && map->key_equals(entry->key, key)) {
            return entry->value;
        }
        
        psl++;
        index = (index + 1) % map->capacity;
    }
}

bool hashmap_contains(HashMap* map, const void* key) {
    return hashmap_get(map, key) != NULL;
}

bool hashmap_remove(HashMap* map, const void* key) {
    if (!map || !key) return false;
    
    uint64_t hash = map->hash_func(key);
    size_t index = hash % map->capacity;
    int32_t psl = 0;
    
    while (true) {
        HashMapEntry* entry = &map->entries[index];
        
        if (!entry->occupied || psl > entry->psl) {
            return false;
        }
        
        if (entry->hash == hash && map->key_equals(entry->key, key)) {
            // Free key and value
            if (map->key_free) map->key_free(entry->key);
            if (map->value_free) map->value_free(entry->value);
            
            // Backward shift deletion
            size_t current = index;
            while (true) {
                size_t next = (current + 1) % map->capacity;
                HashMapEntry* next_entry = &map->entries[next];
                
                if (!next_entry->occupied || next_entry->psl == 0) {
                    map->entries[current].occupied = false;
                    map->size--;
                    return true;
                }
                
                map->entries[current] = *next_entry;
                map->entries[current].psl--;
                current = next;
            }
        }
        
        psl++;
        index = (index + 1) % map->capacity;
    }
}

void hashmap_clear(HashMap* map) {
    if (!map) return;
    
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].occupied) {
            if (map->key_free) map->key_free(map->entries[i].key);
            if (map->value_free) map->value_free(map->entries[i].value);
            map->entries[i].occupied = false;
        }
    }
    
    map->size = 0;
}

size_t hashmap_size(HashMap* map) {
    return map ? map->size : 0;
}

bool hashmap_is_empty(HashMap* map) {
    return map ? (map->size == 0) : true;
}

// Iterator implementation
HashMapIterator hashmap_iterator(HashMap* map) {
    HashMapIterator iter = { .map = map, .index = 0 };
    return iter;
}

bool hashmap_iterator_next(HashMapIterator* iter, void** key, void** value) {
    if (!iter || !iter->map) return false;
    
    while (iter->index < iter->map->capacity) {
        HashMapEntry* entry = &iter->map->entries[iter->index];
        iter->index++;
        
        if (entry->occupied) {
            if (key) *key = entry->key;
            if (value) *value = entry->value;
            return true;
        }
    }
    
    return false;
}

// Convenience constructors
HashMap* hashmap_create_string_to_int(size_t initial_capacity) {
    return hashmap_create(initial_capacity,
                         hashmap_hash_string,
                         hashmap_equals_string,
                         hashmap_free_string,
                         hashmap_free_int,
                         hashmap_clone_string,
                         hashmap_clone_int);
}

HashMap* hashmap_create_string_to_string(size_t initial_capacity) {
    return hashmap_create(initial_capacity,
                         hashmap_hash_string,
                         hashmap_equals_string,
                         hashmap_free_string,
                         hashmap_free_string,
                         hashmap_clone_string,
                         hashmap_clone_string);
}

HashMap* hashmap_create_int_to_int(size_t initial_capacity) {
    return hashmap_create(initial_capacity,
                         hashmap_hash_int,
                         hashmap_equals_int,
                         hashmap_free_int,
                         hashmap_free_int,
                         hashmap_clone_int,
                         hashmap_clone_int);
}

