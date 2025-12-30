#include "test_harness.h"
#include "../compiler/lexer.h"
#include "../compiler/tokens.h"
#include <string.h>

TEST(lexer_basic_tokens) {
    lexer_init("int x = 42;");
    Token* token = next_token();
    ASSERT_NOT_NULL(token);
    ASSERT_EQ(token->type, TOKEN_INT);
    free_token(token);
}

TEST(lexer_keywords) {
    lexer_init("actor struct func main");
    Token* t1 = next_token();
    ASSERT_EQ(t1->type, TOKEN_ACTOR);
    free_token(t1);
    
    Token* t2 = next_token();
    ASSERT_EQ(t2->type, TOKEN_STRUCT);
    free_token(t2);
    
    Token* t3 = next_token();
    ASSERT_EQ(t3->type, TOKEN_FUNC);
    free_token(t3);
    
    Token* t4 = next_token();
    ASSERT_EQ(t4->type, TOKEN_MAIN);
    free_token(t4);
}

TEST(lexer_numbers) {
    lexer_init("123 456");
    Token* t1 = next_token();
    ASSERT_EQ(t1->type, TOKEN_NUMBER);
    ASSERT_STREQ(t1->value, "123");
    free_token(t1);
    
    Token* t2 = next_token();
    ASSERT_EQ(t2->type, TOKEN_NUMBER);
    ASSERT_STREQ(t2->value, "456");
    free_token(t2);
}

TEST(lexer_strings) {
    lexer_init("\"hello\" \"world\"");
    Token* t1 = next_token();
    ASSERT_EQ(t1->type, TOKEN_STRING_LITERAL);
    ASSERT_STREQ(t1->value, "hello");
    free_token(t1);
    
    Token* t2 = next_token();
    ASSERT_EQ(t2->type, TOKEN_STRING_LITERAL);
    ASSERT_STREQ(t2->value, "world");
    free_token(t2);
}
