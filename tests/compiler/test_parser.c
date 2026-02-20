#include "../runtime/test_harness.h"
#include "../../compiler/parser/lexer.h"
#include "../../compiler/parser/parser.h"
#include <string.h>
#include <stdlib.h>

// Helper: tokenize + parse using current API
static ASTNode* parse_source(const char* source) {
    lexer_init(source);
    Token** tokens = malloc(sizeof(Token*) * 256);
    int count = 0;
    Token* tok;
    while ((tok = next_token()) != NULL && tok->type != TOKEN_EOF && count < 255) {
        tokens[count++] = tok;
    }
    if (tok) tokens[count++] = tok;
    Parser* parser = create_parser(tokens, count);
    ASTNode* ast = parse_program(parser);
    free_parser(parser);
    for (int i = 0; i < count; i++) free_token(tokens[i]);
    free(tokens);
    return ast;
}

TEST(parser_basic_expressions) {
    ASTNode* ast = parse_source("main() { x = 1 + 2 }");
    ASSERT_NOT_NULL(ast);
    ASSERT_TRUE(ast->child_count > 0);
    free_ast_node(ast);
}

TEST(parser_for_loops) {
    ASTNode* ast = parse_source("main() { for i = 0; i < 10; i++ { x = i } }");
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(parser_while_loops) {
    ASTNode* ast = parse_source("main() { while x > 0 { x = x - 1 } }");
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}
