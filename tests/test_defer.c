#include "test_harness.h"
#include "../compiler/lexer.h"
#include "../compiler/parser.h"
#include "../compiler/ast.h"

TEST(defer_token) {
    lexer_init("defer print(\"cleanup\");");
    Token* token = next_token();
    ASSERT_EQ(TOKEN_DEFER, token->type);
    free_token(token);
    lexer_cleanup();
}

TEST(defer_parse_simple) {
    lexer_init("func test() { defer print(\"cleanup\"); }");
    Parser* parser = create_parser();
    ASTNode* ast = parse_program(parser);
    
    ASSERT_NOT_NULL(ast);
    ASSERT_EQ(AST_PROGRAM, ast->type);
    ASSERT_TRUE(ast->child_count > 0);
    
    ASTNode* func = ast->children[0];
    ASSERT_EQ(AST_FUNCTION_DEFINITION, func->type);
    
    ASTNode* body = func->children[func->child_count - 1];
    ASSERT_EQ(AST_BLOCK, body->type);
    ASSERT_TRUE(body->child_count > 0);
    
    ASTNode* defer_stmt = body->children[0];
    ASSERT_EQ(AST_DEFER_STATEMENT, defer_stmt->type);
    ASSERT_EQ(1, defer_stmt->child_count);
    
    free_ast(ast);
    free_parser(parser);
    lexer_cleanup();
}

TEST(defer_multiple_statements) {
    lexer_init("func test() { defer close(file); defer free(mem); return 42; }");
    Parser* parser = create_parser();
    ASTNode* ast = parse_program(parser);
    
    ASSERT_NOT_NULL(ast);
    ASTNode* func = ast->children[0];
    ASTNode* body = func->children[func->child_count - 1];
    
    ASSERT_TRUE(body->child_count >= 2);
    ASSERT_EQ(AST_DEFER_STATEMENT, body->children[0]->type);
    ASSERT_EQ(AST_DEFER_STATEMENT, body->children[1]->type);
    
    free_ast(ast);
    free_parser(parser);
    lexer_cleanup();
}

TEST(defer_with_assignment) {
    lexer_init("func test() { x = 10; defer print(x); return x; }");
    Parser* parser = create_parser();
    ASTNode* ast = parse_program(parser);
    
    ASSERT_NOT_NULL(ast);
    ASTNode* func = ast->children[0];
    ASTNode* body = func->children[func->child_count - 1];
    
    ASSERT_TRUE(body->child_count >= 2);
    
    int defer_found = 0;
    for (int i = 0; i < body->child_count; i++) {
        if (body->children[i]->type == AST_DEFER_STATEMENT) {
            defer_found = 1;
            break;
        }
    }
    ASSERT_TRUE(defer_found);
    
    free_ast(ast);
    free_parser(parser);
    lexer_cleanup();
}

