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
void string_free(AetherString* str) {
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

// Reference counting
void string_retain(AetherString* str) {
    if (str && is_aether_string(str)) str->ref_count++;
}

void string_release(AetherString* str) {
    if (!str || !is_aether_string(str)) return;
    str->ref_count--;
    if (str->ref_count <= 0) {
        free(str->data);
        free(str);
    }
}

// String operations
AetherString* string_concat(AetherString* a, AetherString* b) {
    if (!a || !b) return NULL;
    size_t la = str_len(a), lb = str_len(b);
    const char* da = str_data(a);
    const char* db = str_data(b);

    size_t new_length = la + lb;
    char* new_data = (char*)malloc(new_length + 1);

    memcpy(new_data, da, la);
    memcpy(new_data + la, db, lb);
    new_data[new_length] = '\0';

    AetherString* result = string_new_with_length(new_data, new_length);
    free(new_data);
    return result;
}

int string_length(AetherString* str) {
    return (int)str_len(str);
}

char string_char_at(AetherString* str, int index) {
    size_t len = str_len(str);
    if (!str || index < 0 || index >= (int)len) return '\0';
    return str_data(str)[index];
}

int string_equals(AetherString* a, AetherString* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    size_t la = str_len(a), lb = str_len(b);
    if (la != lb) return 0;
    return memcmp(str_data(a), str_data(b), la) == 0;
}

int string_compare(AetherString* a, AetherString* b) {
    if (!a || !b) return 0;
    return strcmp(str_data(a), str_data(b));
}

// String methods
int string_starts_with(AetherString* str, const char* prefix) {
    if (!str || !prefix) return 0;
    size_t prefix_len = strlen(prefix);
    size_t slen = str_len(str);
    if (prefix_len > slen) return 0;
    return memcmp(str_data(str), prefix, prefix_len) == 0;
}

int string_ends_with(AetherString* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t suffix_len = strlen(suffix);
    size_t slen = str_len(str);
    if (suffix_len > slen) return 0;
    return memcmp(str_data(str) + (slen - suffix_len),
                  suffix, suffix_len) == 0;
}

int string_contains(AetherString* str, const char* substring) {
    return string_index_of(str, substring) >= 0;
}

int string_index_of(AetherString* str, const char* substring) {
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

AetherString* string_substring(AetherString* str, int start, int end) {
    if (!str) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);
    if (start < 0) start = 0;
    if (end > (int)slen) end = (int)slen;
    if (start >= end) return string_empty();

    return string_new_with_length(sdata + start, end - start);
}

AetherString* string_to_upper(AetherString* str) {
    if (!str) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);

    char* new_data = (char*)malloc(slen + 1);
    for (size_t i = 0; i < slen; i++) {
        new_data[i] = toupper(sdata[i]);
    }
    new_data[slen] = '\0';

    AetherString* result = string_new_with_length(new_data, slen);
    free(new_data);
    return result;
}

AetherString* string_to_lower(AetherString* str) {
    if (!str) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);

    char* new_data = (char*)malloc(slen + 1);
    for (size_t i = 0; i < slen; i++) {
        new_data[i] = tolower(sdata[i]);
    }
    new_data[slen] = '\0';

    AetherString* result = string_new_with_length(new_data, slen);
    free(new_data);
    return result;
}

AetherString* string_trim(AetherString* str) {
    if (!str) return NULL;
    size_t slen = str_len(str);
    const char* sdata = str_data(str);

    size_t start = 0;
    size_t end = slen;

    while (start < slen && isspace(sdata[start])) start++;
    while (end > start && isspace(sdata[end - 1])) end--;

    if (start == 0 && end == slen && is_aether_string(str)) {
        string_retain(str);
        return str;
    }

    return string_new_with_length(sdata + start, end - start);
}

// String array operations
AetherStringArray* string_split(AetherString* str, const char* delimiter) {
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

AetherString* string_array_get(AetherStringArray* arr, int index) {
    if (!arr || index < 0 || (size_t)index >= arr->count) return NULL;
    return arr->strings[index];
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
const char* string_to_cstr(AetherString* str) {
    if (!str) return "";
    if (is_aether_string(str)) return str->data;
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
int string_to_int(AetherString* str, int* out_value) {
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

int string_to_long(AetherString* str, long* out_value) {
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

int string_to_float(AetherString* str, float* out_value) {
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

int string_to_double(AetherString* str, double* out_value) {
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
