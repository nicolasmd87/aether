#include "aether_json.h"
#include "../collections/aether_collections.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

struct JsonValue {
    JsonType type;
    union {
        int bool_value;
        double number_value;
        AetherString* string_value;
        ArrayList* array_value;
        HashMap* object_value;
    } data;
};

static void skip_whitespace(const char** json) {
    while (**json && isspace(**json)) (*json)++;
}

static JsonValue* parse_value(const char** json);

static JsonValue* parse_null(const char** json) {
    if (strncmp(*json, "null", 4) == 0) {
        *json += 4;
        return json_create_null();
    }
    return NULL;
}

static JsonValue* parse_bool(const char** json) {
    if (strncmp(*json, "true", 4) == 0) {
        *json += 4;
        return json_create_bool(1);
    }
    if (strncmp(*json, "false", 5) == 0) {
        *json += 5;
        return json_create_bool(0);
    }
    return NULL;
}

static JsonValue* parse_number(const char** json) {
    char* end;
    double value = strtod(*json, &end);
    if (end == *json) return NULL;
    *json = end;
    return json_create_number(value);
}

static JsonValue* parse_string(const char** json) {
    if (**json != '"') return NULL;
    (*json)++;

    int capacity = 256;
    char* buffer = malloc(capacity);
    if (!buffer) return NULL;
    int i = 0;

    while (**json && **json != '"') {
        // Grow buffer if needed (leave room for null terminator)
        if (i >= capacity - 2) {
            capacity *= 2;
            char* nb = realloc(buffer, capacity);
            if (!nb) { free(buffer); return NULL; }
            buffer = nb;
        }
        if (**json == '\\') {
            (*json)++;
            switch (**json) {
                case 'n': buffer[i++] = '\n'; break;
                case 't': buffer[i++] = '\t'; break;
                case 'r': buffer[i++] = '\r'; break;
                case '\\': buffer[i++] = '\\'; break;
                case '"': buffer[i++] = '"'; break;
                default: buffer[i++] = **json; break;
            }
            (*json)++;
        } else {
            buffer[i++] = **json;
            (*json)++;
        }
    }

    if (**json == '"') (*json)++;
    buffer[i] = '\0';

    JsonValue* value = (JsonValue*)malloc(sizeof(JsonValue));
    value->type = JSON_STRING;
    value->data.string_value = string_new(buffer);
    free(buffer);
    return value;
}

static JsonValue* parse_array(const char** json) {
    if (**json != '[') return NULL;
    (*json)++;

    JsonValue* arr = json_create_array();
    skip_whitespace(json);

    if (**json == ']') {
        (*json)++;
        return arr;
    }

    while (1) {
        skip_whitespace(json);
        JsonValue* value = parse_value(json);
        if (value) {
            json_array_add(arr, value);
        }

        skip_whitespace(json);
        if (**json == ',') {
            (*json)++;
        } else {
            break;
        }
    }

    skip_whitespace(json);
    if (**json == ']') (*json)++;

    return arr;
}

static JsonValue* parse_object(const char** json) {
    if (**json != '{') return NULL;
    (*json)++;

    JsonValue* obj = json_create_object();
    skip_whitespace(json);

    if (**json == '}') {
        (*json)++;
        return obj;
    }

    while (1) {
        skip_whitespace(json);

        if (**json != '"') break;
        JsonValue* key_val = parse_string(json);
        if (!key_val) break;

        AetherString* key = key_val->data.string_value;
        free(key_val);  // Free the JsonValue wrapper, but not the string

        skip_whitespace(json);
        if (**json == ':') (*json)++;

        skip_whitespace(json);
        JsonValue* value = parse_value(json);
        if (value) {
            json_object_set(obj, key->data, value);
        }
        string_release(key);

        skip_whitespace(json);
        if (**json == ',') {
            (*json)++;
        } else {
            break;
        }
    }

    skip_whitespace(json);
    if (**json == '}') (*json)++;

    return obj;
}

static JsonValue* parse_value(const char** json) {
    skip_whitespace(json);
    
    if (**json == 'n') return parse_null(json);
    if (**json == 't' || **json == 'f') return parse_bool(json);
    if (**json == '"') return parse_string(json);
    if (**json == '[') return parse_array(json);
    if (**json == '{') return parse_object(json);
    if (**json == '-' || isdigit(**json)) return parse_number(json);
    
    return NULL;
}

JsonValue* json_parse(const char* json_str) {
    if (!json_str) return NULL;
    const char* json = json_str;
    return parse_value(&json);
}

static void stringify_value(JsonValue* value, AetherString** result);

static void append_string(AetherString** result, const char* str) {
    AetherString* temp = string_new(str);
    AetherString* new_result = string_concat(*result, temp);
    string_release(*result);
    string_release(temp);
    *result = new_result;
}

char* json_stringify(JsonValue* value) {
    AetherString* result = string_empty();
    stringify_value(value, &result);
    char* cstr = strdup(result->data);
    string_release(result);
    return cstr;
}

