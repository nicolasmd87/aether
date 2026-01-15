#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../runtime/test_harness.h"
#include "../../compiler/tokens.h"
#include "../../compiler/frontend/lexer.h"
#include "../../compiler/ast.h"
#include "../../compiler/frontend/parser.h"
#include "../../compiler/analysis/typechecker.h"

// Helper function to create parser with error suppression
static Parser* create_test_parser(Token** tokens, int token_count) {
    Parser* parser = create_parser(tokens, token_count);
    parser->suppress_errors = 1;  // Suppress parse errors during testing
    return parser;
}

// Test struct lexing
void test_struct_keyword() {
    lexer_init("struct");
    Token* token = next_token();
    assert(token->type == TOKEN_STRUCT);
    assert(strcmp(token->value, "struct") == 0);
    free_token(token);
}

// Test struct parsing
void test_parse_simple_struct() {
    const char* code = "struct Point { int x; int y; }";
    lexer_init(code);
    
    // Collect tokens
    Token** tokens = NULL;
    int token_count = 0;
    Token* token;
    while ((token = next_token())->type != TOKEN_EOF) {
        tokens = realloc(tokens, (token_count + 1) * sizeof(Token*));
        tokens[token_count++] = token;
    }
    tokens = realloc(tokens, (token_count + 1) * sizeof(Token*));
    tokens[token_count++] = token;
    
    // Parse
    Parser* parser = create_parser(tokens, token_count);
    parser->suppress_errors = 1;  // Suppress parse errors during testing
    ASTNode* struct_def = parse_struct_definition(parser);
    
    assert(struct_def != NULL);
    assert(struct_def->type == AST_STRUCT_DEFINITION);
    assert(strcmp(struct_def->value, "Point") == 0);
    assert(struct_def->child_count == 2); // x and y fields
    
    // Check fields
    assert(struct_def->children[0]->type == AST_STRUCT_FIELD);
    assert(strcmp(struct_def->children[0]->value, "x") == 0);
    assert(struct_def->children[0]->node_type->kind == TYPE_INT);
    
    assert(struct_def->children[1]->type == AST_STRUCT_FIELD);
    assert(strcmp(struct_def->children[1]->value, "y") == 0);
    assert(struct_def->children[1]->node_type->kind == TYPE_INT);
    
    free_ast_node(struct_def);
    free_parser(parser);
    for (int i = 0; i < token_count; i++) {
        free_token(tokens[i]);
    }
    free(tokens);
}

// Test struct type checking
void test_typecheck_struct() {
    const char* code = "struct Player { int health; int score; }";
    lexer_init(code);
    
    Token** tokens = NULL;
    int token_count = 0;
    Token* token;
    while ((token = next_token())->type != TOKEN_EOF) {
        tokens = realloc(tokens, (token_count + 1) * sizeof(Token*));
        tokens[token_count++] = token;
    }
    tokens = realloc(tokens, (token_count + 1) * sizeof(Token*));
    tokens[token_count++] = token;
    
    Parser* parser = create_parser(tokens, token_count);
    ASTNode* struct_def = parse_struct_definition(parser);
    
    SymbolTable* table = create_symbol_table(NULL);
    int result = typecheck_struct_definition(struct_def, table);
    
    assert(result == 1); // Should succeed
    
    free_symbol_table(table);
    free_ast_node(struct_def);
    free_parser(parser);
    for (int i = 0; i < token_count; i++) {
        free_token(tokens[i]);
    }
    free(tokens);
}

// Test duplicate field detection
void test_duplicate_field_detection() {
    const char* code = "struct Bad { int x; int x; }";
    lexer_init(code);
    
    Token** tokens = NULL;
    int token_count = 0;
    Token* token;
    while ((token = next_token())->type != TOKEN_EOF) {
        tokens = realloc(tokens, (token_count + 1) * sizeof(Token*));
        tokens[token_count++] = token;
    }
    tokens = realloc(tokens, (token_count + 1) * sizeof(Token*));
    tokens[token_count++] = token;
    
    Parser* parser = create_parser(tokens, token_count);
    ASTNode* struct_def = parse_struct_definition(parser);
    
    SymbolTable* table = create_symbol_table(NULL);
    int result = typecheck_struct_definition(struct_def, table);
    
    assert(result == 0); // Should fail due to duplicate field
    
    free_symbol_table(table);
    free_ast_node(struct_def);
    free_parser(parser);
    for (int i = 0; i < token_count; i++) {
        free_token(tokens[i]);
    }
    free(tokens);
}

// Test functions are now wrapped in TEST() macros above
// No separate main() needed - test harness provides it
