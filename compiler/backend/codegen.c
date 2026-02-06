#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include "codegen.h"

// Forward declarations
static int has_return_value(ASTNode* node);

// Returns the field name if msg has exactly one int field (eligible for inline encoding),
// or NULL otherwise. Inline messages skip pool allocation entirely — the single int field
// is stored in Message.payload_int, avoiding memcpy and pool lookup on every send.
static const char* get_single_int_field(MessageDef* msg_def) {
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
    gen->emit_header = 0;
    gen->header_file = NULL;
    gen->header_path = NULL;
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
    gen->declared_vars = realloc(gen->declared_vars, sizeof(char*) * (gen->declared_var_count + 1));
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
        strcpy(guard, "AETHER_GENERATED_H");
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
            fprintf(stderr, "Warning: Unresolved type (TYPE_UNKNOWN) in codegen, defaulting to int\n");
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

void generate_expression(CodeGenerator* gen, ASTNode* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case AST_LITERAL:
            if (expr->node_type && expr->node_type->kind == TYPE_STRING) {
                // String literal: just use C string directly
                fprintf(gen->output, "\"");
                // Escape string characters
                const char* str = expr->value;
                while (*str) {
                    switch (*str) {
                        case '\n': fprintf(gen->output, "\\n"); break;
                        case '\t': fprintf(gen->output, "\\t"); break;
                        case '\r': fprintf(gen->output, "\\r"); break;
                        case '\\': fprintf(gen->output, "\\\\"); break;
                        case '"': fprintf(gen->output, "\\\""); break;
                        default: fprintf(gen->output, "%c", *str); break;
                    }
                    str++;
                }
                fprintf(gen->output, "\"");
            } else {
                fprintf(gen->output, "%s", expr->value);
            }
            break;
            
        case AST_IDENTIFIER:
            if (gen->current_actor) {
                int is_state_var = 0;
                for (int i = 0; i < gen->state_var_count; i++) {
                    if (strcmp(expr->value, gen->actor_state_vars[i]) == 0) {
                        is_state_var = 1;
                        break;
                    }
                }
                if (is_state_var) {
                    fprintf(gen->output, "self->%s", expr->value);
                } else {
                    fprintf(gen->output, "%s", expr->value);
                }
            } else {
                fprintf(gen->output, "%s", expr->value);
            }
            break;
        
        case AST_MEMBER_ACCESS:
            // expr.field becomes expr.field in C (or expr->field for actor refs)
            if (expr->child_count > 0) {
                ASTNode* child = expr->children[0];

                // Check if this is a cross-actor atomic access
                // (accessing an actor's state from outside that actor)
                int needs_atomic = 0;
                if (child->node_type && child->node_type->kind == TYPE_ACTOR_REF) {
                    // This is accessing a field of an actor reference
                    // If we're not inside an actor, we need atomic operations for thread safety
                    // But NOT for lvalues (assignment targets) or _ref fields (pointers)
                    size_t name_len = strlen(expr->value);
                    int is_ref_field = (name_len > 4 && strcmp(expr->value + name_len - 4, "_ref") == 0);

                    if (!gen->current_actor && !gen->generating_lvalue && !is_ref_field) {
                        // We're in main() reading an actor's state - need atomic read
                        needs_atomic = 1;
                    }
                }

                if (needs_atomic) {
                    // Generate atomic_load for cross-actor read
                    fprintf(gen->output, "atomic_load(&");
                    generate_expression(gen, child);
                    fprintf(gen->output, "->%s)", expr->value);
                } else if (child->node_type && child->node_type->kind == TYPE_ACTOR_REF) {
                    // Normal actor field access
                    generate_expression(gen, child);
                    fprintf(gen->output, "->%s", expr->value);
                } else {
                    // Normal struct field access
                    generate_expression(gen, child);
                    fprintf(gen->output, ".%s", expr->value);
                }
            }
            break;
            
        case AST_BINARY_EXPRESSION:
            if (expr->child_count >= 2) {
                // Skip outer parens if we're in a condition (if/while already provide parens)
                int skip_parens = gen->in_condition;
                gen->in_condition = 0;  // Only skip for top-level, not nested

                if (!skip_parens) fprintf(gen->output, "(");
                generate_expression(gen, expr->children[0]);
                fprintf(gen->output, " %s ", get_c_operator(expr->value));
                generate_expression(gen, expr->children[1]);
                if (!skip_parens) fprintf(gen->output, ")");
            }
            break;
            
        case AST_UNARY_EXPRESSION:
            if (expr->child_count >= 1) {
                fprintf(gen->output, "%s", get_c_operator(expr->value));
                generate_expression(gen, expr->children[0]);
            }
            break;
            
        case AST_FUNCTION_CALL:
            // Function calls: expr->value is the function name, children are arguments
            if (expr->value) {
                const char* func_name = expr->value;
                
                // Special handling for make()
                if (strcmp(func_name, "make") == 0 && expr->node_type && expr->node_type->kind == TYPE_ARRAY) {
                    // make([]type, size) → malloc(size * sizeof(type))
                    fprintf(gen->output, "(%s)malloc(", get_c_type(expr->node_type));
                    if (expr->child_count > 0) {
                        fprintf(gen->output, "(");
                        generate_expression(gen, expr->children[0]); // size expression
                        fprintf(gen->output, ") * sizeof(%s)", get_c_type(expr->node_type->element_type));
                    }
                    fprintf(gen->output, ")");
                }
                // Runtime type checking builtins
                else if (strcmp(func_name, "typeof") == 0) {
                    // typeof(value) → aether_typeof(value)
                    fprintf(gen->output, "aether_typeof(");
                    if (expr->child_count > 0) {
                        generate_expression(gen, expr->children[0]);
                    }
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "is_type") == 0) {
                    // is_type(value, "typename") → aether_is_type(value, "typename")
                    fprintf(gen->output, "aether_is_type(");
                    for (int i = 0; i < expr->child_count; i++) {
                        if (i > 0) fprintf(gen->output, ", ");
                        generate_expression(gen, expr->children[i]);
                    }
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "convert_type") == 0) {
                    // convert_type(value, "typename") → aether_convert_type(value, "typename")
                    fprintf(gen->output, "aether_convert_type(");
                    for (int i = 0; i < expr->child_count; i++) {
                        if (i > 0) fprintf(gen->output, ", ");
                        generate_expression(gen, expr->children[i]);
                    }
                    fprintf(gen->output, ")");
                }
                // Special handling for print() function
                else if (strcmp(func_name, "print") == 0) {
                    // print(value) needs to determine type and use correct format
                    if (expr->child_count == 1 && expr->children[0]->node_type) {
                        ASTNode* arg = expr->children[0];
                        Type* arg_type = arg->node_type;

                        // Determine the correct printf format based on type
                        if (arg_type->kind == TYPE_INT) {
                            fprintf(gen->output, "printf(\"%%d\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_FLOAT) {
                            fprintf(gen->output, "printf(\"%%f\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_STRING) {
                            fprintf(gen->output, "printf(\"%%s\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_BOOL) {
                            fprintf(gen->output, "printf(\"%%s\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, " ? \"true\" : \"false\")");
                        } else {
                            // Fallback for unknown/unresolved types - default to %d
                            fprintf(gen->output, "printf(\"%%d\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        }
                    } else if (expr->child_count == 1) {
                        // Check if string literal
                        ASTNode* a = expr->children[0];
                        if (a->type == AST_LITERAL && a->node_type && a->node_type->kind == TYPE_STRING) {
                            fprintf(gen->output, "printf(");
                            generate_expression(gen, a);
                            fprintf(gen->output, ")");
                        } else {
                            // Default to %d
                            fprintf(gen->output, "printf(\"%%d\", ");
                            generate_expression(gen, a);
                            fprintf(gen->output, ")");
                        }
                    } else {
                        // Multiple arguments - treat first as format string
                        fprintf(gen->output, "printf(");
                        for (int i = 0; i < expr->child_count; i++) {
                            if (i > 0) fprintf(gen->output, ", ");
                            generate_expression(gen, expr->children[i]);
                        }
                        fprintf(gen->output, ")");
                    }
                }
                else {
                    // Regular function call: func_name(arg1, arg2, ...)
                    fprintf(gen->output, "%s(", func_name);
                    for (int i = 0; i < expr->child_count; i++) {
                        if (i > 0) fprintf(gen->output, ", ");
                        generate_expression(gen, expr->children[i]);
                    }
                    fprintf(gen->output, ")");
                }
            }
            break;
            
        case AST_ACTOR_REF:
            if (strcmp(expr->value, "self") == 0) {
                fprintf(gen->output, "aether_self()");
            } else {
                fprintf(gen->output, "%s", expr->value);
            }
            break;
        
        case AST_ARRAY_LITERAL:
            // Array literal: {1, 2, 3}
            fprintf(gen->output, "{");
            for (int i = 0; i < expr->child_count; i++) {
                if (i > 0) fprintf(gen->output, ", ");
                generate_expression(gen, expr->children[i]);
            }
            fprintf(gen->output, "}");
            break;
        
        case AST_STRUCT_LITERAL:
            // Struct literal: (StructName){.field1 = value1, .field2 = value2}
            fprintf(gen->output, "(%s){", expr->value);
            for (int i = 0; i < expr->child_count; i++) {
                ASTNode* field_init = expr->children[i];
                if (field_init && field_init->type == AST_ASSIGNMENT) {
                    if (i > 0) fprintf(gen->output, ", ");
                    fprintf(gen->output, ".%s = ", field_init->value);
                    if (field_init->child_count > 0) {
                        generate_expression(gen, field_init->children[0]);
                    }
                }
            }
            fprintf(gen->output, "}");
            break;
        
        case AST_ARRAY_ACCESS:
            // Array indexing: arr[index]
            if (expr->child_count >= 2) {
                generate_expression(gen, expr->children[0]);  // array
                fprintf(gen->output, "[");
                generate_expression(gen, expr->children[1]);  // index
                fprintf(gen->output, "]");
            }
            break;
        
        // Actor V2 - Fire-and-forget send: actor ! Message { ... }
        case AST_SEND_FIRE_FORGET:
            if (expr->child_count >= 2) {
                ASTNode* target = expr->children[0];
                ASTNode* message = expr->children[1];
                
                if (message && message->type == AST_MESSAGE_CONSTRUCTOR) {
                    MessageDef* msg_def = lookup_message(gen->message_registry, message->value);
                    if (msg_def) {
                        const char* single_int = get_single_int_field(msg_def);
                        if (single_int) {
                            // Inline fast path: single-int messages bypass pool allocation.
                            // Encode _message_id in msg.type, field value in msg.payload_int.
                            fprintf(gen->output, "{ Message _imsg = {%d, 0, ", msg_def->message_id);
                            // Find and emit the single field initializer
                            for (int i = 0; i < message->child_count; i++) {
                                ASTNode* field_init = message->children[i];
                                if (field_init && field_init->type == AST_FIELD_INIT && field_init->child_count > 0) {
                                    generate_expression(gen, field_init->children[0]);
                                    break;
                                }
                            }
                            fprintf(gen->output, ", NULL, {NULL, 0, 0}}; ");
                            fprintf(gen->output, "if (current_core_id >= 0 && current_core_id == ((ActorBase*)(");
                            generate_expression(gen, target);
                            fprintf(gen->output, "))->assigned_core) { scheduler_send_local((ActorBase*)(");
                            generate_expression(gen, target);
                            fprintf(gen->output, "), _imsg); } else { scheduler_send_remote((ActorBase*)(");
                            generate_expression(gen, target);
                            fprintf(gen->output, "), _imsg, current_core_id); } }");
                        } else {
                            // General path: multi-field messages go through pool allocation
                            fprintf(gen->output, "{ %s _msg = { ._message_id = %d",
                                    message->value, msg_def->message_id);

                            for (int i = 0; i < message->child_count; i++) {
                                ASTNode* field_init = message->children[i];
                                if (field_init && field_init->type == AST_FIELD_INIT) {
                                    fprintf(gen->output, ", .%s = ", field_init->value);
                                    if (field_init->child_count > 0) {
                                        generate_expression(gen, field_init->children[0]);
                                    }
                                }
                            }

                            fprintf(gen->output, " }; aether_send_message(");
                            fprintf(gen->output, "(void*)"); generate_expression(gen, target);
                            fprintf(gen->output, ", &_msg, sizeof(%s)); }", message->value);
                        }
                    } else {
                        fprintf(gen->output, "/* ERROR: unknown message type %s */", message->value);
                    }
                }
            }
            break;
        
        // Actor V2 - Ask pattern: result = actor ? Message { ... }
        case AST_SEND_ASK:
            if (expr->child_count >= 2) {
                ASTNode* target = expr->children[0];
                ASTNode* message = expr->children[1];
                
                if (message && message->type == AST_MESSAGE_CONSTRUCTOR) {
                    MessageDef* msg_def = lookup_message(gen->message_registry, message->value);
                    if (msg_def) {
                        fprintf(gen->output, "({ %s _msg = { ._message_id = %d", 
                                message->value, msg_def->message_id);
                        
                        for (int i = 0; i < message->child_count; i++) {
                            ASTNode* field_init = message->children[i];
                            if (field_init && field_init->type == AST_FIELD_INIT) {
                                fprintf(gen->output, ", .%s = ", field_init->value);
                                if (field_init->child_count > 0) {
                                    generate_expression(gen, field_init->children[0]);
                                }
                            }
                        }
                        
                        fprintf(gen->output, " }; aether_ask_message(");
                        generate_expression(gen, target);
                        fprintf(gen->output, ", &_msg, sizeof(%s), 5000); })", message->value);
                    } else {
                        fprintf(gen->output, "/* ERROR: unknown message type %s */", message->value);
                    }
                }
            }
            break;
            
        default:
            // Generate all children
            for (int i = 0; i < expr->child_count; i++) {
                generate_expression(gen, expr->children[i]);
            }
            break;
    }
}

// Helper function to generate condition for list pattern in match statement
static void generate_list_pattern_condition(CodeGenerator* gen, ASTNode* pattern,
                                            const char* len_name) {
    if (!pattern) return;

    if (pattern->type == AST_PATTERN_LIST) {
        if (pattern->child_count == 0) {
            // Empty list pattern: check len == 0
            fprintf(gen->output, "%s == 0", len_name);
        } else {
            // Fixed-size list pattern: check len == count
            fprintf(gen->output, "%s == %d", len_name, pattern->child_count);
        }
    } else if (pattern->type == AST_PATTERN_CONS) {
        // Cons pattern [H|T]: check len >= 1
        fprintf(gen->output, "%s >= 1", len_name);
    }
}

// Helper function to generate variable bindings for list pattern elements
static void generate_list_pattern_bindings(CodeGenerator* gen, ASTNode* pattern,
                                           const char* array_name, const char* len_name) {
    if (!pattern) return;

    if (pattern->type == AST_PATTERN_LIST && pattern->child_count > 0) {
        // Bind each element: int x0 = arr[0]; int x1 = arr[1]; ...
        for (int i = 0; i < pattern->child_count; i++) {
            ASTNode* elem = pattern->children[i];
            if (elem && elem->type == AST_PATTERN_VARIABLE && elem->value) {
                print_line(gen, "int %s = %s[%d];", elem->value, array_name, i);
            }
        }
    } else if (pattern->type == AST_PATTERN_CONS && pattern->child_count >= 2) {
        // Cons pattern: bind head and tail
        ASTNode* head = pattern->children[0];
        ASTNode* tail = pattern->children[1];

        if (head && head->type == AST_PATTERN_VARIABLE && head->value) {
            print_line(gen, "int %s = %s[0];", head->value, array_name);
        }
        if (tail && tail->type == AST_PATTERN_VARIABLE && tail->value) {
            print_line(gen, "int* %s = &%s[1];", tail->value, array_name);
            print_line(gen, "int %s_len = %s - 1;", tail->value, len_name);
        }
    }
}

// Check if any arm in match statement uses list patterns
static int has_list_patterns(ASTNode* match_stmt) {
    for (int i = 1; i < match_stmt->child_count; i++) {
        ASTNode* arm = match_stmt->children[i];
        if (arm && arm->type == AST_MATCH_ARM && arm->child_count >= 1) {
            ASTNode* pattern = arm->children[0];
            if (pattern && (pattern->type == AST_PATTERN_LIST ||
                           pattern->type == AST_PATTERN_CONS)) {
                return 1;
            }
        }
    }
    return 0;
}

void generate_statement(CodeGenerator* gen, ASTNode* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case AST_VARIABLE_DECLARATION: {
            // Check if this is a state variable assignment in an actor
            int is_state_var = 0;
            if (gen->current_actor && stmt->value) {
                for (int i = 0; i < gen->state_var_count; i++) {
                    if (strcmp(stmt->value, gen->actor_state_vars[i]) == 0) {
                        is_state_var = 1;
                        break;
                    }
                }
            }
            
            if (is_state_var) {
                // Generate as assignment to self->field
                fprintf(gen->output, "self->%s", stmt->value);
                if (stmt->child_count > 0) {
                    fprintf(gen->output, " = ");
                    generate_expression(gen, stmt->children[0]);
                }
                fprintf(gen->output, ";\n");
            } else {
                // Check if this is a reassignment (Python-style)
                if (is_var_declared(gen, stmt->value)) {
                    // Already declared - generate assignment only
                    fprintf(gen->output, "%s", stmt->value);
                    if (stmt->child_count > 0) {
                        fprintf(gen->output, " = ");
                        generate_expression(gen, stmt->children[0]);
                    }
                    fprintf(gen->output, ";\n");
                } else {
                    // First declaration - generate type + variable
                    mark_var_declared(gen, stmt->value);

                    // Detect if initializer is an array literal (type system may not tag empty arrays)
                    int is_array_init = (stmt->child_count > 0 &&
                                         stmt->children[0] &&
                                         stmt->children[0]->type == AST_ARRAY_LITERAL);

                    // Handle array types specially (C syntax: int name[size])
                    if (stmt->node_type && stmt->node_type->kind == TYPE_ARRAY) {
                        const char* elem_type = get_c_type(stmt->node_type->element_type);
                        if (stmt->node_type->array_size > 0) {
                            fprintf(gen->output, "%s %s[%d]", elem_type, stmt->value, stmt->node_type->array_size);
                        } else {
                            // Dynamic/empty array - use pointer
                            fprintf(gen->output, "%s* %s", elem_type, stmt->value);
                        }
                    } else if (is_array_init) {
                        // Type system missed array type but initializer is array literal
                        int arr_size = stmt->children[0]->child_count;
                        if (arr_size > 0) {
                            fprintf(gen->output, "int %s[%d]", stmt->value, arr_size);
                        } else {
                            // Empty array [] - use NULL pointer
                            fprintf(gen->output, "int* %s", stmt->value);
                        }
                    } else if (stmt->child_count > 0 && stmt->children[0] &&
                               (stmt->children[0]->type == AST_MESSAGE_CONSTRUCTOR ||
                                stmt->children[0]->type == AST_STRUCT_LITERAL) &&
                               stmt->children[0]->value) {
                        // Message/struct constructor — use the constructor name as type
                        fprintf(gen->output, "%s %s", stmt->children[0]->value, stmt->value);
                    } else {
                        // Determine the best type for this variable
                        Type* var_type = stmt->node_type;

                        // If type is void/unknown, try to get it from the initializer
                        if ((!var_type || var_type->kind == TYPE_VOID || var_type->kind == TYPE_UNKNOWN)
                            && stmt->child_count > 0 && stmt->children[0]) {
                            ASTNode* init = stmt->children[0];
                            // Check initializer's own node_type
                            if (init->node_type && init->node_type->kind != TYPE_VOID
                                && init->node_type->kind != TYPE_UNKNOWN) {
                                var_type = init->node_type;
                            }
                            // For function calls, look up the function's return type
                            else if (init->type == AST_FUNCTION_CALL && init->value) {
                                for (int fi = 0; fi < gen->program->child_count; fi++) {
                                    ASTNode* fn = gen->program->children[fi];
                                    if (fn && fn->type == AST_FUNCTION_DEFINITION
                                        && fn->value && strcmp(fn->value, init->value) == 0) {
                                        if (fn->node_type && fn->node_type->kind != TYPE_VOID
                                            && fn->node_type->kind != TYPE_UNKNOWN) {
                                            var_type = fn->node_type;
                                        } else if (has_return_value(fn)) {
                                            // Same heuristic as generate_function_definition:
                                            // function has return-with-value but type is void → int
                                            static Type int_type = { .kind = TYPE_INT };
                                            var_type = &int_type;
                                        }
                                        break;
                                    }
                                }
                            }
                        }

                        generate_type(gen, var_type);
                        fprintf(gen->output, " %s", stmt->value);
                    }

                    if (stmt->child_count > 0) {
                        // Empty array literal gets NULL, not {}
                        if (is_array_init && stmt->children[0]->child_count == 0) {
                            fprintf(gen->output, " = NULL");
                        } else {
                            fprintf(gen->output, " = ");
                            generate_expression(gen, stmt->children[0]);
                        }
                    }

                    fprintf(gen->output, ";\n");
                }
            }
            break;
        }
        
        case AST_ASSIGNMENT:
            if (stmt->child_count >= 2) {
                // Left side is lvalue (assignment target) - no atomic operations
                gen->generating_lvalue = 1;
                generate_expression(gen, stmt->children[0]);
                gen->generating_lvalue = 0;

                fprintf(gen->output, " = ");

                // Right side is rvalue (read) - may need atomic operations
                generate_expression(gen, stmt->children[1]);
                fprintf(gen->output, ";\n");
            }
            break;
            
        case AST_IF_STATEMENT:
            fprintf(gen->output, "if (");
            if (stmt->child_count > 0) {
                gen->in_condition = 1;
                generate_expression(gen, stmt->children[0]);
                gen->in_condition = 0;
            }
            fprintf(gen->output, ") {\n");

            indent(gen);
            if (stmt->child_count > 1) {
                generate_statement(gen, stmt->children[1]);
            }
            unindent(gen);

            if (stmt->child_count > 2) {
                print_line(gen, "} else {");
                indent(gen);
                generate_statement(gen, stmt->children[2]);
                unindent(gen);
            }

            print_line(gen, "}");
            break;
            
        case AST_FOR_LOOP:
            fprintf(gen->output, "for (");
            if (stmt->child_count > 0 && stmt->children[0]) {
                ASTNode* init = stmt->children[0];
                if (init->type == AST_VARIABLE_DECLARATION) {
                    generate_type(gen, init->node_type);
                    fprintf(gen->output, " %s", init->value);
                    if (init->child_count > 0) {
                        fprintf(gen->output, " = ");
                        generate_expression(gen, init->children[0]);
                    }
                } else {
                    generate_expression(gen, init);
                }
            }
            fprintf(gen->output, "; ");
            if (stmt->child_count > 1 && stmt->children[1]) {
                generate_expression(gen, stmt->children[1]); // condition
            }
            // Note: If no condition, C for loop becomes infinite (for (;;))
            fprintf(gen->output, "; ");
            if (stmt->child_count > 2 && stmt->children[2]) {
                generate_expression(gen, stmt->children[2]); // increment
            }
            fprintf(gen->output, ") {\n");
            
            indent(gen);
            if (stmt->child_count > 3 && stmt->children[3]) {
                // Body is always a statement (could be a block or single statement)
                generate_statement(gen, stmt->children[3]); // body
            }
            unindent(gen);
            
            print_line(gen, "}");
            break;
            
        case AST_WHILE_LOOP:
            fprintf(gen->output, "while (");
            if (stmt->child_count > 0) {
                gen->in_condition = 1;
                generate_expression(gen, stmt->children[0]);
                gen->in_condition = 0;
            }
            fprintf(gen->output, ") {\n");

            indent(gen);
            if (stmt->child_count > 1) {
                generate_statement(gen, stmt->children[1]);
            }
            unindent(gen);
            
            print_line(gen, "}");
            break;
            
        case AST_MATCH_STATEMENT:
            // Generate match as a series of if-else statements
            // match (x) { 1 -> a, 2 -> b, _ -> c }
            // becomes: if (x == 1) { a; } else if (x == 2) { b; } else { c; }
            if (stmt->child_count > 0) {
                ASTNode* match_expr = stmt->children[0];

                // Check if any arm uses list patterns
                int uses_list_patterns = has_list_patterns(stmt);
                char array_name[64] = "_match_arr";
                char len_name[64] = "_match_len";

                // If using list patterns, wrap in block and generate array setup
                if (uses_list_patterns) {
                    print_line(gen, "{");
                    indent(gen);
                    print_indent(gen);
                    fprintf(gen->output, "int* %s = ", array_name);
                    generate_expression(gen, match_expr);
                    fprintf(gen->output, ";\n");
                    // For arrays, assume a corresponding _len variable exists
                    // Convention: if matching on 'arr', expect 'arr_len' to exist
                    print_indent(gen);
                    fprintf(gen->output, "int %s = ", len_name);
                    generate_expression(gen, match_expr);
                    fprintf(gen->output, "_len;\n");
                }

                for (int i = 1; i < stmt->child_count; i++) {
                    ASTNode* match_arm = stmt->children[i];
                    if (match_arm->type != AST_MATCH_ARM || match_arm->child_count < 2) continue;

                    ASTNode* pattern = match_arm->children[0];
                    ASTNode* result = match_arm->children[1];

                    // Check if wildcard pattern
                    int is_wildcard = (pattern->type == AST_LITERAL &&
                                      pattern->value &&
                                      strcmp(pattern->value, "_") == 0) ||
                                     (pattern->node_type &&
                                      pattern->node_type->kind == TYPE_WILDCARD);

                    // Check if list pattern
                    int is_list_pattern = (pattern->type == AST_PATTERN_LIST ||
                                          pattern->type == AST_PATTERN_CONS);

                    if (is_wildcard) {
                        // else clause
                        if (i > 1) {
                            print_indent(gen);
                            fprintf(gen->output, "else {\n");
                        } else {
                            print_indent(gen);
                            fprintf(gen->output, "{\n");
                        }
                    } else if (is_list_pattern) {
                        // List pattern clause
                        if (i > 1) {
                            print_indent(gen);
                            fprintf(gen->output, "else if (");
                        } else {
                            print_indent(gen);
                            fprintf(gen->output, "if (");
                        }
                        generate_list_pattern_condition(gen, pattern, len_name);
                        fprintf(gen->output, ") {\n");
                    } else {
                        // Regular literal/expression pattern
                        if (i > 1) {
                            print_indent(gen);
                            fprintf(gen->output, "else if (");
                        } else {
                            print_indent(gen);
                            fprintf(gen->output, "if (");
                        }
                        generate_expression(gen, match_expr);
                        fprintf(gen->output, " == ");
                        generate_expression(gen, pattern);
                        fprintf(gen->output, ") {\n");
                    }

                    indent(gen);

                    // Generate list pattern bindings if needed
                    if (is_list_pattern) {
                        generate_list_pattern_bindings(gen, pattern, array_name, len_name);
                    }

                    if (result->type == AST_BLOCK) {
                        // Already a block, generate its statements
                        for (int j = 0; j < result->child_count; j++) {
                            generate_statement(gen, result->children[j]);
                        }
                    } else {
                        // Single expression, make it a statement
                        print_indent(gen);
                        generate_expression(gen, result);
                        fprintf(gen->output, ";\n");
                    }
                    unindent(gen);
                    print_line(gen, "}");
                }

                // Close the scoping block for list pattern variables
                if (uses_list_patterns) {
                    unindent(gen);
                    print_line(gen, "}");
                }
            }
            break;

        case AST_SWITCH_STATEMENT:
            fprintf(gen->output, "switch (");
            if (stmt->child_count > 0) {
                generate_expression(gen, stmt->children[0]);
            }
            fprintf(gen->output, ") {\n");
            
            indent(gen);
            for (int i = 1; i < stmt->child_count; i++) {
                generate_statement(gen, stmt->children[i]);
            }
            unindent(gen);
            
            print_line(gen, "}");
            break;
            
        case AST_CASE_STATEMENT:
            if (stmt->value && strcmp(stmt->value, "default") == 0) {
                print_line(gen, "default:");
            } else {
                fprintf(gen->output, "case ");
                if (stmt->child_count > 0) {
                    generate_expression(gen, stmt->children[0]);
                }
                fprintf(gen->output, ":\n");
            }
            
            indent(gen);
            // Generate all statements in the case block (skip first child which is the case value)
            int start_idx = (stmt->value && strcmp(stmt->value, "default") == 0) ? 0 : 1;
            for (int i = start_idx; i < stmt->child_count; i++) {
                generate_statement(gen, stmt->children[i]);
            }
            unindent(gen);
            break;
            
        case AST_RETURN_STATEMENT:
            fprintf(gen->output, "return");
            if (stmt->child_count > 0) {
                fprintf(gen->output, " ");
                generate_expression(gen, stmt->children[0]);
            }
            fprintf(gen->output, ";\n");
            break;
            
        case AST_BREAK_STATEMENT:
            print_line(gen, "break;");
            break;
            
        case AST_CONTINUE_STATEMENT:
            print_line(gen, "continue;");
            break;
            
        case AST_DEFER_STATEMENT:
            if (stmt->child_count > 0) {
                fprintf(gen->output, "// defer: ");
                generate_statement(gen, stmt->children[0]);
            }
            break;
            
        case AST_EXPRESSION_STATEMENT:
            if (stmt->child_count > 0) {
                generate_expression(gen, stmt->children[0]);
                fprintf(gen->output, ";\n");
            }
            break;
            
        case AST_PRINT_STATEMENT:
            // Generate printf call with all arguments
            if (stmt->child_count > 0) {
                ASTNode* first_arg = stmt->children[0];

                // Check if we have a single typed argument (not a string literal)
                if (stmt->child_count == 1 && first_arg->node_type &&
                    !(first_arg->type == AST_LITERAL && first_arg->node_type->kind == TYPE_STRING)) {

                    Type* arg_type = first_arg->node_type;

                    // Generate printf with appropriate format string based on type
                    if (arg_type->kind == TYPE_INT) {
                        fprintf(gen->output, "printf(\"%%d\", ");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, ");\n");
                    } else if (arg_type->kind == TYPE_FLOAT) {
                        fprintf(gen->output, "printf(\"%%f\", ");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, ");\n");
                    } else if (arg_type->kind == TYPE_STRING) {
                        fprintf(gen->output, "printf(\"%%s\", ");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, ");\n");
                    } else if (arg_type->kind == TYPE_BOOL) {
                        fprintf(gen->output, "printf(\"%%s\", ");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, " ? \"true\" : \"false\");\n");
                    } else {
                        // Unknown type - default to %d
                        fprintf(gen->output, "printf(\"%%d\", ");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, ");\n");
                    }
                } else if (stmt->child_count == 1) {
                    // String literal - print directly
                    ASTNode* arg = stmt->children[0];
                    if (arg->type == AST_LITERAL && arg->node_type && arg->node_type->kind == TYPE_STRING) {
                        fprintf(gen->output, "printf(");
                        generate_expression(gen, arg);
                        fprintf(gen->output, ");\n");
                    } else {
                        // Unknown type - default to %d
                        fprintf(gen->output, "printf(\"%%d\", ");
                        generate_expression(gen, arg);
                        fprintf(gen->output, ");\n");
                    }
                } else {
                    // Multiple arguments - first is format string
                    fprintf(gen->output, "printf(");
                    generate_expression(gen, stmt->children[0]);
                    for (int i = 1; i < stmt->child_count; i++) {
                        fprintf(gen->output, ", ");
                        generate_expression(gen, stmt->children[i]);
                    }
                    fprintf(gen->output, ");\n");
                }
            }
            break;
            
        case AST_SEND_STATEMENT:
            // Note: Generic send() syntax not yet implemented
            // Use type-specific send_ActorName() functions generated for each actor
            fprintf(stderr, "Error: Generic send() not supported. Use send_ActorName() functions.\n");
            fprintf(gen->output, "/* ERROR: Generic send() not supported - use type-specific send functions */\n");
            break;
            
        case AST_SPAWN_ACTOR_STATEMENT:
            // Note: Generic spawn_actor() syntax not yet implemented  
            // Use type-specific spawn_ActorName() functions generated for each actor
            fprintf(stderr, "Error: Generic spawn_actor() not supported. Use spawn_ActorName() functions.\n");
            fprintf(gen->output, "/* ERROR: Generic spawn_actor() not supported - use type-specific spawn functions */\n");
            break;
            
        case AST_BLOCK:
            print_line(gen, "{");
            indent(gen);
            for (int i = 0; i < stmt->child_count; i++) {
                generate_statement(gen, stmt->children[i]);
            }
            unindent(gen);
            print_line(gen, "}");
            break;
        
        // Actor V2 - Reply statement
        case AST_REPLY_STATEMENT:
            if (stmt->child_count > 0) {
                ASTNode* reply_expr = stmt->children[0];
                
                if (reply_expr->type == AST_MESSAGE_CONSTRUCTOR) {
                    MessageDef* msg_def = lookup_message(gen->message_registry, reply_expr->value);
                    if (msg_def) {
                        print_indent(gen);
                        fprintf(gen->output, "{ %s _reply = { ._message_id = %d", 
                                reply_expr->value, msg_def->message_id);
                        
                        for (int i = 0; i < reply_expr->child_count; i++) {
                            ASTNode* field_init = reply_expr->children[i];
                            if (field_init && field_init->type == AST_FIELD_INIT) {
                                fprintf(gen->output, ", .%s = ", field_init->value);
                                if (field_init->child_count > 0) {
                                    generate_expression(gen, field_init->children[0]);
                                }
                            }
                        }
                        
                        fprintf(gen->output, " }; aether_reply(self, &_reply, sizeof(%s)); }\n", 
                                reply_expr->value);
                    } else {
                        print_line(gen, "/* ERROR: unknown reply message type %s */", reply_expr->value);
                    }
                }
            }
            break;
            
        default:
            for (int i = 0; i < stmt->child_count; i++) {
                generate_statement(gen, stmt->children[i]);
            }
            break;
    }
}

