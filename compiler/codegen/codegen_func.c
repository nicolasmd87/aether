#include "codegen_internal.h"

// Register an extern function's parameter types for call-site cast emission.
// Called whenever generate_extern_declaration() processes a function.
void register_extern_func(CodeGenerator* gen, ASTNode* ext) {
    if (!ext || !ext->value) return;

    // Grow registry if needed
    if (gen->extern_registry_count >= gen->extern_registry_capacity) {
        int new_cap = gen->extern_registry_capacity * 2 + 8;
        void* new_reg = realloc(gen->extern_registry,
            (size_t)new_cap * sizeof(*gen->extern_registry));
        if (!new_reg) return;  // OOM: skip registration
        gen->extern_registry = new_reg;
        gen->extern_registry_capacity = new_cap;
    }

    int idx = gen->extern_registry_count++;
    gen->extern_registry[idx].name = strdup(ext->value);
    gen->extern_registry[idx].param_count = ext->child_count;
    gen->extern_registry[idx].params = NULL;

    if (ext->child_count > 0) {
        gen->extern_registry[idx].params = malloc(ext->child_count * sizeof(TypeKind));
        for (int i = 0; i < ext->child_count; i++) {
            ASTNode* param = ext->children[i];
            if (param && param->node_type) {
                gen->extern_registry[idx].params[i] = param->node_type->kind;
            } else {
                gen->extern_registry[idx].params[i] = TYPE_UNKNOWN;
            }
        }
    }
}

