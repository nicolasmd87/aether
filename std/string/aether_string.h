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
AetherString* string_new(const char* cstr);
AetherString* string_from_cstr(const char* cstr);  // Alias for new
AetherString* string_from_literal(const char* cstr);  // Alias for new
AetherString* string_new_with_length(const char* data, size_t length);
AetherString* string_empty();

// Reference counting
void string_retain(AetherString* str);
void string_release(AetherString* str);
void string_free(AetherString* str);  // Alias for release

// String operations
AetherString* string_concat(AetherString* a, AetherString* b);
int string_length(AetherString* str);
char string_char_at(AetherString* str, int index);
int string_equals(AetherString* a, AetherString* b);
int string_compare(AetherString* a, AetherString* b);

// String methods
int string_starts_with(AetherString* str, AetherString* prefix);
int string_ends_with(AetherString* str, AetherString* suffix);
int string_contains(AetherString* str, AetherString* substring);
int string_index_of(AetherString* str, AetherString* substring);
AetherString* string_substring(AetherString* str, int start, int end);
AetherString* string_to_upper(AetherString* str);
AetherString* string_to_lower(AetherString* str);
AetherString* string_trim(AetherString* str);

// String array operations (for split)
typedef struct {
    AetherString** strings;
    size_t count;
} AetherStringArray;

AetherStringArray* string_split(AetherString* str, AetherString* delimiter);
void string_array_free(AetherStringArray* arr);

// Conversion
const char* string_to_cstr(AetherString* str);
AetherString* string_from_int(int value);
AetherString* string_from_float(float value);

// Parsing (string -> number)
// Returns 1 on success, 0 on failure. Result stored in out_value.
int string_to_int(AetherString* str, int* out_value);
int string_to_long(AetherString* str, long* out_value);
int string_to_float(AetherString* str, float* out_value);
int string_to_double(AetherString* str, double* out_value);

// Formatting (printf-style)
AetherString* string_format(const char* fmt, ...);

#endif // AETHER_STRING_H