void generate_actor_definition(CodeGenerator* gen, ASTNode* actor) {
    if (!actor || actor->type != AST_ACTOR_DEFINITION) return;
    
    gen->current_actor = strdup(actor->value);
    gen->state_var_count = 0;
    gen->actor_state_vars = NULL;
    
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_STATE_DECLARATION) {
            gen->state_var_count++;
            gen->actor_state_vars = realloc(gen->actor_state_vars, 
                                           gen->state_var_count * sizeof(char*));
            gen->actor_state_vars[gen->state_var_count - 1] = strdup(child->value);
        }
    }
    
    // Generate cache-aligned actor struct with optimized field layout
    print_line(gen, "typedef struct __attribute__((aligned(64))) %s {", actor->value);
    indent(gen);
    
    // Hot fields (accessed every message) - first cache line
    print_line(gen, "int active;              // Hot: checked every loop iteration");
    print_line(gen, "int id;                  // Hot: used for identification");
    print_line(gen, "Mailbox mailbox;         // Hot: message queue");
    print_line(gen, "void (*step)(void*);     // Hot: message handler");
    
    // Warm fields (accessed occasionally)
    print_line(gen, "pthread_t thread;        // Warm: thread handle");
    print_line(gen, "int auto_process;        // Warm: auto-processing flag");
    print_line(gen, "int assigned_core;       // Cold: core assignment");
    print_line(gen, "SPSCQueue spsc_queue;    // Lock-free same-core messaging");
    print_line(gen, "");

    // State fields (user-defined)
    // NOTE: All state fields are atomic to allow safe cross-thread access
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_STATE_DECLARATION) {
            print_indent(gen);
            // Check if field name ends with "_ref" - these are actor references stored as void*
            size_t name_len = strlen(child->value);
            if (name_len > 4 && strcmp(child->value + name_len - 4, "_ref") == 0) {
                fprintf(gen->output, "void* %s;\n", child->value);
            } else {
                // Use atomic types for int to enable safe concurrent access
                if (child->node_type && child->node_type->kind == TYPE_INT) {
                    fprintf(gen->output, "atomic_int %s;\n", child->value);
                } else {
                    generate_type(gen, child->node_type);
                    fprintf(gen->output, " %s;\n", child->value);
                }
            }        }
    }
    
    unindent(gen);
    print_line(gen, "} %s;", actor->value);
    print_line(gen, "");
    
    // Generate individual message handler functions
    int pattern_count = 0;
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_RECEIVE_STATEMENT && child->child_count > 0) {
            // V2 syntax: receive { Pattern -> body, ... }
            // V1 syntax: receive(msg) { body }
            for (int j = 0; j < child->child_count; j++) {
                ASTNode* arm = child->children[j];

                ASTNode* pattern = NULL;
                ASTNode* arm_body = NULL;

                // Check for V2 receive arm structure
                if (arm->type == AST_RECEIVE_ARM && arm->child_count >= 2) {
                    pattern = arm->children[0];
                    arm_body = arm->children[1];
                }
                // Check for V1 BLOCK containing MESSAGE_PATTERN
                else if (arm->type == AST_BLOCK) {
                    for (int k = 0; k < arm->child_count; k++) {
                        if (arm->children[k]->type == AST_MESSAGE_PATTERN) {
                            pattern = arm->children[k];
                            // Find the body (last BLOCK child of pattern)
                            if (pattern->child_count > 0) {
                                ASTNode* last = pattern->children[pattern->child_count - 1];
                                if (last->type == AST_BLOCK) {
                                    arm_body = last;
                                }
                            }
                            break;
                        }
                    }
                }

                // Generate handler if we found a pattern
                if (pattern && pattern->type == AST_MESSAGE_PATTERN) {
                    MessageDef* msg_def = lookup_message(gen->message_registry, pattern->value);
                    if (msg_def) {
                        print_line(gen, "static __attribute__((hot)) void %s_handle_%s(%s* self, void* _msg_data) {",
                                  actor->value, pattern->value, actor->value);
                        indent(gen);
                        print_line(gen, "%s* _pattern = (%s*)_msg_data;", pattern->value, pattern->value);

                        // Extract pattern fields with correct types from message definition
                        for (int k = 0; k < pattern->child_count; k++) {
                            ASTNode* field = pattern->children[k];
                            if (field->type == AST_PATTERN_FIELD) {
                                const char* c_type = "int";
                                if (msg_def && msg_def->fields) {
                                    MessageFieldDef* fdef = msg_def->fields;
                                    while (fdef) {
                                        if (strcmp(fdef->name, field->value) == 0) {
                                            Type temp_type = { .kind = fdef->type_kind, .element_type = NULL, .array_size = 0, .struct_name = NULL };
                                            c_type = get_c_type(&temp_type);
                                            break;
                                        }
                                        fdef = fdef->next;
                                    }
                                }
                                print_line(gen, "%s %s = _pattern->%s;", c_type, field->value, field->value);
                            }
                        }

                        // Generate handler body
                        if (arm_body && arm_body->type == AST_BLOCK) {
                            for (int k = 0; k < arm_body->child_count; k++) {
                                generate_statement(gen, arm_body->children[k]);
                            }
                        }

                        unindent(gen);
                        print_line(gen, "}");
                        print_line(gen, "");
                        pattern_count++;
                    }
                }
            }
        }
    }
    
    // Generate function pointer table
    if (pattern_count > 0) {
        print_line(gen, "typedef void (*%s_MessageHandler)(%s*, void*);", actor->value, actor->value);
        print_line(gen, "static %s_MessageHandler %s_handlers[256] = {0};", actor->value, actor->value);
        print_line(gen, "static int %s_handlers_initialized = 0;", actor->value);
        print_line(gen, "");
        
        print_line(gen, "static void %s_init_handlers(%s* self) {", actor->value, actor->value);
        indent(gen);
        print_line(gen, "if (%s_handlers_initialized) return;", actor->value);
        
        for (int i = 0; i < actor->child_count; i++) {
            ASTNode* child = actor->children[i];
            if (child->type == AST_RECEIVE_STATEMENT && child->child_count > 0) {
                for (int j = 0; j < child->child_count; j++) {
                    ASTNode* arm = child->children[j];
                    ASTNode* pattern = NULL;

                    if (arm->type == AST_RECEIVE_ARM && arm->child_count >= 1) {
                        pattern = arm->children[0];
                    } else if (arm->type == AST_BLOCK) {
                        for (int k = 0; k < arm->child_count; k++) {
                            if (arm->children[k]->type == AST_MESSAGE_PATTERN) {
                                pattern = arm->children[k];
                                break;
                            }
                        }
                    }

                    if (pattern && pattern->type == AST_MESSAGE_PATTERN) {
                        MessageDef* msg_def = lookup_message(gen->message_registry, pattern->value);
                        if (msg_def) {
                            print_line(gen, "%s_handlers[%d] = %s_handle_%s;",
                                      actor->value, msg_def->message_id, actor->value, pattern->value);
                        }
                    }
                }
            }
        }
        
        print_line(gen, "%s_handlers_initialized = 1;", actor->value);
        unindent(gen);
        print_line(gen, "}");
        print_line(gen, "");
    }
    
    print_line(gen, "void %s_step(%s* self) {", actor->value, actor->value);
    indent(gen);
    print_line(gen, "Message msg;");
    print_line(gen, "");
    print_line(gen, "// Likely path: mailbox has message");
    print_line(gen, "if (__builtin_expect(!mailbox_receive(&self->mailbox, &msg), 0)) {");
    indent(gen);
    print_line(gen, "self->active = 0;");
    print_line(gen, "return;");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
    
    if (pattern_count > 0) {
        print_line(gen, "// COMPUTED GOTO DISPATCH - 15-30%% faster than function pointers");
        print_line(gen, "// Used by CPython, LuaJIT for ultra-fast message dispatch");
        print_line(gen, "void* _msg_data = msg.payload_ptr;");
        print_line(gen, "int _msg_id = msg.type;  // Already set by aether_send_message, avoids pointer dereference");
        print_line(gen, "");
        print_line(gen, "// Dispatch table: direct jumps to labels (no indirect call overhead)");
        print_line(gen, "static void* dispatch_table[256] = {");
        indent(gen);
        
        // Generate dispatch table with labels
        for (int i = 0; i < actor->child_count; i++) {
            ASTNode* child = actor->children[i];
            if (child->type == AST_RECEIVE_STATEMENT && child->child_count > 0) {
                for (int j = 0; j < child->child_count; j++) {
                    ASTNode* arm = child->children[j];
                    ASTNode* pattern = NULL;

                    // V2: AST_RECEIVE_ARM contains pattern
                    if (arm->type == AST_RECEIVE_ARM && arm->child_count >= 1) {
                        pattern = arm->children[0];
                    }
                    // V1: AST_BLOCK contains MESSAGE_PATTERN
                    else if (arm->type == AST_BLOCK) {
                        for (int k = 0; k < arm->child_count; k++) {
                            if (arm->children[k]->type == AST_MESSAGE_PATTERN) {
                                pattern = arm->children[k];
                                break;
                            }
                        }
                    }

                    if (pattern && pattern->type == AST_MESSAGE_PATTERN) {
                        MessageDef* msg_def = lookup_message(gen->message_registry, pattern->value);
                        if (msg_def) {
                            print_line(gen, "[%d] = &&handle_%s,", msg_def->message_id, pattern->value);
                        }
                    }
                }
            }
        }
        
        unindent(gen);
        print_line(gen, "};");
        print_line(gen, "");
        print_line(gen, "// Bounds check with likely hint (message IDs are usually valid)");
        print_line(gen, "if (__builtin_expect(_msg_id >= 0 && _msg_id < 256 && dispatch_table[_msg_id], 1)) {");
        indent(gen);
        print_line(gen, "goto *dispatch_table[_msg_id];  // Direct jump - zero overhead");
        unindent(gen);
        print_line(gen, "}");
        print_line(gen, "return;  // Unknown message type");
        print_line(gen, "");
        
        // Generate labels for each handler
        for (int i = 0; i < actor->child_count; i++) {
            ASTNode* child = actor->children[i];
            if (child->type == AST_RECEIVE_STATEMENT && child->child_count > 0) {
                for (int j = 0; j < child->child_count; j++) {
                    ASTNode* arm = child->children[j];
                    ASTNode* pattern = NULL;

                    // V2: AST_RECEIVE_ARM contains pattern
                    if (arm->type == AST_RECEIVE_ARM && arm->child_count >= 1) {
                        pattern = arm->children[0];
                    }
                    // V1: AST_BLOCK contains MESSAGE_PATTERN
                    else if (arm->type == AST_BLOCK) {
                        for (int k = 0; k < arm->child_count; k++) {
                            if (arm->children[k]->type == AST_MESSAGE_PATTERN) {
                                pattern = arm->children[k];
                                break;
                            }
                        }
                    }

                    if (pattern && pattern->type == AST_MESSAGE_PATTERN) {
                        MessageDef* msg_def = lookup_message(gen->message_registry, pattern->value);
                        const char* single_int = msg_def ? get_single_int_field(msg_def) : NULL;

                        print_line(gen, "handle_%s:", pattern->value);
                        indent(gen);
                        if (single_int) {
                            // Inline fast path: reconstruct struct on stack from msg fields.
                            // No pool buffer exists, no free needed.
                            print_line(gen, "if (_msg_data) {");
                            indent(gen);
                            print_line(gen, "%s_handle_%s(self, _msg_data);", actor->value, pattern->value);
                            print_line(gen, "aether_free_message(_msg_data);");
                            unindent(gen);
                            print_line(gen, "} else {");
                            indent(gen);
                            print_line(gen, "%s _inline = { ._message_id = msg.type, .%s = msg.payload_int };",
                                      pattern->value, single_int);
                            print_line(gen, "%s_handle_%s(self, &_inline);", actor->value, pattern->value);
                            unindent(gen);
                            print_line(gen, "}");
                        } else {
                            print_line(gen, "%s_handle_%s(self, _msg_data);", actor->value, pattern->value);
                            print_line(gen, "aether_free_message(_msg_data);  // Return to pool or free");
                        }
                        print_line(gen, "return;");
                        unindent(gen);
                        print_line(gen, "");
                    }
                }
            }
        }
    }
    
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
    
    print_line(gen, "%s* spawn_%s() {", actor->value, actor->value);
    indent(gen);
    print_line(gen, "int core = atomic_fetch_add(&next_actor_id, 1) %% num_cores;");
    print_line(gen, "%s* actor = (%s*)scheduler_spawn_pooled(core, (void (*)(void*))%s_step, sizeof(%s));",
               actor->value, actor->value, actor->value, actor->value);
    print_line(gen, "if (!actor) {");
    indent(gen);
    print_line(gen, "// Fallback to aligned allocation if pool exhausted");
    print_line(gen, "actor = aligned_alloc(64, sizeof(%s));", actor->value);
    print_line(gen, "if (!actor) return NULL;");
    print_line(gen, "actor->id = atomic_fetch_add(&next_actor_id, 1);");
    print_line(gen, "actor->assigned_core = -1;");
    print_line(gen, "actor->step = (void (*)(void*))%s_step;", actor->value);
    print_line(gen, "mailbox_init(&actor->mailbox);");
    print_line(gen, "scheduler_register_actor((ActorBase*)actor, -1);");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "actor->active = 1;");
    print_line(gen, "actor->auto_process = 0;");
    print_line(gen, "");
    
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_STATE_DECLARATION) {
            if (child->child_count > 0) {
                print_indent(gen);
                fprintf(gen->output, "actor->%s = ", child->value);
                generate_expression(gen, child->children[0]);
                fprintf(gen->output, ";\n");
            } else {
                print_line(gen, "actor->%s = 0;", child->value);
            }
        }
    }
    
    print_line(gen, "");
    print_line(gen, "if (actor->auto_process) {");
    indent(gen);
    print_line(gen, "pthread_create(&actor->thread, NULL, (void*(*)(void*))aether_actor_thread, actor);");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
    print_line(gen, "return actor;");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
    
    print_line(gen, "void send_%s(%s* actor, int type, int payload) {", actor->value, actor->value);
    indent(gen);
    print_line(gen, "Message msg = {type, 0, payload, NULL};");
    print_line(gen, "if (actor->assigned_core == current_core_id) {");
    indent(gen);
    print_line(gen, "scheduler_send_local((ActorBase*)actor, msg);");
    unindent(gen);
    print_line(gen, "} else {");
    indent(gen);
    print_line(gen, "scheduler_send_remote((ActorBase*)actor, msg, current_core_id);");
    unindent(gen);
    print_line(gen, "}");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
    
    if (gen->current_actor) free(gen->current_actor);
    gen->current_actor = NULL;
    if (gen->actor_state_vars) {
        for (int i = 0; i < gen->state_var_count; i++) {
            free(gen->actor_state_vars[i]);
        }
        free(gen->actor_state_vars);
        gen->actor_state_vars = NULL;
    }
    gen->state_var_count = 0;
    gen->actor_count++;
}

