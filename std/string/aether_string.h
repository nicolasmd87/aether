#ifndef AETHER_STRING_H
#define AETHER_STRING_H

#include <stddef.h>

// String structure - immutable, reference counted
typedef struct AetherString {
    char* data;
    size_t length;
    size_t capacity;
    int ref_count;
} AetherString;

// String creation
AetherString* aether_string_new(const char* cstr);
AetherString* aether_string_from_cstr(const char* cstr);  // Alias for new
AetherString* aether_string_from_literal(const char* cstr);  // Alias for new
AetherString* aether_string_new_with_length(const char* data, size_t length);
AetherString* aether_string_empty();

// Reference counting
void aether_string_retain(AetherString* str);
void aether_string_release(AetherString* str);
void aether_string_free(AetherString* str);  // Alias for release

// String operations
AetherString* aether_string_concat(AetherString* a, AetherString* b);
int aether_string_length(AetherString* str);
char aether_string_char_at(AetherString* str, int index);
int aether_string_equals(AetherString* a, AetherString* b);
int aether_string_compare(AetherString* a, AetherString* b);

// String methods
int aether_string_starts_with(AetherString* str, AetherString* prefix);
int aether_string_ends_with(AetherString* str, AetherString* suffix);
int aether_string_contains(AetherString* str, AetherString* substring);
int aether_string_index_of(AetherString* str, AetherString* substring);
AetherString* aether_string_substring(AetherString* str, int start, int end);
AetherString* aether_string_to_upper(AetherString* str);
AetherString* aether_string_to_lower(AetherString* str);
AetherString* aether_string_trim(AetherString* str);

// String array operations (for split)
typedef struct {
    AetherString** strings;
    size_t count;
} AetherStringArray;

AetherStringArray* aether_string_split(AetherString* str, AetherString* delimiter);
void aether_string_array_free(AetherStringArray* arr);

// Conversion
const char* aether_string_to_cstr(AetherString* str);
AetherString* aether_string_from_int(int value);
AetherString* aether_string_from_float(float value);

// Parsing (string -> number)
// Returns 1 on success, 0 on failure. Result stored in out_value.
int aether_string_to_int(AetherString* str, int* out_value);
int aether_string_to_long(AetherString* str, long* out_value);
int aether_string_to_float(AetherString* str, float* out_value);
int aether_string_to_double(AetherString* str, double* out_value);

// Formatting (printf-style)
AetherString* aether_string_format(const char* fmt, ...);

#endif // AETHER_STRING_H

