#ifndef CODEGEN_H
#define CODEGEN_H

#include "../ast.h"
#include "../../runtime/actors/aether_message_registry.h"

typedef struct {
    FILE* output;
    int indent_level;
    int actor_count;
    int function_count;
    char* current_actor;
    char** actor_state_vars;
    int state_var_count;
    MessageRegistry* message_registry;
    char** declared_vars;  // Track variables declared in current function
    int declared_var_count;
    int generating_lvalue;  // Track if we're generating an assignment target (lvalue)
    int in_condition;  // Track if we're in a condition (if/while) to avoid double parens
    ASTNode* program;  // Reference to program root for lookups

    // Header generation (--emit-header)
    int emit_header;         // Whether to emit a C header file
    FILE* header_file;       // Output stream for header
    const char* header_path; // Path to header file
} CodeGenerator;

// Code generation functions
CodeGenerator* create_code_generator(FILE* output);
CodeGenerator* create_code_generator_with_header(FILE* output, FILE* header, const char* header_path);
void free_code_generator(CodeGenerator* gen);
void generate_program(CodeGenerator* gen, ASTNode* program);

// Header generation (for C embedding)
void emit_header_prologue(CodeGenerator* gen, const char* guard_name);
void emit_header_epilogue(CodeGenerator* gen);
void emit_message_to_header(CodeGenerator* gen, ASTNode* msg_def);
void emit_actor_to_header(CodeGenerator* gen, ASTNode* actor);
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
