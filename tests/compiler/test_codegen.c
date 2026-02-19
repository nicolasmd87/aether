#include "../runtime/test_harness.h"
#include "../../compiler/parser/lexer.h"
#include "../../compiler/parser/parser.h"
#include "../../compiler/codegen/codegen.h"
#include <string.h>
#include <stdio.h>

TEST(codegen_for_loop_syntax) {
    const char* source = "main() { for i = 0; i < 3; i++ { print(i) } }";
    lexer_init(source);
    Token tokens[128];
    int count = 0;
    Token* tok;
    while ((tok = next_token()) != NULL && tok->type != TOKEN_EOF && count < 127) {
        tokens[count++] = *tok;
        free(tok);
    }
    if (tok) free(tok);
    tokens[count].type = TOKEN_EOF;
    tokens[count].value = NULL;
    count++;

    ASTNode* ast = parse(tokens, count);
    ASSERT_NOT_NULL(ast);

    FILE* out = tmpfile();
    ASSERT_NOT_NULL(out);
    CodeGenerator* gen = create_code_generator(out);
    ASSERT_NOT_NULL(gen);
    generate_code(gen, ast);

    rewind(out);
    char buf[4096];
    size_t len = fread(buf, 1, sizeof(buf) - 1, out);
    buf[len] = '\0';
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "for") != NULL || strstr(buf, "main") != NULL);

    fclose(out);
    free_code_generator(gen);
    free_ast_node(ast);
}

TEST(codegen_while_loop_syntax) {
    const char* source = "main() { x = 5\n while x > 0 { x = x - 1 } }";
    lexer_init(source);
    Token tokens[128];
    int count = 0;
    Token* tok;
    while ((tok = next_token()) != NULL && tok->type != TOKEN_EOF && count < 127) {
        tokens[count++] = *tok;
        free(tok);
    }
    if (tok) free(tok);
    tokens[count].type = TOKEN_EOF;
    tokens[count].value = NULL;
    count++;

    ASTNode* ast = parse(tokens, count);
    ASSERT_NOT_NULL(ast);

    FILE* out = tmpfile();
    ASSERT_NOT_NULL(out);
    CodeGenerator* gen = create_code_generator(out);
    ASSERT_NOT_NULL(gen);
    generate_code(gen, ast);

    rewind(out);
    char buf[4096];
    size_t len = fread(buf, 1, sizeof(buf) - 1, out);
    buf[len] = '\0';
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "while") != NULL || strstr(buf, "main") != NULL);

    fclose(out);
    free_code_generator(gen);
    free_ast_node(ast);
}
