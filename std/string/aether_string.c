#include "aether_string.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

// Alias for string literal creation
AetherString* aether_string_from_literal(const char* cstr) {
    return aether_string_new(cstr);
}

// Alias for from_cstr
AetherString* aether_string_from_cstr(const char* cstr) {
    return aether_string_new(cstr);
}

// Alias for free
void aether_string_free(AetherString* str) {
    aether_string_release(str);
}

// String creation
AetherString* aether_string_new(const char* cstr) {
    if (!cstr) return aether_string_empty();
    return aether_string_new_with_length(cstr, strlen(cstr));
}

AetherString* aether_string_new_with_length(const char* data, size_t length) {
    AetherString* str = (AetherString*)malloc(sizeof(AetherString));
    str->length = length;
    str->capacity = length + 1;
    str->data = (char*)malloc(str->capacity);
    memcpy(str->data, data, length);
    str->data[length] = '\0';
    str->ref_count = 1;
    return str;
}

AetherString* aether_string_empty() {
    return aether_string_new_with_length("", 0);
}

// Reference counting
void aether_string_retain(AetherString* str) {
    if (str) str->ref_count++;
}

void aether_string_release(AetherString* str) {
    if (!str) return;
    str->ref_count--;
    if (str->ref_count <= 0) {
        free(str->data);
        free(str);
    }
}

// String operations
AetherString* aether_string_concat(AetherString* a, AetherString* b) {
    if (!a || !b) return NULL;
    
    size_t new_length = a->length + b->length;
    char* new_data = (char*)malloc(new_length + 1);
    
    memcpy(new_data, a->data, a->length);
    memcpy(new_data + a->length, b->data, b->length);
    new_data[new_length] = '\0';
    
    AetherString* result = aether_string_new_with_length(new_data, new_length);
    free(new_data);
    return result;
}

int aether_string_length(AetherString* str) {
    return str ? (int)str->length : 0;
}

char aether_string_char_at(AetherString* str, int index) {
    if (!str || index < 0 || index >= (int)str->length) return '\0';
    return str->data[index];
}

int aether_string_equals(AetherString* a, AetherString* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->length != b->length) return 0;
    return memcmp(a->data, b->data, a->length) == 0;
}

int aether_string_compare(AetherString* a, AetherString* b) {
    if (!a || !b) return 0;
    return strcmp(a->data, b->data);
}

// String methods
int aether_string_starts_with(AetherString* str, AetherString* prefix) {
    if (!str || !prefix) return 0;
    if (prefix->length > str->length) return 0;
    return memcmp(str->data, prefix->data, prefix->length) == 0;
}

int aether_string_ends_with(AetherString* str, AetherString* suffix) {
    if (!str || !suffix) return 0;
    if (suffix->length > str->length) return 0;
    return memcmp(str->data + (str->length - suffix->length), 
                  suffix->data, suffix->length) == 0;
}

int aether_string_contains(AetherString* str, AetherString* substring) {
    return aether_string_index_of(str, substring) >= 0;
}

int aether_string_index_of(AetherString* str, AetherString* substring) {
    if (!str || !substring) return -1;
    if (substring->length > str->length) return -1;
    
    for (size_t i = 0; i <= str->length - substring->length; i++) {
        if (memcmp(str->data + i, substring->data, substring->length) == 0) {
            return (int)i;
        }
    }
    return -1;
}

AetherString* aether_string_substring(AetherString* str, int start, int end) {
    if (!str) return NULL;
    if (start < 0) start = 0;
    if (end > (int)str->length) end = (int)str->length;
    if (start >= end) return aether_string_empty();
    
    return aether_string_new_with_length(str->data + start, end - start);
}

AetherString* aether_string_to_upper(AetherString* str) {
    if (!str) return NULL;
    
    char* new_data = (char*)malloc(str->length + 1);
    for (size_t i = 0; i < str->length; i++) {
        new_data[i] = toupper(str->data[i]);
    }
    new_data[str->length] = '\0';
    
    AetherString* result = aether_string_new_with_length(new_data, str->length);
    free(new_data);
    return result;
}

AetherString* aether_string_to_lower(AetherString* str) {
    if (!str) return NULL;
    
    char* new_data = (char*)malloc(str->length + 1);
    for (size_t i = 0; i < str->length; i++) {
        new_data[i] = tolower(str->data[i]);
    }
    new_data[str->length] = '\0';
    
    AetherString* result = aether_string_new_with_length(new_data, str->length);
    free(new_data);
    return result;
}

