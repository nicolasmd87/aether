#include "../runtime/test_harness.h"
#include "../../compiler/parser/lexer.h"
#include "../../compiler/parser/parser.h"
#include <string.h>

TEST(parser_basic_expressions) {
    const char* source = "x = 1 + 2";
    lexer_init(source);
    Token tokens[64];
    int count = 0;
    Token* tok;
    while ((tok = next_token()) != NULL && tok->type != TOKEN_EOF && count < 63) {
        tokens[count++] = *tok;
        free(tok);
    }
    if (tok) free(tok);
    tokens[count].type = TOKEN_EOF;
    tokens[count].value = NULL;
    count++;

    ASTNode* ast = parse(tokens, count);
    ASSERT_NOT_NULL(ast);
    ASSERT_TRUE(ast->child_count > 0);
    free_ast_node(ast);
}

TEST(parser_for_loops) {
    const char* source = "for i = 0; i < 10; i++ { x = i }";
    lexer_init(source);
    Token tokens[64];
    int count = 0;
    Token* tok;
    while ((tok = next_token()) != NULL && tok->type != TOKEN_EOF && count < 63) {
        tokens[count++] = *tok;
        free(tok);
    }
    if (tok) free(tok);
    tokens[count].type = TOKEN_EOF;
    tokens[count].value = NULL;
    count++;

    ASTNode* ast = parse(tokens, count);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(parser_while_loops) {
    const char* source = "while x > 0 { x = x - 1 }";
    lexer_init(source);
    Token tokens[64];
    int count = 0;
    Token* tok;
    while ((tok = next_token()) != NULL && tok->type != TOKEN_EOF && count < 63) {
        tokens[count++] = *tok;
        free(tok);
    }
    if (tok) free(tok);
    tokens[count].type = TOKEN_EOF;
    tokens[count].value = NULL;
    count++;

    ASTNode* ast = parse(tokens, count);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}
