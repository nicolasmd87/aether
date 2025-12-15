#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

typedef struct {
    FILE* output;
    int indent_level;
    int actor_count;
    int function_count;
    char* current_actor;
    char** actor_state_vars;
    int state_var_count;
} CodeGenerator;

// Code generation functions
CodeGenerator* create_code_generator(FILE* output);
void free_code_generator(CodeGenerator* gen);
void generate_program(CodeGenerator* gen, ASTNode* program);
void generate_actor_definition(CodeGenerator* gen, ASTNode* actor);
void generate_function_definition(CodeGenerator* gen, ASTNode* func);
void generate_struct_definition(CodeGenerator* gen, ASTNode* struct_def);
void generate_main_function(CodeGenerator* gen, ASTNode* main);
void generate_statement(CodeGenerator* gen, ASTNode* stmt);
void generate_expression(CodeGenerator* gen, ASTNode* expr);
void generate_type(CodeGenerator* gen, Type* type);

// Utility functions
void indent(CodeGenerator* gen);
void print_indent(CodeGenerator* gen);
void print_line(CodeGenerator* gen, const char* format, ...);
void print_expression(CodeGenerator* gen, ASTNode* expr);
const char* get_c_type(Type* type);
const char* get_c_operator(const char* aether_op);

#endif