// Check if an AST subtree contains a return statement with a value
static int has_return_value(ASTNode* node) {
    if (!node) return 0;
    if (node->type == AST_RETURN_STATEMENT && node->child_count > 0 && node->children[0]) {
        return 1;
    }
    for (int i = 0; i < node->child_count; i++) {
        if (has_return_value(node->children[i])) return 1;
    }
    return 0;
}

// Generate extern C function declaration
// extern printf(format: string) -> int  =>  extern int printf(const char*);
void generate_extern_declaration(CodeGenerator* gen, ASTNode* ext) {
    if (!ext || ext->type != AST_EXTERN_FUNCTION) return;

    fprintf(gen->output, "// Extern C function: %s\n", ext->value);

    // Generate return type (map Aether types to C types)
    if (ext->node_type && ext->node_type->kind != TYPE_VOID) {
        switch (ext->node_type->kind) {
            case TYPE_STRING:
                fprintf(gen->output, "const char*");
                break;
            case TYPE_FLOAT:
                fprintf(gen->output, "double");  // C uses double by default
                break;
            case TYPE_PTR:
                fprintf(gen->output, "void*");
                break;
            case TYPE_BOOL:
                fprintf(gen->output, "int");
                break;
            default:
                generate_type(gen, ext->node_type);
                break;
        }
    } else {
        fprintf(gen->output, "void");
    }

    fprintf(gen->output, " %s(", ext->value);

    // Generate parameters
    int first_param = 1;
    for (int i = 0; i < ext->child_count; i++) {
        ASTNode* param = ext->children[i];
        if (param->type == AST_IDENTIFIER) {
            if (!first_param) fprintf(gen->output, ", ");
            first_param = 0;

            // Map Aether types to C types
            if (param->node_type) {
                switch (param->node_type->kind) {
                    case TYPE_STRING:
                        fprintf(gen->output, "const char*");
                        break;
                    case TYPE_FLOAT:
                        fprintf(gen->output, "double");
                        break;
                    case TYPE_PTR:
                        fprintf(gen->output, "void*");
                        break;
                    case TYPE_BOOL:
                        fprintf(gen->output, "int");
                        break;
                    default:
                        generate_type(gen, param->node_type);
                        break;
                }
            } else {
                fprintf(gen->output, "int");
            }
        }
    }

    fprintf(gen->output, ");\n\n");
}

