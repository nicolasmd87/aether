#include "test_harness.h"
#include "../compiler/lexer.h"
#include "../compiler/parser.h"
#include "../compiler/codegen.h"
#include "../compiler/typechecker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Integration test: Full compilation pipeline

static int compile_and_check(const char* source_code, const char* output_file) {
    // Lexer phase
    lexer_init(source_code);
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
    
    if (count == 0) {
        free(tokens);
        return 0;  // Failed: no tokens
    }
    
    // Parser phase
    Parser* parser = create_parser(tokens, count);
    ASTNode* ast = parse_program(parser);
    free_parser(parser);
    free(tokens);
    
    if (!ast) {
        return 0;  // Failed: parse error
    }
    
    // Code generation phase
    FILE* out = fopen(output_file, "w");
    if (!out) {
        free_ast_node(ast);
        return 0;  // Failed: couldn't open output
    }
    
    CodeGenerator* gen = create_codegen(out);
    generate_program(gen, ast);
    free_codegen(gen);
    fclose(out);
    
    free_ast_node(ast);
    return 1;  // Success
}

TEST(integration_hello_world) {
    const char* code = "main() { print(\"Hello, World!\") }\n";
    int result = compile_and_check(code, "test_hello.c");
    ASSERT_TRUE(result);
    
    // Check output file exists
    FILE* f = fopen("test_hello.c", "r");
    ASSERT_NOT_NULL(f);
    fclose(f);
    remove("test_hello.c");
}

TEST(integration_simple_function) {
    const char* code = 
        "add(a, b) { return a + b }\n"
        "main() { x = add(5, 3) }\n";
    
    int result = compile_and_check(code, "test_func.c");
    ASSERT_TRUE(result);
    remove("test_func.c");
}

TEST(integration_struct_definition) {
    const char* code = 
        "struct Point { int x int y }\n"
        "main() { p = Point{ x: 10, y: 20 } }\n";
    
    int result = compile_and_check(code, "test_struct.c");
    ASSERT_TRUE(result);
    remove("test_struct.c");
}

TEST(integration_control_flow) {
    const char* code = 
        "main() {\n"
        "    x = 10\n"
        "    if (x > 5) {\n"
        "        print(\"big\")\n"
        "    } else {\n"
        "        print(\"small\")\n"
        "    }\n"
        "    for (i = 0; i < 10; i = i + 1) {\n"
        "        print(i)\n"
        "    }\n"
        "    while (x > 0) {\n"
        "        x = x - 1\n"
        "    }\n"
        "}\n";
    
    int result = compile_and_check(code, "test_control.c");
    ASSERT_TRUE(result);
    remove("test_control.c");
}

TEST(integration_switch_statement) {
    const char* code = 
        "main() {\n"
        "    x = 2\n"
        "    switch (x) {\n"
        "        case 1:\n"
        "            print(\"one\")\n"
        "        case 2:\n"
        "            print(\"two\")\n"
        "        default:\n"
        "            print(\"other\")\n"
        "    }\n"
        "}\n";
    
    int result = compile_and_check(code, "test_switch.c");
    ASSERT_TRUE(result);
    remove("test_switch.c");
}

TEST(integration_pattern_matching) {
    const char* code = 
        "main() {\n"
        "    x = 5\n"
        "    match (x) {\n"
        "        1 => print(\"one\")\n"
        "        5 => print(\"five\")\n"
        "        _ => print(\"other\")\n"
        "    }\n"
        "}\n";
    
    int result = compile_and_check(code, "test_match.c");
    ASSERT_TRUE(result);
    remove("test_match.c");
}

TEST(integration_arrays) {
    const char* code = 
        "main() {\n"
        "    arr = [1, 2, 3, 4, 5]\n"
        "    x = arr[0]\n"
        "    arr[1] = 10\n"
        "}\n";
    
    int result = compile_and_check(code, "test_arrays.c");
    ASSERT_TRUE(result);
    remove("test_arrays.c");
}

TEST(integration_nested_functions) {
    const char* code = 
        "outer() {\n"
        "    inner(x) {\n"
        "        return x * 2\n"
        "    }\n"
        "    return inner(5)\n"
        "}\n"
        "main() {\n"
        "    result = outer()\n"
        "}\n";
    
    // Note: nested functions may not fully work yet
    int result = compile_and_check(code, "test_nested.c");
    ASSERT_TRUE(result);
    remove("test_nested.c");
}

