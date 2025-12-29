#include "test_harness.h"
#include "../compiler/aether_error.h"
#include "../compiler/lexer.h"
#include <string.h>

TEST(error_init_cleanup) {
    aether_error_init();
    ASSERT_FALSE(aether_error_has_errors());
    aether_error_cleanup();
}

TEST(error_single_report) {
    aether_error_init();
    aether_error_report(ERROR_SYNTAX, "test.ae", 10, "Test error");
    ASSERT_TRUE(aether_error_has_errors());
    aether_error_cleanup();
}

TEST(error_multiple_reports) {
    aether_error_init();
    aether_error_report(ERROR_SYNTAX, "test.ae", 1, "Error 1");
    aether_error_report(ERROR_TYPE_MISMATCH, "test.ae", 2, "Error 2");
    aether_error_report(ERROR_UNDEFINED_VARIABLE, "test.ae", 3, "Error 3");
    ASSERT_TRUE(aether_error_has_errors());
    aether_error_cleanup();
}

TEST(error_codes_unique) {
    ASSERT_NE(ERROR_SYNTAX, ERROR_TYPE_MISMATCH);
    ASSERT_NE(ERROR_SYNTAX, ERROR_UNDEFINED_VARIABLE);
    ASSERT_NE(ERROR_TYPE_MISMATCH, ERROR_UNDEFINED_VARIABLE);
}

TEST(lexer_error_detection) {
    lexer_init("\"unterminated string");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_ERROR, tok->type);
    free_token(tok);
}

TEST(lexer_valid_tokens) {
    lexer_init("x = 42");
    Token* tok1 = next_token();
    ASSERT_EQ(TOKEN_IDENTIFIER, tok1->type);
    Token* tok2 = next_token();
    ASSERT_EQ(TOKEN_ASSIGN, tok2->type);
    Token* tok3 = next_token();
    ASSERT_EQ(TOKEN_NUMBER, tok3->type);
    free_token(tok1);
    free_token(tok2);
    free_token(tok3);
}

TEST(lexer_keywords) {
    lexer_init("if while for actor");
    Token* tok1 = next_token();
    ASSERT_EQ(TOKEN_IF, tok1->type);
    Token* tok2 = next_token();
    ASSERT_EQ(TOKEN_WHILE, tok2->type);
    Token* tok3 = next_token();
    ASSERT_EQ(TOKEN_FOR, tok3->type);
    Token* tok4 = next_token();
    ASSERT_EQ(TOKEN_ACTOR, tok4->type);
    free_token(tok1);
    free_token(tok2);
    free_token(tok3);
    free_token(tok4);
}

TEST(lexer_operators) {
    lexer_init("+ - * / == != < > <= >=");
    Token* tok1 = next_token();
    ASSERT_EQ(TOKEN_PLUS, tok1->type);
    Token* tok2 = next_token();
    ASSERT_EQ(TOKEN_MINUS, tok2->type);
    Token* tok3 = next_token();
    ASSERT_EQ(TOKEN_MULTIPLY, tok3->type);
    Token* tok4 = next_token();
    ASSERT_EQ(TOKEN_DIVIDE, tok4->type);
    free_token(tok1);
    free_token(tok2);
    free_token(tok3);
    free_token(tok4);
}

