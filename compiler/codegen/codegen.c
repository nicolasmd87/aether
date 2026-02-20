#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include "codegen_internal.h"

#ifdef _WIN32
    #include <io.h>
    #define access _access
    #define F_OK 0
#else
    #include <unistd.h>
#endif

// Maximum tokens for parsing module files
#define MAX_MODULE_TOKENS 2000

// Helper to load and parse a module.ae file (same as typechecker)
static ASTNode* codegen_load_module_file(const char* module_path) {
    FILE* f = fopen(module_path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(source, 1, size, f);
    fclose(f);
    source[bytes_read] = '\0';

    // Tokenize
    lexer_init(source);
    Token* tokens[MAX_MODULE_TOKENS];
    int token_count = 0;

    while (token_count < MAX_MODULE_TOKENS - 1) {
        Token* token = next_token();
        tokens[token_count++] = token;
        if (token->type == TOKEN_EOF || token->type == TOKEN_ERROR) break;
    }

    // Parse
    Parser* parser = create_parser(tokens, token_count);
    ASTNode* ast = parse_program(parser);

    // Cleanup
    for (int i = 0; i < token_count; i++) {
        free_token(tokens[i]);
    }
    free_parser(parser);
    free(source);

    return ast;
}

// Try multiple paths to find a module file
static ASTNode* codegen_resolve_and_load_module(const char* module_name) {
    char path[512];

    // Try 1: Local development path
    snprintf(path, sizeof(path), "std/%s/module.ae", module_name);
    ASTNode* ast = codegen_load_module_file(path);
    if (ast) return ast;

    // Try 2: Installed path via AETHER_HOME
    const char* aether_home = getenv("AETHER_HOME");
    if (aether_home) {
        snprintf(path, sizeof(path), "%s/share/aether/std/%s/module.ae", aether_home, module_name);
        ast = codegen_load_module_file(path);
        if (ast) return ast;
    }

    // Try 3: Common install locations
    snprintf(path, sizeof(path), "/usr/local/share/aether/std/%s/module.ae", module_name);
    ast = codegen_load_module_file(path);
    if (ast) return ast;

    snprintf(path, sizeof(path), "%s/.aether/share/aether/std/%s/module.ae",
             getenv("HOME") ? getenv("HOME") : "", module_name);
    ast = codegen_load_module_file(path);
    if (ast) return ast;

    return NULL;
}

// Resolve local package modules (non-std imports)
// Converts dots to slashes: "mypackage.utils" -> "mypackage/utils/module.ae"
static ASTNode* codegen_resolve_local_module(const char* module_path) {
    char converted[512];
    char path[sizeof(converted) + 16];

    // Convert dots to slashes
    strncpy(converted, module_path, sizeof(converted) - 1);
    converted[sizeof(converted) - 1] = '\0';
    for (char* p = converted; *p; p++) {
        if (*p == '.') *p = '/';
    }

    // Try 1: lib/module_path/module.ae (library directory)
    snprintf(path, sizeof(path), "lib/%s/module.ae", converted);
    ASTNode* ast = codegen_load_module_file(path);
    if (ast) return ast;

    // Try 2: lib/module_path.ae (single file module)
    snprintf(path, sizeof(path), "lib/%s.ae", converted);
    ast = codegen_load_module_file(path);
    if (ast) return ast;

    // Try 3: src/module_path/module.ae
    snprintf(path, sizeof(path), "src/%s/module.ae", converted);
    ast = codegen_load_module_file(path);
    if (ast) return ast;

    // Try 4: src/module_path.ae
    snprintf(path, sizeof(path), "src/%s.ae", converted);
    ast = codegen_load_module_file(path);
    if (ast) return ast;

    // Try 5: module_path/module.ae (project root)
    snprintf(path, sizeof(path), "%s/module.ae", converted);
    ast = codegen_load_module_file(path);
    if (ast) return ast;

    // Try 6: module_path.ae (single file in root)
    snprintf(path, sizeof(path), "%s.ae", converted);
    ast = codegen_load_module_file(path);
    if (ast) return ast;

    return NULL;
}

// Check if an AST node contains send expressions (for batch optimization)
int contains_send_expression(ASTNode* node) {
    if (!node) return 0;
    if (node->type == AST_SEND_FIRE_FORGET || node->type == AST_SEND_STATEMENT) return 1;
    for (int i = 0; i < node->child_count; i++) {
        if (contains_send_expression(node->children[i])) return 1;
    }
    return 0;
}

// Returns the field name if msg has exactly one int field (eligible for inline encoding),
// or NULL otherwise. Inline messages skip pool allocation entirely — the single int field
// is stored in Message.payload_int, avoiding memcpy and pool lookup on every send.
const char* get_single_int_field(MessageDef* msg_def) {
    if (!msg_def || !msg_def->fields) return NULL;
    MessageFieldDef* field = msg_def->fields;
    if (field->type_kind != TYPE_INT) return NULL;
    if (field->next != NULL) return NULL;  // More than one field
    return field->name;
}

CodeGenerator* create_code_generator(FILE* output) {
    CodeGenerator* gen = malloc(sizeof(CodeGenerator));
    gen->output = output;
    gen->indent_level = 0;
    gen->actor_count = 0;
    gen->function_count = 0;
    gen->current_actor = NULL;
    gen->actor_state_vars = NULL;
    gen->state_var_count = 0;
    gen->message_registry = create_message_registry();
    gen->declared_vars = NULL;
    gen->declared_var_count = 0;
    gen->generating_lvalue = 0;  // Not generating lvalue by default
    gen->in_condition = 0;  // Not in condition by default
    gen->in_main_loop = 0;  // Not in main loop by default
    gen->in_main_function = 0;
    gen->emit_header = 0;
    gen->header_file = NULL;
    gen->header_path = NULL;
    gen->generated_functions = NULL;
    gen->generated_function_count = 0;
    // Initialize defer tracking
    gen->defer_count = 0;
    gen->scope_depth = 0;
    memset(gen->defer_stack, 0, sizeof(gen->defer_stack));
    memset(gen->scope_defer_start, 0, sizeof(gen->scope_defer_start));
    // Memory management
    gen->no_auto_free = 0;
    gen->synthetic_nodes = NULL;
    gen->synthetic_node_count = 0;
    gen->synthetic_node_capacity = 0;
    // Extern function parameter registry
    gen->extern_registry = NULL;
    gen->extern_registry_count = 0;
    gen->extern_registry_capacity = 0;
    return gen;
}

CodeGenerator* create_code_generator_with_header(FILE* output, FILE* header, const char* header_path) {
    CodeGenerator* gen = create_code_generator(output);
    gen->emit_header = 1;
    gen->header_file = header;
    gen->header_path = header_path;
    return gen;
}

void free_code_generator(CodeGenerator* gen) {
    if (gen) {
        if (gen->current_actor) free(gen->current_actor);
        if (gen->actor_state_vars) {
            for (int i = 0; i < gen->state_var_count; i++) {
                free(gen->actor_state_vars[i]);
            }
            free(gen->actor_state_vars);
        }
        if (gen->declared_vars) {
            for (int i = 0; i < gen->declared_var_count; i++) {
                free(gen->declared_vars[i]);
            }
            free(gen->declared_vars);
        }
        if (gen->message_registry) {
            free_message_registry(gen->message_registry);
        }
        if (gen->generated_functions) {
            for (int i = 0; i < gen->generated_function_count; i++) {
                free(gen->generated_functions[i]);
            }
            free(gen->generated_functions);
        }
        if (gen->synthetic_nodes) {
            for (int i = 0; i < gen->synthetic_node_count; i++) {
                free_ast_node(gen->synthetic_nodes[i]);
            }
            free(gen->synthetic_nodes);
        }
        if (gen->extern_registry) {
            for (int i = 0; i < gen->extern_registry_count; i++) {
                free(gen->extern_registry[i].name);
                free(gen->extern_registry[i].params);
            }
            free(gen->extern_registry);
        }
        free(gen);
    }
}

// Helper: check if variable was already declared in current function
int is_var_declared(CodeGenerator* gen, const char* var_name) {
    for (int i = 0; i < gen->declared_var_count; i++) {
        if (strcmp(gen->declared_vars[i], var_name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Helper: mark variable as declared in current function
void mark_var_declared(CodeGenerator* gen, const char* var_name) {
    char** new_vars = realloc(gen->declared_vars, sizeof(char*) * (gen->declared_var_count + 1));
    if (!new_vars) return;
    gen->declared_vars = new_vars;
    gen->declared_vars[gen->declared_var_count] = strdup(var_name);
    gen->declared_var_count++;
}

// Helper: clear declared vars (call at function start)
void clear_declared_vars(CodeGenerator* gen) {
    if (gen->declared_vars) {
        for (int i = 0; i < gen->declared_var_count; i++) {
            free(gen->declared_vars[i]);
        }
        free(gen->declared_vars);
    }
    gen->declared_vars = NULL;
    gen->declared_var_count = 0;
}

// Helper: check if a function was already generated
int is_function_generated(CodeGenerator* gen, const char* func_name) {
    for (int i = 0; i < gen->generated_function_count; i++) {
        if (strcmp(gen->generated_functions[i], func_name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Helper: mark a function as generated
void mark_function_generated(CodeGenerator* gen, const char* func_name) {
    char** new_funcs = realloc(gen->generated_functions,
                               sizeof(char*) * (gen->generated_function_count + 1));
    if (!new_funcs) return;
    gen->generated_functions = new_funcs;
    gen->generated_functions[gen->generated_function_count] = strdup(func_name);
    gen->generated_function_count++;
}

// Helper: count how many function clauses exist with the same name
int count_function_clauses(ASTNode* program, const char* func_name) {
    int count = 0;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_FUNCTION_DEFINITION &&
            child->value && strcmp(child->value, func_name) == 0) {
            count++;
        }
    }
    return count;
}

// Helper: collect all function clauses with the same name
ASTNode** collect_function_clauses(ASTNode* program, const char* func_name, int* out_count) {
    int count = count_function_clauses(program, func_name);
    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    ASTNode** clauses = malloc(sizeof(ASTNode*) * count);
    int idx = 0;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_FUNCTION_DEFINITION &&
            child->value && strcmp(child->value, func_name) == 0) {
            clauses[idx++] = child;
        }
    }
    *out_count = count;
    return clauses;
}

void indent(CodeGenerator* gen) {
    gen->indent_level++;
}

void unindent(CodeGenerator* gen) {
    if (gen->indent_level > 0) {
        gen->indent_level--;
    }
}

void print_indent(CodeGenerator* gen) {
    for (int i = 0; i < gen->indent_level; i++) {
        fprintf(gen->output, "    ");
    }
}

// ============================================================================
// Defer Implementation - Real LIFO execution at scope exit
// ============================================================================

// Push a deferred statement onto the stack
void push_defer(CodeGenerator* gen, ASTNode* stmt) {
    if (gen->defer_count < MAX_DEFER_STACK) {
        gen->defer_stack[gen->defer_count++] = stmt;
    } else {
        fprintf(stderr, "Warning: defer stack overflow (max %d)\n", MAX_DEFER_STACK);
    }
}

// Push an auto-generated free call as a deferred statement.
// Creates a synthetic AST_EXPRESSION_STATEMENT { AST_FUNCTION_CALL(free_fn, var_name) }
// and pushes it onto the defer stack. The synthetic node is tracked for cleanup.
void push_auto_defer(CodeGenerator* gen, const char* free_fn, const char* var_name) {
    // Build: free_fn(var_name)
    ASTNode* call = create_ast_node(AST_FUNCTION_CALL, free_fn, 0, 0);
    ASTNode* arg  = create_ast_node(AST_IDENTIFIER, var_name, 0, 0);
    add_child(call, arg);
    // Wrap in expression statement so generate_statement() handles it correctly
    ASTNode* stmt = create_ast_node(AST_EXPRESSION_STATEMENT, NULL, 0, 0);
    add_child(stmt, call);

    push_defer(gen, stmt);

    // Track for cleanup in free_code_generator
    if (gen->synthetic_node_count >= gen->synthetic_node_capacity) {
        gen->synthetic_node_capacity = gen->synthetic_node_capacity * 2 + 8;
        gen->synthetic_nodes = realloc(gen->synthetic_nodes,
            gen->synthetic_node_capacity * sizeof(ASTNode*));
    }
    gen->synthetic_nodes[gen->synthetic_node_count++] = stmt;
}

// Enter a new scope - remember where defers started for this scope
void enter_scope(CodeGenerator* gen) {
    if (gen->scope_depth < MAX_SCOPE_DEPTH) {
        gen->scope_defer_start[gen->scope_depth] = gen->defer_count;
        gen->scope_depth++;
    }
}

// Emit deferred statements for current scope only (in reverse order)
void emit_defers_for_scope(CodeGenerator* gen) {
    if (gen->scope_depth <= 0) return;

    int scope_start = gen->scope_defer_start[gen->scope_depth - 1];

    // Emit defers in LIFO order (reverse)
    for (int i = gen->defer_count - 1; i >= scope_start; i--) {
        ASTNode* deferred = gen->defer_stack[i];
        if (deferred) {
            print_indent(gen);
            fprintf(gen->output, "/* deferred */ ");
            generate_statement(gen, deferred);
        }
    }
}

// Exit scope - emit defers and pop scope
void exit_scope(CodeGenerator* gen) {
    emit_defers_for_scope(gen);

    if (gen->scope_depth > 0) {
        // Pop all defers for this scope
        gen->defer_count = gen->scope_defer_start[gen->scope_depth - 1];
        gen->scope_depth--;
    }
}

// Emit ALL deferred statements (for return - unwinds entire function)
void emit_all_defers(CodeGenerator* gen) {
    // Emit all defers in LIFO order across all scopes
    for (int i = gen->defer_count - 1; i >= 0; i--) {
        ASTNode* deferred = gen->defer_stack[i];
        if (deferred) {
            print_indent(gen);
            fprintf(gen->output, "/* deferred */ ");
            generate_statement(gen, deferred);
        }
    }
}

// ============================================================================
// Header Generation Functions (for --emit-header)
// ============================================================================

// Convert filename to uppercase guard name (e.g., "counter.h" -> "COUNTER_H")
static void make_guard_name(const char* path, char* guard, size_t guard_size) {
    const char* filename = path;
    // Find last path separator
    for (const char* p = path; *p; p++) {
        if (*p == '/' || *p == '\\') filename = p + 1;
    }

    size_t i = 0;
    for (; filename[i] && i < guard_size - 1; i++) {
        char c = filename[i];
        if (c == '.') guard[i] = '_';
        else if (c >= 'a' && c <= 'z') guard[i] = c - 32;  // toupper
        else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') guard[i] = c;
        else guard[i] = '_';
    }
    guard[i] = '\0';
}

void emit_header_prologue(CodeGenerator* gen, const char* guard_name) {
    if (!gen->header_file) return;

    char guard[256];
    if (guard_name) {
        strncpy(guard, guard_name, sizeof(guard) - 1);
        guard[sizeof(guard) - 1] = '\0';
    } else if (gen->header_path) {
        make_guard_name(gen->header_path, guard, sizeof(guard));
    } else {
        snprintf(guard, sizeof(guard), "AETHER_GENERATED_H");
    }

    fprintf(gen->header_file, "// Auto-generated by aetherc - DO NOT EDIT\n");
    fprintf(gen->header_file, "// Generated from Aether source for C embedding\n");
    fprintf(gen->header_file, "#ifndef %s\n", guard);
    fprintf(gen->header_file, "#define %s\n\n", guard);
    fprintf(gen->header_file, "#include <stdint.h>\n");
    fprintf(gen->header_file, "#include \"runtime/scheduler/multicore_scheduler.h\"\n");
    fprintf(gen->header_file, "\n");
    fprintf(gen->header_file, "// Forward declarations\n");
}

void emit_header_epilogue(CodeGenerator* gen) {
    if (!gen->header_file) return;

    fprintf(gen->header_file, "\n#endif // header guard\n");
}

void emit_message_to_header(CodeGenerator* gen, ASTNode* msg_def) {
    if (!gen->header_file || !msg_def || !msg_def->value) return;

    const char* msg_name = msg_def->value;
    MessageDef* msg_entry = lookup_message(gen->message_registry, msg_name);
    int msg_id = msg_entry ? msg_entry->message_id : 0;

    fprintf(gen->header_file, "\n// Message: %s\n", msg_name);
    fprintf(gen->header_file, "#define MSG_%s %d\n", msg_name, msg_id);

    // Generate struct typedef
    fprintf(gen->header_file, "typedef struct {\n");
    fprintf(gen->header_file, "    int _message_id;\n");

    for (int i = 0; i < msg_def->child_count; i++) {
        ASTNode* field = msg_def->children[i];
        if (field && field->type == AST_MESSAGE_FIELD && field->value) {
            const char* c_type = "int";  // Default
            if (field->node_type) {
                c_type = get_c_type(field->node_type);
            }
            fprintf(gen->header_file, "    %s %s;\n", c_type, field->value);
        }
    }

    fprintf(gen->header_file, "} %s;\n", msg_name);
}

void emit_actor_to_header(CodeGenerator* gen, ASTNode* actor) {
    if (!gen->header_file || !actor || !actor->value) return;

    const char* actor_name = actor->value;

    fprintf(gen->header_file, "\n// Actor: %s\n", actor_name);
    fprintf(gen->header_file, "typedef struct %s %s;\n", actor_name, actor_name);
    fprintf(gen->header_file, "%s* spawn_%s(void);\n", actor_name, actor_name);

    // Generate typed send helpers for each message this actor handles
    // We look for receive handlers in the actor definition
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child && child->type == AST_RECEIVE_STATEMENT) {
            // Each handler arm in the receive statement
            for (int j = 0; j < child->child_count; j++) {
                ASTNode* handler = child->children[j];
                if (handler && handler->type == AST_RECEIVE_ARM && handler->value) {
                    const char* msg_name = handler->value;
                    MessageDef* msg_def = lookup_message(gen->message_registry, msg_name);
                    int msg_id = msg_def ? msg_def->message_id : 0;

                    // Generate inline send helper
                    fprintf(gen->header_file, "\nstatic inline void %s_%s(%s* actor",
                            actor_name, msg_name, actor_name);

                    // Add parameters for each field
                    if (msg_def && msg_def->fields) {
                        MessageFieldDef* field = msg_def->fields;
                        while (field) {
                            const char* c_type = "int";
                            switch (field->type_kind) {
                                case TYPE_INT: c_type = "int"; break;
                                case TYPE_FLOAT: c_type = "float"; break;
                                case TYPE_STRING: c_type = "const char*"; break;
                                case TYPE_BOOL: c_type = "int"; break;
                                default: c_type = "int"; break;
                            }
                            fprintf(gen->header_file, ", %s %s", c_type, field->name);
                            field = field->next;
                        }
                    }

                    fprintf(gen->header_file, ") {\n");
                    fprintf(gen->header_file, "    Message msg = {0};\n");
                    fprintf(gen->header_file, "    msg.type = %d;\n", msg_id);

                    // For single-int messages, use payload_int
                    if (msg_def && msg_def->fields && !msg_def->fields->next &&
                        msg_def->fields->type_kind == TYPE_INT) {
                        fprintf(gen->header_file, "    msg.payload_int = %s;\n", msg_def->fields->name);
                    }

                    fprintf(gen->header_file, "    scheduler_send_remote((ActorBase*)actor, msg, -1);\n");
                    fprintf(gen->header_file, "}\n");
                }
            }
        }
    }
}

void print_line(CodeGenerator* gen, const char* format, ...) {
    print_indent(gen);
    
    va_list args;
    va_start(args, format);
    vfprintf(gen->output, format, args);
    va_end(args);
    
    fprintf(gen->output, "\n");
}

const char* get_c_type(Type* type) {
    if (!type) {
        fprintf(stderr, "Warning: NULL type encountered in codegen, defaulting to int\n");
        return "int";
    }

    switch (type->kind) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "int";
        case TYPE_STRING: return "const char*";
        case TYPE_VOID: return "void";
        case TYPE_ACTOR_REF: {
            // Actor reference is a pointer to the actor struct
            static char buffer[256];
            if (type->element_type && type->element_type->kind == TYPE_STRUCT && type->element_type->struct_name) {
                snprintf(buffer, sizeof(buffer), "%s*", type->element_type->struct_name);
            } else {
                snprintf(buffer, sizeof(buffer), "ActorRef*");
            }
            return buffer;
        }
        case TYPE_MESSAGE: return "Message";
        case TYPE_PTR: return "void*";
        case TYPE_STRUCT: {
            static char buffer[256];
            // Just use the struct name (typedef removes need for "struct" keyword)
            snprintf(buffer, sizeof(buffer), "%s",
                    type->struct_name ? type->struct_name : "unnamed");
            return buffer;
        }
        case TYPE_ARRAY: {
            static char buffer[256];
            const char* element_type = get_c_type(type->element_type);
            if (type->array_size > 0) {
                snprintf(buffer, sizeof(buffer), "%s[%d]", element_type, type->array_size);
            } else {
                snprintf(buffer, sizeof(buffer), "%s*", element_type);
            }
            return buffer;
        }
        case TYPE_UNKNOWN:
            // TYPE_UNKNOWN is common for pattern variables - silently default to int
            return "int";
        default:
            fprintf(stderr, "Warning: Unknown type kind %d in codegen, defaulting to void\n", type->kind);
            return "void";
    }
}

const char* get_c_operator(const char* aether_op) {
    if (!aether_op) return "";
    
    if (strcmp(aether_op, "&&") == 0) return "&&";
    if (strcmp(aether_op, "||") == 0) return "||";
    if (strcmp(aether_op, "==") == 0) return "==";
    if (strcmp(aether_op, "!=") == 0) return "!=";
    if (strcmp(aether_op, "<") == 0) return "<";
    if (strcmp(aether_op, "<=") == 0) return "<=";
    if (strcmp(aether_op, ">") == 0) return ">";
    if (strcmp(aether_op, ">=") == 0) return ">=";
    if (strcmp(aether_op, "+") == 0) return "+";
    if (strcmp(aether_op, "-") == 0) return "-";
    if (strcmp(aether_op, "*") == 0) return "*";
    if (strcmp(aether_op, "/") == 0) return "/";
    if (strcmp(aether_op, "%") == 0) return "%";
    if (strcmp(aether_op, "!") == 0) return "!";
    if (strcmp(aether_op, "=") == 0) return "=";
    if (strcmp(aether_op, "++") == 0) return "++";
    if (strcmp(aether_op, "--") == 0) return "--";
    
    return aether_op;
}

void generate_type(CodeGenerator* gen, Type* type) {
    fprintf(gen->output, "%s", get_c_type(type));
}

// Generate a default return value for pattern match failures
// This outputs a sentinel value that indicates "no match" for this clause
void generate_default_return_value(CodeGenerator* gen, Type* type) {
    if (!type) {
        fprintf(gen->output, "0");
        return;
    }
    switch (type->kind) {
        case TYPE_INT:
            fprintf(gen->output, "0");
            break;
        case TYPE_FLOAT:
            fprintf(gen->output, "0.0");
            break;
        case TYPE_STRING:
            fprintf(gen->output, "\"\"");
            break;
        case TYPE_BOOL:
            fprintf(gen->output, "0");
            break;
        case TYPE_VOID:
            // For void functions, just return without value
            // The caller should handle this - output nothing
            break;
        case TYPE_PTR:
            fprintf(gen->output, "NULL");
            break;
        default:
            fprintf(gen->output, "0");
            break;
    }
}

void generate_main_function(CodeGenerator* gen, ASTNode* main) {
    if (!main || main->type != AST_MAIN_FUNCTION) return;

    print_line(gen, "int main(int argc, char** argv) {");
    indent(gen);
    clear_declared_vars(gen);  // Reset for main function
    // Reset defer state for main function and enter scope
    gen->defer_count = 0;
    gen->scope_depth = 0;
    enter_scope(gen);

    // Initialize command-line arguments
    print_line(gen, "aether_args_init(argc, argv);");
    print_line(gen, "int main_exit_ret = 0;");
    print_line(gen, "");

    // Initialize scheduler with recommended core count if actors were defined
    if (gen->actor_count > 0) {
        print_line(gen, "// Initialize Aether runtime with auto-detected optimizations");
        print_line(gen, "// TIER 1 (always-on): Actor pooling, Direct send, Adaptive batching");
        print_line(gen, "// TIER 2 (auto-detect): SIMD (if AVX2/NEON), MWAIT (if supported)");
        print_line(gen, "int num_cores = cpu_recommend_cores();");
        print_line(gen, "scheduler_init(num_cores);  // Auto-detects hardware capabilities");
        print_line(gen, "");
        print_line(gen, "#ifdef AETHER_VERBOSE");
        print_line(gen, "aether_print_config();");
        print_line(gen, "#endif");
        print_line(gen, "");
        print_line(gen, "scheduler_start();");
        print_line(gen, "current_core_id = -1;  // Main thread is not a scheduler thread");
        print_line(gen, "");
    }
    
    if (main->child_count > 0) {
        gen->in_main_function = 1;
        generate_statement(gen, main->children[0]);
        gen->in_main_function = 0;
    }
    
    // Clean up scheduler (all return paths in main() jump here via goto main_exit)
    print_line(gen, "main_exit:");
    if (gen->actor_count > 0) {
        print_line(gen, "");
        print_line(gen, "// Signal scheduler threads to stop, then wait for them to join");
        print_line(gen, "scheduler_stop();");
        print_line(gen, "scheduler_wait();");
    }

    // Print message pool statistics (only for actor programs)
    if (gen->actor_count > 0) {
        print_line(gen, "");
        print_line(gen, "// Message pool statistics");
        print_line(gen, "{");
        indent(gen);
        print_line(gen, "uint64_t pool_hits = 0, pool_misses = 0, too_large = 0;");
        print_line(gen, "aether_message_pool_stats(&pool_hits, &pool_misses, &too_large);");
        print_line(gen, "if (pool_hits + pool_misses + too_large > 0) {");
        indent(gen);
        print_line(gen, "printf(\"\\n=== Message Pool Statistics ===\\n\");");
        print_line(gen, "printf(\"Pool hits:      %%llu\\n\", (unsigned long long)pool_hits);");
        print_line(gen, "printf(\"Pool misses:    %%llu (exhausted)\\n\", (unsigned long long)pool_misses);");
        print_line(gen, "printf(\"Too large:      %%llu (>256 bytes)\\n\", (unsigned long long)too_large);");
        print_line(gen, "uint64_t total = pool_hits + pool_misses + too_large;");
        print_line(gen, "double hit_rate = (double)pool_hits / total * 100.0;");
        print_line(gen, "printf(\"Hit rate:       %%.1f%%%%\\n\", hit_rate);");
        unindent(gen);
        print_line(gen, "}");
        unindent(gen);
        print_line(gen, "}");
        print_line(gen, "");
    }

    // Emit main function defers before return
    exit_scope(gen);

    print_line(gen, "return main_exit_ret;");
    unindent(gen);
    print_line(gen, "}");
}

void generate_program(CodeGenerator* gen, ASTNode* program) {
    if (!program || program->type != AST_PROGRAM) return;
    gen->program = program;

    // If emitting header, write prologue
    if (gen->emit_header && gen->header_file) {
        emit_header_prologue(gen, NULL);
    }

    // Generate includes for runtime libraries
    print_line(gen, "#include <stdio.h>");
    print_line(gen, "#include <stdlib.h>");
    print_line(gen, "#include <string.h>");
    print_line(gen, "#include <stdbool.h>");
    print_line(gen, "#include <stdatomic.h>");
    print_line(gen, "#include <stdint.h>");
    print_line(gen, "#ifndef _WIN32");
    print_line(gen, "#include <unistd.h>");
    print_line(gen, "#endif");
    /* aligned_alloc: C11 POSIX; Windows uses _aligned_malloc with swapped args */
    print_line(gen, "#ifdef _WIN32");
    print_line(gen, "#  define aether_aligned_alloc(align, size) _aligned_malloc((size), (align))");
    print_line(gen, "#else");
    print_line(gen, "#  define aether_aligned_alloc(align, size) aligned_alloc((align), (size))");
    print_line(gen, "#endif");
    print_line(gen, "#ifndef likely");
    print_line(gen, "#  if defined(__GNUC__) || defined(__clang__)");
    print_line(gen, "#    define likely(x)   __builtin_expect(!!(x), 1)");
    print_line(gen, "#    define unlikely(x) __builtin_expect(!!(x), 0)");
    print_line(gen, "#  else");
    print_line(gen, "#    define likely(x)   (x)");
    print_line(gen, "#    define unlikely(x) (x)");
    print_line(gen, "#  endif");
    print_line(gen, "#endif");
    print_line(gen, "");
    // Declare runtime args function (avoid full header to prevent conflicts with actor runtime)
    print_line(gen, "void aether_args_init(int argc, char** argv);");
    print_line(gen, "");

    // Only include actor runtime if program uses actors
    bool has_actors = false;
    for (int i = 0; i < program->child_count; i++) {
        if (program->children[i] && program->children[i]->type == AST_ACTOR_DEFINITION) {
            has_actors = true;
            break;
        }
    }
    
    if (has_actors) {
        print_line(gen, "#include <stdatomic.h>");
        print_line(gen, "");
        print_line(gen, "// Aether runtime libraries");
        print_line(gen, "#include \"actor_state_machine.h\"");
        print_line(gen, "#include \"aether_send_message.h\"");
        print_line(gen, "#include \"aether_actor_thread.h\"");
        print_line(gen, "#include \"multicore_scheduler.h\"");
        print_line(gen, "#include \"aether_cpu_detect.h\"");
        print_line(gen, "#include \"aether_optimization_config.h\"");
        print_line(gen, "#include \"aether_supervision.h\"");
        print_line(gen, "#include \"aether_tracing.h\"");
        print_line(gen, "#include \"aether_bounds_check.h\"");
        print_line(gen, "#include \"aether_runtime_types.h\"");
        print_line(gen, "");
        print_line(gen, "extern __thread int current_core_id;");
        print_line(gen, "");
        print_line(gen, "// Benchmark timing function");
        print_line(gen, "static inline uint64_t rdtsc() {");
        print_line(gen, "#if defined(__x86_64__) || defined(__i386__)");
        print_line(gen, "    unsigned int lo, hi;");
        print_line(gen, "    __asm__ __volatile__ (\"rdtsc\" : \"=a\" (lo), \"=d\" (hi));");
        print_line(gen, "    return ((uint64_t)hi << 32) | lo;");
        print_line(gen, "#elif defined(__aarch64__) || defined(__arm__)");
        print_line(gen, "    struct timespec ts;");
        print_line(gen, "    clock_gettime(CLOCK_MONOTONIC, &ts);");
        print_line(gen, "    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;");
        print_line(gen, "#else");
        print_line(gen, "    return 0;");
        print_line(gen, "#endif");
        print_line(gen, "}");
    }
    print_line(gen, "");

    // Generate forward declarations for all functions (handles mutual recursion)
    print_line(gen, "// Forward declarations");
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (!child || child->type != AST_FUNCTION_DEFINITION) continue;
        if (!child->value) continue;

        // Skip if already forward-declared (pattern matching generates combined functions)
        int already_declared = 0;
        for (int j = 0; j < i; j++) {
            ASTNode* prev = program->children[j];
            if (prev && prev->type == AST_FUNCTION_DEFINITION &&
                prev->value && strcmp(prev->value, child->value) == 0) {
                already_declared = 1;
                break;
            }
        }
        if (already_declared) continue;

        // Determine return type
        Type* ret_type = child->node_type;
        int func_has_return = has_return_value(child);
        if ((!ret_type || ret_type->kind == TYPE_VOID || ret_type->kind == TYPE_UNKNOWN) && func_has_return) {
            fprintf(gen->output, "int");
        } else {
            generate_type(gen, ret_type);
        }
        fprintf(gen->output, " %s(", child->value);

        // Generate parameter types
        int param_count = 0;
        for (int j = 0; j < child->child_count; j++) {
            ASTNode* param = child->children[j];
            if (param->type == AST_GUARD_CLAUSE || param->type == AST_BLOCK) continue;

            if (param->type == AST_PATTERN_LIST || param->type == AST_PATTERN_CONS) {
                if (param_count > 0) fprintf(gen->output, ", ");
                fprintf(gen->output, "int*, int");
                param_count++;
            } else if (param->type == AST_PATTERN_LITERAL ||
                       param->type == AST_PATTERN_VARIABLE ||
                       param->type == AST_PATTERN_STRUCT ||
                       param->type == AST_VARIABLE_DECLARATION) {
                if (param_count > 0) fprintf(gen->output, ", ");
                generate_type(gen, param->node_type);
                param_count++;
            }
        }
        fprintf(gen->output, ");\n");
    }
    print_line(gen, "");

    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (!child) continue;

        switch (child->type) {
            case AST_MODULE_DECLARATION:
                // Module declaration: just a comment in generated C
                print_line(gen, "// Module: %s", child->value ? child->value : "unnamed");
                print_line(gen, "");
                break;
            case AST_IMPORT_STATEMENT:
                // Import statement: generate extern declarations for stdlib imports
                if (child->value) {
                    const char* module_path = child->value;

                    // Check for alias
                    const char* alias = NULL;
                    if (child->child_count > 0) {
                        ASTNode* last = child->children[child->child_count - 1];
                        if (last && last->type == AST_IDENTIFIER) {
                            alias = last->value;
                        }
                    }

                    if (alias) {
                        print_line(gen, "// Import: %s as %s", module_path, alias);
                    } else {
                        print_line(gen, "// Import: %s", module_path);
                    }

                    // Handle stdlib imports: import std.X
                    if (strncmp(module_path, "std.", 4) == 0) {
                        const char* module_name = module_path + 4;

                        // Load the module definition file
                        ASTNode* mod_ast = codegen_resolve_and_load_module(module_name);
                        if (mod_ast) {
                            // Generate extern declarations for imported functions
                            for (int j = 0; j < mod_ast->child_count; j++) {
                                ASTNode* decl = mod_ast->children[j];
                                if (decl->type == AST_EXTERN_FUNCTION && decl->value) {
                                    // Check if selective import
                                    int should_import = 1;
                                    if (child->child_count > 0) {
                                        ASTNode* first = child->children[0];
                                        if (first && first->type == AST_IDENTIFIER) {
                                            should_import = 0;
                                            for (int k = 0; k < child->child_count; k++) {
                                                ASTNode* sel = child->children[k];
                                                if (sel && sel->type == AST_IDENTIFIER &&
                                                    strcmp(sel->value, decl->value) == 0) {
                                                    should_import = 1;
                                                    break;
                                                }
                                            }
                                        }
                                    }

                                    if (should_import) {
                                        generate_extern_declaration(gen, decl);
                                    }
                                }
                            }
                            free_ast_node(mod_ast);
                        }
                    } else {
                        // Handle local package imports: import mypackage.utils
                        ASTNode* mod_ast = codegen_resolve_local_module(module_path);
                        if (mod_ast) {
                            // Generate extern declarations for imported functions
                            for (int j = 0; j < mod_ast->child_count; j++) {
                                ASTNode* decl = mod_ast->children[j];
                                if (decl->type == AST_EXTERN_FUNCTION && decl->value) {
                                    generate_extern_declaration(gen, decl);
                                } else if (decl->type == AST_FUNCTION_DEFINITION && decl->value) {
                                    // For user-defined functions, generate a forward declaration
                                    generate_extern_declaration(gen, decl);
                                }
                            }
                            free_ast_node(mod_ast);
                        }
                    }
                }
                print_line(gen, "");
                break;
            case AST_EXPORT_STATEMENT:
                // Export: just generate the item (exports are implicit in C)
                if (child->child_count > 0) {
                    ASTNode* exported = child->children[0];
                    print_line(gen, "// Exported:");
                    switch (exported->type) {
                        case AST_FUNCTION_DEFINITION:
                            // Handle exports like regular functions (pattern matching aware)
                            if (exported->value && !is_function_generated(gen, exported->value)) {
                                int clause_count = 0;
                                ASTNode** clauses = collect_function_clauses(program, exported->value, &clause_count);
                                if (clause_count > 1) {
                                    generate_combined_function(gen, clauses, clause_count);
                                } else {
                                    generate_function_definition(gen, exported);
                                }
                                mark_function_generated(gen, exported->value);
                                free(clauses);
                            }
                            break;
                        case AST_STRUCT_DEFINITION:
                            generate_struct_definition(gen, exported);
                            break;
                        case AST_ACTOR_DEFINITION:
                            generate_actor_definition(gen, exported);
                            break;
                        default:
                            break;
                    }
                }
                break;
            case AST_ACTOR_DEFINITION:
                generate_actor_definition(gen, child);
                // Emit to header if enabled
                if (gen->emit_header) {
                    emit_actor_to_header(gen, child);
                }
                break;
            case AST_MESSAGE_DEFINITION:
                // Generate optimized message struct with field packing
                if (child && child->value) {
                    int field_count = 0;
                    for (int i = 0; i < child->child_count; i++) {
                        if (child->children[i] && child->children[i]->type == AST_MESSAGE_FIELD) {
                            field_count++;
                        }
                    }
                    
                    print_line(gen, "// Message: %s (%d fields)", child->value, field_count);
                    
                    // Align large messages to cache line
                    if (field_count > 4) {
                        print_line(gen, "typedef struct __attribute__((aligned(64))) %s {", child->value);
                    } else {
                        print_line(gen, "typedef struct %s {", child->value);
                    }
                    indent(gen);
                    print_line(gen, "int _message_id;");
                    
                    MessageFieldDef* first_field = NULL;
                    MessageFieldDef* last_field = NULL;
                    
                    // Pack int fields together first for better alignment
                    for (int i = 0; i < child->child_count; i++) {
                        ASTNode* field = child->children[i];
                        if (field && field->type == AST_MESSAGE_FIELD) {
                            if (field->node_type && (field->node_type->kind == TYPE_INT || field->node_type->kind == TYPE_BOOL)) {
                                print_indent(gen);
                                generate_type(gen, field->node_type);
                                fprintf(gen->output, " %s;\n", field->value);
                            }
                        }
                    }
                    
                    // Then pointer types
                    for (int i = 0; i < child->child_count; i++) {
                        ASTNode* field = child->children[i];
                        if (field && field->type == AST_MESSAGE_FIELD) {
                            if (field->node_type && (field->node_type->kind == TYPE_ACTOR_REF || field->node_type->kind == TYPE_STRING)) {
                                print_indent(gen);
                                generate_type(gen, field->node_type);
                                fprintf(gen->output, " %s;\n", field->value);
                            }
                        }
                    }
                    
                    // Finally other types
                    for (int i = 0; i < child->child_count; i++) {
                        ASTNode* field = child->children[i];
                        if (field && field->type == AST_MESSAGE_FIELD) {
                            if (field->node_type && field->node_type->kind != TYPE_INT && field->node_type->kind != TYPE_BOOL &&
                                field->node_type->kind != TYPE_ACTOR_REF && field->node_type->kind != TYPE_STRING) {
                                print_indent(gen);
                                generate_type(gen, field->node_type);
                                fprintf(gen->output, " %s;\n", field->value);
                            }
                        }
                    }
                    
                    // Build field list for registry
                    for (int i = 0; i < child->child_count; i++) {
                        ASTNode* field = child->children[i];
                        if (field && field->type == AST_MESSAGE_FIELD) {
                            MessageFieldDef* field_def = (MessageFieldDef*)malloc(sizeof(MessageFieldDef));
                            field_def->name = strdup(field->value);
                            field_def->type_kind = field->node_type ? field->node_type->kind : TYPE_UNKNOWN;
                            field_def->next = NULL;
                            
                            if (!first_field) {
                                first_field = field_def;
                                last_field = field_def;
                            } else {
                                last_field->next = field_def;
                                last_field = field_def;
                            }
                        }
                    }
                    unindent(gen);
                    print_line(gen, "} %s;", child->value);
                    print_line(gen, "");
                    
                    // Generate type-specific memory pool for this message type
                    print_line(gen, "// Type-specific memory pool for %s", child->value);
                    print_line(gen, "// DECLARE_TYPE_POOL(%s)", child->value);
                    print_line(gen, "// DECLARE_TLS_POOL(%s)", child->value);
                    print_line(gen, "");
                    
                    register_message_type(gen->message_registry, child->value, first_field);

                    // Emit to header if enabled
                    if (gen->emit_header) {
                        emit_message_to_header(gen, child);
                    }
                }
                break;
            case AST_FUNCTION_DEFINITION:
                // Check if this function was already generated (handles pattern matching clauses)
                if (child->value && !is_function_generated(gen, child->value)) {
                    int clause_count = 0;
                    ASTNode** clauses = collect_function_clauses(program, child->value, &clause_count);

                    if (clause_count > 1) {
                        // Multiple clauses - generate combined function
                        generate_combined_function(gen, clauses, clause_count);
                    } else {
                        // Single clause - use standard generation
                        generate_function_definition(gen, child);
                    }

                    mark_function_generated(gen, child->value);
                    free(clauses);
                }
                break;
            case AST_STRUCT_DEFINITION:
                generate_struct_definition(gen, child);
                break;
            case AST_MAIN_FUNCTION:
                generate_main_function(gen, child);
                break;
            case AST_EXTERN_FUNCTION:
                generate_extern_declaration(gen, child);
                break;
            default:
                break;
        }
    }

    // Close header file if emitting
    if (gen->emit_header && gen->header_file) {
        emit_header_epilogue(gen);
    }
}