// Check if a function name is registered as an extern function.
int is_extern_func(CodeGenerator* gen, const char* func_name) {
    if (!gen || !func_name) return 0;
    for (int i = 0; i < gen->extern_registry_count; i++) {
        if (gen->extern_registry[i].name && strcmp(gen->extern_registry[i].name, func_name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Look up the expected TypeKind for the nth parameter of an extern function.
// Returns TYPE_UNKNOWN if the function or parameter is not found.
TypeKind lookup_extern_param_kind(CodeGenerator* gen, const char* func_name, int param_idx) {
    for (int i = 0; i < gen->extern_registry_count; i++) {
        if (strcmp(gen->extern_registry[i].name, func_name) == 0) {
            if (param_idx < gen->extern_registry[i].param_count && gen->extern_registry[i].params) {
                return gen->extern_registry[i].params[param_idx];
            }
            return TYPE_UNKNOWN;
        }
    }
    return TYPE_UNKNOWN;
}

// Check if an AST subtree contains a return statement with a value
int has_return_value(ASTNode* node) {
    if (!node) return 0;
    if (node->type == AST_RETURN_STATEMENT && node->child_count > 0 && node->children[0]) {
        // Print statements don't count as "return values" - they're void
        if (node->children[0]->type == AST_PRINT_STATEMENT) {
            return 0;
        }
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

    // Register parameter types for call-site type-aware casting
    register_extern_func(gen, ext);

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

    fprintf(gen->output, " %s(", ext->value);  // No mangling: extern refers to actual C symbol

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
    fprintf(gen->output, " %s(", safe_c_name(func->value));

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
            // Determine element type from first child's node_type if available
            const char* elem_ctype = "int";
            if (child->child_count > 0 && child->children[0]->node_type &&
                child->children[0]->node_type->kind != TYPE_UNKNOWN) {
                elem_ctype = get_c_type(child->children[0]->node_type);
            }
            fprintf(gen->output, "%s* _list_%d, int _len_%d", elem_ctype, param_count, param_count);
            // has_list_patterns = 1;  // Reserved for future optimization
            param_count++;
        }
    }

    fprintf(gen->output, ") {\n");
    indent(gen);
    clear_declared_vars(gen);  // Reset for each function
    // Reset defer state for new function and enter function scope
    gen->defer_count = 0;
    gen->scope_depth = 0;
    enter_scope(gen);
    
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
            generate_default_return_value(gen, func->node_type);
            fprintf(gen->output, ";\n");
        }
        
        // Generate list pattern checks
        if (child->type == AST_PATTERN_LIST) {
            if (strcmp(child->value, "[]") == 0 && child->child_count == 0) {
                // Empty list check
                print_indent(gen);
                fprintf(gen->output, "if (_len_%d != 0) return ", list_idx);
                generate_default_return_value(gen, func->node_type);
                fprintf(gen->output, ";\n");
            } else {
                // Fixed-size list check
                print_indent(gen);
                fprintf(gen->output, "if (_len_%d != %d) return ",
                        list_idx, child->child_count);
                generate_default_return_value(gen, func->node_type);
                fprintf(gen->output, ";\n");
                
                // Bind pattern variables to list elements
                for (int j = 0; j < child->child_count; j++) {
                    ASTNode* elem = child->children[j];
                    if (elem->type == AST_PATTERN_VARIABLE) {
                        print_indent(gen);
                        const char* etype = "int";
                        if (elem->node_type && elem->node_type->kind != TYPE_UNKNOWN)
                            etype = get_c_type(elem->node_type);
                        fprintf(gen->output, "%s %s = _list_%d[%d];\n",
                                etype, elem->value, list_idx, j);
                    }
                }
            }
            list_idx++;
        } else if (child->type == AST_PATTERN_CONS) {
            // [H|T] pattern - check non-empty
            print_indent(gen);
            fprintf(gen->output, "if (_len_%d < 1) return ", list_idx);
            generate_default_return_value(gen, func->node_type);
            fprintf(gen->output, ";\n");
            
            // Bind head and tail
            if (child->child_count >= 1 && child->children[0]->type == AST_PATTERN_VARIABLE) {
                print_indent(gen);
                const char* htype = "int";
                if (child->children[0]->node_type && child->children[0]->node_type->kind != TYPE_UNKNOWN)
                    htype = get_c_type(child->children[0]->node_type);
                fprintf(gen->output, "%s %s = _list_%d[0];\n",
                        htype, child->children[0]->value, list_idx);
            }
            if (child->child_count >= 2 && child->children[1]->type == AST_PATTERN_VARIABLE) {
                print_indent(gen);
                const char* ttype = "int";
                if (child->children[1]->node_type && child->children[1]->node_type->kind != TYPE_UNKNOWN)
                    ttype = get_c_type(child->children[1]->node_type);
                fprintf(gen->output, "%s* %s = &_list_%d[1];\n",
                        ttype, child->children[1]->value, list_idx);
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
            generate_default_return_value(gen, func->node_type);
            fprintf(gen->output, ";\n");
        }
    }
    
    // Generate body
    if (body) {
        // If body is a block, it handles its own scope
        // If not a block, we still need to generate the statements
        if (body->type == AST_BLOCK) {
            // Block will handle inner scope, but we need to generate contents
            // without the extra braces since we're already in function body
            for (int i = 0; i < body->child_count; i++) {
                generate_statement(gen, body->children[i]);
            }
        } else {
            generate_statement(gen, body);
        }
    }

    // Emit function-level defers at implicit return (end of function)
    exit_scope(gen);

    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");
}

// Structure to hold pattern variable to parameter mapping
typedef struct {
    const char* var_name;
    int arg_index;
} PatternVarMapping;

// Forward declaration for expression generation with substitution
static void generate_expression_with_subst(CodeGenerator* gen, ASTNode* expr,
                                           PatternVarMapping* mappings, int mapping_count);

// Helper for expression substitution with optional parentheses
static void generate_expression_with_subst_inner(CodeGenerator* gen, ASTNode* expr,
                                                  PatternVarMapping* mappings, int mapping_count,
                                                  int add_parens);

// Generate an expression, substituting pattern variables with _argN
static void generate_expression_with_subst(CodeGenerator* gen, ASTNode* expr,
                                           PatternVarMapping* mappings, int mapping_count) {
    // Top-level: no extra parentheses needed (if statement provides them)
    generate_expression_with_subst_inner(gen, expr, mappings, mapping_count, 0);
}

static void generate_expression_with_subst_inner(CodeGenerator* gen, ASTNode* expr,
                                                  PatternVarMapping* mappings, int mapping_count,
                                                  int add_parens) {
    if (!expr) return;

    // Check if this is an identifier that should be substituted
    if (expr->type == AST_IDENTIFIER && expr->value) {
        for (int i = 0; i < mapping_count; i++) {
            if (strcmp(mappings[i].var_name, expr->value) == 0) {
                fprintf(gen->output, "_arg%d", mappings[i].arg_index);
                return;
            }
        }
        // Not in mapping, output as-is
        fprintf(gen->output, "%s", expr->value);
        return;
    }

    // For binary operations
    if (expr->type == AST_BINARY_EXPRESSION) {
        if (add_parens) fprintf(gen->output, "(");
        generate_expression_with_subst_inner(gen, expr->children[0], mappings, mapping_count, 1);
        fprintf(gen->output, " %s ", get_c_operator(expr->value));
        generate_expression_with_subst_inner(gen, expr->children[1], mappings, mapping_count, 1);
        if (add_parens) fprintf(gen->output, ")");
        return;
    }

    // For unary operations
    if (expr->type == AST_UNARY_EXPRESSION) {
        if (add_parens) fprintf(gen->output, "(");
        fprintf(gen->output, "%s", get_c_operator(expr->value));
        if (expr->child_count > 0) {
            generate_expression_with_subst_inner(gen, expr->children[0], mappings, mapping_count, 1);
        }
        if (add_parens) fprintf(gen->output, ")");
        return;
    }

    // For literals, just output value
    if (expr->type == AST_LITERAL) {
        if (expr->node_type && expr->node_type->kind == TYPE_STRING) {
            fprintf(gen->output, "\"%s\"", expr->value);
        } else {
            fprintf(gen->output, "%s", expr->value);
        }
        return;
    }

    // Fallback: use normal expression generation
    generate_expression(gen, expr);
}

// Generate a single clause's pattern match condition and body
// Returns 1 if this clause has a pattern/guard that needs checking, 0 if it's a catch-all
static int generate_clause_condition(CodeGenerator* gen, ASTNode* func, int is_first) {
    int has_condition = 0;
    int param_idx = 0;

    // First pass: check if this clause has any conditions (literals or guards)
    for (int i = 0; i < func->child_count; i++) {
        ASTNode* child = func->children[i];
        if (child->type == AST_PATTERN_LITERAL && strcmp(child->value, "_") != 0) {
            has_condition = 1;
            break;
        }
        if (child->type == AST_GUARD_CLAUSE) {
            has_condition = 1;
            break;
        }
        if (child->type == AST_PATTERN_LIST || child->type == AST_PATTERN_CONS) {
            has_condition = 1;
            break;
        }
    }

    if (!has_condition) {
        // This is a catch-all clause (e.g., factorial(n) with no guard)
        // Generate body directly
        for (int i = 0; i < func->child_count; i++) {
            ASTNode* child = func->children[i];
            if (child->type == AST_BLOCK) {
                generate_statement(gen, child);
                break;
            }
        }
        return 0;
    }

    // Build pattern variable to _argN mapping first
    PatternVarMapping mappings[32];  // Max 32 parameters
    int mapping_count = 0;
    param_idx = 0;

    for (int i = 0; i < func->child_count; i++) {
        ASTNode* child = func->children[i];
        if (child->type == AST_GUARD_CLAUSE || child->type == AST_BLOCK) continue;

        if (child->type == AST_PATTERN_VARIABLE && child->value && mapping_count < 32) {
            mappings[mapping_count].var_name = child->value;
            mappings[mapping_count].arg_index = param_idx;
            mapping_count++;
        }

        if (child->type == AST_PATTERN_LITERAL ||
            child->type == AST_PATTERN_VARIABLE ||
            child->type == AST_PATTERN_STRUCT ||
            child->type == AST_VARIABLE_DECLARATION) {
            param_idx++;
        }
    }

    // Generate condition
    print_indent(gen);
    if (is_first) {
        fprintf(gen->output, "if (");
    } else {
        fprintf(gen->output, "} else if (");
    }

    int first_cond = 1;
    param_idx = 0;
    int list_idx = 0;
    ASTNode* guard = NULL;

    for (int i = 0; i < func->child_count; i++) {
        ASTNode* child = func->children[i];

        if (child->type == AST_GUARD_CLAUSE) {
            guard = child;
            continue;
        }

        if (child->type == AST_BLOCK) continue;

        if (child->type == AST_PATTERN_LITERAL && strcmp(child->value, "_") != 0) {
            if (!first_cond) fprintf(gen->output, " && ");
            fprintf(gen->output, "_arg%d == %s", param_idx, child->value);
            first_cond = 0;
        }

        if (child->type == AST_PATTERN_LIST) {
            if (strcmp(child->value, "[]") == 0 && child->child_count == 0) {
                if (!first_cond) fprintf(gen->output, " && ");
                fprintf(gen->output, "_len%d == 0", list_idx);
                first_cond = 0;
            } else {
                if (!first_cond) fprintf(gen->output, " && ");
                fprintf(gen->output, "_len%d == %d", list_idx, child->child_count);
                first_cond = 0;
            }
            list_idx++;
        } else if (child->type == AST_PATTERN_CONS) {
            if (!first_cond) fprintf(gen->output, " && ");
            fprintf(gen->output, "_len%d >= 1", list_idx);
            first_cond = 0;
            list_idx++;
        }

        if (child->type == AST_PATTERN_LITERAL ||
            child->type == AST_PATTERN_VARIABLE ||
            child->type == AST_PATTERN_STRUCT ||
            child->type == AST_VARIABLE_DECLARATION) {
            param_idx++;
        }
    }

    // Add guard condition with variable substitution
    if (guard && guard->child_count > 0) {
        if (!first_cond) fprintf(gen->output, " && ");
        generate_expression_with_subst(gen, guard->children[0], mappings, mapping_count);
    }

    fprintf(gen->output, ") {\n");
    indent(gen);

    // Bind pattern variables
    param_idx = 0;
    list_idx = 0;
    for (int i = 0; i < func->child_count; i++) {
        ASTNode* child = func->children[i];

        if (child->type == AST_PATTERN_VARIABLE) {
            print_indent(gen);
            generate_type(gen, child->node_type);
            fprintf(gen->output, " %s = _arg%d;\n", child->value, param_idx);
        }

        if (child->type == AST_PATTERN_LIST && child->child_count > 0) {
            for (int j = 0; j < child->child_count; j++) {
                ASTNode* elem = child->children[j];
                if (elem->type == AST_PATTERN_VARIABLE) {
                    print_indent(gen);
                    fprintf(gen->output, "int %s = _list%d[%d];\n",
                            elem->value, list_idx, j);
                }
            }
            list_idx++;
        } else if (child->type == AST_PATTERN_CONS) {
            if (child->child_count >= 1 && child->children[0]->type == AST_PATTERN_VARIABLE) {
                print_indent(gen);
                fprintf(gen->output, "int %s = _list%d[0];\n",
                        child->children[0]->value, list_idx);
            }
            if (child->child_count >= 2 && child->children[1]->type == AST_PATTERN_VARIABLE) {
                print_indent(gen);
                fprintf(gen->output, "int* %s = &_list%d[1];\n",
                        child->children[1]->value, list_idx);
                print_indent(gen);
                fprintf(gen->output, "int %s_len = _len%d - 1;\n",
                        child->children[1]->value, list_idx);
            }
            list_idx++;
        }

        if (child->type == AST_PATTERN_LITERAL ||
            child->type == AST_PATTERN_VARIABLE ||
            child->type == AST_PATTERN_STRUCT ||
            child->type == AST_VARIABLE_DECLARATION) {
            param_idx++;
        }
    }

    // Generate body
    for (int i = 0; i < func->child_count; i++) {
        ASTNode* child = func->children[i];
        if (child->type == AST_BLOCK) {
            generate_statement(gen, child);
            break;
        }
    }

    unindent(gen);
    return 1;
}

// Generate a combined function from multiple pattern-matching clauses
void generate_combined_function(CodeGenerator* gen, ASTNode** clauses, int clause_count) {
    if (clause_count == 0) return;

    ASTNode* first = clauses[0];

    // Determine return type from first clause
    Type* ret_type = first->node_type;
    int has_return = has_return_value(first);

    // Check all clauses for return value
    for (int i = 1; i < clause_count && !has_return; i++) {
        if (has_return_value(clauses[i])) {
            has_return = 1;
        }
    }

    if ((!ret_type || ret_type->kind == TYPE_VOID || ret_type->kind == TYPE_UNKNOWN) && has_return) {
        fprintf(gen->output, "int");
    } else {
        generate_type(gen, ret_type);
    }
    fprintf(gen->output, " %s(", safe_c_name(first->value));

    // Generate unified parameter list using _argN naming
    // Count parameters from first clause
    int param_count = 0;
    int list_count = 0;

    for (int i = 0; i < first->child_count; i++) {
        ASTNode* child = first->children[i];
        if (child->type == AST_GUARD_CLAUSE || child->type == AST_BLOCK) continue;

        if (child->type == AST_PATTERN_LIST || child->type == AST_PATTERN_CONS) {
            if (param_count > 0 || list_count > 0) fprintf(gen->output, ", ");
            fprintf(gen->output, "int* _list%d, int _len%d", list_count, list_count);
            list_count++;
        } else if (child->type == AST_PATTERN_LITERAL ||
                   child->type == AST_PATTERN_VARIABLE ||
                   child->type == AST_PATTERN_STRUCT ||
                   child->type == AST_VARIABLE_DECLARATION) {
            if (param_count > 0 || list_count > 0) fprintf(gen->output, ", ");
            generate_type(gen, child->node_type);
            fprintf(gen->output, " _arg%d", param_count);
            param_count++;
        }
    }

    fprintf(gen->output, ") {\n");
    indent(gen);
    clear_declared_vars(gen);

    // Generate each clause as an if/else-if branch
    int is_first = 1;
    int had_catchall = 0;

    for (int i = 0; i < clause_count; i++) {
        int had_condition = generate_clause_condition(gen, clauses[i], is_first);
        if (had_condition) {
            is_first = 0;
        } else {
            had_catchall = 1;
        }
    }

    // Close last if block if we had conditions
    if (!is_first) {
        print_indent(gen);
        fprintf(gen->output, "}\n");
    }

    // Add fallback return if no catch-all and function returns value
    if (!had_catchall && has_return) {
        print_indent(gen);
        fprintf(gen->output, "return ");
        // If ret_type was void/unknown but we're returning int, use 0
        if (!ret_type || ret_type->kind == TYPE_VOID || ret_type->kind == TYPE_UNKNOWN) {
            fprintf(gen->output, "0");
        } else {
            generate_default_return_value(gen, ret_type);
        }
        fprintf(gen->output, ";\n");
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
