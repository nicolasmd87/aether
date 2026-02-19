#include "../runtime/test_harness.h"
#include "../../compiler/parser/lexer.h"
#include "../../compiler/parser/parser.h"
#include "../../compiler/analysis/typechecker.h"
#include <string.h>

TEST(typechecker_basic_types) {
    const char* source = "x = 42";
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

    int errors = typecheck(ast);
    ASSERT_EQ(0, errors);
    free_ast_node(ast);
}

TEST(typechecker_loop_conditions) {
    const char* source = "for i = 0; i < 5; i++ { x = i }";
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

    int errors = typecheck(ast);
    ASSERT_EQ(0, errors);
    free_ast_node(ast);
}
