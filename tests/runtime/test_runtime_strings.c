#include "test_harness.h"
#include "../../std/string/aether_string.h"
#include <string.h>
#include <stdlib.h>

TEST_CATEGORY(string_concat_basic, TEST_CATEGORY_STDLIB) {
    AetherString* s1 = string_from_cstr("Hello");
    AetherString* s2 = string_from_cstr(" World");
    char* result = string_concat(s1, s2);

    ASSERT_NOT_NULL(result);
    ASSERT_STREQ("Hello World", result);

    string_free(s1);
    string_free(s2);
    free(result);
}

TEST_CATEGORY(string_length, TEST_CATEGORY_STDLIB) {
    AetherString* s = string_from_cstr("Hello");
    ASSERT_EQ(5, string_length(s));
    string_free(s);
}

TEST_CATEGORY(string_char_at, TEST_CATEGORY_STDLIB) {
    AetherString* s = string_from_cstr("Hello");
    ASSERT_EQ('H', string_char_at(s, 0));
    ASSERT_EQ('e', string_char_at(s, 1));
    ASSERT_EQ('o', string_char_at(s, 4));
    string_free(s);
}

TEST_CATEGORY(string_index_of, TEST_CATEGORY_STDLIB) {
    AetherString* s = string_from_cstr("Hello World");
    ASSERT_EQ(6, string_index_of(s, "World"));
    ASSERT_EQ(-1, string_index_of(s, "xyz"));
    string_free(s);
}

TEST_CATEGORY(string_empty, TEST_CATEGORY_STDLIB) {
    AetherString* s = string_from_cstr("");
    ASSERT_EQ(0, string_length(s));
    string_free(s);
}

TEST_CATEGORY(string_reference_counting, TEST_CATEGORY_STDLIB) {
    AetherString* s1 = string_from_cstr("Test");
    ASSERT_EQ(1, s1->ref_count);

    // Simulate reference increment
    s1->ref_count++;
    ASSERT_EQ(2, s1->ref_count);

    // Free once (should decrement)
    s1->ref_count--;
    ASSERT_EQ(1, s1->ref_count);

    string_free(s1);
}

TEST_CATEGORY(string_concat_empty, TEST_CATEGORY_STDLIB) {
    AetherString* s1 = string_from_cstr("");
    AetherString* s2 = string_from_cstr("Hello");
    char* result = string_concat(s1, s2);

    ASSERT_STREQ("Hello", result);

    string_free(s1);
    string_free(s2);
    free(result);
}

TEST_CATEGORY(string_special_chars, TEST_CATEGORY_STDLIB) {
    AetherString* s = string_from_cstr("Hello\nWorld\t!");
    ASSERT_EQ(13, string_length(s));
    ASSERT_EQ('\n', string_char_at(s, 5));
    ASSERT_EQ('\t', string_char_at(s, 11));
    string_free(s);
}

TEST_CATEGORY(string_unicode_basic, TEST_CATEGORY_STDLIB) {
    // Basic unicode test (if supported)
    AetherString* s = string_from_cstr("Hello 世界");
    ASSERT_NOT_NULL(s);
    string_free(s);
}

TEST_CATEGORY(string_large, TEST_CATEGORY_STDLIB) {
    // Test with large string
    char large[1000];
    memset(large, 'A', 999);
    large[999] = '\0';

    AetherString* s = string_from_cstr(large);
    ASSERT_EQ(999, string_length(s));
    string_free(s);
}
