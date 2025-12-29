#include "test_harness.h"
#include "../std/collections/aether_collections.h"
#include "../std/string/aether_string.h"

TEST(list_create_and_free) {
    ArrayList* list = aether_list_new();
    ASSERT_NOT_NULL(list);
    ASSERT_EQ(0, aether_list_size(list));
    aether_list_free(list);
}

TEST(list_add_and_get) {
    ArrayList* list = aether_list_new();
    
    int val1 = 42;
    int val2 = 100;
    aether_list_add(list, &val1);
    aether_list_add(list, &val2);
    
    ASSERT_EQ(2, aether_list_size(list));
    ASSERT_EQ(&val1, aether_list_get(list, 0));
    ASSERT_EQ(&val2, aether_list_get(list, 1));
    
    aether_list_free(list);
}

TEST(list_set_and_remove) {
    ArrayList* list = aether_list_new();
    
    int val1 = 1, val2 = 2, val3 = 3;
    aether_list_add(list, &val1);
    aether_list_add(list, &val2);
    aether_list_add(list, &val3);
    
    int val4 = 99;
    aether_list_set(list, 1, &val4);
    ASSERT_EQ(&val4, aether_list_get(list, 1));
    
    aether_list_remove(list, 0);
    ASSERT_EQ(2, aether_list_size(list));
    ASSERT_EQ(&val4, aether_list_get(list, 0));
    
    aether_list_free(list);
}

TEST(map_create_and_free) {
    HashMap* map = aether_map_new();
    ASSERT_NOT_NULL(map);
    ASSERT_EQ(0, aether_map_size(map));
    aether_map_free(map);
}

TEST(map_put_and_get) {
    HashMap* map = aether_map_new();
    
    AetherString* key1 = aether_string_new("name");
    AetherString* key2 = aether_string_new("age");
    
    int val1 = 42;
    int val2 = 100;
    
    aether_map_put(map, key1, &val1);
    aether_map_put(map, key2, &val2);
    
    ASSERT_EQ(2, aether_map_size(map));
    ASSERT_EQ(&val1, aether_map_get(map, key1));
    ASSERT_EQ(&val2, aether_map_get(map, key2));
    
    aether_string_release(key1);
    aether_string_release(key2);
    aether_map_free(map);
}

TEST(map_has_and_remove) {
    HashMap* map = aether_map_new();
    
    AetherString* key = aether_string_new("test");
    int val = 123;
    
    aether_map_put(map, key, &val);
    ASSERT_TRUE(aether_map_has(map, key));
    
    aether_map_remove(map, key);
    ASSERT_FALSE(aether_map_has(map, key));
    ASSERT_EQ(0, aether_map_size(map));
    
    aether_string_release(key);
    aether_map_free(map);
}

TEST(map_keys) {
    HashMap* map = aether_map_new();
    
    AetherString* key1 = aether_string_new("a");
    AetherString* key2 = aether_string_new("b");
    
    int val = 1;
    aether_map_put(map, key1, &val);
    aether_map_put(map, key2, &val);
    
    MapKeys* keys = aether_map_keys(map);
    ASSERT_NOT_NULL(keys);
    ASSERT_EQ(2, keys->count);
    
    aether_map_keys_free(keys);
    aether_string_release(key1);
    aether_string_release(key2);
    aether_map_free(map);
}