AetherString* aether_string_trim(AetherString* str) {
    if (!str) return NULL;
    
    size_t start = 0;
    size_t end = str->length;
    
    // Trim from start
    while (start < str->length && isspace(str->data[start])) start++;
    
    // Trim from end
    while (end > start && isspace(str->data[end - 1])) end--;
    
    if (start == 0 && end == str->length) {
        aether_string_retain(str);
        return str;
    }
    
    return aether_string_substring(str, start, end);
}

// String array operations
AetherStringArray* aether_string_split(AetherString* str, AetherString* delimiter) {
    if (!str || !delimiter) return NULL;
    
    AetherStringArray* arr = (AetherStringArray*)malloc(sizeof(AetherStringArray));
    arr->count = 0;
    arr->strings = NULL;
    
    if (delimiter->length == 0) {
        // Split into characters
        arr->count = str->length;
        arr->strings = (AetherString**)malloc(sizeof(AetherString*) * arr->count);
        for (size_t i = 0; i < str->length; i++) {
            arr->strings[i] = aether_string_new_with_length(str->data + i, 1);
        }
        return arr;
    }
    
    // Count delimiters
    size_t count = 1;
    for (size_t i = 0; i <= str->length - delimiter->length; i++) {
        if (memcmp(str->data + i, delimiter->data, delimiter->length) == 0) {
            count++;
            i += delimiter->length - 1;
        }
    }
    
    arr->count = count;
    arr->strings = (AetherString**)malloc(sizeof(AetherString*) * count);
    
    size_t start = 0;
    size_t idx = 0;
    for (size_t i = 0; i <= str->length - delimiter->length; i++) {
        if (memcmp(str->data + i, delimiter->data, delimiter->length) == 0) {
            arr->strings[idx++] = aether_string_substring(str, start, i);
            start = i + delimiter->length;
            i += delimiter->length - 1;
        }
    }
    // Add remaining part
    arr->strings[idx] = aether_string_substring(str, start, str->length);
    
    return arr;
}

void aether_string_array_free(AetherStringArray* arr) {
    if (!arr) return;
    for (size_t i = 0; i < arr->count; i++) {
        aether_string_release(arr->strings[i]);
    }
    free(arr->strings);
    free(arr);
}

// Conversion
const char* aether_string_to_cstr(AetherString* str) {
    return str ? str->data : "";
}

AetherString* aether_string_from_int(int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return aether_string_new(buffer);
}

AetherString* aether_string_from_float(float value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%g", value);
    return aether_string_new(buffer);
}

// Parsing functions - convert string to numbers
int aether_string_to_int(AetherString* str, int* out_value) {
    if (!str || !str->data || !out_value) return 0;

    char* endptr;
    errno = 0;
    long val = strtol(str->data, &endptr, 10);

    // Check for errors: no conversion, overflow, or trailing garbage
    if (endptr == str->data || errno == ERANGE || val > INT_MAX || val < INT_MIN) {
        return 0;
    }

    // Skip trailing whitespace
    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return 0;  // Trailing non-whitespace

    *out_value = (int)val;
    return 1;
}

int aether_string_to_long(AetherString* str, long* out_value) {
    if (!str || !str->data || !out_value) return 0;

    char* endptr;
    errno = 0;
    long val = strtol(str->data, &endptr, 10);

    if (endptr == str->data || errno == ERANGE) {
        return 0;
    }

    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return 0;

    *out_value = val;
    return 1;
}

int aether_string_to_float(AetherString* str, float* out_value) {
    if (!str || !str->data || !out_value) return 0;

    char* endptr;
    errno = 0;
    float val = strtof(str->data, &endptr);

    if (endptr == str->data || errno == ERANGE) {
        return 0;
    }

    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return 0;

    *out_value = val;
    return 1;
}

int aether_string_to_double(AetherString* str, double* out_value) {
    if (!str || !str->data || !out_value) return 0;

    char* endptr;
    errno = 0;
    double val = strtod(str->data, &endptr);

    if (endptr == str->data || errno == ERANGE) {
        return 0;
    }

    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return 0;

    *out_value = val;
    return 1;
}

// Printf-style string formatting
AetherString* aether_string_format(const char* fmt, ...) {
    if (!fmt) return aether_string_empty();

    va_list args;

    // First pass: calculate required size
    va_start(args, fmt);
    int size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (size < 0) return aether_string_empty();

    // Allocate buffer
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) return aether_string_empty();

    // Second pass: format string
    va_start(args, fmt);
    vsnprintf(buffer, size + 1, fmt, args);
    va_end(args);

    AetherString* result = aether_string_new_with_length(buffer, size);
    free(buffer);
    return result;
}

