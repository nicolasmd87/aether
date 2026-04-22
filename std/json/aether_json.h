#ifndef AETHER_JSON_H
#define AETHER_JSON_H

#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

// Parse a JSON document. Returns the root value on success (caller owns
// and must free with json_free), or NULL on failure — retrieve the
// reason from json_last_error().
JsonValue* json_parse_raw(const char* json_str);

// Length-taking parse variant. Safe for inputs without a trailing null
// byte (e.g. fuzzer-provided buffers, memory-mapped files). Same return
// semantics as json_parse_raw.
JsonValue* json_parse_raw_n(const char* data, size_t len);

// Most recent parse error from json_parse_raw* on this thread. Returns
// a "<reason> at <line>:<col>" string, or "" if the last parse
// succeeded. The pointer is owned by the runtime; do not free it.
const char* json_last_error(void);

// Serialize `value` to a newly malloc'd compact JSON string. Caller
// must free. Returns a non-NULL empty string on allocation failure.
// The Aether-side wrapper `json.stringify` copies into an owned
// Aether string and frees the raw C buffer; use that from Aether code.
char* json_stringify_raw(JsonValue* value);

// Free the value and every node reachable from it. For parsed roots
// this releases the parse arena in O(chunks). For heap-allocated values
// (from json_create_*) it recurses and frees each node.
void json_free(JsonValue* value);

JsonType json_type(JsonValue* value);
int json_is_null(JsonValue* value);

int json_get_bool(JsonValue* value);
double json_get_number(JsonValue* value);
int json_get_int(JsonValue* value);
// Borrowed pointer into the JsonValue's internal string. NULL when
// `value` is not a JSON_STRING. Valid until json_free(root) is called.
// The Aether wrapper `json.get_string` copies this into an owned
// Aether string.
const char* json_get_string_raw(JsonValue* value);

JsonValue* json_object_get_raw(JsonValue* obj, const char* key);
// Returns 1 on success, 0 if `obj` is not a JSON_OBJECT, null key/
// value, or allocation failure. Ownership of `value` transfers to
// the container — do NOT json_free it separately.
int json_object_set_raw(JsonValue* obj, const char* key, JsonValue* value);
int json_object_has(JsonValue* obj, const char* key);

JsonValue* json_array_get_raw(JsonValue* arr, int index);
// Returns 1 on success, 0 if `arr` is not a JSON_ARRAY, null value,
// or allocation failure. Ownership of `value` transfers to the array.
int json_array_add_raw(JsonValue* arr, JsonValue* value);
int json_array_size(JsonValue* arr);

JsonValue* json_create_null(void);
JsonValue* json_create_bool(int value);
JsonValue* json_create_number(double value);
JsonValue* json_create_string(const char* value);
JsonValue* json_create_array(void);
JsonValue* json_create_object(void);

#endif
