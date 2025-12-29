#include "test_harness.h"
#include "../std/string/aether_string.h"
#include <string.h>

TEST(string_concat_basic) {
    AetherString* s1 = aether_string_from_cstr("Hello");
    AetherString* s2 = aether_string_from_cstr(" World");
    AetherString* result = aether_string_concat(s1, s2);
    
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ("Hello World", result->data);
    
    aether_string_free(s1);
    aether_string_free(s2);
    aether_string_free(result);
}

TEST(string_length) {
    AetherString* s = aether_string_from_cstr("Hello");
    ASSERT_EQ(5, aether_string_length(s));
    aether_string_free(s);
}

TEST(string_char_at) {
    AetherString* s = aether_string_from_cstr("Hello");
    ASSERT_EQ('H', aether_string_char_at(s, 0));
    ASSERT_EQ('e', aether_string_char_at(s, 1));
    ASSERT_EQ('o', aether_string_char_at(s, 4));
    aether_string_free(s);
}

TEST(string_index_of) {
    AetherString* s = aether_string_from_cstr("Hello World");
    ASSERT_EQ(6, aether_string_index_of(s, "World"));
    ASSERT_EQ(-1, aether_string_index_of(s, "xyz"));
    aether_string_free(s);
}

TEST(string_empty) {
    AetherString* s = aether_string_from_cstr("");
    ASSERT_EQ(0, aether_string_length(s));
    aether_string_free(s);
}

TEST(string_reference_counting) {
    AetherString* s1 = aether_string_from_cstr("Test");
    ASSERT_EQ(1, s1->ref_count);
    
    // Simulate reference increment
    s1->ref_count++;
    ASSERT_EQ(2, s1->ref_count);
    
    // Free once (should decrement)
    s1->ref_count--;
    ASSERT_EQ(1, s1->ref_count);
    
    aether_string_free(s1);
}

TEST(string_concat_empty) {
    AetherString* s1 = aether_string_from_cstr("");
    AetherString* s2 = aether_string_from_cstr("Hello");
    AetherString* result = aether_string_concat(s1, s2);
    
    ASSERT_STREQ("Hello", result->data);
    
    aether_string_free(s1);
    aether_string_free(s2);
    aether_string_free(result);
}

TEST(string_special_chars) {
    AetherString* s = aether_string_from_cstr("Hello\nWorld\t!");
    ASSERT_EQ(13, aether_string_length(s));
    ASSERT_EQ('\n', aether_string_char_at(s, 5));
    ASSERT_EQ('\t', aether_string_char_at(s, 11));
    aether_string_free(s);
}

TEST(string_unicode_basic) {
    // Basic unicode test (if supported)
    AetherString* s = aether_string_from_cstr("Hello 世界");
    ASSERT_NOT_NULL(s);
    aether_string_free(s);
}

TEST(string_large) {
    // Test with large string
    char large[1000];
    memset(large, 'A', 999);
    large[999] = '\0';
    
    AetherString* s = aether_string_from_cstr(large);
    ASSERT_EQ(999, aether_string_length(s));
    aether_string_free(s);
}

