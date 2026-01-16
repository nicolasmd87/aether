#include "../runtime/test_harness.h"
#include "../../compiler/frontend/lexer.h"
#include <string.h>

TEST(lexer_integer) {
    lexer_init("42");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_NUMBER, tok->type);
    ASSERT_STREQ("42", tok->value);
    free_token(tok);
}

TEST(lexer_float) {
    lexer_init("3.14");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_NUMBER, tok->type);
    ASSERT_STREQ("3.14", tok->value);
    free_token(tok);
}

TEST(lexer_identifier) {
    lexer_init("variable");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_IDENTIFIER, tok->type);
    ASSERT_STREQ("variable", tok->value);
    free_token(tok);
}

TEST(lexer_keyword_if) {
    lexer_init("if");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_IF, tok->type);
    free_token(tok);
}

TEST(lexer_keyword_while) {
    lexer_init("while");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_WHILE, tok->type);
    free_token(tok);
}

TEST(lexer_keyword_for) {
    lexer_init("for");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_FOR, tok->type);
    free_token(tok);
}

TEST(lexer_keyword_actor) {
    lexer_init("actor");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_ACTOR, tok->type);
    free_token(tok);
}

TEST(lexer_keyword_struct) {
    lexer_init("struct");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_STRUCT, tok->type);
    free_token(tok);
}

TEST(lexer_keyword_return) {
    lexer_init("return");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_RETURN, tok->type);
    free_token(tok);
}

TEST(lexer_string_simple) {
    lexer_init("\"hello\"");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_STRING_LITERAL, tok->type);
    ASSERT_STREQ("hello", tok->value);
    free_token(tok);
}

TEST(lexer_string_with_spaces) {
    lexer_init("\"hello world\"");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_STRING_LITERAL, tok->type);
    ASSERT_STREQ("hello world", tok->value);
    free_token(tok);
}

TEST(lexer_string_escape_newline) {
    lexer_init("\"hello\\nworld\"");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_STRING_LITERAL, tok->type);
    free_token(tok);
}

TEST(lexer_plus) {
    lexer_init("+");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_PLUS, tok->type);
    free_token(tok);
}

TEST(lexer_minus) {
    lexer_init("-");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_MINUS, tok->type);
    free_token(tok);
}

TEST(lexer_multiply) {
    lexer_init("*");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_MULTIPLY, tok->type);
    free_token(tok);
}

TEST(lexer_divide) {
    lexer_init("/");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_DIVIDE, tok->type);
    free_token(tok);
}

TEST(lexer_assign) {
    lexer_init("=");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_ASSIGN, tok->type);
    free_token(tok);
}

TEST(lexer_equals) {
    lexer_init("==");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_EQUALS, tok->type);
    free_token(tok);
}

TEST(lexer_not_equals) {
    lexer_init("!=");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_NOT_EQUALS, tok->type);
    free_token(tok);
}

TEST(lexer_less_than) {
    lexer_init("<");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_LESS, tok->type);
    free_token(tok);
}

TEST(lexer_greater_than) {
    lexer_init(">");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_GREATER, tok->type);
    free_token(tok);
}

TEST(lexer_less_equal) {
    lexer_init("<=");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_LESS_EQUAL, tok->type);
    free_token(tok);
}

TEST(lexer_greater_equal) {
    lexer_init(">=");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_GREATER_EQUAL, tok->type);
    free_token(tok);
}

TEST(lexer_left_paren) {
    lexer_init("(");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_LEFT_PAREN, tok->type);
    free_token(tok);
}

TEST(lexer_right_paren) {
    lexer_init(")");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_RIGHT_PAREN, tok->type);
    free_token(tok);
}

TEST(lexer_left_brace) {
    lexer_init("{");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_LEFT_BRACE, tok->type);
    free_token(tok);
}

TEST(lexer_right_brace) {
    lexer_init("}");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_RIGHT_BRACE, tok->type);
    free_token(tok);
}

TEST(lexer_left_bracket) {
    lexer_init("[");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_LEFT_BRACKET, tok->type);
    free_token(tok);
}

TEST(lexer_right_bracket) {
    lexer_init("]");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_RIGHT_BRACKET, tok->type);
    free_token(tok);
}

TEST(lexer_semicolon) {
    lexer_init(";");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_SEMICOLON, tok->type);
    free_token(tok);
}

TEST(lexer_comma) {
    lexer_init(",");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_COMMA, tok->type);
    free_token(tok);
}

TEST(lexer_dot) {
    lexer_init(".");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_DOT, tok->type);
    free_token(tok);
}

TEST(lexer_expression) {
    lexer_init("x = 1 + 2");
    Token* tok1 = next_token();
    ASSERT_EQ(TOKEN_IDENTIFIER, tok1->type);
    Token* tok2 = next_token();
    ASSERT_EQ(TOKEN_ASSIGN, tok2->type);
    Token* tok3 = next_token();
    ASSERT_EQ(TOKEN_NUMBER, tok3->type);
    Token* tok4 = next_token();
    ASSERT_EQ(TOKEN_PLUS, tok4->type);
    Token* tok5 = next_token();
    ASSERT_EQ(TOKEN_NUMBER, tok5->type);
    free_token(tok1);
    free_token(tok2);
    free_token(tok3);
    free_token(tok4);
    free_token(tok5);
}

TEST(lexer_skip_whitespace) {
    lexer_init("  x  =  42  ");
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

TEST(lexer_skip_line_comment) {
    lexer_init("x = 42 // this is a comment\ny = 10");
    Token* tok1 = next_token();
    ASSERT_EQ(TOKEN_IDENTIFIER, tok1->type);
    Token* tok2 = next_token();
    ASSERT_EQ(TOKEN_ASSIGN, tok2->type);
    Token* tok3 = next_token();
    ASSERT_EQ(TOKEN_NUMBER, tok3->type);
    Token* tok4 = next_token();
    ASSERT_EQ(TOKEN_IDENTIFIER, tok4->type);
    free_token(tok1);
    free_token(tok2);
    free_token(tok3);
    free_token(tok4);
}

TEST(lexer_multiple_lines) {
    lexer_init("x = 1\ny = 2\nz = 3");
    int count = 0;
    Token* tok;
    while ((tok = next_token())->type != TOKEN_EOF) {
        count++;
        free_token(tok);
    }
    free_token(tok);
    ASSERT_EQ(9, count);
}

TEST(lexer_eof) {
    lexer_init("");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_EOF, tok->type);
    free_token(tok);
}

TEST(lexer_keyword_true) {
    lexer_init("true");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_TRUE, tok->type);
    free_token(tok);
}

TEST(lexer_keyword_false) {
    lexer_init("false");
    Token* tok = next_token();
    ASSERT_EQ(TOKEN_FALSE, tok->type);
    free_token(tok);
}

