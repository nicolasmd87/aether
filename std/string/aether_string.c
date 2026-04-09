#include "aether_string.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

// Helper: get data pointer and length from either AetherString* or plain char*
static inline const char* str_data(const void* s) {
    if (!s) return "";
    if (is_aether_string(s)) return ((const AetherString*)s)->data;
    return (const char*)s;
}

static inline size_t str_len(const void* s) {
    if (!s) return 0;
    if (is_aether_string(s)) return ((const AetherString*)s)->length;
    return strlen((const char*)s);
}

// Alias for string literal creation
AetherString* string_from_literal(const char* cstr) {
    return string_new(cstr);
}

// Alias for from_cstr
AetherString* string_from_cstr(const char* cstr) {
    return string_new(cstr);
}

// Alias for free
void string_free(const void* str) {
    string_release(str);
}

// String creation
AetherString* string_new(const char* cstr) {
    if (!cstr) return string_empty();
    return string_new_with_length(cstr, strlen(cstr));
}

AetherString* string_new_with_length(const char* data, size_t length) {
    AetherString* str = (AetherString*)malloc(sizeof(AetherString));
    str->magic = AETHER_STRING_MAGIC;
    str->length = length;
    str->capacity = length + 1;
    str->data = (char*)malloc(str->capacity);
    memcpy(str->data, data, length);
    str->data[length] = '\0';
    str->ref_count = 1;
    return str;
}

AetherString* string_empty() {
    return string_new_with_length("", 0);
}

// Reference counting — safe to call with plain char* (no-op)
void string_retain(const void* str) {
    if (str && is_aether_string(str)) ((AetherString*)str)->ref_count++;
}

void string_release(const void* str) {
    if (!str || !is_aether_string(str)) return;
    AetherString* s = (AetherString*)str;
    s->ref_count--;
    if (s->ref_count <= 0) {
        free(s->data);
        free(s);
    }
}

// String operations
// Returns plain char* — usable directly with print/interpolation.
// Caller owns the memory (free with free() or string_release()).
char* string_concat(const void* a, const void* b) {
    if (!a || !b) return NULL;
    size_t la = str_len(a), lb = str_len(b);
    const char* da = str_data(a);
    const char* db = str_data(b);

    size_t new_length = la + lb;
    char* new_data = (char*)malloc(new_length + 1);

    memcpy(new_data, da, la);
    memcpy(new_data + la, db, lb);
    new_data[new_length] = '\0';

    return new_data;
}

int string_length(const void* str) {
    return (int)str_len(str);
}

char string_char_at(const void* str, int index) {
    size_t len = str_len(str);
    if (!str || index < 0 || index >= (int)len) return '\0';
    return str_data(str)[index];
}

int string_equals(const void* a, const void* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    size_t la = str_len(a), lb = str_len(b);
    if (la != lb) return 0;
    return memcmp(str_data(a), str_data(b), la) == 0;
}

int string_compare(const void* a, const void* b) {
    if (!a || !b) return 0;
    return strcmp(str_data(a), str_data(b));
}

// String methods
int string_starts_with(const void* str, const char* prefix) {
    if (!str || !prefix) return 0;
    size_t prefix_len = strlen(prefix);
    size_t slen = str_len(str);
    if (prefix_len > slen) return 0;
    return memcmp(str_data(str), prefix, prefix_len) == 0;
}

int string_ends_with(const void* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t suffix_len = strlen(suffix);
    size_t slen = str_len(str);
    if (suffix_len > slen) return 0;
    return memcmp(str_data(str) + (slen - suffix_len),
                  suffix, suffix_len) == 0;
}

int string_contains(const void* str, const char* substring) {
    return string_index_of(str, substring) >= 0;
}

int string_index_of(const void* str, const char* substring) {
    if (!str || !substring) return -1;
    size_t sub_len = strlen(substring);
    size_t slen = str_len(str);
    const char* sdata = str_data(str);
    if (sub_len > slen) return -1;

    for (size_t i = 0; i <= slen - sub_len; i++) {
        if (memcmp(sdata + i, substring, sub_len) == 0) {
            return (int)i;
        }
    }
    return -1;
}

char* string_substring(const void* str, int start, int end) {
    if (!str) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);
    if (start < 0) start = 0;
    if (end > (int)slen) end = (int)slen;
    if (start >= end) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    size_t len = end - start;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, sdata + start, len);
    result[len] = '\0';
    return result;
}

char* string_to_upper(const void* str) {
    if (!str) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);

    char* new_data = (char*)malloc(slen + 1);
    if (!new_data) return NULL;
    for (size_t i = 0; i < slen; i++) {
        new_data[i] = toupper(sdata[i]);
    }
    new_data[slen] = '\0';
    return new_data;
}

char* string_to_lower(const void* str) {
    if (!str) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);

    char* new_data = (char*)malloc(slen + 1);
    if (!new_data) return NULL;
    for (size_t i = 0; i < slen; i++) {
        new_data[i] = tolower(sdata[i]);
    }
    new_data[slen] = '\0';
    return new_data;
}