void generate_function_definition(CodeGenerator* gen, ASTNode* func) {
    if (!func || func->type != AST_FUNCTION_DEFINITION) return;

    // Determine return type: if type is void but function has return-with-value, use int
    Type* ret_type = func->node_type;
    if ((!ret_type || ret_type->kind == TYPE_VOID || ret_type->kind == TYPE_UNKNOWN) && has_return_value(func)) {
        fprintf(gen->output, "int");
    } else {
        generate_type(gen, ret_type);
    }
    fprintf(gen->output, " %s(", func->value);

    // Generate parameters - handle pattern matching
    int param_count = 0;
    ASTNode* body = NULL;
    
    for (int i = 0; i < func->child_count; i++) {
        ASTNode* child = func->children[i];
        
        if (child->type == AST_GUARD_CLAUSE) {
            // has_guards = 1;  // Reserved for future optimization
            continue;
        }
        
        if (child->type == AST_BLOCK) {
            body = child;
            continue;
        }
        
        // Handle different parameter pattern types
        if (child->type == AST_PATTERN_VARIABLE || 
            child->type == AST_VARIABLE_DECLARATION) {
            if (param_count > 0) fprintf(gen->output, ", ");
            generate_type(gen, child->node_type);
            fprintf(gen->output, " %s", child->value);
            param_count++;
        } else if (child->type == AST_PATTERN_LITERAL) {
            // Pattern literal becomes regular parameter
            if (param_count > 0) fprintf(gen->output, ", ");
            generate_type(gen, child->node_type);
            fprintf(gen->output, " _pattern_%d", param_count);
            param_count++;
        } else if (child->type == AST_PATTERN_STRUCT) {
            if (param_count > 0) fprintf(gen->output, ", ");
            fprintf(gen->output, "%s _pattern_%d", child->value, param_count);
            param_count++;
        } else if (child->type == AST_PATTERN_LIST || child->type == AST_PATTERN_CONS) {
            // List pattern becomes array pointer
            if (param_count > 0) fprintf(gen->output, ", ");
            fprintf(gen->output, "int* _list_%d, int _len_%d", param_count, param_count);
            // has_list_patterns = 1;  // Reserved for future optimization
            param_count++;
        }
    }

    fprintf(gen->output, ") {\n");
    indent(gen);
    clear_declared_vars(gen);  // Reset for each function
    
    // Generate pattern matching checks
    int pattern_idx = 0;
    int list_idx = 0;
    
    for (int i = 0; i < func->child_count; i++) {
        ASTNode* child = func->children[i];
        
        if (child->type == AST_PATTERN_LITERAL && 
            strcmp(child->value, "_") != 0) {
            // Generate pattern match check
            print_indent(gen);
            fprintf(gen->output, "if (_pattern_%d != %s) return ", 
                    pattern_idx, child->value);
            generate_type(gen, func->node_type);
            fprintf(gen->output, ";\n");
        }
        
        // Generate list pattern checks
        if (child->type == AST_PATTERN_LIST) {
            if (strcmp(child->value, "[]") == 0 && child->child_count == 0) {
                // Empty list check
                print_indent(gen);
                fprintf(gen->output, "if (_len_%d != 0) return ", list_idx);
                generate_type(gen, func->node_type);
                fprintf(gen->output, ";\n");
            } else {
                // Fixed-size list check
                print_indent(gen);
                fprintf(gen->output, "if (_len_%d != %d) return ", 
                        list_idx, child->child_count);
                generate_type(gen, func->node_type);
                fprintf(gen->output, ";\n");
                
                // Bind pattern variables to list elements
                for (int j = 0; j < child->child_count; j++) {
                    ASTNode* elem = child->children[j];
                    if (elem->type == AST_PATTERN_VARIABLE) {
                        print_indent(gen);
                        fprintf(gen->output, "int %s = _list_%d[%d];\n", 
                                elem->value, list_idx, j);
                    }
                }
            }
            list_idx++;
        } else if (child->type == AST_PATTERN_CONS) {
            // [H|T] pattern - check non-empty
            print_indent(gen);
            fprintf(gen->output, "if (_len_%d < 1) return ", list_idx);
            generate_type(gen, func->node_type);
            fprintf(gen->output, ";\n");
            
            // Bind head and tail
            if (child->child_count >= 1 && child->children[0]->type == AST_PATTERN_VARIABLE) {
                print_indent(gen);
                fprintf(gen->output, "int %s = _list_%d[0];\n", 
                        child->children[0]->value, list_idx);
            }
            if (child->child_count >= 2 && child->children[1]->type == AST_PATTERN_VARIABLE) {
                print_indent(gen);
                fprintf(gen->output, "int* %s = &_list_%d[1];\n", 
                        child->children[1]->value, list_idx);
                print_indent(gen);
                fprintf(gen->output, "int %s_len = _len_%d - 1;\n",
                        child->children[1]->value, list_idx);
            }
            list_idx++;
        }
        
        if (child->type == AST_PATTERN_LITERAL ||
            child->type == AST_PATTERN_VARIABLE ||
            child->type == AST_PATTERN_STRUCT ||
            child->type == AST_VARIABLE_DECLARATION) {
            pattern_idx++;
        }
        
        // Generate guard clause check
        if (child->type == AST_GUARD_CLAUSE && child->child_count > 0) {
            print_indent(gen);
            fprintf(gen->output, "if (!(");
            generate_expression(gen, child->children[0]);
            fprintf(gen->output, ")) return ");
            generate_type(gen, func->node_type);
            fprintf(gen->output, ";\n");
        }
    }
    
    // Generate body
    if (body) {
        generate_statement(gen, body);
    }
    
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
}

