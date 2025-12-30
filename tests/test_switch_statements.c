#include "test_harness.h"
#include "../compiler/lexer.h"
#include "../compiler/parser.h"
#include "../compiler/codegen.h"
#include "../compiler/typechecker.h"
#include <string.h>
#include <stdio.h>

static ASTNode* parse_code(const char* code) {
    lexer_init(code);
    Token** tokens = malloc(1000 * sizeof(Token*));
    int count = 0;
    Token* tok;
    int safety = 0;
    const int MAX_TOKENS = 999;
    while (safety++ < MAX_TOKENS && (tok = next_token())->type != TOKEN_EOF && count < MAX_TOKENS) {
        tokens[count++] = tok;
    }
    if (tok && tok->type == TOKEN_EOF) {
        tokens[count++] = tok;
    }
    Parser* parser = create_parser(tokens, count);
    ASTNode* ast = parse_program(parser);
    free_parser(parser);
    free(tokens);
    return ast;
}

TEST(switch_parse_basic) {
    const char* code = 
        "main() {\n"
        "    x = 1\n"
        "    switch (x) {\n"
        "        case 1:\n"
        "            print(\"one\")\n"
        "        case 2:\n"
        "            print(\"two\")\n"
        "        default:\n"
        "            print(\"other\")\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    ASSERT_EQ(AST_PROGRAM, ast->type);
    
    // Find main function
    ASSERT_TRUE(ast->child_count > 0);
    ASTNode* main_func = ast->children[0];
    ASSERT_EQ(AST_FUNCTION_DEFINITION, main_func->type);
    
    // Find function body
    ASSERT_TRUE(main_func->child_count > 0);
    ASTNode* body = main_func->children[main_func->child_count - 1];
    
    // Find switch statement (should be second statement after x = 1)
    ASSERT_TRUE(body->child_count >= 2);
    ASTNode* switch_stmt = body->children[1];
    ASSERT_EQ(AST_SWITCH_STATEMENT, switch_stmt->type);
    
    // Switch should have: expression + cases
    ASSERT_TRUE(switch_stmt->child_count >= 2);
    
    free_ast_node(ast);
}

TEST(switch_parse_empty) {
    const char* code = 
        "main() {\n"
        "    switch (x) {\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(switch_parse_single_case) {
    const char* code = 
        "main() {\n"
        "    switch (x) {\n"
        "        case 1:\n"
        "            print(\"one\")\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(switch_parse_default_only) {
    const char* code = 
        "main() {\n"
        "    switch (x) {\n"
        "        default:\n"
        "            print(\"default\")\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(switch_parse_multiple_statements_per_case) {
    const char* code = 
        "main() {\n"
        "    switch (x) {\n"
        "        case 1:\n"
        "            y = 10\n"
        "            z = 20\n"
        "            print(y + z)\n"
        "        case 2:\n"
        "            print(\"two\")\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(switch_parse_nested_blocks) {
    const char* code = 
        "main() {\n"
        "    switch (x) {\n"
        "        case 1:\n"
        "            if (y > 0) {\n"
        "                print(\"positive\")\n"
        "            }\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(switch_codegen_basic) {
    const char* code = 
        "main() {\n"
        "    x = 1\n"
        "    switch (x) {\n"
        "        case 1:\n"
        "            print(\"one\")\n"
        "        case 2:\n"
        "            print(\"two\")\n"
        "        default:\n"
        "            print(\"other\")\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    
    // Generate code to a temp file
    FILE* out = fopen("test_switch_output.c", "w");
    ASSERT_NOT_NULL(out);
    
    CodeGenerator* gen = create_codegen(out);
    generate_code(gen, ast);
    free_codegen(gen);
    fclose(out);
    
    // Verify the output file was created
    out = fopen("test_switch_output.c", "r");
    ASSERT_NOT_NULL(out);
    
    char buffer[4096];
    size_t bytes = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[bytes] = '\0';
    fclose(out);
    
    // Check for switch keyword in generated code
    ASSERT_TRUE(strstr(buffer, "switch") != NULL);
    ASSERT_TRUE(strstr(buffer, "case") != NULL);
    
    free_ast_node(ast);
    remove("test_switch_output.c");
}

TEST(switch_string_cases) {
    // Note: C doesn't support string switch, but we should handle parsing
    const char* code = 
        "main() {\n"
        "    x = \"hello\"\n"
        "    switch (x) {\n"
        "        case \"hello\":\n"
        "            print(\"greeting\")\n"
        "        default:\n"
        "            print(\"unknown\")\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(switch_expression_in_condition) {
    const char* code = 
        "main() {\n"
        "    switch (x + y * 2) {\n"
        "        case 10:\n"
        "            print(\"ten\")\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(switch_break_statements) {
    const char* code = 
        "main() {\n"
        "    switch (x) {\n"
        "        case 1:\n"
        "            print(\"one\")\n"
        "            break\n"
        "        case 2:\n"
        "            print(\"two\")\n"
        "            break\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

TEST(switch_fallthrough) {
    const char* code = 
        "main() {\n"
        "    switch (x) {\n"
        "        case 1:\n"
        "        case 2:\n"
        "        case 3:\n"
        "            print(\"1, 2, or 3\")\n"
        "    }\n"
        "}\n";
    
    ASTNode* ast = parse_code(code);
    ASSERT_NOT_NULL(ast);
    free_ast_node(ast);
}