TEST(integration_multiple_statements) {
    const char* code = 
        "main() {\n"
        "    a = 1\n"
        "    b = 2\n"
        "    c = a + b\n"
        "    d = c * 2\n"
        "    e = d - 1\n"
        "    print(e)\n"
        "}\n";
    
    int result = compile_and_check(code, "test_multi.c");
    ASSERT_TRUE(result);
    remove("test_multi.c");
}

TEST(integration_expressions) {
    const char* code = 
        "main() {\n"
        "    x = (5 + 3) * 2 - 1\n"
        "    y = x / 3\n"
        "    z = y % 2\n"
        "    w = x == y\n"
        "    v = x < y || y > z\n"
        "    u = !v && w\n"
        "}\n";
    
    int result = compile_and_check(code, "test_expr.c");
    ASSERT_TRUE(result);
    remove("test_expr.c");
}

TEST(integration_struct_member_access) {
    const char* code = 
        "struct Vec2 { int x int y }\n"
        "main() {\n"
        "    v = Vec2{ x: 5, y: 10 }\n"
        "    a = v.x\n"
        "    b = v.y\n"
        "    v.x = 20\n"
        "}\n";
    
    int result = compile_and_check(code, "test_member.c");
    ASSERT_TRUE(result);
    remove("test_member.c");
}

TEST(integration_complex_program) {
    const char* code = 
        "struct Point { int x int y }\n"
        "\n"
        "distance(p1, p2) {\n"
        "    dx = p1.x - p2.x\n"
        "    dy = p1.y - p2.y\n"
        "    return dx * dx + dy * dy\n"
        "}\n"
        "\n"
        "main() {\n"
        "    p1 = Point{ x: 0, y: 0 }\n"
        "    p2 = Point{ x: 3, y: 4 }\n"
        "    d = distance(p1, p2)\n"
        "    print(d)\n"
        "}\n";
    
    int result = compile_and_check(code, "test_complex.c");
    ASSERT_TRUE(result);
    remove("test_complex.c");
}

TEST(integration_module_declaration) {
    const char* code = 
        "module math.geometry\n"
        "\n"
        "export struct Point { int x int y }\n"
        "\n"
        "export distance(p1, p2) {\n"
        "    return 0\n"
        "}\n"
        "\n"
        "main() { }\n";
    
    int result = compile_and_check(code, "test_module.c");
    ASSERT_TRUE(result);
    remove("test_module.c");
}

TEST(integration_import_statement) {
    const char* code = 
        "import std.io\n"
        "import math.geometry (Point, distance)\n"
        "\n"
        "main() {\n"
        "    print(\"test\")\n"
        "}\n";
    
    int result = compile_and_check(code, "test_import.c");
    ASSERT_TRUE(result);
    remove("test_import.c");
}

TEST(integration_type_inference) {
    const char* code = 
        "main() {\n"
        "    x = 42\n"
        "    y = 3.14\n"
        "    z = \"hello\"\n"
        "    w = true\n"
        "    arr = [1, 2, 3]\n"
        "}\n";
    
    int result = compile_and_check(code, "test_inference.c");
    ASSERT_TRUE(result);
    remove("test_inference.c");
}

TEST(integration_empty_functions) {
    const char* code = 
        "empty() { }\n"
        "also_empty() { return }\n"
        "main() { empty() }\n";
    
    int result = compile_and_check(code, "test_empty.c");
    ASSERT_TRUE(result);
    remove("test_empty.c");
}

TEST(integration_string_operations) {
    const char* code = 
        "main() {\n"
        "    s1 = \"Hello\"\n"
        "    s2 = \"World\"\n"
        "    print(s1)\n"
        "    print(s2)\n"
        "}\n";
    
    int result = compile_and_check(code, "test_strings.c");
    ASSERT_TRUE(result);
    remove("test_strings.c");
}

TEST(integration_boolean_logic) {
    const char* code = 
        "main() {\n"
        "    a = true\n"
        "    b = false\n"
        "    c = a && b\n"
        "    d = a || b\n"
        "    e = !a\n"
        "    f = a == b\n"
        "    g = a != b\n"
        "}\n";
    
    int result = compile_and_check(code, "test_bool.c");
    ASSERT_TRUE(result);
    remove("test_bool.c");
}