char* string_trim(const void* str) {
    if (!str) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);

    size_t start = 0;
    size_t end = slen;

    while (start < slen && isspace(sdata[start])) start++;
    while (end > start && isspace(sdata[end - 1])) end--;

    size_t len = end - start;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, sdata + start, len);
    result[len] = '\0';
    return result;
}

// String array operations
AetherStringArray* string_split(const void* str, const char* delimiter) {
    if (!str || !delimiter) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);

    size_t delim_len = strlen(delimiter);

    AetherStringArray* arr = (AetherStringArray*)malloc(sizeof(AetherStringArray));
    arr->count = 0;
    arr->strings = NULL;

    if (delim_len == 0) {
        // Split into characters
        arr->count = slen;
        arr->strings = (AetherString**)malloc(sizeof(AetherString*) * arr->count);
        for (size_t i = 0; i < slen; i++) {
            arr->strings[i] = string_new_with_length(sdata + i, 1);
        }
        return arr;
    }

    // Count delimiters
    size_t count = 1;
    for (size_t i = 0; i <= slen - delim_len; i++) {
        if (memcmp(sdata + i, delimiter, delim_len) == 0) {
            count++;
            i += delim_len - 1;
        }
    }

    arr->count = count;
    arr->strings = (AetherString**)malloc(sizeof(AetherString*) * count);

    size_t start = 0;
    size_t idx = 0;
    for (size_t i = 0; i <= slen - delim_len; i++) {
        if (memcmp(sdata + i, delimiter, delim_len) == 0) {
            arr->strings[idx++] = string_new_with_length(sdata + start, i - start);
            start = i + delim_len;
            i += delim_len - 1;
        }
    }
    // Add remaining part
    arr->strings[idx] = string_new_with_length(sdata + start, slen - start);

    return arr;
}

int string_array_size(AetherStringArray* arr) {
    return arr ? (int)arr->count : 0;
}

// Returns the raw C string data (const char*) for the element at index.
// Aether treats strings as const char* — returning AetherString* would cause
// printf("%s", ...) to print garbage (struct pointer instead of char data).
const char* string_array_get(AetherStringArray* arr, int index) {
    if (!arr || index < 0 || (size_t)index >= arr->count) return NULL;
    AetherString* s = arr->strings[index];
    return s ? s->data : NULL;
}

void string_array_free(AetherStringArray* arr) {
    if (!arr) return;
    for (size_t i = 0; i < arr->count; i++) {
        string_release(arr->strings[i]);
    }
    free(arr->strings);
    free(arr);
}

// Conversion
const char* string_to_cstr(const void* str) {
    if (!str) return "";
    if (is_aether_string(str)) return ((const AetherString*)str)->data;
    // Already a plain char*
    return (const char*)str;
}

AetherString* string_from_int(int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return string_new(buffer);
}

AetherString* string_from_float(float value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%g", value);
    return string_new(buffer);
}

// Parsing functions - convert string to numbers
int string_to_int(const void* str, int* out_value) {
    const char* data = str_data(str);
    if (!str || !data[0] || !out_value) return 0;

    char* endptr;
    errno = 0;
    long val = strtol(data, &endptr, 10);

    // Check for errors: no conversion, overflow, or trailing garbage
    if (endptr == data || errno == ERANGE || val > INT_MAX || val < INT_MIN) {
        return 0;
    }

    // Skip trailing whitespace
    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return 0;  // Trailing non-whitespace

    *out_value = (int)val;
    return 1;
}

int string_to_long(const void* str, long* out_value) {
    const char* data = str_data(str);
    if (!str || !data[0] || !out_value) return 0;

    char* endptr;
    errno = 0;
    long val = strtol(data, &endptr, 10);

    if (endptr == data || errno == ERANGE) {
        return 0;
    }

    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return 0;

    *out_value = val;
    return 1;
}

int string_to_float(const void* str, float* out_value) {
    const char* data = str_data(str);
    if (!str || !data[0] || !out_value) return 0;

    char* endptr;
    errno = 0;
    float val = strtof(data, &endptr);

    if (endptr == data || errno == ERANGE) {
        return 0;
    }

    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return 0;

    *out_value = val;
    return 1;
}

int string_to_double(const void* str, double* out_value) {
    const char* data = str_data(str);
    if (!str || !data[0] || !out_value) return 0;

    char* endptr;
    errno = 0;
    double val = strtod(data, &endptr);

    if (endptr == data || errno == ERANGE) {
        return 0;
    }

    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return 0;

    *out_value = val;
    return 1;
}

// Printf-style string formatting
AetherString* string_format(const char* fmt, ...) {
    if (!fmt) return string_empty();

    va_list args;

    // First pass: calculate required size
    va_start(args, fmt);
    int size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (size < 0) return string_empty();

    // Allocate buffer
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) return string_empty();

    // Second pass: format string
    va_start(args, fmt);
    vsnprintf(buffer, size + 1, fmt, args);
    va_end(args);

    AetherString* result = string_new_with_length(buffer, size);
    free(buffer);
    return result;
}