static void stringify_value(JsonValue* value, AetherString** result) {
    if (!value) {
        append_string(result, "null");
        return;
    }
    
    switch (value->type) {
        case JSON_NULL:
            append_string(result, "null");
            break;
            
        case JSON_BOOL:
            append_string(result, value->data.bool_value ? "true" : "false");
            break;
            
        case JSON_NUMBER: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", value->data.number_value);
            append_string(result, buf);
            break;
        }
            
        case JSON_STRING:
            append_string(result, "\"");
            append_string(result, value->data.string_value->data);
            append_string(result, "\"");
            break;
            
        case JSON_ARRAY: {
            append_string(result, "[");
            int size = list_size(value->data.array_value);
            for (int i = 0; i < size; i++) {
                if (i > 0) append_string(result, ",");
                JsonValue* item = (JsonValue*)list_get(value->data.array_value, i);
                stringify_value(item, result);
            }
            append_string(result, "]");
            break;
        }

        case JSON_OBJECT: {
            append_string(result, "{");
            MapKeys* keys = map_keys(value->data.object_value);
            for (int i = 0; i < keys->count; i++) {
                if (i > 0) append_string(result, ",");
                append_string(result, "\"");
                append_string(result, keys->keys[i]->data);
                append_string(result, "\":");
                JsonValue* val = (JsonValue*)map_get(value->data.object_value, keys->keys[i]->data);
                stringify_value(val, result);
            }
            map_keys_free(keys);
            append_string(result, "}");
            break;
        }
    }
}

void json_free(JsonValue* value) {
    if (!value) return;

    switch (value->type) {
        case JSON_STRING:
            string_release(value->data.string_value);
            break;
        case JSON_ARRAY: {
            int size = list_size(value->data.array_value);
            for (int i = 0; i < size; i++) {
                json_free((JsonValue*)list_get(value->data.array_value, i));
            }
            list_free(value->data.array_value);
            break;
        }
        case JSON_OBJECT: {
            MapKeys* keys = map_keys(value->data.object_value);
            for (int i = 0; i < keys->count; i++) {
                json_free((JsonValue*)map_get(value->data.object_value, keys->keys[i]->data));
            }
            map_keys_free(keys);
            map_free(value->data.object_value);
            break;
        }
        default:
            break;
    }

    free(value);
}

JsonType json_type(JsonValue* value) {
    return value ? value->type : JSON_NULL;
}

int json_is_null(JsonValue* value) {
    return !value || value->type == JSON_NULL;
}

int json_get_bool(JsonValue* value) {
    return (value && value->type == JSON_BOOL) ? value->data.bool_value : 0;
}

double json_get_number(JsonValue* value) {
    return (value && value->type == JSON_NUMBER) ? value->data.number_value : 0.0;
}

int json_get_int(JsonValue* value) {
    return (int)json_get_number(value);
}

const char* json_get_string(JsonValue* value) {
    if (!value || value->type != JSON_STRING || !value->data.string_value) return NULL;
    return value->data.string_value->data;
}

JsonValue* json_object_get(JsonValue* obj, const char* key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    return (JsonValue*)map_get(obj->data.object_value, key);
}

void json_object_set(JsonValue* obj, const char* key, JsonValue* value) {
    if (!obj || obj->type != JSON_OBJECT || !key) return;
    map_put(obj->data.object_value, key, value);
}

int json_object_has(JsonValue* obj, const char* key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return 0;
    return map_has(obj->data.object_value, key);
}

JsonValue* json_array_get(JsonValue* arr, int index) {
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    return (JsonValue*)list_get(arr->data.array_value, index);
}

void json_array_add(JsonValue* arr, JsonValue* value) {
    if (!arr || arr->type != JSON_ARRAY) return;
    list_add(arr->data.array_value, value);
}

int json_array_size(JsonValue* arr) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return list_size(arr->data.array_value);
}

JsonValue* json_create_null() {
    JsonValue* value = (JsonValue*)malloc(sizeof(JsonValue));
    value->type = JSON_NULL;
    return value;
}

JsonValue* json_create_bool(int val) {
    JsonValue* value = (JsonValue*)malloc(sizeof(JsonValue));
    value->type = JSON_BOOL;
    value->data.bool_value = val;
    return value;
}

JsonValue* json_create_number(double val) {
    JsonValue* value = (JsonValue*)malloc(sizeof(JsonValue));
    value->type = JSON_NUMBER;
    value->data.number_value = val;
    return value;
}

JsonValue* json_create_string(const char* val) {
    JsonValue* value = (JsonValue*)malloc(sizeof(JsonValue));
    value->type = JSON_STRING;
    value->data.string_value = string_new(val);
    return value;
}

JsonValue* json_create_array() {
    JsonValue* value = (JsonValue*)malloc(sizeof(JsonValue));
    value->type = JSON_ARRAY;
    value->data.array_value = list_new();
    return value;
}

JsonValue* json_create_object() {
    JsonValue* value = (JsonValue*)malloc(sizeof(JsonValue));
    value->type = JSON_OBJECT;
    value->data.object_value = map_new();
    return value;
}

