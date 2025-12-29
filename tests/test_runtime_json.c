#include "test_harness.h"
#include "../std/json/aether_json.h"
#include "../std/string/aether_string.h"

TEST(json_parse_null) {
    AetherString* json_str = aether_string_new("null");
    JsonValue* value = aether_json_parse(json_str);
    
    ASSERT_NOT_NULL(value);
    ASSERT_TRUE(aether_json_is_null(value));
    ASSERT_EQ(JSON_NULL, aether_json_type(value));
    
    aether_json_free(value);
    aether_string_release(json_str);
}

TEST(json_parse_bool) {
    AetherString* json_true = aether_string_new("true");
    JsonValue* val_true = aether_json_parse(json_true);
    ASSERT_EQ(JSON_BOOL, aether_json_type(val_true));
    ASSERT_EQ(1, aether_json_get_bool(val_true));
    
    AetherString* json_false = aether_string_new("false");
    JsonValue* val_false = aether_json_parse(json_false);
    ASSERT_EQ(JSON_BOOL, aether_json_type(val_false));
    ASSERT_EQ(0, aether_json_get_bool(val_false));
    
    aether_json_free(val_true);
    aether_json_free(val_false);
    aether_string_release(json_true);
    aether_string_release(json_false);
}

TEST(json_parse_number) {
    AetherString* json_str = aether_string_new("42.5");
    JsonValue* value = aether_json_parse(json_str);
    
    ASSERT_NOT_NULL(value);
    ASSERT_EQ(JSON_NUMBER, aether_json_type(value));
    ASSERT_EQ(42, aether_json_get_int(value));
    
    aether_json_free(value);
    aether_string_release(json_str);
}

TEST(json_parse_string) {
    AetherString* json_str = aether_string_new("\"hello world\"");
    JsonValue* value = aether_json_parse(json_str);
    
    ASSERT_NOT_NULL(value);
    ASSERT_EQ(JSON_STRING, aether_json_type(value));
    
    AetherString* str_val = aether_json_get_string(value);
    ASSERT_NOT_NULL(str_val);
    ASSERT_STREQ("hello world", str_val->data);
    
    aether_json_free(value);
    aether_string_release(json_str);
}

TEST(json_parse_array) {
    AetherString* json_str = aether_string_new("[1, 2, 3]");
    JsonValue* value = aether_json_parse(json_str);
    
    ASSERT_NOT_NULL(value);
    ASSERT_EQ(JSON_ARRAY, aether_json_type(value));
    ASSERT_EQ(3, aether_json_array_size(value));
    
    JsonValue* first = aether_json_array_get(value, 0);
    ASSERT_EQ(1, aether_json_get_int(first));
    
    aether_json_free(value);
    aether_string_release(json_str);
}

TEST(json_parse_object) {
    AetherString* json_str = aether_string_new("{\"name\":\"Alice\",\"age\":30}");
    JsonValue* value = aether_json_parse(json_str);
    
    ASSERT_NOT_NULL(value);
    ASSERT_EQ(JSON_OBJECT, aether_json_type(value));
    
    AetherString* name_key = aether_string_new("name");
    AetherString* age_key = aether_string_new("age");
    
    ASSERT_TRUE(aether_json_object_has(value, name_key));
    
    JsonValue* name_val = aether_json_object_get(value, name_key);
    ASSERT_NOT_NULL(name_val);
    ASSERT_STREQ("Alice", aether_json_get_string(name_val)->data);
    
    JsonValue* age_val = aether_json_object_get(value, age_key);
    ASSERT_EQ(30, aether_json_get_int(age_val));
    
    aether_string_release(name_key);
    aether_string_release(age_key);
    aether_json_free(value);
    aether_string_release(json_str);
}

TEST(json_create_and_stringify) {
    JsonValue* obj = aether_json_create_object();
    
    AetherString* name_key = aether_string_new("name");
    AetherString* name_val_str = aether_string_new("Bob");
    JsonValue* name_val = aether_json_create_string(name_val_str);
    aether_json_object_set(obj, name_key, name_val);
    
    AetherString* age_key = aether_string_new("age");
    JsonValue* age_val = aether_json_create_number(25);
    aether_json_object_set(obj, age_key, age_val);
    
    AetherString* json_str = aether_json_stringify(obj);
    ASSERT_NOT_NULL(json_str);
    ASSERT_NOT_NULL(strstr(json_str->data, "name"));
    ASSERT_NOT_NULL(strstr(json_str->data, "Bob"));
    
    aether_string_release(name_key);
    aether_string_release(name_val_str);
    aether_string_release(age_key);
    aether_string_release(json_str);
    aether_json_free(obj);
}

TEST(json_array_operations) {
    JsonValue* arr = aether_json_create_array();
    
    aether_json_array_add(arr, aether_json_create_number(10));
    aether_json_array_add(arr, aether_json_create_number(20));
    aether_json_array_add(arr, aether_json_create_number(30));
    
    ASSERT_EQ(3, aether_json_array_size(arr));
    ASSERT_EQ(20, aether_json_get_int(aether_json_array_get(arr, 1)));
    
    aether_json_free(arr);
}

