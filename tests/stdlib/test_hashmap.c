/**
 * Aether HashMap Test Suite
 * Tests for std/collections/aether_hashmap
 */

#include "test_framework.h"
#include "../../std/collections/aether_hashmap.h"

int main(void) {
    TEST_SUITE_BEGIN("HashMap Tests");
    
    // Test 1: Create
    HashMap* map = hashmap_create(16, test_str_hash, test_str_equals, NULL, free, NULL, NULL);
    TEST("Create", map != NULL && hashmap_size(map) == 0);
    
    // Test 2: Insert
    int *v1 = test_make_int(100);
    int *v2 = test_make_int(200);
    hashmap_insert(map, "key1", v1);
    hashmap_insert(map, "key2", v2);
    TEST("Insert", hashmap_size(map) == 2);
    
    // Test 3: Get
    int *retrieved = hashmap_get(map, "key1");
    TEST("Get", retrieved != NULL && *retrieved == 100);
    
    // Test 4: Contains
    TEST("Contains", hashmap_contains(map, "key1") && !hashmap_contains(map, "key3"));
    
    // Test 5: Update
    int *v3 = test_make_int(150);
    hashmap_insert(map, "key1", v3);
    int *updated = hashmap_get(map, "key1");
    TEST("Update", updated != NULL && *updated == 150 && hashmap_size(map) == 2);
    
    // Test 6: Remove
    hashmap_remove(map, "key2");
    TEST("Remove", hashmap_size(map) == 1 && !hashmap_contains(map, "key2"));
    
    // Test 7: Clear
    hashmap_clear(map);
    TEST("Clear", hashmap_is_empty(map));
    
    // Test 8: Collision handling
    for (int i = 0; i < 100; i++) {
        char key[20];
        snprintf(key, sizeof(key), "key%d", i);
        int *val = test_make_int(i);
        hashmap_insert(map, strdup(key), val);
    }
    TEST("Collision", hashmap_size(map) == 100);
    
    // Test 9: Get after collisions
    int *val50 = hashmap_get(map, "key50");
    TEST("Get-collision", val50 != NULL && *val50 == 50);
    
    // Test 10: Resize
    for (int i = 100; i < 200; i++) {
        char *key = malloc(20);
        snprintf(key, 20, "key%d", i);
        int *val = test_make_int(i);
        hashmap_insert(map, key, val);
    }
    TEST("Resize", hashmap_size(map) == 200);
    
    // Cleanup
    hashmap_free(map);
    
    TEST_SUITE_END();
}
