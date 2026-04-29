#ifndef AETHER_STRING_H
#define AETHER_STRING_H

#include <stddef.h>

// Magic number to distinguish AetherString* from raw char*
#define AETHER_STRING_MAGIC 0xAE57C0DE

// String structure - immutable, reference counted
typedef struct AetherString {
    unsigned int magic;     // Always AETHER_STRING_MAGIC for valid AetherString
    int ref_count;
    size_t length;
    size_t capacity;
    char* data;
} AetherString;

// Check if a pointer is an AetherString (vs raw char*).
//
// Reads bytes from the pointer to look for the magic header. Done
// byte-by-byte with short-circuit so that for the 99.6% of inputs
// whose first byte mismatches, we read exactly one byte — keeping
// the dispatch ASan-clean on short string allocations like "hi" (2
// bytes) or "x" (1 byte) where a naive `s->magic` 4-byte read
// overshoots the allocation.
//
// On a confirmed magic match, also validate that the rest of the
// header looks like a real AetherString — non-negative ref_count,
// capacity ≥ length, non-NULL data — to harden against the 1-in-2^32
// chance that arbitrary content happens to begin with the magic
// bytes. This catches spurious dispatch before we deref s->data.
static inline int is_aether_string(const void* ptr) {
    if (!ptr) return 0;
    const unsigned char* p = (const unsigned char*)ptr;
    /* Magic 0xAE57C0DE in little-endian = DE C0 57 AE. */
    if (p[0] != 0xDE) return 0;
    if (p[1] != 0xC0) return 0;
    if (p[2] != 0x57) return 0;
    if (p[3] != 0xAE) return 0;
    /* All four bytes matched. Validate the header for plausibility
     * to reject coincidental magic-bytes-in-payload before any
     * downstream s->data deref. */
    const AetherString* s = (const AetherString*)ptr;
    if (s->ref_count < 0) return 0;
    if (s->capacity < s->length) return 0;
    if (!s->data) return 0;
    return 1;
}

// String creation
AetherString* string_new(const char* cstr);
AetherString* string_from_cstr(const char* cstr);  // Alias for new
AetherString* string_from_literal(const char* cstr);  // Alias for new
AetherString* string_new_with_length(const char* data, size_t length);
AetherString* string_empty();

// Reference counting — safe to call with plain char* (no-op)
void string_retain(const void* str);
void string_release(const void* str);
void string_free(const void* str);  // Alias for release

// String operations — accept both AetherString* and plain char*.
// `string_concat` returns a plain char* payload (no AetherString
// header). Use `string_concat_wrapped` instead when the inputs may
// contain embedded NULs and you need length-aware downstream
// behaviour — see #270 and docs/c-interop.md § AetherString
// return-type contract.
char* string_concat(const void* a, const void* b);
AetherString* string_concat_wrapped(const void* a, const void* b);
int string_length(const void* str);
char string_char_at(const void* str, int index);
int string_equals(const void* a, const void* b);
int string_compare(const void* a, const void* b);

// String methods — accept both AetherString* and plain char*
// Return plain char* (caller owns memory, free with free())
int string_starts_with(const void* str, const char* prefix);
int string_ends_with(const void* str, const char* suffix);
int string_contains(const void* str, const char* substring);
int string_index_of(const void* str, const char* substring);
// Like string_index_of but starts scanning at byte offset `start`.
// Returns the absolute offset of the hit (not relative to `start`),
// or -1. Same binary-safety split as string_index_of: the haystack
// is data-aware (handles AetherString with embedded NULs), the
// needle is strlen-based.
int string_index_of_from(const void* str, const char* substring, int start);
char* string_substring(const void* str, int start, int end);

// Construct a 1-byte AetherString from a byte code (0..255).
// Primary use: emitting known single-byte markers (\x01, \x02, etc.)
// into packed-string record formats without routing through a
// NUL-terminated literal. Always length 1, even for code=0.
AetherString* string_from_char(int code);
char* string_to_upper(const void* str);
char* string_to_lower(const void* str);
char* string_trim(const void* str);

// String array operations (for split)
typedef struct {
    AetherString** strings;
    size_t count;
} AetherStringArray;

AetherStringArray* string_split(const void* str, const char* delimiter);
int string_array_size(AetherStringArray* arr);
const char* string_array_get(AetherStringArray* arr, int index);
void string_array_free(AetherStringArray* arr);

/* Sibling of `string_split` that returns the result as a cons-cell
 * `*StringSeq` (see std/collections/aether_stringseq.h). Same split
 * semantics — empty input gives a single-cell list with "", a delim
 * longer than the input gives a single-cell list with the whole
 * input, trailing/leading delimiters yield empty pieces — but the
 * shape on return is the Erlang-style sequence rather than the
 * dynamic-array AetherStringArray. Useful when the result will be
 * pattern-matched, walked recursively, or sent across an actor
 * boundary as a message field. The returned pointer is a `void*`
 * to keep this header free of a layering dependency on
 * aether_stringseq.h; callers cast to `StringSeq*` (or Aether-side
 * declared as `*StringSeq`). NULL on OOM or NULL inputs. */
void* string_split_to_seq(const void* str, const char* delimiter);

// Conversion
const char* string_to_cstr(const void* str);

// Public FFI accessors for consuming a `-> string` return from C.
//
// A function that an Aether program declares as `-> string` returns an
// AetherString* — a 24-byte magic-tagged header whose `data` field
// points at the payload — NOT a plain char*. C shims that type the
// same extern as `extern const char* foo(...)` and hand the result to
// memcpy/strlen read into the struct header and get garbage (the
// magic bytes, not the content). These helpers accept either shape
// (AetherString* or raw char*), return the byte pointer / length the
// shim actually wanted, and are safe to call on NULL.
//
// Preferred over `string_to_cstr` / `string_length` in FFI code
// because the `aether_` prefix matches the ABI-mangled export names
// and signals intent to reviewers. See docs/aether-string-abi.md for
// the full ABI contract.
const char* aether_string_data(const void* s);
size_t      aether_string_length(const void* s);

AetherString* string_from_int(int value);
// 64-bit sibling of string_from_int. Uses `long long` so it covers
// Aether's `long` type across platforms where `long` is 32-bit (MSVC).
AetherString* string_from_long(long long value);
AetherString* string_from_float(float value);

// Parsing (string -> number) — raw form with out-parameter.
// Returns 1 on success, 0 on failure. Result stored in out_value.
// Aether code should prefer the Go-style wrappers in std.string
// (`string.to_int`, etc.) which return `(value, error)` tuples.
int string_to_int_raw(const void* str, int* out_value);
int string_to_long_raw(const void* str, long* out_value);
int string_to_float_raw(const void* str, float* out_value);
int string_to_double_raw(const void* str, double* out_value);

// Split-return helpers used by the Go-style wrappers. `_try` returns
// 1 if parseable, 0 otherwise. `_get` returns the parsed value or
// zero-value on failure.
int    string_try_int(const void* s);
int    string_get_int(const void* s);
int    string_try_long(const void* s);
long   string_get_long(const void* s);
int    string_try_float(const void* s);
float  string_get_float(const void* s);
int    string_try_double(const void* s);
double string_get_double(const void* s);

// Formatting (printf-style — C-only, varargs)
AetherString* string_format(const char* fmt, ...);

// Aether-callable formatter. Walks `fmt` substituting `{}` with each
// entry of `args` (an Aether ArrayList of strings). `{{` and `}}`
// are literal braces. Returns a refcounted AetherString. Closes #272.
AetherString* string_format_list(const char* fmt, void* args);

#endif // AETHER_STRING_H
