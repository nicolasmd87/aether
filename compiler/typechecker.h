#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "ast.h"

typedef struct Symbol {
    char* name;
    Type* type;
    int is_actor;
    int is_function;
    int is_state;
    struct Symbol* next;
} Symbol;

typedef struct SymbolTable {
    Symbol* symbols;
    struct SymbolTable* parent;
} SymbolTable;

// Symbol table functions
SymbolTable* create_symbol_table(SymbolTable* parent);
void free_symbol_table(SymbolTable* table);
void add_symbol(SymbolTable* table, const char* name, Type* type, int is_actor, int is_function, int is_state);
Symbol* lookup_symbol(SymbolTable* table, const char* name);
Symbol* lookup_symbol_local(SymbolTable* table, const char* name);

// Type checking functions
int typecheck_program(ASTNode* program);
int typecheck_node(ASTNode* node, SymbolTable* table);
int typecheck_actor_definition(ASTNode* actor, SymbolTable* table);
int typecheck_function_definition(ASTNode* func, SymbolTable* table);
int typecheck_struct_definition(ASTNode* struct_def, SymbolTable* table);
int typecheck_statement(ASTNode* stmt, SymbolTable* table);
int typecheck_expression(ASTNode* expr, SymbolTable* table);
int typecheck_binary_expression(ASTNode* expr, SymbolTable* table);
int typecheck_function_call(ASTNode* call, SymbolTable* table);

// Type inference functions
Type* infer_type(ASTNode* expr, SymbolTable* table);
Type* infer_binary_type(ASTNode* left, ASTNode* right, AeTokenType operator);
Type* infer_unary_type(ASTNode* operand, AeTokenType operator);

// Type compatibility functions
int is_type_compatible(Type* from, Type* to);
int is_assignable(Type* from, Type* to);
int is_callable(Type* type);

// Utility functions
AeTokenType get_token_type_from_string(const char* str);

// Error reporting
void type_error(const char* message, int line, int column);
void type_warning(const char* message, int line, int column);

#endif