void generate_struct_definition(CodeGenerator* gen, ASTNode* struct_def) {
    if (!struct_def || struct_def->type != AST_STRUCT_DEFINITION) return;
    
    // Generate C struct
    print_line(gen, "typedef struct %s {", struct_def->value);
    indent(gen);
    
    // Generate fields
    for (int i = 0; i < struct_def->child_count; i++) {
        ASTNode* field = struct_def->children[i];
        if (field->type == AST_STRUCT_FIELD) {
            print_indent(gen);
            
            // Handle array types specially
            if (field->node_type && field->node_type->kind == TYPE_ARRAY) {
                // For arrays: type fieldname[size];
                const char* element_type = get_c_type(field->node_type->element_type);
                if (field->node_type->array_size > 0) {
                    fprintf(gen->output, "%s %s[%d];\n", element_type, field->value, field->node_type->array_size);
                } else {
                    fprintf(gen->output, "%s* %s;\n", element_type, field->value);
                }
            } else {
                // For non-arrays: type fieldname;
                generate_type(gen, field->node_type);
                fprintf(gen->output, " %s;\n", field->value);
            }
        }
    }
    
    unindent(gen);
    print_line(gen, "} %s;", struct_def->value);
    print_line(gen, "");
}

void generate_main_function(CodeGenerator* gen, ASTNode* main) {
    if (!main || main->type != AST_MAIN_FUNCTION) return;

    print_line(gen, "int main(int argc, char** argv) {");
    indent(gen);
    clear_declared_vars(gen);  // Reset for main function

    // Initialize command-line arguments
    print_line(gen, "aether_args_init(argc, argv);");
    print_line(gen, "");

    // Add timing if we have actors (for benchmarking)
    if (gen->actor_count > 0) {
        print_line(gen, "uint64_t _bench_start = rdtsc();");
        print_line(gen, "");
    }

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
        print_line(gen, "current_core_id = 0;");
        print_line(gen, "");
    }
    
    if (main->child_count > 0) {
        generate_statement(gen, main->children[0]);
    }
    
    // Clean up scheduler
    if (gen->actor_count > 0) {
        print_line(gen, "");
        print_line(gen, "// Wait for actors to complete and clean up");
        print_line(gen, "scheduler_stop();");
        print_line(gen, "scheduler_wait();");
        print_line(gen, "");
        print_line(gen, "// Output benchmark results");
        print_line(gen, "uint64_t _bench_end = rdtsc();");
        print_line(gen, "uint64_t _bench_cycles = _bench_end - _bench_start;");
        print_line(gen, "#if defined(__x86_64__) || defined(__i386__)");
        print_line(gen, "double cycles_per_msg = (double)_bench_cycles / (10000000 * 2);");
        print_line(gen, "double cpu_freq_ghz = 3.0;  // Approximate CPU frequency");
        print_line(gen, "double seconds = cycles_per_msg * (10000000 * 2) / (cpu_freq_ghz * 1e9);");
        print_line(gen, "double throughput = (2.0 * 10000000) / seconds;");
        print_line(gen, "printf(\"\\nCycles/msg:     %%.2f\\n\", cycles_per_msg);");
        print_line(gen, "printf(\"Throughput:     %%.2f M msg/sec\\n\", throughput / 1e6);");
        print_line(gen, "#elif defined(__aarch64__) || defined(__arm__)");
        print_line(gen, "double seconds = _bench_cycles / 1e9;");
        print_line(gen, "double throughput = (2.0 * 10000000) / seconds;");
        print_line(gen, "double cycles_per_msg = _bench_cycles / (10000000 * 2.0);");
        print_line(gen, "printf(\"\\nCycles/msg:     %%.2f\\n\", cycles_per_msg);");
        print_line(gen, "printf(\"Throughput:     %%.2f M msg/sec\\n\", throughput / 1e6);");
        print_line(gen, "#endif");
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

    print_line(gen, "return 0;");
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
                // Import statement: comment only (module system pending)
                if (child->value) {
                    const char* alias = NULL;
                    if (child->child_count > 0) {
                        ASTNode* last = child->children[child->child_count - 1];
                        if (last && last->type == AST_IDENTIFIER) {
                            alias = last->value;
                        }
                    }

                    if (alias) {
                        print_line(gen, "// Import: %s as %s", child->value, alias);
                    } else {
                        print_line(gen, "// Import: %s", child->value);
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
                            generate_function_definition(gen, exported);
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
                generate_function_definition(gen, child);
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
