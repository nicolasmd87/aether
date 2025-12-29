#ifndef AETHER_JSON_H
#define AETHER_JSON_H

#include "../string/aether_string.h"

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

JsonValue* aether_json_parse(AetherString* json_str);
AetherString* aether_json_stringify(JsonValue* value);
void aether_json_free(JsonValue* value);

JsonType aether_json_type(JsonValue* value);
int aether_json_is_null(JsonValue* value);

int aether_json_get_bool(JsonValue* value);
double aether_json_get_number(JsonValue* value);
int aether_json_get_int(JsonValue* value);
AetherString* aether_json_get_string(JsonValue* value);

JsonValue* aether_json_object_get(JsonValue* obj, AetherString* key);
void aether_json_object_set(JsonValue* obj, AetherString* key, JsonValue* value);
int aether_json_object_has(JsonValue* obj, AetherString* key);

JsonValue* aether_json_array_get(JsonValue* arr, int index);
void aether_json_array_add(JsonValue* arr, JsonValue* value);
int aether_json_array_size(JsonValue* arr);

JsonValue* aether_json_create_null();
JsonValue* aether_json_create_bool(int value);
JsonValue* aether_json_create_number(double value);
JsonValue* aether_json_create_string(AetherString* value);
JsonValue* aether_json_create_array();
JsonValue* aether_json_create_object();

#endif

