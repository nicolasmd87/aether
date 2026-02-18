#include "test_harness.h"
#include "../../std/collections/aether_collections.h"
#include "../../std/string/aether_string.h"

TEST_CATEGORY(list_create_and_free, TEST_CATEGORY_COLLECTIONS) {
    ArrayList* list = list_new();
    ASSERT_NOT_NULL(list);
    ASSERT_EQ(0, list_size(list));
    list_free(list);
}

TEST_CATEGORY(list_add_and_get, TEST_CATEGORY_COLLECTIONS) {
    ArrayList* list = list_new();

    int val1 = 42;
    int val2 = 100;
    list_add(list, &val1);
    list_add(list, &val2);

    ASSERT_EQ(2, list_size(list));
    ASSERT_EQ(&val1, list_get(list, 0));
    ASSERT_EQ(&val2, list_get(list, 1));

    list_free(list);
}

TEST_CATEGORY(list_set_and_remove, TEST_CATEGORY_COLLECTIONS) {
    ArrayList* list = list_new();

    int val1 = 1, val2 = 2, val3 = 3;
    list_add(list, &val1);
    list_add(list, &val2);
    list_add(list, &val3);

    int val4 = 99;
    list_set(list, 1, &val4);
    ASSERT_EQ(&val4, list_get(list, 1));

    list_remove(list, 0);
    ASSERT_EQ(2, list_size(list));
    ASSERT_EQ(&val4, list_get(list, 0));

    list_free(list);
}

TEST_CATEGORY(map_create_and_free, TEST_CATEGORY_COLLECTIONS) {
    HashMap* map = map_new();
    ASSERT_NOT_NULL(map);
    ASSERT_EQ(0, map_size(map));
    map_free(map);
}

TEST_CATEGORY(map_put_and_get, TEST_CATEGORY_COLLECTIONS) {
    HashMap* map = map_new();

    AetherString* key1 = string_new("name");
    AetherString* key2 = string_new("age");

    int val1 = 42;
    int val2 = 100;

    map_put(map, key1, &val1);
    map_put(map, key2, &val2);

    ASSERT_EQ(2, map_size(map));
    ASSERT_EQ(&val1, map_get(map, key1));
    ASSERT_EQ(&val2, map_get(map, key2));

    string_release(key1);
    string_release(key2);
    map_free(map);
}

TEST_CATEGORY(map_has_and_remove, TEST_CATEGORY_COLLECTIONS) {
    HashMap* map = map_new();

    AetherString* key = string_new("test");
    int val = 123;

    map_put(map, key, &val);
    ASSERT_TRUE(map_has(map, key));

    map_remove(map, key);
    ASSERT_FALSE(map_has(map, key));
    ASSERT_EQ(0, map_size(map));

    string_release(key);
    map_free(map);
}

TEST_CATEGORY(map_keys, TEST_CATEGORY_COLLECTIONS) {
    HashMap* map = map_new();

    AetherString* key1 = string_new("a");
    AetherString* key2 = string_new("b");

    int val = 1;
    map_put(map, key1, &val);
    map_put(map, key2, &val);

    MapKeys* keys = map_keys(map);
    ASSERT_NOT_NULL(keys);
    ASSERT_EQ(2, keys->count);

    map_keys_free(keys);
    string_release(key1);
    string_release(key2);
    map_free(map);
}
