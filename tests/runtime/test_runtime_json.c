#include "test_harness.h"
#include "../../std/json/aether_json.h"
#include "../../std/string/aether_string.h"

TEST_CATEGORY(json_parse_null, TEST_CATEGORY_STDLIB) {
    AetherString* json_str = string_new("null");
    JsonValue* value = json_parse(json_str);

    ASSERT_NOT_NULL(value);
    ASSERT_TRUE(json_is_null(value));
    ASSERT_EQ(JSON_NULL, json_type(value));

    json_free(value);
    string_release(json_str);
}

TEST_CATEGORY(json_parse_bool, TEST_CATEGORY_STDLIB) {
    AetherString* json_true = string_new("true");
    JsonValue* val_true = json_parse(json_true);
    ASSERT_EQ(JSON_BOOL, json_type(val_true));
    ASSERT_EQ(1, json_get_bool(val_true));

    AetherString* json_false = string_new("false");
    JsonValue* val_false = json_parse(json_false);
    ASSERT_EQ(JSON_BOOL, json_type(val_false));
    ASSERT_EQ(0, json_get_bool(val_false));

    json_free(val_true);
    json_free(val_false);
    string_release(json_true);
    string_release(json_false);
}

TEST_CATEGORY(json_parse_number, TEST_CATEGORY_STDLIB) {
    AetherString* json_str = string_new("42.5");
    JsonValue* value = json_parse(json_str);

    ASSERT_NOT_NULL(value);
    ASSERT_EQ(JSON_NUMBER, json_type(value));
    ASSERT_EQ(42, json_get_int(value));

    json_free(value);
    string_release(json_str);
}

TEST_CATEGORY(json_parse_string, TEST_CATEGORY_STDLIB) {
    AetherString* json_str = string_new("\"hello world\"");
    JsonValue* value = json_parse(json_str);

    ASSERT_NOT_NULL(value);
    ASSERT_EQ(JSON_STRING, json_type(value));

    AetherString* str_val = json_get_string(value);
    ASSERT_NOT_NULL(str_val);
    ASSERT_STREQ("hello world", str_val->data);

    json_free(value);
    string_release(json_str);
}

TEST_CATEGORY(json_parse_array, TEST_CATEGORY_STDLIB) {
    AetherString* json_str = string_new("[1, 2, 3]");
    JsonValue* value = json_parse(json_str);

    ASSERT_NOT_NULL(value);
    ASSERT_EQ(JSON_ARRAY, json_type(value));
    ASSERT_EQ(3, json_array_size(value));

    JsonValue* first = json_array_get(value, 0);
    ASSERT_EQ(1, json_get_int(first));

    json_free(value);
    string_release(json_str);
}

TEST_CATEGORY(json_parse_object, TEST_CATEGORY_STDLIB) {
    AetherString* json_str = string_new("{\"name\":\"Alice\",\"age\":30}");
    JsonValue* value = json_parse(json_str);

    ASSERT_NOT_NULL(value);
    ASSERT_EQ(JSON_OBJECT, json_type(value));

    AetherString* name_key = string_new("name");
    AetherString* age_key = string_new("age");

    ASSERT_TRUE(json_object_has(value, name_key));

    JsonValue* name_val = json_object_get(value, name_key);
    ASSERT_NOT_NULL(name_val);
    ASSERT_STREQ("Alice", json_get_string(name_val)->data);

    JsonValue* age_val = json_object_get(value, age_key);
    ASSERT_EQ(30, json_get_int(age_val));

    string_release(name_key);
    string_release(age_key);
    json_free(value);
    string_release(json_str);
}

TEST_CATEGORY(json_create_and_stringify, TEST_CATEGORY_STDLIB) {
    JsonValue* obj = json_create_object();

    AetherString* name_key = string_new("name");
    AetherString* name_val_str = string_new("Bob");
    JsonValue* name_val = json_create_string(name_val_str);
    json_object_set(obj, name_key, name_val);

    AetherString* age_key = string_new("age");
    JsonValue* age_val = json_create_number(25);
    json_object_set(obj, age_key, age_val);

    AetherString* json_str = json_stringify(obj);
    ASSERT_NOT_NULL(json_str);
    ASSERT_NOT_NULL(strstr(json_str->data, "name"));
    ASSERT_NOT_NULL(strstr(json_str->data, "Bob"));

    string_release(name_key);
    string_release(name_val_str);
    string_release(age_key);
    string_release(json_str);
    json_free(obj);
}

TEST_CATEGORY(json_array_operations, TEST_CATEGORY_STDLIB) {
    JsonValue* arr = json_create_array();

    json_array_add(arr, json_create_number(10));
    json_array_add(arr, json_create_number(20));
    json_array_add(arr, json_create_number(30));

    ASSERT_EQ(3, json_array_size(arr));
    ASSERT_EQ(20, json_get_int(json_array_get(arr, 1)));

    json_free(arr);
}
