#include "test_harness.h"
#include "../compiler/lexer.h"
#include "../compiler/parser.h"
#include "../compiler/codegen.h"
#include <string.h>

static char* generate_c_code(const char* aether_code) {
    lexer_init(aether_code);
    Token** tokens = malloc(1000 * sizeof(Token*));
    int count = 0;
    Token* tok;
    while ((tok = next_token())->type != TOKEN_EOF && count < 999) {
        tokens[count++] = tok;
    }
    tokens[count++] = tok;
    
    Parser* parser = create_parser(tokens, count);
    ASTNode* ast = parse_program(parser);
    free_parser(parser);
    
    if (!ast) return NULL;
    
    CodeGenerator* cg = create_codegen();
    generate_code(cg, ast);
    char* code = strdup(cg->output);
    free_codegen(cg);
    free_ast(ast);
    
    return code;
}

TEST(codegen_includes_headers) {
    char* code = generate_c_code("main() { }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "#include"));
    free(code);
}

TEST(codegen_main_function) {
    char* code = generate_c_code("main() { }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "int main"));
    free(code);
}

TEST(codegen_variable_declaration) {
    char* code = generate_c_code("x = 42\nmain() { }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "= 42"));
    free(code);
}

TEST(codegen_function_declaration) {
    char* code = generate_c_code("add(a, b) { return a + b }\nmain() { }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "add"));
    ASSERT_NOT_NULL(strstr(code, "return"));
    free(code);
}

TEST(codegen_if_statement) {
    char* code = generate_c_code("main() { if (1) { x = 2 } }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "if"));
    free(code);
}

TEST(codegen_while_loop) {
    char* code = generate_c_code("main() { x = 0\n while (x < 10) { x = x + 1 } }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "while"));
    free(code);
}

TEST(codegen_for_loop) {
    char* code = generate_c_code("main() { for (i = 0; i < 10; i = i + 1) { } }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "for"));
    free(code);
}

TEST(codegen_array_literal) {
    char* code = generate_c_code("nums = [1, 2, 3]\nmain() { }\n");
    ASSERT_NOT_NULL(code);
    free(code);
}

TEST(codegen_binary_operations) {
    char* code = generate_c_code("x = 1 + 2\ny = 3 * 4\nz = 5 - 6\nmain() { }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "+"));
    ASSERT_NOT_NULL(strstr(code, "*"));
    ASSERT_NOT_NULL(strstr(code, "-"));
    free(code);
}

TEST(codegen_comparison_ops) {
    char* code = generate_c_code("a = 5 < 10\nb = 5 > 3\nmain() { }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "<"));
    ASSERT_NOT_NULL(strstr(code, ">"));
    free(code);
}

TEST(codegen_function_call) {
    char* code = generate_c_code("f(x) { return x }\nmain() { y = f(5) }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "f("));
    free(code);
}

TEST(codegen_struct) {
    char* code = generate_c_code("struct Point { x = 0; y = 0; }\nmain() { }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "struct") || strstr(code, "typedef"));
    free(code);
}

TEST(codegen_actor) {
    char* code = generate_c_code("actor Counter { state count = 0\n receive(msg) { } }\nmain() { }\n");
    ASSERT_NOT_NULL(code);
    free(code);
}

TEST(codegen_return_statement) {
    char* code = generate_c_code("f() { return 42 }\nmain() { }\n");
    ASSERT_NOT_NULL(code);
    ASSERT_NOT_NULL(strstr(code, "return"));
    free(code);
}

TEST(codegen_nested_blocks) {
    char* code = generate_c_code("main() { if (1) { if (2) { x = 3 } } }\n");
    ASSERT_NOT_NULL(code);
    free(code);
}

