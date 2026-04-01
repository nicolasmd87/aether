#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "../ast.h"
#include "../../runtime/actors/aether_message_registry.h"

// Maximum defer nesting depth (scope depth * statements per scope)
#define MAX_DEFER_STACK 256
#define MAX_SCOPE_DEPTH 64

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
    int in_main_loop;  // Track if we're in main's loop for batch send optimization
    int in_main_function;  // Track if we're in main() so return -> goto main_exit
    int uses_main_exit;    // Track if any goto main_exit was emitted (suppress unused label)
    int interp_as_printf;  // When set, string interp generates printf() instead of snprintf+malloc
    ASTNode* program;  // Reference to program root for lookups

    // Header generation (--emit-header)
    int emit_header;         // Whether to emit a C header file
    FILE* header_file;       // Output stream for header
    const char* header_path; // Path to header file

    // Track generated pattern matching functions to avoid duplicates
    char** generated_functions;
    int generated_function_count;

    // Defer stack: tracks deferred statements for LIFO execution at scope exit
    ASTNode* defer_stack[MAX_DEFER_STACK];
    int defer_count;
    int scope_defer_start[MAX_SCOPE_DEPTH];  // defer_count at scope entry
    int scope_depth;

    // Extern function parameter type registry — used at call sites for proper casts
    // e.g., list_add(void*, void*) called with int arg → cast to (void*)(intptr_t)
    struct ExternParamInfo {
        char* name;          // function name
        TypeKind* params;    // array of parameter kinds (TYPE_PTR, TYPE_INT, ...)
        int param_count;
    }* extern_registry;
    int extern_registry_count;
    int extern_registry_capacity;

    // MSVC compat: counter for ask-operator temp variables (_ask_result_N)
    int ask_temp_counter;

    // Match-as-expression: when non-NULL, match arms assign to this variable
    const char* match_result_var;

    // Cooperative preemption: insert sched_yield() at loop back-edges
    int preempt_loops;

    // Current function's return type (for multi-return codegen)
    Type* current_func_return_type;

    // Tuple struct registry: track generated tuple typedefs to avoid duplicates
    char** tuple_type_names;    // e.g., "_tuple_int_string"
    int tuple_type_count;
    int tuple_type_capacity;

    // Builder function registry: functions with _ctx: ptr as first param
    // get builder_context() auto-injected at call sites inside trailing blocks
    char** builder_funcs;
    int builder_func_count;
    int builder_func_capacity;
    int in_trailing_block;  // >0 when generating code inside a trailing block

    // Closure support: track closures for hoisted C function generation
    int closure_counter;    // unique ID for closure env structs and functions
    // Map variable names to closure IDs (set during variable declaration codegen)
    struct ClosureVarMap {
        char* var_name;
        int closure_id;
    }* closure_var_map;
    int closure_var_count;
    int closure_var_capacity;
    // Pending closures: discovered during expression codegen, emitted at file scope
    struct ClosureInfo {
        int id;                  // unique closure ID
        ASTNode* closure_node;   // AST_CLOSURE node
        char** captures;         // captured variable names
        Type** capture_types;    // captured variable types
        int capture_count;
        char* parent_func;       // enclosing function name (for nested context)
    }* closures;
    int closure_count;
    int closure_capacity;

    // Ask/reply type map: request message name -> reply message name.
    // Built by scanning actor receive handlers for reply statements.
    struct ReplyTypeEntry {
        char* request_msg;   // e.g. "Add"
        char* reply_msg;     // e.g. "Result"
    }* reply_type_map;
    int reply_type_count;
    int reply_type_capacity;
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
void ensure_tuple_typedef(CodeGenerator* gen, Type* type);

// Utility functions
void indent(CodeGenerator* gen);
void print_indent(CodeGenerator* gen);
void print_line(CodeGenerator* gen, const char* format, ...);
void print_expression(CodeGenerator* gen, ASTNode* expr);
const char* get_c_type(Type* type);
const char* get_c_operator(const char* aether_op);

// Defer management
void push_defer(CodeGenerator* gen, ASTNode* stmt);
void emit_defers_for_scope(CodeGenerator* gen);
void emit_all_defers(CodeGenerator* gen);
void enter_scope(CodeGenerator* gen);
void exit_scope(CodeGenerator* gen);

#endif
