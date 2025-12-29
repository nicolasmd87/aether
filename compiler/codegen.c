#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen.h"

CodeGenerator* create_code_generator(FILE* output) {
    CodeGenerator* gen = malloc(sizeof(CodeGenerator));
    gen->output = output;
    gen->indent_level = 0;
    gen->actor_count = 0;
    gen->function_count = 0;
    gen->current_actor = NULL;
    gen->actor_state_vars = NULL;
    gen->state_var_count = 0;
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
        free(gen);
    }
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

void print_line(CodeGenerator* gen, const char* format, ...) {
    print_indent(gen);
    
    va_list args;
    va_start(args, format);
    vfprintf(gen->output, format, args);
    va_end(args);
    
    fprintf(gen->output, "\n");
}

const char* get_c_type(Type* type) {
    if (!type) return "void";
    
    switch (type->kind) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "int";
        case TYPE_STRING: return "AetherString*";  // Use runtime string type (pointer)
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
        default: return "void";
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
                // String literal: generate aether_string_from_literal() call
                fprintf(gen->output, "aether_string_from_literal(\"");
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
                fprintf(gen->output, "\")");
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
            // expr.field becomes expr.field in C
            if (expr->child_count > 0) {
                generate_expression(gen, expr->children[0]);
                fprintf(gen->output, ".%s", expr->value);
            }
            break;
            
        case AST_BINARY_EXPRESSION:
            if (expr->child_count >= 2) {
                fprintf(gen->output, "(");
                generate_expression(gen, expr->children[0]);
                fprintf(gen->output, " %s ", get_c_operator(expr->value));
                generate_expression(gen, expr->children[1]);
                fprintf(gen->output, ")");
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
                } else {
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
            
        default:
            // Generate all children
            for (int i = 0; i < expr->child_count; i++) {
                generate_expression(gen, expr->children[i]);
            }
            break;
    }
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
                // Handle array types specially (C syntax: int name[size])
                if (stmt->node_type && stmt->node_type->kind == TYPE_ARRAY) {
                    const char* elem_type = get_c_type(stmt->node_type->element_type);
                    fprintf(gen->output, "%s %s", elem_type, stmt->value);
                    if (stmt->node_type->array_size > 0) {
                        fprintf(gen->output, "[%d]", stmt->node_type->array_size);
                    } else {
                        // Dynamic array - use pointer
                        fprintf(gen->output, "*");
                    }
                } else {
                    generate_type(gen, stmt->node_type);
                    fprintf(gen->output, " %s", stmt->value);
                }
                
                if (stmt->child_count > 0) {
                    fprintf(gen->output, " = ");
                    generate_expression(gen, stmt->children[0]);
                }
                
                fprintf(gen->output, ";\n");
            }
            break;
        }
        
        case AST_ASSIGNMENT:
            if (stmt->child_count >= 2) {
                generate_expression(gen, stmt->children[0]);
                fprintf(gen->output, " = ");
                generate_expression(gen, stmt->children[1]);
                fprintf(gen->output, ";\n");
            }
            break;
            
        case AST_IF_STATEMENT:
            fprintf(gen->output, "if (");
            if (stmt->child_count > 0) {
                generate_expression(gen, stmt->children[0]);
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
                generate_expression(gen, stmt->children[0]);
            }
            fprintf(gen->output, ") {\n");
            
            indent(gen);
            if (stmt->child_count > 1) {
                generate_statement(gen, stmt->children[1]);
            }
            unindent(gen);
            
            print_line(gen, "}");
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
            if (strcmp(stmt->value, "default") == 0) {
                print_line(gen, "default:");
            } else {
                fprintf(gen->output, "case ");
                if (stmt->child_count > 0) {
                    generate_expression(gen, stmt->children[0]);
                }
                fprintf(gen->output, ":\n");
            }
            
            indent(gen);
            if (stmt->child_count > 1) {
                generate_statement(gen, stmt->children[1]);
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
            // Generate type-appropriate printf calls
            for (int i = 0; i < stmt->child_count; i++) {
                ASTNode* expr = stmt->children[i];
                if (expr->node_type) {
                    switch (expr->node_type->kind) {
                        case TYPE_INT:
                            fprintf(gen->output, "printf(\"%%d\\n\", ");
                            generate_expression(gen, expr);
                            fprintf(gen->output, ");\n");
                            break;
                        case TYPE_FLOAT:
                            fprintf(gen->output, "printf(\"%%f\\n\", ");
                            generate_expression(gen, expr);
                            fprintf(gen->output, ");\n");
                            break;
                        case TYPE_BOOL:
                            fprintf(gen->output, "printf(\"%%s\\n\", ");
                            generate_expression(gen, expr);
                            fprintf(gen->output, " ? \"true\" : \"false\");\n");
                            break;
                        case TYPE_STRING:
                            fprintf(gen->output, "printf(\"%%s\\n\", ");
                            generate_expression(gen, expr);
                            fprintf(gen->output, "->data);\n");  // AetherString* has data field
                            break;
                        default:
                            fprintf(gen->output, "printf(\"[value]\\n\");\n");
                            break;
                    }
                } else {
                    fprintf(gen->output, "printf(\"[unknown]\\n\");\n");
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
    
    print_line(gen, "typedef struct %s {", actor->value);
    indent(gen);
    
    print_line(gen, "int id;");
    print_line(gen, "int active;");
    print_line(gen, "int assigned_core;");
    print_line(gen, "Mailbox mailbox;");
    print_line(gen, "void (*step)(void*);");
    print_line(gen, "");
    
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_STATE_DECLARATION) {
            print_indent(gen);
            generate_type(gen, child->node_type);
            fprintf(gen->output, " %s;\n", child->value);
        }
    }
    
    unindent(gen);
    print_line(gen, "} %s;", actor->value);
    print_line(gen, "");
    
    print_line(gen, "void %s_step(%s* self) {", actor->value, actor->value);
    indent(gen);
    print_line(gen, "Message msg;");
    print_line(gen, "");
    print_line(gen, "if (!mailbox_receive(&self->mailbox, &msg)) {");
    indent(gen);
    print_line(gen, "self->active = 0;");
    print_line(gen, "return;");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
    
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_RECEIVE_STATEMENT) {
            if (child->child_count > 0) {
                ASTNode* body = child->children[0];
                if (body->type == AST_BLOCK) {
                    for (int j = 0; j < body->child_count; j++) {
                        ASTNode* stmt = body->children[j];
                        print_indent(gen);
                        if (stmt->type == AST_EXPRESSION_STATEMENT && stmt->child_count > 0) {
                            generate_expression(gen, stmt->children[0]);
                            fprintf(gen->output, ";\n");
                        } else {
                            generate_statement(gen, stmt);
                        }
                    }
                } else {
                    generate_statement(gen, body);
                }
            }
        }
    }
    
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
    
    print_line(gen, "%s* spawn_%s() {", actor->value, actor->value);
    indent(gen);
    print_line(gen, "%s* actor = malloc(sizeof(%s));", actor->value, actor->value);
    print_line(gen, "actor->id = atomic_fetch_add(&next_actor_id, 1);");
    print_line(gen, "actor->active = 1;");
    print_line(gen, "actor->assigned_core = -1;");
    print_line(gen, "actor->step = (void (*)(void*))%s_step;", actor->value);
    print_line(gen, "mailbox_init(&actor->mailbox);");
    
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
    
    print_line(gen, "scheduler_register_actor((ActorBase*)actor, -1);");
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

