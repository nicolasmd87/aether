#include "test_harness.h"
#include "../compiler/lexer.h"
#include "../compiler/parser.h"
#include "../compiler/typechecker.h"
#include "../compiler/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compile_and_check(const char* code) {
    lexer_init(code);
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
    
    if (!ast) return 0;
    
    TypeChecker* tc = create_typechecker();
    int result = typecheck_program(tc, ast);
    free_typechecker(tc);
    free_ast(ast);
    
    return result;
}

TEST(compile_simple_assignment) {
    const char* code = "x = 42\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_function_definition) {
    const char* code = "add(a, b) { return a + b }\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_if_statement) {
    const char* code = "main() { if (1) { x = 2 } }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_while_loop) {
    const char* code = "main() { x = 0\n while (x < 10) { x = x + 1 } }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_for_loop) {
    const char* code = "main() { for (i = 0; i < 10; i = i + 1) { } }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_array_declaration) {
    const char* code = "nums = [1, 2, 3, 4, 5]\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_array_indexing) {
    const char* code = "nums = [1, 2, 3]\nx = nums[0]\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_struct_definition) {
    const char* code = "struct Point { x = 0; y = 0; }\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_actor_definition) {
    const char* code = "actor Counter { state count = 0\n receive(msg) { } }\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_binary_operations) {
    const char* code = "x = 1 + 2 * 3 - 4 / 2\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_comparison_operations) {
    const char* code = "a = 5 < 10\nb = 5 > 10\nc = 5 == 5\nd = 5 != 10\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_nested_function_calls) {
    const char* code = "f(x) { return x * 2 }\ng(x) { return f(x) + 1 }\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_multiple_functions) {
    const char* code = "add(a,b) { return a+b }\nsub(a,b) { return a-b }\nmul(a,b) { return a*b }\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_conditional_return) {
    const char* code = "max(a, b) { if (a > b) { return a } return b }\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

TEST(compile_string_literal) {
    const char* code = "msg = \"Hello World\"\nmain() { }\n";
    int result = compile_and_check(code);
    ASSERT_EQ(1, result);
}

