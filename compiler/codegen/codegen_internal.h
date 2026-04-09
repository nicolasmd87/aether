#ifndef CODEGEN_INTERNAL_H
#define CODEGEN_INTERNAL_H

#include "codegen.h"
#include "../parser/lexer.h"
#include "../parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* Utilities (codegen.c) */
void indent(CodeGenerator* gen);
void unindent(CodeGenerator* gen);
void print_indent(CodeGenerator* gen);
void print_line(CodeGenerator* gen, const char* format, ...);
const char* get_c_type(Type* type);
int is_c_reserved_word(const char* name);
const char* safe_c_name(const char* name);
const char* get_c_operator(const char* aether_op);
void generate_type(CodeGenerator* gen, Type* type);
int is_var_declared(CodeGenerator* gen, const char* var_name);
void mark_var_declared(CodeGenerator* gen, const char* var_name);
void clear_declared_vars(CodeGenerator* gen);

/* Defer management (codegen.c) */
void push_defer(CodeGenerator* gen, ASTNode* stmt);
void push_auto_defer(CodeGenerator* gen, const char* free_fn, const char* var_name);
void emit_defers_for_scope(CodeGenerator* gen);
void emit_all_defers(CodeGenerator* gen);
void enter_scope(CodeGenerator* gen);
void exit_scope(CodeGenerator* gen);

/* Expression generation (codegen_expr.c) */
void generate_expression(CodeGenerator* gen, ASTNode* expr);

/* Statement generation (codegen_stmt.c) */
void generate_statement(CodeGenerator* gen, ASTNode* stmt);

/* Actor generation (codegen_actor.c) */
void generate_actor_definition(CodeGenerator* gen, ASTNode* actor);

/* Extern function registry — tracks param types for call-site cast emission */
void register_extern_func(CodeGenerator* gen, ASTNode* ext);
int is_extern_func(CodeGenerator* gen, const char* func_name);
TypeKind lookup_extern_param_kind(CodeGenerator* gen, const char* func_name, int param_idx);

/* Builder function registry — functions where block configures first, then function executes */
int is_builder_func_reg(CodeGenerator* gen, const char* func_name);
const char* get_builder_factory(CodeGenerator* gen, const char* func_name);

/* Function/struct generation (codegen_func.c) */
int has_return_value(ASTNode* node);
void generate_extern_declaration(CodeGenerator* gen, ASTNode* ext);
void generate_function_definition(CodeGenerator* gen, ASTNode* func);
void generate_struct_definition(CodeGenerator* gen, ASTNode* struct_def);
void generate_combined_function(CodeGenerator* gen, ASTNode** clauses, int clause_count);

/* Main/program (codegen.c) */
void generate_main_function(CodeGenerator* gen, ASTNode* main);

/* Closure support (codegen_expr.c) */
void discover_closures(CodeGenerator* gen, ASTNode* node);
void emit_closure_definitions(CodeGenerator* gen);

/* Internal helpers shared across files */
int contains_send_expression(ASTNode* node);
const char* get_single_int_field(MessageDef* msg_def);
void generate_default_return_value(CodeGenerator* gen, Type* type);
int is_function_generated(CodeGenerator* gen, const char* func_name);
void mark_function_generated(CodeGenerator* gen, const char* func_name);
int count_function_clauses(ASTNode* program, const char* func_name);
ASTNode** collect_function_clauses(ASTNode* program, const char* func_name, int* out_count);

#endif