void generate_function_definition(CodeGenerator* gen, ASTNode* func) {
    if (!func || func->type != AST_FUNCTION_DEFINITION) return;

    generate_type(gen, func->node_type);
    fprintf(gen->output, " %s(", func->value);

    // Generate parameters
    int param_count = 0;
    for (int i = 0; i < func->child_count - 1; i++) { // Last child is body
        ASTNode* param = func->children[i];
        if (param->type == AST_VARIABLE_DECLARATION) {
            if (param_count > 0) fprintf(gen->output, ", ");
            generate_type(gen, param->node_type);
            fprintf(gen->output, " %s", param->value);
            param_count++;
        }
    }

    fprintf(gen->output, ") {\n");

    indent(gen);
    if (func->child_count > 0) {
        generate_statement(gen, func->children[func->child_count - 1]); // body
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
    
    print_line(gen, "int main() {");
    indent(gen);
    
    // Initialize scheduler if actors were defined
    if (gen->actor_count > 0) {
        print_line(gen, "scheduler_init(1);  // Single-threaded for now");
        print_line(gen, "current_core_id = 0;");
        print_line(gen, "");
    }
    
    if (main->child_count > 0) {
        generate_statement(gen, main->children[0]);
    }
    
    print_line(gen, "return 0;");
    unindent(gen);
    print_line(gen, "}");
}

void generate_program(CodeGenerator* gen, ASTNode* program) {
    if (!program || program->type != AST_PROGRAM) return;
    
    // Generate includes for runtime libraries
    print_line(gen, "#include <stdio.h>");
    print_line(gen, "#include <stdlib.h>");
    print_line(gen, "#include <string.h>");
    print_line(gen, "#include <stdbool.h>");
    print_line(gen, "#include <stdatomic.h>");
    print_line(gen, "");
    print_line(gen, "// Aether runtime libraries");
    print_line(gen, "#include \"actor_state_machine.h\"");
    print_line(gen, "#include \"multicore_scheduler.h\"");
    print_line(gen, "#include \"aether_string.h\"");
    print_line(gen, "#include \"aether_io.h\"");
    print_line(gen, "#include \"aether_math.h\"");
    print_line(gen, "#include \"aether_supervision.h\"");
    print_line(gen, "#include \"aether_tracing.h\"");
    print_line(gen, "#include \"aether_bounds_check.h\"");
    print_line(gen, "");
    print_line(gen, "extern __thread int current_core_id;");
    print_line(gen, "");
    
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (!child) continue;
        
        switch (child->type) {
            case AST_ACTOR_DEFINITION:
                generate_actor_definition(gen, child);
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
            default:
                break;
        }
    }
}
