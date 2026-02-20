#include "../runtime/test_harness.h"
#include "../../compiler/parser/lexer.h"
#include "../../compiler/parser/parser.h"
#include "../../compiler/analysis/typechecker.h"
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

TEST(typechecker_basic_types) {
    ASTNode* ast = parse_source("main() { x = 42; }");
    ASSERT_NOT_NULL(ast);
    // typecheck_program returns 1 on success, 0 if there are type errors
    int result = typecheck_program(ast);
    ASSERT_EQ(1, result);
    free_ast_node(ast);
}

TEST(typechecker_loop_conditions) {
    ASTNode* ast = parse_source("main() { i = 0; while i < 5 { i = i + 1; } }");
    ASSERT_NOT_NULL(ast);
    int result = typecheck_program(ast);
    ASSERT_EQ(1, result);
    free_ast_node(ast);
}
