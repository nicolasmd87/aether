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
    return gen;
}

void free_code_generator(CodeGenerator* gen) {
    if (gen) {
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
        case TYPE_STRING: return "char*";
        case TYPE_VOID: return "void";
        case TYPE_ACTOR_REF: return "ActorRef*";
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
            fprintf(gen->output, "%s", expr->value);
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
            fprintf(gen->output, "%s(", expr->value);
            for (int i = 0; i < expr->child_count; i++) {
                if (i > 0) fprintf(gen->output, ", ");
                generate_expression(gen, expr->children[i]);
            }
            fprintf(gen->output, ")");
            break;
            
        case AST_ACTOR_REF:
            if (strcmp(expr->value, "self") == 0) {
                fprintf(gen->output, "aether_self()");
            } else {
                fprintf(gen->output, "%s", expr->value);
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
    
    // Debug: Print statement type being generated
    static int depth = 0;
    static int call_count = 0;
    call_count++;
    depth++;
    if (call_count > 1000) {
        fprintf(stderr, "ERROR: generate_statement called too many times (possible infinite loop)\n");
        fprintf(stderr, "Current statement type: %d, depth: %d\n", stmt->type, depth);
        exit(1);
    }
    
    switch (stmt->type) {
        case AST_VARIABLE_DECLARATION: {
            generate_type(gen, stmt->node_type);
            fprintf(gen->output, " %s", stmt->value);
            
            if (stmt->child_count > 0) {
                fprintf(gen->output, " = ");
                generate_expression(gen, stmt->children[0]);
            }
            
            fprintf(gen->output, ";\n");
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
            
        case AST_EXPRESSION_STATEMENT:
            if (stmt->child_count > 0) {
                generate_expression(gen, stmt->children[0]);
                fprintf(gen->output, ";\n");
            }
            break;
            
        case AST_PRINT_STATEMENT:
            fprintf(gen->output, "printf(");
            for (int i = 0; i < stmt->child_count; i++) {
                if (i > 0) fprintf(gen->output, ", ");
                generate_expression(gen, stmt->children[i]);
            }
            fprintf(gen->output, ");\n");
            break;
            
        case AST_SEND_STATEMENT:
            fprintf(gen->output, "aether_send_message(");
            if (stmt->child_count >= 2) {
                generate_expression(gen, stmt->children[0]);
                fprintf(gen->output, ", aether_self(), ");
                generate_expression(gen, stmt->children[1]);
                fprintf(gen->output, ", sizeof(");
                generate_expression(gen, stmt->children[1]);
                fprintf(gen->output, ")");
            }
            fprintf(gen->output, ");\n");
            break;
            
        case AST_SPAWN_ACTOR_STATEMENT:
            fprintf(gen->output, "ActorRef* actor_%d = aether_spawn_actor(\"", gen->actor_count++);
            if (stmt->child_count > 0) {
                generate_expression(gen, stmt->children[0]);
            }
            fprintf(gen->output, "\", NULL, NULL);\n");
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
            // Generate all children
            fprintf(stderr, "[DEBUG] generate_statement: Unknown type %d, generating %d children\n", stmt->type, stmt->child_count);
            for (int i = 0; i < stmt->child_count; i++) {
                generate_statement(gen, stmt->children[i]);
            }
            break;
    }
    
    depth--;
}

void generate_actor_definition(CodeGenerator* gen, ASTNode* actor) {
    if (!actor || actor->type != AST_ACTOR_DEFINITION) return;
    
    // Generate actor struct
    print_line(gen, "typedef struct %s {", actor->value);
    indent(gen);
    
    // Generate state variables
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_STATE_DECLARATION) {
            generate_type(gen, child->node_type);
            fprintf(gen->output, " %s;\n", child->value);
        }
    }
    
    unindent(gen);
    print_line(gen, "} %s;", actor->value);
    print_line(gen, "");
    
    // Generate actor receive function
    fprintf(gen->output, "void %s_receive(Actor* self, Message* msg) {\n", actor->value);
    indent(gen);
    print_line(gen, "%s* state = (%s*)self->user_data;", actor->value, actor->value);
    print_line(gen, "// Actor receive logic would go here");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
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

void generate_main_function(CodeGenerator* gen, ASTNode* main) {
    if (!main || main->type != AST_MAIN_FUNCTION) return;
    
    print_line(gen, "int main() {");
    indent(gen);
    print_line(gen, "aether_runtime_init(4); // Initialize with 4 worker threads");
    print_line(gen, "");
    
    if (main->child_count > 0) {
        generate_statement(gen, main->children[0]); // main body
    }
    
    print_line(gen, "");
    print_line(gen, "aether_runtime_shutdown();");
    print_line(gen, "return 0;");
    unindent(gen);
    print_line(gen, "}");
}

void generate_program(CodeGenerator* gen, ASTNode* program) {
    if (!program || program->type != AST_PROGRAM) {
        fprintf(stderr, "[DEBUG] generate_program: Invalid program node\n");
        return;
    }
    
    fprintf(stderr, "[DEBUG] generate_program: Processing %d top-level nodes\n", program->child_count);
    
    // Generate includes
    print_line(gen, "#include <stdio.h>");
    print_line(gen, "#include <stdlib.h>");
    print_line(gen, "#include <string.h>");
    print_line(gen, "#include \"aether_runtime.h\"");
    print_line(gen, "");

    // Generate all top-level definitions
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (!child) {
            fprintf(stderr, "[DEBUG] generate_program: Child %d is NULL\n", i);
            continue;
        }
        
        fprintf(stderr, "[DEBUG] generate_program: Processing child %d, type=%d\n", i, child->type);
        
        switch (child->type) {
            case AST_ACTOR_DEFINITION:
                fprintf(stderr, "[DEBUG] Generating actor definition\n");
                generate_actor_definition(gen, child);
                break;
            case AST_FUNCTION_DEFINITION:
                fprintf(stderr, "[DEBUG] Generating function definition\n");
                generate_function_definition(gen, child);
                break;
            case AST_MAIN_FUNCTION:
                fprintf(stderr, "[DEBUG] Generating main function\n");
                generate_main_function(gen, child);
                break;
            default:
                fprintf(stderr, "[DEBUG] Unknown child type: %d\n", child->type);
                break;
        }
        
        fprintf(stderr, "[DEBUG] Finished processing child %d\n", i);
    }
    
    fprintf(stderr, "[DEBUG] generate_program: Complete\n");
}
