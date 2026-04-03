#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "codegen_internal.h"
#include "../aether_module.h"
#include "../aether_error.h"

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
    gen->interp_as_printf = 0;  // Default: interp generates _aether_interp() not printf()
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
    // Extern function parameter registry
    gen->extern_registry = NULL;
    gen->extern_registry_count = 0;
    gen->extern_registry_capacity = 0;
    // MSVC compat: counter for ask-operator temp variables
    gen->ask_temp_counter = 0;
    gen->match_result_var = NULL;
    gen->preempt_loops = 0;
    gen->current_func_return_type = NULL;
    gen->tuple_type_names = NULL;
    gen->tuple_type_count = 0;
    gen->tuple_type_capacity = 0;
    // Builder function registry
    gen->builder_funcs = NULL;
    gen->builder_func_count = 0;
    gen->builder_func_capacity = 0;
    gen->in_trailing_block = 0;
    // Closure support
    gen->closure_counter = 0;
    gen->closures = NULL;
    gen->closure_count = 0;
    gen->closure_capacity = 0;
    gen->closure_var_map = NULL;
    gen->closure_var_count = 0;
    gen->closure_var_capacity = 0;
    // Ask/reply type map
    gen->reply_type_map = NULL;
    gen->reply_type_count = 0;
    gen->reply_type_capacity = 0;
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
        if (gen->extern_registry) {
            for (int i = 0; i < gen->extern_registry_count; i++) {
                free(gen->extern_registry[i].name);
                free(gen->extern_registry[i].params);
            }
            free(gen->extern_registry);
        }
        if (gen->reply_type_map) {
            for (int i = 0; i < gen->reply_type_count; i++) {
                free(gen->reply_type_map[i].request_msg);
                free(gen->reply_type_map[i].reply_msg);
            }
            free(gen->reply_type_map);
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
        AetherError w = {NULL, NULL, 0, 0, "defer stack overflow — too many nested defers",
                         "simplify scope nesting or reduce number of deferred statements",
                         NULL, AETHER_ERR_NONE};
        aether_warning_report(&w);
    }
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

    // Check if this message uses the inline payload_int path (single int field).
    // If so, the field must be intptr_t to match Message.payload_int's width,
    // which is pointer-sized to allow actor refs stored in int message fields.
    MessageDef* reg_def = lookup_message(gen->message_registry, msg_name);
    int uses_inline = reg_def && get_single_int_field(reg_def) != NULL;

    for (int i = 0; i < msg_def->child_count; i++) {
        ASTNode* field = msg_def->children[i];
        if (field && field->type == AST_MESSAGE_FIELD && field->value) {
            const char* c_type = "int";  // Default
            if (field->node_type) {
                c_type = get_c_type(field->node_type);
            }
            // Inline-path int fields use intptr_t to match payload_int width
            if (uses_inline && field->node_type && field->node_type->kind == TYPE_INT) {
                c_type = "intptr_t";
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

// Check if a name is a C/C++ reserved keyword that would cause compilation errors
int is_c_reserved_word(const char* name) {
    static const char* reserved[] = {
        "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if",
        "inline", "int", "long", "register", "restrict", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while",
        // C99/C11
        "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
        "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
        // Common standard library names that conflict
        "malloc", "free", "printf", "sprintf", "strlen", "strcmp",
        "puts", "gets", "abort", "exit", "time", "read", "write",
        "open", "close", "signal", "sleep",
        NULL
    };
    for (int i = 0; reserved[i]; i++) {
        if (strcmp(name, reserved[i]) == 0) return 1;
    }
    return 0;
}

// Mangle an Aether name to avoid C reserved word collision.
// Returns a static buffer — caller must use before next call.
const char* safe_c_name(const char* name) {
    if (!name) return name;
    if (!is_c_reserved_word(name)) return name;
    static char buf[280];
    snprintf(buf, sizeof(buf), "ae_%s", name);
    return buf;
}

const char* get_c_type(Type* type) {
    if (!type) {
        AetherError w = {NULL, NULL, 0, 0, "internal: NULL type in codegen, defaulting to int",
                         "this is a compiler bug — please report it", NULL, AETHER_ERR_NONE};
        aether_warning_report(&w);
        return "int";
    }

    switch (type->kind) {
        case TYPE_INT: return "int";
        case TYPE_INT64: return "int64_t";
        case TYPE_UINT64: return "uint64_t";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "int";
        case TYPE_STRING: return "const char*";
        case TYPE_VOID: return "void";
        case TYPE_ACTOR_REF: {
            // Rotating buffers prevent clobber when get_c_type() is called
            // multiple times in the same printf/expression
            static char buffers[4][256];
            static int buf_idx = 0;
            char* buffer = buffers[buf_idx++ & 3];
            if (type->element_type && type->element_type->kind == TYPE_STRUCT && type->element_type->struct_name) {
                snprintf(buffer, 256, "%s*", type->element_type->struct_name);
            } else {
                snprintf(buffer, 256, "void*");
            }
            return buffer;
        }
        case TYPE_MESSAGE: return "Message";
        case TYPE_PTR: return "void*";
        case TYPE_STRUCT: {
            static char buffers[4][256];
            static int buf_idx = 0;
            char* buffer = buffers[buf_idx++ & 3];
            snprintf(buffer, 256, "%s",
                    type->struct_name ? type->struct_name : "unnamed");
            return buffer;
        }
        case TYPE_ARRAY: {
            static char buffers[4][256];
            static int buf_idx = 0;
            char* buffer = buffers[buf_idx++ & 3];
            const char* element_type = get_c_type(type->element_type);
            if (type->array_size > 0) {
                snprintf(buffer, 256, "%s[%d]", element_type, type->array_size);
            } else {
                snprintf(buffer, 256, "%s*", element_type);
            }
            return buffer;
        }
        case TYPE_TUPLE: {
            static char buffers[4][256];
            static int buf_idx = 0;
            char* buffer = buffers[buf_idx++ & 3];
            int pos = snprintf(buffer, 256, "_tuple");
            for (int i = 0; i < type->tuple_count && pos < 240; i++) {
                const char* elem = get_c_type(type->tuple_types[i]);
                // Sanitize: "const char*" -> "string", "void*" -> "ptr"
                if (strcmp(elem, "const char*") == 0) elem = "string";
                else if (strcmp(elem, "void*") == 0) elem = "ptr";
                pos += snprintf(buffer + pos, 256 - pos, "_%s", elem);
            }
            return buffer;
        }
        case TYPE_FUNCTION:
            return "_AeClosure";
        case TYPE_UNKNOWN: {
            AetherError w = {NULL, NULL, 0, 0,
                             "unresolved type in codegen, defaulting to int",
                             "add explicit type annotation or check that the variable is initialized",
                             NULL, AETHER_ERR_NONE};
            aether_warning_report(&w);
            return "int";
        }
        default: {
            char wbuf[128];
            snprintf(wbuf, sizeof(wbuf), "internal: unknown type kind %d in codegen, defaulting to void", type->kind);
            AetherError w = {NULL, NULL, 0, 0, wbuf,
                             "this is a compiler bug — please report it", NULL, AETHER_ERR_NONE};
            aether_warning_report(&w);
            return "void";
        }
    }
}

// Emit a typedef for a tuple type if not already emitted
void ensure_tuple_typedef(CodeGenerator* gen, Type* type) {
    if (!type || type->kind != TYPE_TUPLE) return;
    const char* name = get_c_type(type);

    // Check if already emitted
    for (int i = 0; i < gen->tuple_type_count; i++) {
        if (strcmp(gen->tuple_type_names[i], name) == 0) return;
    }

    // Emit typedef
    fprintf(gen->output, "typedef struct { ");
    for (int i = 0; i < type->tuple_count; i++) {
        fprintf(gen->output, "%s _%d; ", get_c_type(type->tuple_types[i]), i);
    }
    fprintf(gen->output, "} %s;\n", name);

    // Register
    if (gen->tuple_type_count >= gen->tuple_type_capacity) {
        gen->tuple_type_capacity = gen->tuple_type_capacity ? gen->tuple_type_capacity * 2 : 8;
        gen->tuple_type_names = realloc(gen->tuple_type_names, gen->tuple_type_capacity * sizeof(char*));
    }
    gen->tuple_type_names[gen->tuple_type_count++] = strdup(name);
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
    if (strcmp(aether_op, "&") == 0) return "&";
    if (strcmp(aether_op, "|") == 0) return "|";
    if (strcmp(aether_op, "^") == 0) return "^";
    if (strcmp(aether_op, "~") == 0) return "~";
    if (strcmp(aether_op, "<<") == 0) return "<<";
    if (strcmp(aether_op, ">>") == 0) return ">>";

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

// Check if an AST subtree contains any return statements
static int has_return_statement(ASTNode* node) {
    if (!node) return 0;
    if (node->type == AST_RETURN_STATEMENT) return 1;
    for (int i = 0; i < node->child_count; i++) {
        if (has_return_statement(node->children[i])) return 1;
    }
    return 0;
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

    // Set UTF-8 console codepage on Windows so programs can print Unicode correctly
    print_line(gen, "#ifdef _WIN32");
    print_line(gen, "SetConsoleOutputCP(65001);  // CP_UTF8");
    print_line(gen, "SetConsoleCP(65001);");
    print_line(gen, "#endif");
    // Initialize command-line arguments
    print_line(gen, "aether_args_init(argc, argv);");
    // main_exit_ret and main_exit: label are needed when actors exist
    // (scheduler cleanup) or when main() contains return statements.
    int needs_main_exit = gen->actor_count > 0 || has_return_statement(main);
    gen->uses_main_exit = needs_main_exit;
    if (needs_main_exit) {
        print_line(gen, "int main_exit_ret = 0;");
    }
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
    // Only emit the label if it's actually targeted by a goto (actors or return
    // in main), otherwise GCC warns about an unused label.
    if (needs_main_exit) {
        print_line(gen, "main_exit:");
    }
    if (gen->actor_count > 0) {
        print_line(gen, "");
        print_line(gen, "// Wait for quiescence, stop scheduler threads, and join them");
        print_line(gen, "scheduler_shutdown();");
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

    if (needs_main_exit) {
        print_line(gen, "return main_exit_ret;");
    } else {
        print_line(gen, "return 0;");
    }
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
    print_line(gen, "#include <time.h>");
    print_line(gen, "#ifdef _WIN32");
    print_line(gen, "#define NOMINMAX");
    print_line(gen, "#include <windows.h>");
    print_line(gen, "#elif defined(__EMSCRIPTEN__)");
    print_line(gen, "#include <emscripten.h>");
    print_line(gen, "#else");
    print_line(gen, "#include <unistd.h>");
    print_line(gen, "#include <sched.h>");
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
    // Cooperative preemption: reduction counter for loop yield points
    if (gen->preempt_loops) {
        print_line(gen, "static int _aether_reductions = 10000;");
        print_line(gen, "#ifdef _WIN32");
        print_line(gen, "#define sched_yield() SwitchToThread()");
        print_line(gen, "#elif defined(__EMSCRIPTEN__)");
        print_line(gen, "#define sched_yield() ((void)0)");
        print_line(gen, "#endif");
    }
    /* GCC/Clang vs MSVC: guards for statement expressions ({...}) and computed goto */
    print_line(gen, "#ifndef AETHER_GCC_COMPAT");
    print_line(gen, "#  if (defined(__GNUC__) || defined(__clang__)) && !defined(__EMSCRIPTEN__)");
    print_line(gen, "#    define AETHER_GCC_COMPAT 1");
    print_line(gen, "#  else");
    print_line(gen, "#    define AETHER_GCC_COMPAT 0");
    print_line(gen, "#  endif");
    print_line(gen, "#endif");
    /* Suppress unused-function warnings for runtime helpers that may not
       be called in every program (clock_ns, interp, safe_str) */
    print_line(gen, "#if defined(__GNUC__) || defined(__clang__)");
    print_line(gen, "#pragma GCC diagnostic push");
    print_line(gen, "#pragma GCC diagnostic ignored \"-Wunused-function\"");
    print_line(gen, "#endif");
    /* clock_ns helper — always available (used by timeout checks + clock_ns() builtin) */
    print_line(gen, "#ifdef _WIN32");
    print_line(gen, "static int64_t _aether_clock_ns(void) {");
    print_line(gen, "    LARGE_INTEGER freq, now;");
    print_line(gen, "    QueryPerformanceFrequency(&freq);");
    print_line(gen, "    QueryPerformanceCounter(&now);");
    print_line(gen, "    return (int64_t)((double)now.QuadPart / freq.QuadPart * 1000000000.0);");
    print_line(gen, "}");
    print_line(gen, "#elif defined(__EMSCRIPTEN__)");
    print_line(gen, "static int64_t _aether_clock_ns(void) {");
    print_line(gen, "    return (int64_t)(emscripten_get_now() * 1000000.0);");
    print_line(gen, "}");
    print_line(gen, "#elif defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 0)");
    print_line(gen, "static int64_t _aether_clock_ns(void) { return 0; }");
    print_line(gen, "#else");
    print_line(gen, "static int64_t _aether_clock_ns(void) {");
    print_line(gen, "    struct timespec _ts;");
    print_line(gen, "    clock_gettime(CLOCK_MONOTONIC, &_ts);");
    print_line(gen, "    return (int64_t)_ts.tv_sec * 1000000000LL + _ts.tv_nsec;");
    print_line(gen, "}");
    print_line(gen, "#endif");
    /* String interpolation helper — portable, always available */
    print_line(gen, "#include <stdarg.h>");
    print_line(gen, "static void* _aether_interp(const char* fmt, ...) {");
    print_line(gen, "    va_list args, args2;");
    print_line(gen, "    va_start(args, fmt);");
    print_line(gen, "    va_copy(args2, args);");
    print_line(gen, "    int len = vsnprintf(NULL, 0, fmt, args);");
    print_line(gen, "    va_end(args);");
    print_line(gen, "    char* str = (char*)malloc(len + 1);");
    print_line(gen, "    vsnprintf(str, len + 1, fmt, args2);");
    print_line(gen, "    va_end(args2);");
    print_line(gen, "    return (void*)str;");
    print_line(gen, "}");
    /* NULL-safe string helper for print/println — avoids double-evaluating the expression */
    print_line(gen, "static inline const char* _aether_safe_str(const void* s) {");
    print_line(gen, "    return s ? (const char*)s : \"(null)\";");
    print_line(gen, "}");
    // Ref cells: heap-allocated mutable values for shared state in closures
    print_line(gen, "#if !AETHER_GCC_COMPAT");
    print_line(gen, "static void* _aether_ref_new(intptr_t val) { intptr_t* r = malloc(sizeof(intptr_t)); *r = val; return (void*)r; }");
    print_line(gen, "#endif");
    // Closure support: generic closure struct (function pointer + captured environment)
    print_line(gen, "typedef struct { void (*fn)(void); void* env; } _AeClosure;");
    // Box a closure onto the heap so it can be stored in a list (void*)
    print_line(gen, "static inline void* _aether_box_closure(_AeClosure c) { _AeClosure* p = malloc(sizeof(_AeClosure)); *p = c; return (void*)p; }");
    print_line(gen, "static inline _AeClosure _aether_unbox_closure(void* p) { return *(_AeClosure*)p; }");
    // Lazy evaluation: thunks (deferred computation with memoization)
    print_line(gen, "typedef struct { _AeClosure compute; intptr_t value; int evaluated; } _AeThunk;");
    print_line(gen, "static inline void* _aether_thunk_new(_AeClosure c) { _AeThunk* t = malloc(sizeof(_AeThunk)); t->compute = c; t->value = 0; t->evaluated = 0; return (void*)t; }");
    print_line(gen, "static inline intptr_t _aether_thunk_force(void* p) { _AeThunk* t = (_AeThunk*)p; if (!t->evaluated) { t->value = (intptr_t)((intptr_t(*)(void*))t->compute.fn)(t->compute.env); t->evaluated = 1; } return t->value; }");
    // thunk_free: free the thunk struct. The closure env is owned by
    // the closure variable (auto-deferred), not the thunk. The thunk
    // borrows the env pointer — freeing it here would double-free
    // when the closure variable's defer runs.
    print_line(gen, "static inline void _aether_thunk_free(void* p) { if (p) free(p); }");
    // Terminal raw mode helpers for interactive input
    // Only available on hosted POSIX systems (not embedded/bare-metal or Windows)
    print_line(gen, "#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 1) && !defined(__arm__) && !defined(__thumb__)");
    print_line(gen, "#include <termios.h>");
    print_line(gen, "static struct termios _aether_orig_termios;");
    print_line(gen, "static void _aether_raw_mode(void) {");
    print_line(gen, "    tcgetattr(0, &_aether_orig_termios);");
    print_line(gen, "    struct termios raw = _aether_orig_termios;");
    print_line(gen, "    raw.c_lflag &= ~(ICANON | ECHO);");
    print_line(gen, "    tcsetattr(0, TCSANOW, &raw);");
    print_line(gen, "}");
    print_line(gen, "static void _aether_cooked_mode(void) {");
    print_line(gen, "    tcsetattr(0, TCSANOW, &_aether_orig_termios);");
    print_line(gen, "}");
    print_line(gen, "#else");
    print_line(gen, "static void _aether_raw_mode(void) {}");
    print_line(gen, "static void _aether_cooked_mode(void) {}");
    print_line(gen, "#endif");
    // Builder context stack: trailing blocks push/pop the return value
    print_line(gen, "static void* _aether_ctx_stack[64];");
    print_line(gen, "static int _aether_ctx_depth = 0;");
    print_line(gen, "static inline void _aether_ctx_push(void* ctx) { if (_aether_ctx_depth < 64) _aether_ctx_stack[_aether_ctx_depth++] = ctx; }");
    print_line(gen, "static inline void _aether_ctx_pop(void) { if (_aether_ctx_depth > 0) _aether_ctx_depth--; }");
    print_line(gen, "static inline void* _aether_ctx_get(void) { return _aether_ctx_depth > 0 ? _aether_ctx_stack[_aether_ctx_depth-1] : (void*)0; }");
    // End of static helper definitions — close the warning suppression
    print_line(gen, "#if defined(__GNUC__) || defined(__clang__)");
    print_line(gen, "#pragma GCC diagnostic pop");
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
        print_line(gen, "#include \"aether_compiler.h\"");
        print_line(gen, "");
        print_line(gen, "extern AETHER_TLS int current_core_id;");
        print_line(gen, "");
        print_line(gen, "// Benchmark timing function");
        print_line(gen, "static inline uint64_t rdtsc() {");
        print_line(gen, "#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))");
        print_line(gen, "    return __rdtsc();");
        print_line(gen, "#elif defined(__x86_64__) || defined(__i386__)");
        print_line(gen, "    unsigned int lo, hi;");
        print_line(gen, "    __asm__ __volatile__ (\"rdtsc\" : \"=a\" (lo), \"=d\" (hi));");
        print_line(gen, "    return ((uint64_t)hi << 32) | lo;");
        print_line(gen, "#elif (defined(__aarch64__) || defined(__arm__)) && defined(__unix__)");
        print_line(gen, "    struct timespec ts;");
        print_line(gen, "    clock_gettime(CLOCK_MONOTONIC, &ts);");
        print_line(gen, "    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;");
        print_line(gen, "#elif defined(__EMSCRIPTEN__)");
        print_line(gen, "    return (uint64_t)(emscripten_get_now() * 1000000.0);");
        print_line(gen, "#else");
        print_line(gen, "    return 0;");
        print_line(gen, "#endif");
        print_line(gen, "}");
        // MSVC ask-operator helper: does ask, extracts field by offset, frees reply
        print_line(gen, "#if !AETHER_GCC_COMPAT");
        print_line(gen, "#include <stddef.h>");
        print_line(gen, "static intptr_t _aether_ask_helper(ActorBase* target, void* msg, size_t msg_size, int timeout_ms, size_t field_offset, size_t field_size) {");
        print_line(gen, "    void* reply = scheduler_ask_message(target, msg, msg_size, timeout_ms);");
        print_line(gen, "    if (!reply) return 0;");
        print_line(gen, "    intptr_t val = 0;");
        print_line(gen, "    memcpy(&val, (char*)reply + field_offset, field_size < sizeof(intptr_t) ? field_size : sizeof(intptr_t));");
        print_line(gen, "    free(reply);");
        print_line(gen, "    return val;");
        print_line(gen, "}");
        print_line(gen, "#endif");
    }
    print_line(gen, "");

    // Pre-scan: merge tuple return types across all returns in each function,
    // then emit tuple typedefs. Must happen before forward declarations.
    extern void merge_return_tuple_types(ASTNode* node, Type* merged);
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child && child->type == AST_FUNCTION_DEFINITION && child->node_type &&
            child->node_type->kind == TYPE_TUPLE) {
            merge_return_tuple_types(child, child->node_type);
            ensure_tuple_typedef(gen, child->node_type);
        }
    }
    // Propagate merged return types to all function call sites in the program
    // (call node_types may have UNKNOWN elements from before the merge)
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (!child) continue;
        // For each function with a tuple return type, update matching call sites
        if (child->type == AST_FUNCTION_DEFINITION && child->node_type &&
            child->node_type->kind == TYPE_TUPLE && child->value) {
            // Find all calls to this function in the program and update their node_type
            extern void propagate_tuple_type_to_calls(ASTNode* node, const char* func_name, Type* type);
            propagate_tuple_type_to_calls(program, child->value, child->node_type);
        }
    }
    print_line(gen, "");

    // Pre-pass: identify builder functions (first param is _ctx: ptr)
    // These get builder_context() auto-injected at call sites inside trailing blocks
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (!child || child->type != AST_FUNCTION_DEFINITION || !child->value) continue;
        // Check if first non-guard, non-block child is _ctx: ptr
        for (int j = 0; j < child->child_count; j++) {
            ASTNode* param = child->children[j];
            if (param->type == AST_GUARD_CLAUSE || param->type == AST_BLOCK) continue;
            if ((param->type == AST_PATTERN_VARIABLE || param->type == AST_VARIABLE_DECLARATION) &&
                param->value && strcmp(param->value, "_ctx") == 0 &&
                param->node_type && param->node_type->kind == TYPE_PTR) {
                // Register as builder function
                if (gen->builder_func_count >= gen->builder_func_capacity) {
                    gen->builder_func_capacity = gen->builder_func_capacity ? gen->builder_func_capacity * 2 : 16;
                    gen->builder_funcs = realloc(gen->builder_funcs,
                        gen->builder_func_capacity * sizeof(char*));
                }
                gen->builder_funcs[gen->builder_func_count++] = strdup(child->value);
            }
            break; // only check first param
        }
    }

    // Pre-pass: discover closures in the entire program and emit their
    // environment structs + static functions before any user code.
    discover_closures(gen, program);
    if (gen->closure_count > 0) {
        print_line(gen, "// Closure definitions");
        emit_closure_definitions(gen);
    }

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
        fprintf(gen->output, " %s(", safe_c_name(child->value));

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

    // Forward declarations for actor spawn functions (actors can spawn other actors
    // from within receive handlers, which appear before the spawn function definition)
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (!child || child->type != AST_ACTOR_DEFINITION || !child->value) continue;
        fprintf(gen->output, "struct %s* spawn_%s();\n", child->value, child->value);
    }
    print_line(gen, "");

    // Pre-pass: build request->reply type map from actor receive handlers.
    // This lets the ? operator know the reply message type at codegen time.
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* actor = program->children[i];
        if (!actor || actor->type != AST_ACTOR_DEFINITION) continue;
        for (int j = 0; j < actor->child_count; j++) {
            ASTNode* recv = actor->children[j];
            if (!recv || recv->type != AST_RECEIVE_STATEMENT) continue;
            for (int k = 0; k < recv->child_count; k++) {
                ASTNode* arm = recv->children[k];
                if (!arm || arm->type != AST_RECEIVE_ARM || arm->child_count < 2) continue;
                ASTNode* pattern = arm->children[0];
                ASTNode* body = arm->children[1];
                if (!pattern || !pattern->value || !body) continue;
                const char* req_msg = pattern->value;
                for (int s = 0; s < body->child_count; s++) {
                    ASTNode* stmt = body->children[s];
                    if (!stmt || stmt->type != AST_REPLY_STATEMENT) continue;
                    if (stmt->child_count > 0 && stmt->children[0] &&
                        stmt->children[0]->type == AST_MESSAGE_CONSTRUCTOR &&
                        stmt->children[0]->value) {
                        const char* reply_msg = stmt->children[0]->value;
                        if (gen->reply_type_count >= gen->reply_type_capacity) {
                            gen->reply_type_capacity = gen->reply_type_capacity ? gen->reply_type_capacity * 2 : 16;
                            gen->reply_type_map = realloc(gen->reply_type_map,
                                gen->reply_type_capacity * sizeof(*gen->reply_type_map));
                        }
                        gen->reply_type_map[gen->reply_type_count].request_msg = strdup(req_msg);
                        gen->reply_type_map[gen->reply_type_count].reply_msg = strdup(reply_msg);
                        gen->reply_type_count++;
                        break;
                    }
                }
            }
        }
    }

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
                        const char* mod_name = module_path + 4;
                        char mod_prefix[128];
                        snprintf(mod_prefix, sizeof(mod_prefix), "%s_", mod_name);
                        int mod_prefix_len = (int)strlen(mod_prefix);

                        // Look up cached module from orchestrator
                        AetherModule* mod_entry = module_find(module_path);
                        ASTNode* mod_ast = mod_entry ? mod_entry->ast : NULL;
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
                                            // Strip module prefix: "math_sqrt" -> "sqrt"
                                            const char* short_name = decl->value;
                                            if (strncmp(decl->value, mod_prefix, mod_prefix_len) == 0) {
                                                short_name = decl->value + mod_prefix_len;
                                            }
                                            for (int k = 0; k < child->child_count; k++) {
                                                ASTNode* sel = child->children[k];
                                                if (sel && sel->type == AST_IDENTIFIER &&
                                                    strcmp(sel->value, short_name) == 0) {
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
                            // NOTE: do NOT free mod_ast — registry owns it
                        }
                    } else {
                        // Handle local package imports: import mypackage.utils
                        AetherModule* mod_entry = module_find(module_path);
                        ASTNode* mod_ast = mod_entry ? mod_entry->ast : NULL;
                        if (mod_ast) {
                            for (int j = 0; j < mod_ast->child_count; j++) {
                                ASTNode* decl = mod_ast->children[j];
                                if (decl->type == AST_EXTERN_FUNCTION && decl->value) {
                                    generate_extern_declaration(gen, decl);
                                }
                                // AST_FUNCTION_DEFINITION handled by module_merge_into_program()
                            }
                            // NOTE: do NOT free mod_ast — registry owns it
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
                        print_line(gen, "#ifdef _MSC_VER");
                        print_line(gen, "__declspec(align(64))");
                        print_line(gen, "#endif");
                        print_line(gen, "typedef struct");
                        print_line(gen, "#if defined(__GNUC__) || defined(__clang__)");
                        print_line(gen, "__attribute__((aligned(64)))");
                        print_line(gen, "#endif");
                        print_line(gen, "%s {", child->value);
                    } else {
                        print_line(gen, "typedef struct %s {", child->value);
                    }
                    indent(gen);
                    print_line(gen, "int _message_id;");
                    
                    MessageFieldDef* first_field = NULL;
                    MessageFieldDef* last_field = NULL;
                    
                    // Detect single-int-field messages (inline payload_int path).
                    // Their int field must be intptr_t to match Message.payload_int width.
                    int int_field_count = 0, other_field_count = 0;
                    for (int i = 0; i < child->child_count; i++) {
                        ASTNode* f = child->children[i];
                        if (f && f->type == AST_MESSAGE_FIELD && f->node_type) {
                            if (f->node_type->kind == TYPE_INT) int_field_count++;
                            else other_field_count++;
                        }
                    }
                    int is_inline_msg = (int_field_count == 1 && other_field_count == 0);

                    // Pack int fields together first for better alignment
                    for (int i = 0; i < child->child_count; i++) {
                        ASTNode* field = child->children[i];
                        if (field && field->type == AST_MESSAGE_FIELD) {
                            if (field->node_type && (field->node_type->kind == TYPE_INT || field->node_type->kind == TYPE_BOOL)) {
                                print_indent(gen);
                                if (is_inline_msg && field->node_type->kind == TYPE_INT) {
                                    // intptr_t for inline-path field (matches payload_int width)
                                    fprintf(gen->output, "intptr_t");
                                } else {
                                    generate_type(gen, field->node_type);
                                }
                                fprintf(gen->output, " %s;\n", field->value);
                            }
                        }
                    }
                    
                    // Then pointer types
                    for (int i = 0; i < child->child_count; i++) {
                        ASTNode* field = child->children[i];
                        if (field && field->type == AST_MESSAGE_FIELD) {
                            if (field->node_type && (field->node_type->kind == TYPE_ACTOR_REF || field->node_type->kind == TYPE_STRING || field->node_type->kind == TYPE_PTR)) {
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
                                field->node_type->kind != TYPE_ACTOR_REF && field->node_type->kind != TYPE_STRING && field->node_type->kind != TYPE_PTR) {
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
            case AST_CONST_DECLARATION:
                // Emit top-level constant as #define
                if (child->value && child->child_count > 0) {
                    fprintf(gen->output, "#define %s (", child->value);
                    generate_expression(gen, child->children[0]);
                    fprintf(gen->output, ")\n");
                }
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
