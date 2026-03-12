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

JsonValue* json_parse(const char* json_str);
char* json_stringify(JsonValue* value);
void json_free(JsonValue* value);

JsonType json_type(JsonValue* value);
int json_is_null(JsonValue* value);

int json_get_bool(JsonValue* value);
double json_get_number(JsonValue* value);
int json_get_int(JsonValue* value);
const char* json_get_string(JsonValue* value);

JsonValue* json_object_get(JsonValue* obj, const char* key);
void json_object_set(JsonValue* obj, const char* key, JsonValue* value);
int json_object_has(JsonValue* obj, const char* key);

JsonValue* json_array_get(JsonValue* arr, int index);
void json_array_add(JsonValue* arr, JsonValue* value);
int json_array_size(JsonValue* arr);

JsonValue* json_create_null();
JsonValue* json_create_bool(int value);
JsonValue* json_create_number(double value);
JsonValue* json_create_string(const char* value);
JsonValue* json_create_array();
JsonValue* json_create_object();

#endif
