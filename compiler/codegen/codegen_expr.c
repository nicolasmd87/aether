#include "codegen_internal.h"

// ---- Closure support ----

// Collect identifiers referenced in an AST subtree
static void collect_identifiers(ASTNode* node, char*** names, int* count, int* cap) {
    if (!node) return;
    if (node->type == AST_IDENTIFIER && node->value) {
        // Check if already in list
        for (int i = 0; i < *count; i++) {
            if (strcmp((*names)[i], node->value) == 0) return;
        }
        if (*count >= *cap) {
            *cap = *cap ? *cap * 2 : 16;
            *names = realloc(*names, *cap * sizeof(char*));
        }
        (*names)[(*count)++] = strdup(node->value);
    }
    for (int i = 0; i < node->child_count; i++) {
        collect_identifiers(node->children[i], names, count, cap);
    }
}

// Check if a name is a closure parameter
static int is_closure_param(ASTNode* closure, const char* name) {
    for (int i = 0; i < closure->child_count; i++) {
        ASTNode* child = closure->children[i];
        if (child && child->type == AST_CLOSURE_PARAM && child->value &&
            strcmp(child->value, name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Check if a name is declared locally inside a block
static int is_local_var(ASTNode* block, const char* name) {
    if (!block) return 0;
    for (int i = 0; i < block->child_count; i++) {
        ASTNode* s = block->children[i];
        if (!s) continue;
        if ((s->type == AST_VARIABLE_DECLARATION || s->type == AST_CONST_DECLARATION) &&
            s->value && strcmp(s->value, name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Built-in function names that should not be treated as captures
static int is_builtin_name(const char* name) {
    static const char* builtins[] = {
        "print", "println", "make", "spawn", "exit", "sleep", "free",
        "getenv", "atoi", "clock_ns", "typeof", "is_type", "convert_type",
        "print_char", "wait_for_idle", "each", "map", "filter",
        NULL
    };
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(name, builtins[i]) == 0) return 1;
    }
    return 0;
}

// Discover closures in an AST subtree and register them with the codegen
void discover_closures(CodeGenerator* gen, ASTNode* node) {
    if (!node) return;
    if (node->type == AST_CLOSURE) {
        // Skip trailing blocks — they are inlined at the call site, not hoisted
        if (node->value && strcmp(node->value, "trailing") == 0) {
            // Still recurse into children to find nested non-trailing closures
            for (int i = 0; i < node->child_count; i++) {
                discover_closures(gen, node->children[i]);
            }
            return;
        }
        int id = gen->closure_counter++;
        // Store ID in closure node's value field for later reference
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%d", id);
        if (node->value) free(node->value);
        node->value = strdup(id_str);

        // Find the body (last child, should be AST_BLOCK)
        ASTNode* body = NULL;
        for (int i = node->child_count - 1; i >= 0; i--) {
            if (node->children[i] && node->children[i]->type == AST_BLOCK) {
                body = node->children[i];
                break;
            }
        }

        // Collect all identifiers in the body
        char** all_ids = NULL;
        int id_count = 0, id_cap = 0;
        collect_identifiers(body, &all_ids, &id_count, &id_cap);

        // Filter to captures: not a param, not a local, not a builtin
        char** captures = NULL;
        int cap_count = 0, cap_cap = 0;
        for (int i = 0; i < id_count; i++) {
            if (!is_closure_param(node, all_ids[i]) &&
                !is_local_var(body, all_ids[i]) &&
                !is_builtin_name(all_ids[i])) {
                if (cap_count >= cap_cap) {
                    cap_cap = cap_cap ? cap_cap * 2 : 8;
                    captures = realloc(captures, cap_cap * sizeof(char*));
                }
                captures[cap_count++] = strdup(all_ids[i]);
            }
            free(all_ids[i]);
        }
        free(all_ids);

        // Register closure
        if (gen->closure_count >= gen->closure_capacity) {
            gen->closure_capacity = gen->closure_capacity ? gen->closure_capacity * 2 : 16;
            gen->closures = realloc(gen->closures, gen->closure_capacity * sizeof(gen->closures[0]));
        }
        gen->closures[gen->closure_count].id = id;
        gen->closures[gen->closure_count].closure_node = node;
        gen->closures[gen->closure_count].captures = captures;
        gen->closures[gen->closure_count].capture_types = NULL; // resolved during emit
        gen->closures[gen->closure_count].capture_count = cap_count;
        gen->closures[gen->closure_count].parent_func = NULL;
        gen->closure_count++;
    }

    // Recurse into children (including nested closures)
    for (int i = 0; i < node->child_count; i++) {
        discover_closures(gen, node->children[i]);
    }
}

// Look up a variable's C type by searching the program AST
static const char* lookup_var_c_type(CodeGenerator* gen, const char* var_name) {
    if (!gen->program || !var_name) return "int";
    // Search through all function bodies for variable declarations
    for (int i = 0; i < gen->program->child_count; i++) {
        ASTNode* top = gen->program->children[i];
        if (!top) continue;
        // Look in function bodies and main
        if (top->type == AST_FUNCTION_DEFINITION || top->type == AST_MAIN_FUNCTION) {
            for (int j = 0; j < top->child_count; j++) {
                ASTNode* body = top->children[j];
                if (!body || body->type != AST_BLOCK) continue;
                for (int k = 0; k < body->child_count; k++) {
                    ASTNode* stmt = body->children[k];
                    if (stmt && (stmt->type == AST_VARIABLE_DECLARATION ||
                                 stmt->type == AST_CONST_DECLARATION) &&
                        stmt->value && strcmp(stmt->value, var_name) == 0) {
                        if (stmt->node_type && stmt->node_type->kind != TYPE_UNKNOWN) {
                            return get_c_type(stmt->node_type);
                        }
                        // Try to infer from initializer
                        if (stmt->child_count > 0 && stmt->children[0] &&
                            stmt->children[0]->node_type &&
                            stmt->children[0]->node_type->kind != TYPE_UNKNOWN) {
                            return get_c_type(stmt->children[0]->node_type);
                        }
                    }
                }
            }
        }
    }
    return "int"; // fallback
}

// Emit all hoisted closure environment structs and static functions
void emit_closure_definitions(CodeGenerator* gen) {
    for (int ci = 0; ci < gen->closure_count; ci++) {
        int id = gen->closures[ci].id;
        ASTNode* closure = gen->closures[ci].closure_node;
        char** captures = gen->closures[ci].captures;
        int cap_count = gen->closures[ci].capture_count;

        // Emit environment struct (even if empty, for uniform calling convention)
        fprintf(gen->output, "typedef struct {\n");
        if (cap_count == 0) {
            fprintf(gen->output, "    int _dummy;\n");
        } else {
            for (int i = 0; i < cap_count; i++) {
                const char* ctype = lookup_var_c_type(gen, captures[i]);
                fprintf(gen->output, "    %s %s;\n", ctype, safe_c_name(captures[i]));
            }
        }
        fprintf(gen->output, "} _closure_env_%d;\n\n", id);

        // Determine return type: check if body has return statements
        int has_return = 0;
        ASTNode* body_check = NULL;
        for (int i = closure->child_count - 1; i >= 0; i--) {
            if (closure->children[i] && closure->children[i]->type == AST_BLOCK) {
                body_check = closure->children[i];
                break;
            }
        }
        if (body_check) {
            has_return = has_return_value(body_check);
        }
        const char* ret_type = has_return ? "int" : "void";

        // Emit static function
        fprintf(gen->output, "static %s _closure_fn_%d(_closure_env_%d* _env", ret_type, id, id);
        for (int i = 0; i < closure->child_count; i++) {
            ASTNode* p = closure->children[i];
            if (p && p->type == AST_CLOSURE_PARAM) {
                const char* ptype = "int"; // default
                if (p->node_type) {
                    ptype = get_c_type(p->node_type);
                }
                fprintf(gen->output, ", %s %s", ptype, safe_c_name(p->value));
            }
        }
        fprintf(gen->output, ") {\n");

        // Emit capture aliases (local vars that reference env fields)
        for (int i = 0; i < cap_count; i++) {
            const char* ctype = lookup_var_c_type(gen, captures[i]);
            fprintf(gen->output, "    %s %s = _env->%s;\n",
                    ctype, safe_c_name(captures[i]), safe_c_name(captures[i]));
        }

        // Find and emit body
        ASTNode* body = NULL;
        for (int i = closure->child_count - 1; i >= 0; i--) {
            if (closure->children[i] && closure->children[i]->type == AST_BLOCK) {
                body = closure->children[i];
                break;
            }
        }

        if (body) {
            gen->indent_level = 1;
            // Closures called from trailing blocks need builder context injection
            // for _ctx: ptr functions. Set the flag so codegen injects _aether_ctx_get().
            gen->in_trailing_block++;
            for (int i = 0; i < body->child_count; i++) {
                generate_statement(gen, body->children[i]);
            }
            gen->in_trailing_block--;
            gen->indent_level = 0;
        }

        fprintf(gen->output, "}\n\n");

        // Emit MSVC-compatible closure constructor function (avoids statement expressions)
        if (cap_count > 0) {
            fprintf(gen->output, "#if !AETHER_GCC_COMPAT\n");
            fprintf(gen->output, "static _AeClosure _aether_make_closure_%d(", id);
            for (int i = 0; i < cap_count; i++) {
                if (i > 0) fprintf(gen->output, ", ");
                const char* ctype = lookup_var_c_type(gen, captures[i]);
                fprintf(gen->output, "%s %s", ctype, safe_c_name(captures[i]));
            }
            fprintf(gen->output, ") {\n");
            fprintf(gen->output, "    _closure_env_%d* _e = malloc(sizeof(_closure_env_%d));\n", id, id);
            for (int i = 0; i < cap_count; i++) {
                fprintf(gen->output, "    _e->%s = %s;\n", safe_c_name(captures[i]), safe_c_name(captures[i]));
            }
            fprintf(gen->output, "    _AeClosure _c = { (void(*)(void))_closure_fn_%d, _e };\n", id);
            fprintf(gen->output, "    return _c;\n");
            fprintf(gen->output, "}\n");
            fprintf(gen->output, "#endif\n\n");
        }
    }
}

// Emit a send target expression with the correct C cast.
// Actor refs produce (ActorBase*)(expr) directly.
// Int/int64 values (actor refs stored in int message fields or state) need
// (ActorBase*)(intptr_t)(expr) to avoid pointer-width conversion warnings.
static void emit_send_target(CodeGenerator* gen, ASTNode* target, const char* cast_type) {
    int needs_intptr = target->node_type &&
        (target->node_type->kind == TYPE_INT || target->node_type->kind == TYPE_INT64);
    fprintf(gen->output, "(%s)(", cast_type);
    if (needs_intptr) fprintf(gen->output, "(intptr_t)");
    generate_expression(gen, target);
    fprintf(gen->output, ")");
}

void generate_expression(CodeGenerator* gen, ASTNode* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case AST_LITERAL:
            if (expr->node_type && expr->node_type->kind == TYPE_STRING) {
                fprintf(gen->output, "\"");
                const char* str = expr->value;
                while (*str) {
                    unsigned char ch = (unsigned char)*str;
                    switch (*str) {
                        case '\n': fprintf(gen->output, "\\n"); break;
                        case '\t': fprintf(gen->output, "\\t"); break;
                        case '\r': fprintf(gen->output, "\\r"); break;
                        case '\\': fprintf(gen->output, "\\\\"); break;
                        case '"': fprintf(gen->output, "\\\""); break;
                        default:
                            if (ch < 0x20 || ch == 0x7F) {
                                fprintf(gen->output, "\\x%02x", ch);
                            } else {
                                fprintf(gen->output, "%c", *str);
                            }
                            break;
                    }
                    str++;
                }
                fprintf(gen->output, "\"");
            } else {
                fprintf(gen->output, "%s", expr->value);
            }
            break;

        case AST_NULL_LITERAL:
            fprintf(gen->output, "NULL");
            break;

        case AST_IF_EXPRESSION:
            // if cond { then } else { else } → C ternary: (cond) ? (then) : (else)
            if (expr->child_count >= 3) {
                fprintf(gen->output, "(");
                generate_expression(gen, expr->children[0]);
                fprintf(gen->output, ") ? (");
                generate_expression(gen, expr->children[1]);
                fprintf(gen->output, ") : (");
                generate_expression(gen, expr->children[2]);
                fprintf(gen->output, ")");
            }
            break;

        case AST_IDENTIFIER:
            if (!expr->value) { fprintf(gen->output, "/* NULL identifier */0"); break; }
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
            if (expr->child_count > 0) {
                ASTNode* child = expr->children[0];

                int needs_atomic = 0;
                if (child->node_type && child->node_type->kind == TYPE_ACTOR_REF && expr->value) {
                    size_t name_len = strlen(expr->value);
                    int is_ref_field = (name_len > 4 && strcmp(expr->value + name_len - 4, "_ref") == 0);

                    if (!gen->current_actor && !gen->generating_lvalue && !is_ref_field) {
                        needs_atomic = 1;
                    }
                }

                if (needs_atomic) {
                    fprintf(gen->output, "atomic_load(&");
                    generate_expression(gen, child);
                    fprintf(gen->output, "->%s)", expr->value);
                } else if (child->node_type && child->node_type->kind == TYPE_ACTOR_REF) {
                    generate_expression(gen, child);
                    fprintf(gen->output, "->%s", expr->value);
                } else {
                    generate_expression(gen, child);
                    fprintf(gen->output, ".%s", expr->value);
                }
            }
            break;
            
        case AST_BINARY_EXPRESSION:
            if (expr->child_count >= 2) {
                int skip_parens = gen->in_condition;
                gen->in_condition = 0;

                int is_assignment = (expr->value && strcmp(expr->value, "=") == 0);

                // String comparison: emit strcmp instead of pointer ==
                // Applies to ==, !=, <, >, <=, >= when BOTH sides are strings.
                // NOT when comparing to 0/NULL (null check, not string compare).
                int is_string_cmp = 0;
                if (expr->value && (strcmp(expr->value, "==") == 0 || strcmp(expr->value, "!=") == 0
                    || strcmp(expr->value, "<") == 0 || strcmp(expr->value, ">") == 0
                    || strcmp(expr->value, "<=") == 0 || strcmp(expr->value, ">=") == 0)) {
                    Type* lhs_type = expr->children[0]->node_type;
                    Type* rhs_type = expr->children[1]->node_type;
                    ASTNode* rhs = expr->children[1];
                    ASTNode* lhs_node = expr->children[0];
                    int rhs_is_null = (rhs->type == AST_LITERAL && rhs->value && strcmp(rhs->value, "0") == 0)
                                   || (rhs->type == AST_IDENTIFIER && rhs->value && strcmp(rhs->value, "NULL") == 0);
                    int lhs_is_null = (lhs_node->type == AST_LITERAL && lhs_node->value && strcmp(lhs_node->value, "0") == 0)
                                   || (lhs_node->type == AST_IDENTIFIER && lhs_node->value && strcmp(lhs_node->value, "NULL") == 0);
                    // Both sides must be strings and neither can be a null literal
                    int rhs_is_string = (rhs_type && rhs_type->kind == TYPE_STRING);
                    if (lhs_type && lhs_type->kind == TYPE_STRING && rhs_is_string
                        && !rhs_is_null && !lhs_is_null) {
                        is_string_cmp = 1;
                    }
                }

                if (is_string_cmp) {
                    if (!skip_parens) fprintf(gen->output, "(");
                    fprintf(gen->output, "strcmp(_aether_safe_str(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, "), _aether_safe_str(");
                    generate_expression(gen, expr->children[1]);
                    fprintf(gen->output, ")) %s 0", get_c_operator(expr->value));
                    if (!skip_parens) fprintf(gen->output, ")");
                } else {
                    if (!skip_parens) fprintf(gen->output, "(");

                    // Detect ptr/int mixed comparisons and cast ptr to intptr_t
                    // to suppress -Wpointer-integer-compare warnings.
                    // Common case: list.get() returns void*, compared to int literal.
                    int is_comparison = expr->value && (
                        strcmp(expr->value, "==") == 0 || strcmp(expr->value, "!=") == 0 ||
                        strcmp(expr->value, "<") == 0  || strcmp(expr->value, ">") == 0  ||
                        strcmp(expr->value, "<=") == 0 || strcmp(expr->value, ">=") == 0);
                    Type* ltype = expr->children[0]->node_type;
                    Type* rtype = expr->children[1]->node_type;
                    int lhs_is_ptr = ltype && ltype->kind == TYPE_PTR;
                    int rhs_is_ptr = rtype && rtype->kind == TYPE_PTR;
                    int lhs_is_int = ltype && (ltype->kind == TYPE_INT || ltype->kind == TYPE_INT64);
                    int rhs_is_int = rtype && (rtype->kind == TYPE_INT || rtype->kind == TYPE_INT64);
                    int ptr_int_cmp = is_comparison && ((lhs_is_ptr && rhs_is_int) || (rhs_is_ptr && lhs_is_int));

                    if (is_assignment) {
                        gen->generating_lvalue = 1;
                    }
                    if (ptr_int_cmp && lhs_is_ptr) fprintf(gen->output, "(intptr_t)");
                    generate_expression(gen, expr->children[0]);
                    if (is_assignment) {
                        gen->generating_lvalue = 0;
                    }

                    fprintf(gen->output, " %s ", get_c_operator(expr->value));
                    if (ptr_int_cmp && rhs_is_ptr) fprintf(gen->output, "(intptr_t)");
                    generate_expression(gen, expr->children[1]);
                    if (!skip_parens) fprintf(gen->output, ")");
                }
            }
            break;
            
        case AST_UNARY_EXPRESSION:
            if (expr->child_count >= 1) {
                // Wrap the entire unary expression in parens: (!x) not !(x).
                // This prevents GCC -Wlogical-not-parentheses when the unary
                // result is compared: (!x) != y  instead of  !x != y.
                fprintf(gen->output, "(%s(", get_c_operator(expr->value));
                generate_expression(gen, expr->children[0]);
                fprintf(gen->output, "))");
            }
            break;
            
        case AST_FUNCTION_CALL:
            if (expr->value) {
                const char* func_name = expr->value;
                
                if (strcmp(func_name, "make") == 0 && expr->node_type && expr->node_type->kind == TYPE_ARRAY) {
                    fprintf(gen->output, "(%s)malloc(", get_c_type(expr->node_type));
                    if (expr->child_count > 0) {
                        fprintf(gen->output, "(");
                        generate_expression(gen, expr->children[0]);
                        fprintf(gen->output, ") * sizeof(%s)", get_c_type(expr->node_type->element_type));
                    }
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "typeof") == 0) {
                    fprintf(gen->output, "aether_typeof(");
                    if (expr->child_count > 0) {
                        generate_expression(gen, expr->children[0]);
                    }
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "is_type") == 0) {
                    fprintf(gen->output, "aether_is_type(");
                    for (int i = 0; i < expr->child_count; i++) {
                        if (i > 0) fprintf(gen->output, ", ");
                        generate_expression(gen, expr->children[i]);
                    }
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "convert_type") == 0) {
                    fprintf(gen->output, "aether_convert_type(");
                    for (int i = 0; i < expr->child_count; i++) {
                        if (i > 0) fprintf(gen->output, ", ");
                        generate_expression(gen, expr->children[i]);
                    }
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "print") == 0) {
                    if (expr->child_count == 1 && expr->children[0]->type == AST_STRING_INTERP) {
                        // print("Hello ${name}!") — use printf mode for interp
                        gen->interp_as_printf = 1;
                        generate_expression(gen, expr->children[0]);
                        gen->interp_as_printf = 0;
                    } else
                    if (expr->child_count == 1 && expr->children[0]->node_type) {
                        ASTNode* arg = expr->children[0];
                        Type* arg_type = arg->node_type;

                        if (arg_type->kind == TYPE_INT) {
                            fprintf(gen->output, "printf(\"%%d\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_INT64) {
                            fprintf(gen->output, "printf(\"%%lld\", (long long)");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_UINT64) {
                            fprintf(gen->output, "printf(\"%%llu\", (unsigned long long)");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_FLOAT) {
                            fprintf(gen->output, "printf(\"%%f\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_STRING) {
                            if (arg->type == AST_LITERAL) {
                                // String literal — never NULL, use printf directly
                                fprintf(gen->output, "printf(");
                                generate_expression(gen, arg);
                                fprintf(gen->output, ")");
                            } else {
                                // Runtime string — could be NULL
                                fprintf(gen->output, "printf(\"%%s\", _aether_safe_str(");
                                generate_expression(gen, arg);
                                fprintf(gen->output, "))");
                            }
                        } else if (arg_type->kind == TYPE_PTR) {
                            // Runtime pointer — could be NULL
                            fprintf(gen->output, "printf(\"%%s\", _aether_safe_str(");
                            generate_expression(gen, arg);
                            fprintf(gen->output, "))");
                        } else if (arg_type->kind == TYPE_BOOL) {
                            fprintf(gen->output, "printf(\"%%s\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, " ? \"true\" : \"false\")");
                        } else {
                            fprintf(gen->output, "printf(\"%%d\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        }
                    } else if (expr->child_count == 1) {
                        ASTNode* a = expr->children[0];
                        if (a->type == AST_LITERAL && a->node_type && a->node_type->kind == TYPE_STRING) {
                            fprintf(gen->output, "printf(");
                            generate_expression(gen, a);
                            fprintf(gen->output, ")");
                        } else {
                            fprintf(gen->output, "printf(\"%%d\", ");
                            generate_expression(gen, a);
                            fprintf(gen->output, ")");
                        }
                    } else if (expr->child_count >= 2 && expr->children[0]->type == AST_LITERAL &&
                               expr->children[0]->node_type && expr->children[0]->node_type->kind == TYPE_STRING &&
                               expr->children[0]->value) {
                        // Multi-arg with literal format string: auto-fix specifiers
                        const char* fmt = expr->children[0]->value;
                        fprintf(gen->output, "printf(\"");
                        int arg_idx = 1;
                        for (int fi = 0; fmt[fi]; fi++) {
                            if (fmt[fi] == '%' && fmt[fi + 1]) {
                                fi++;
                                while (fmt[fi] == '-' || fmt[fi] == '+' || fmt[fi] == ' ' ||
                                       fmt[fi] == '#' || fmt[fi] == '0') fi++;
                                while (fmt[fi] >= '0' && fmt[fi] <= '9') fi++;
                                if (fmt[fi] == '.') { fi++; while (fmt[fi] >= '0' && fmt[fi] <= '9') fi++; }
                                if (fmt[fi] == '%') {
                                    fprintf(gen->output, "%%%%");
                                } else if (arg_idx < expr->child_count) {
                                    Type* atype = expr->children[arg_idx]->node_type;
                                    if (atype && atype->kind == TYPE_FLOAT) fprintf(gen->output, "%%f");
                                    else if (atype && atype->kind == TYPE_INT64) fprintf(gen->output, "%%lld");
                                    else if (atype && (atype->kind == TYPE_STRING || atype->kind == TYPE_PTR)) fprintf(gen->output, "%%s");
                                    else if (atype && atype->kind == TYPE_BOOL) fprintf(gen->output, "%%s");
                                    else fprintf(gen->output, "%%d");
                                    arg_idx++;
                                } else {
                                    fprintf(gen->output, "%%%c", fmt[fi]);
                                }
                            } else {
                                switch (fmt[fi]) {
                                    case '\n': fprintf(gen->output, "\\n"); break;
                                    case '\t': fprintf(gen->output, "\\t"); break;
                                    case '\r': fprintf(gen->output, "\\r"); break;
                                    case '\\': fprintf(gen->output, "\\\\"); break;
                                    case '"':  fprintf(gen->output, "\\\""); break;
                                    default:   fprintf(gen->output, "%c", fmt[fi]); break;
                                }
                            }
                        }
                        fprintf(gen->output, "\", ");
                        for (int i = 1; i < expr->child_count; i++) {
                            if (i > 1) fprintf(gen->output, ", ");
                            Type* atype = expr->children[i]->node_type;
                            if (atype && atype->kind == TYPE_INT64) { fprintf(gen->output, "(long long)"); generate_expression(gen, expr->children[i]); }
                            else if (atype && atype->kind == TYPE_BOOL) { generate_expression(gen, expr->children[i]); fprintf(gen->output, " ? \"true\" : \"false\""); }
                            else if (atype && (atype->kind == TYPE_STRING || atype->kind == TYPE_PTR)) { fprintf(gen->output, "_aether_safe_str("); generate_expression(gen, expr->children[i]); fprintf(gen->output, ")"); }
                            else generate_expression(gen, expr->children[i]);
                        }
                        fprintf(gen->output, ")");
                    } else {
                        // Non-literal format string — use %s to prevent format injection
                        fprintf(gen->output, "printf(\"%%s\", ");
                        generate_expression(gen, expr->children[0]);
                        fprintf(gen->output, ")");
                    }
                }
                else if (strcmp(func_name, "println") == 0) {
                    // println(x) = print(x) then putchar('\n')
                    // Special case: println("...${expr}...") — generate interp then add \n
                    if (expr->child_count == 1 && expr->children[0]->type == AST_STRING_INTERP) {
                        gen->interp_as_printf = 1;
                        generate_expression(gen, expr->children[0]);
                        gen->interp_as_printf = 0;
                        fprintf(gen->output, "; putchar('\\n')");
                    } else
                    if (expr->child_count == 1 && expr->children[0]->node_type) {
                        ASTNode* arg = expr->children[0];
                        Type* arg_type = arg->node_type;
                        if (arg_type->kind == TYPE_INT) {
                            fprintf(gen->output, "printf(\"%%d\\n\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_INT64) {
                            fprintf(gen->output, "printf(\"%%lld\\n\", (long long)");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_UINT64) {
                            fprintf(gen->output, "printf(\"%%llu\\n\", (unsigned long long)");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_FLOAT) {
                            fprintf(gen->output, "printf(\"%%f\\n\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_STRING) {
                            if (arg->type == AST_LITERAL) {
                                // String literal — never NULL, use puts() directly
                                fprintf(gen->output, "puts(");
                                generate_expression(gen, arg);
                                fprintf(gen->output, ")");
                            } else {
                                // Runtime string — could be NULL
                                fprintf(gen->output, "printf(\"%%s\\n\", _aether_safe_str(");
                                generate_expression(gen, arg);
                                fprintf(gen->output, "))");
                            }
                        } else if (arg_type->kind == TYPE_PTR) {
                            // Runtime pointer — could be NULL
                            fprintf(gen->output, "printf(\"%%s\\n\", _aether_safe_str(");
                            generate_expression(gen, arg);
                            fprintf(gen->output, "))");
                        } else if (arg_type->kind == TYPE_BOOL) {
                            fprintf(gen->output, "printf(\"%%s\\n\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, " ? \"true\" : \"false\")");
                        } else {
                            fprintf(gen->output, "printf(\"%%d\\n\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        }
                    } else if (expr->child_count == 1) {
                        ASTNode* a = expr->children[0];
                        if (a->type == AST_LITERAL && a->node_type && a->node_type->kind == TYPE_STRING) {
                            // println("text") → puts("text") which adds \n automatically
                            fprintf(gen->output, "puts(");
                            generate_expression(gen, a);
                            fprintf(gen->output, ")");
                        } else {
                            fprintf(gen->output, "printf(\"%%d\\n\", ");
                            generate_expression(gen, a);
                            fprintf(gen->output, ")");
                        }
                    } else if (expr->child_count == 0) {
                        fprintf(gen->output, "putchar('\\n')");
                    } else if (expr->child_count >= 2 && expr->children[0]->type == AST_LITERAL &&
                               expr->children[0]->node_type && expr->children[0]->node_type->kind == TYPE_STRING &&
                               expr->children[0]->value) {
                        // Multi-arg with literal format: auto-fix specifiers + newline
                        const char* fmt = expr->children[0]->value;
                        fprintf(gen->output, "printf(\"");
                        int arg_idx = 1;
                        for (int fi = 0; fmt[fi]; fi++) {
                            if (fmt[fi] == '%' && fmt[fi + 1]) {
                                fi++;
                                while (fmt[fi] == '-' || fmt[fi] == '+' || fmt[fi] == ' ' ||
                                       fmt[fi] == '#' || fmt[fi] == '0') fi++;
                                while (fmt[fi] >= '0' && fmt[fi] <= '9') fi++;
                                if (fmt[fi] == '.') { fi++; while (fmt[fi] >= '0' && fmt[fi] <= '9') fi++; }
                                if (fmt[fi] == '%') {
                                    fprintf(gen->output, "%%%%");
                                } else if (arg_idx < expr->child_count) {
                                    Type* atype = expr->children[arg_idx]->node_type;
                                    if (atype && atype->kind == TYPE_FLOAT) fprintf(gen->output, "%%f");
                                    else if (atype && atype->kind == TYPE_INT64) fprintf(gen->output, "%%lld");
                                    else if (atype && (atype->kind == TYPE_STRING || atype->kind == TYPE_PTR)) fprintf(gen->output, "%%s");
                                    else if (atype && atype->kind == TYPE_BOOL) fprintf(gen->output, "%%s");
                                    else fprintf(gen->output, "%%d");
                                    arg_idx++;
                                } else {
                                    fprintf(gen->output, "%%%c", fmt[fi]);
                                }
                            } else {
                                switch (fmt[fi]) {
                                    case '\n': fprintf(gen->output, "\\n"); break;
                                    case '\t': fprintf(gen->output, "\\t"); break;
                                    case '\r': fprintf(gen->output, "\\r"); break;
                                    case '\\': fprintf(gen->output, "\\\\"); break;
                                    case '"':  fprintf(gen->output, "\\\""); break;
                                    default:   fprintf(gen->output, "%c", fmt[fi]); break;
                                }
                            }
                        }
                        fprintf(gen->output, "\\n\", ");
                        for (int i = 1; i < expr->child_count; i++) {
                            if (i > 1) fprintf(gen->output, ", ");
                            Type* atype = expr->children[i]->node_type;
                            if (atype && atype->kind == TYPE_INT64) { fprintf(gen->output, "(long long)"); generate_expression(gen, expr->children[i]); }
                            else if (atype && atype->kind == TYPE_BOOL) { generate_expression(gen, expr->children[i]); fprintf(gen->output, " ? \"true\" : \"false\""); }
                            else if (atype && (atype->kind == TYPE_STRING || atype->kind == TYPE_PTR)) { fprintf(gen->output, "_aether_safe_str("); generate_expression(gen, expr->children[i]); fprintf(gen->output, ")"); }
                            else generate_expression(gen, expr->children[i]);
                        }
                        fprintf(gen->output, ")");
                    } else {
                        // Non-literal format string — use %s to prevent format injection
                        fprintf(gen->output, "printf(\"%%s\\n\", ");
                        generate_expression(gen, expr->children[0]);
                        fprintf(gen->output, ")");
                    }
                }
                else if (strcmp(func_name, "wait_for_idle") == 0) {
                    fprintf(gen->output, "scheduler_wait()");
                }
                else if (strcmp(func_name, "sleep") == 0 && expr->child_count == 1) {
                    // Emit target-guarded code (not host-guarded)
                    fprintf(gen->output, "\n#ifdef _WIN32\nSleep(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ");\n#else\nusleep(1000 * (");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, "));\n#endif\n");
                }
                else if (strcmp(func_name, "getenv") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "getenv(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "atoi") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "atoi(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "exit") == 0) {
                    fprintf(gen->output, "exit(");
                    if (expr->child_count == 1) {
                        generate_expression(gen, expr->children[0]);
                    } else {
                        fprintf(gen->output, "0");
                    }
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "free") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "free((void*)");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")");
                }
                // ref(value) — create a heap-allocated mutable cell
                else if (strcmp(func_name, "ref") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "\n#if AETHER_GCC_COMPAT\n");
                    fprintf(gen->output, "({ intptr_t* _r = malloc(sizeof(intptr_t)); *_r = (intptr_t)(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, "); (void*)_r; })");
                    fprintf(gen->output, "\n#else\n");
                    fprintf(gen->output, "_aether_ref_new((intptr_t)(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, "))");
                    fprintf(gen->output, "\n#endif\n");
                }
                // ref_get(r) — read from a ref cell
                else if (strcmp(func_name, "ref_get") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "(int)(*(intptr_t*)");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")");
                }
                // ref_set(r, value) — write to a ref cell
                else if (strcmp(func_name, "ref_set") == 0 && expr->child_count == 2) {
                    fprintf(gen->output, "(*(intptr_t*)");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, " = (intptr_t)(");
                    generate_expression(gen, expr->children[1]);
                    fprintf(gen->output, "))");
                }
                // ref_free(r) — free a ref cell
                else if (strcmp(func_name, "ref_free") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "free(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")");
                }
                else if (strcmp(func_name, "clock_ns") == 0 && expr->child_count == 0) {
                    fprintf(gen->output, "\n#if AETHER_GCC_COMPAT\n");
                    fprintf(gen->output, "({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC, &_ts); (int64_t)_ts.tv_sec * 1000000000LL + _ts.tv_nsec; })");
                    fprintf(gen->output, "\n#else\n");
                    fprintf(gen->output, "_aether_clock_ns()");
                    fprintf(gen->output, "\n#endif\n");
                }
                else if (strcmp(func_name, "print_char") == 0 && expr->child_count >= 1) {
                    fprintf(gen->output, "putchar(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")");
                }
                // each(array, count, closure) — iterate array calling closure for each element
                // Usage: each(items, count) |item| { ... }
                // The trailing block becomes the last child (a closure)
                // box_closure(closure) — heap-allocate a closure so it can be stored in a list
                else if (strcmp(func_name, "box_closure") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "_aether_box_closure(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")");
                }
                // unbox_closure(ptr) — retrieve a closure from a heap pointer
                else if (strcmp(func_name, "unbox_closure") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "_aether_unbox_closure(");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")");
                }
                // read_char() — read a single character from stdin (blocking)
                else if (strcmp(func_name, "read_char") == 0 && expr->child_count == 0) {
                    fprintf(gen->output, "getchar()");
                }
                // char_at(str, index) — ASCII value of character at position
                else if (strcmp(func_name, "char_at") == 0 && expr->child_count >= 1) {
                    fprintf(gen->output, "((int)((const char*)");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ")[");
                    if (expr->child_count >= 2) {
                        generate_expression(gen, expr->children[1]);
                    } else {
                        fprintf(gen->output, "0");
                    }
                    fprintf(gen->output, "])");
                }
                // str_eq(a, b) — string equality (returns 1 or 0)
                else if (strcmp(func_name, "str_eq") == 0 && expr->child_count == 2) {
                    fprintf(gen->output, "(strcmp((const char*)");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, ", ");
                    generate_expression(gen, expr->children[1]);
                    fprintf(gen->output, ") == 0)");
                }
                // raw_mode() / cooked_mode() — terminal mode control
                else if (strcmp(func_name, "raw_mode") == 0 && expr->child_count == 0) {
                    fprintf(gen->output, "_aether_raw_mode()");
                }
                else if (strcmp(func_name, "cooked_mode") == 0 && expr->child_count == 0) {
                    fprintf(gen->output, "_aether_cooked_mode()");
                }
                // builder_context() — returns the current builder context from the stack
                else if (strcmp(func_name, "builder_context") == 0) {
                    fprintf(gen->output, "_aether_ctx_get()");
                }
                // builder_depth() — returns the current builder nesting depth
                else if (strcmp(func_name, "builder_depth") == 0) {
                    fprintf(gen->output, "_aether_ctx_depth");
                }
                // call(closure_var, args...) — invoke a closure stored in a variable
                // Looks up the closure's hoisted function signature and calls through it
                else if (strcmp(func_name, "call") == 0 && expr->child_count >= 1) {
                    ASTNode* closure_arg = expr->children[0];
                    // Look up the closure ID from the variable name
                    int found_id = -1;
                    if (closure_arg && closure_arg->type == AST_IDENTIFIER && closure_arg->value) {
                        for (int ci = 0; ci < gen->closure_var_count; ci++) {
                            if (strcmp(gen->closure_var_map[ci].var_name, closure_arg->value) == 0) {
                                found_id = gen->closure_var_map[ci].closure_id;
                                break;
                            }
                        }
                    }

                    if (found_id >= 0) {
                        // Generate typed call: _closure_fn_N((_closure_env_N*)closure.env, args...)
                        fprintf(gen->output, "_closure_fn_%d((_closure_env_%d*)",
                                found_id, found_id);
                        generate_expression(gen, closure_arg);
                        fprintf(gen->output, ".env");
                        for (int i = 1; i < expr->child_count; i++) {
                            ASTNode* arg = expr->children[i];
                            if (arg && arg->type == AST_CLOSURE) continue;
                            fprintf(gen->output, ", ");
                            generate_expression(gen, arg);
                        }
                        fprintf(gen->output, ")");
                    } else {
                        // Fallback: generic closure invocation via function pointer cast
                        // Determine return type — if call() result is used, assume int
                        const char* ret = (expr->node_type &&
                            expr->node_type->kind != TYPE_VOID &&
                            expr->node_type->kind != TYPE_UNKNOWN) ?
                            get_c_type(expr->node_type) : "int";
                        fprintf(gen->output, "((%s(*)(void*", ret);
                        for (int i = 1; i < expr->child_count; i++) {
                            ASTNode* arg = expr->children[i];
                            if (arg && arg->type == AST_CLOSURE) continue;
                            // Infer arg type
                            const char* atype = "int";
                            if (arg && arg->node_type) {
                                atype = get_c_type(arg->node_type);
                            }
                            fprintf(gen->output, ", %s", atype);
                        }
                        fprintf(gen->output, "))");
                        generate_expression(gen, closure_arg);
                        fprintf(gen->output, ".fn)(");
                        generate_expression(gen, closure_arg);
                        fprintf(gen->output, ".env");
                        for (int i = 1; i < expr->child_count; i++) {
                            ASTNode* arg = expr->children[i];
                            if (arg && arg->type == AST_CLOSURE) continue;
                            fprintf(gen->output, ", ");
                            generate_expression(gen, arg);
                        }
                        fprintf(gen->output, ")");
                    }
                }
                else {
                    char c_func_name[256];
                    // Don't mangle extern functions — they refer to real C symbols
                    const char* mangled = is_extern_func(gen, func_name) ? func_name : safe_c_name(func_name);
                    strncpy(c_func_name, mangled, sizeof(c_func_name) - 1);
                    c_func_name[sizeof(c_func_name) - 1] = '\0';
                    for (char* p = c_func_name; *p; p++) {
                        if (*p == '.') *p = '_';
                    }
                    fprintf(gen->output, "%s(", c_func_name);
                    int arg_printed = 0;
                    // Auto-inject builder context for builder functions
                    // (functions with _ctx: ptr as first param)
                    if (gen->in_trailing_block > 0) {
                        for (int bi = 0; bi < gen->builder_func_count; bi++) {
                            if (strcmp(gen->builder_funcs[bi], func_name) == 0) {
                                fprintf(gen->output, "_aether_ctx_get()");
                                arg_printed++;
                                break;
                            }
                        }
                    }
                    for (int i = 0; i < expr->child_count; i++) {
                        ASTNode* arg = expr->children[i];
                        // Skip trailing DSL blocks that are just inline syntax sugar
                        // (value == "trailing" AND function doesn't expect fn param)
                        if (arg && arg->type == AST_CLOSURE &&
                            arg->value && strcmp(arg->value, "trailing") == 0) {
                            // Check if function expects this arg as fn type
                            // by looking up the function definition
                            int func_wants_fn = 0;
                            for (int fi = 0; fi < gen->program->child_count; fi++) {
                                ASTNode* fdef = gen->program->children[fi];
                                if (fdef && fdef->type == AST_FUNCTION_DEFINITION &&
                                    fdef->value && strcmp(fdef->value, func_name) == 0) {
                                    int pi = 0;
                                    for (int fj = 0; fj < fdef->child_count; fj++) {
                                        ASTNode* p = fdef->children[fj];
                                        if (p->type == AST_GUARD_CLAUSE || p->type == AST_BLOCK) continue;
                                        if (pi == i && p->node_type &&
                                            p->node_type->kind == TYPE_FUNCTION) {
                                            func_wants_fn = 1;
                                        }
                                        pi++;
                                    }
                                    break;
                                }
                            }
                            if (!func_wants_fn) continue; // skip DSL trailing block
                        }
                        if (arg_printed > 0) fprintf(gen->output, ", ");
                        // Cast int→void* when param expects void* (TYPE_PTR).
                        // Check extern registry first, then user-defined function params.
                        TypeKind expected = lookup_extern_param_kind(gen, c_func_name, arg_printed);
                        if (expected == TYPE_UNKNOWN) {
                            // Look up user-defined function's param type
                            for (int fi = 0; fi < gen->program->child_count; fi++) {
                                ASTNode* fdef = gen->program->children[fi];
                                if (fdef && fdef->type == AST_FUNCTION_DEFINITION &&
                                    fdef->value && strcmp(fdef->value, func_name) == 0) {
                                    int pi = 0;
                                    for (int fj = 0; fj < fdef->child_count; fj++) {
                                        ASTNode* fp = fdef->children[fj];
                                        if (fp->type == AST_GUARD_CLAUSE || fp->type == AST_BLOCK) continue;
                                        if (pi == arg_printed && fp->node_type) {
                                            expected = fp->node_type->kind;
                                        }
                                        pi++;
                                    }
                                    break;
                                }
                            }
                        }
                        if (expected == TYPE_PTR && arg->node_type &&
                            (arg->node_type->kind == TYPE_INT || arg->node_type->kind == TYPE_BOOL)) {
                            fprintf(gen->output, "(void*)(intptr_t)(");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else {
                            generate_expression(gen, arg);
                        }
                        arg_printed++;
                    }
                    fprintf(gen->output, ")");
                }
            }
            break;

        case AST_STRING_INTERP: {
            // Children alternate: AST_LITERAL (string) and expression nodes.
            // Two modes:
            //   1. interp_as_printf: emit printf() directly (used by print/println)
            //   2. default: emit snprintf+malloc → returns (void*) heap string (TYPE_PTR)

            // Helper macro: emit the format string for both modes
            #define EMIT_INTERP_FMT() do { \
                for (int i = 0; i < expr->child_count; i++) { \
                    ASTNode* ch = expr->children[i]; \
                    if (ch->type == AST_LITERAL && ch->node_type && ch->node_type->kind == TYPE_STRING) { \
                        const char* s = ch->value ? ch->value : ""; \
                        for (; *s; s++) { \
                            switch (*s) { \
                                case '\n': fprintf(gen->output, "\\n");   break; \
                                case '\t': fprintf(gen->output, "\\t");   break; \
                                case '\r': fprintf(gen->output, "\\r");   break; \
                                case '"':  fprintf(gen->output, "\\\"");  break; \
                                case '%':  fprintf(gen->output, "%%%%");  break; \
                                case '\\': { \
                                    char esc = *(s+1); \
                                    if (esc == 'n')       { fprintf(gen->output, "\\n");   s++; } \
                                    else if (esc == 't')  { fprintf(gen->output, "\\t");   s++; } \
                                    else if (esc == 'r')  { fprintf(gen->output, "\\r");   s++; } \
                                    else if (esc == '\\') { fprintf(gen->output, "\\\\");  s++; } \
                                    else if (esc == '"')  { fprintf(gen->output, "\\\"");  s++; } \
                                    else if (esc == 'x') { \
                                        s += 2; \
                                        int hval = 0, hd = 0; \
                                        while (hd < 2 && ((*s >= '0' && *s <= '9') || \
                                               (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'))) { \
                                            char hc = *s; \
                                            hval = hval * 16 + (hc >= 'a' ? hc-'a'+10 : hc >= 'A' ? hc-'A'+10 : hc-'0'); \
                                            s++; hd++; \
                                        } \
                                        s--; \
                                        if (hd > 0) fprintf(gen->output, "\\x%02x", hval & 0xFF); \
                                        else        fprintf(gen->output, "\\\\x"); \
                                    } else if (esc >= '0' && esc <= '7') { \
                                        s++; \
                                        int oval = esc - '0', od = 1; \
                                        while (od < 3 && *(s+1) >= '0' && *(s+1) <= '7') { \
                                            s++; oval = oval * 8 + (*s - '0'); od++; \
                                        } \
                                        fprintf(gen->output, "\\x%02x", oval & 0xFF); \
                                    } else { \
                                        fprintf(gen->output, "\\\\"); \
                                    } \
                                    break; \
                                } \
                                default: \
                                    if ((unsigned char)*s < 0x20 || *s == 0x7F) \
                                        fprintf(gen->output, "\\x%02x", (unsigned char)*s); \
                                    else \
                                        fputc(*s, gen->output); \
                                    break; \
                            } \
                        } \
                    } else { \
                        TypeKind tk = (ch->node_type) ? ch->node_type->kind : TYPE_UNKNOWN; \
                        switch (tk) { \
                            case TYPE_INT:    fprintf(gen->output, "%%d");  break; \
                            case TYPE_INT64:  fprintf(gen->output, "%%lld"); break; \
                            case TYPE_UINT64: fprintf(gen->output, "%%llu"); break; \
                            case TYPE_FLOAT:  fprintf(gen->output, "%%g");  break; \
                            case TYPE_BOOL:   fprintf(gen->output, "%%s");  break; \
                            case TYPE_STRING: fprintf(gen->output, "%%s");  break; \
                            case TYPE_PTR:    fprintf(gen->output, "%%s");  break; \
                            default:          fprintf(gen->output, "%%d");  break; \
                        } \
                    } \
                } \
            } while(0)

            // Helper macro: emit the arguments for both modes
            #define EMIT_INTERP_ARGS() do { \
                for (int i = 0; i < expr->child_count; i++) { \
                    ASTNode* ch = expr->children[i]; \
                    if (ch->type == AST_LITERAL && ch->node_type && ch->node_type->kind == TYPE_STRING) \
                        continue; \
                    fprintf(gen->output, ", "); \
                    TypeKind tk = ch->node_type ? ch->node_type->kind : TYPE_UNKNOWN; \
                    if (tk == TYPE_BOOL) { \
                        generate_expression(gen, ch); \
                        fprintf(gen->output, " ? \"true\" : \"false\""); \
                    } else if (tk == TYPE_STRING || tk == TYPE_PTR) { \
                        fprintf(gen->output, "_aether_safe_str("); \
                        generate_expression(gen, ch); \
                        fprintf(gen->output, ")"); \
                    } else if (tk == TYPE_INT64) { \
                        fprintf(gen->output, "(long long)"); \
                        generate_expression(gen, ch); \
                    } else if (tk == TYPE_UINT64) { \
                        fprintf(gen->output, "(unsigned long long)"); \
                        generate_expression(gen, ch); \
                    } else { \
                        generate_expression(gen, ch); \
                    } \
                } \
            } while(0)

            if (gen->interp_as_printf) {
                // Mode 1: direct printf (for print/println)
                fprintf(gen->output, "printf(\"");
                EMIT_INTERP_FMT();
                fprintf(gen->output, "\"");
                EMIT_INTERP_ARGS();
                fprintf(gen->output, ")");
            } else {
                // Mode 2: heap-allocated C string — always use portable helper function
                fprintf(gen->output, "_aether_interp(\"");
                EMIT_INTERP_FMT();
                fprintf(gen->output, "\"");
                EMIT_INTERP_ARGS();
                fprintf(gen->output, ")");
            }

            #undef EMIT_INTERP_FMT
            #undef EMIT_INTERP_ARGS
            break;
        }

        case AST_ACTOR_REF:
            if (!expr->value) { fprintf(gen->output, "NULL"); break; }
            if (strcmp(expr->value, "self") == 0) {
                if (gen->current_actor) {
                    // Inside actor handler: self is the function parameter
                    fprintf(gen->output, "(ActorBase*)self");
                } else {
                    fprintf(gen->output, "NULL /* self outside actor */");
                }
            } else {
                fprintf(gen->output, "%s", expr->value);
            }
            break;
        
        case AST_ARRAY_LITERAL:
            fprintf(gen->output, "{");
            for (int i = 0; i < expr->child_count; i++) {
                if (i > 0) fprintf(gen->output, ", ");
                generate_expression(gen, expr->children[i]);
            }
            fprintf(gen->output, "}");
            break;
        
        case AST_STRUCT_LITERAL:
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
            if (expr->child_count >= 2) {
                generate_expression(gen, expr->children[0]);
                fprintf(gen->output, "[");
                generate_expression(gen, expr->children[1]);
                fprintf(gen->output, "]");
            }
            break;
        
        case AST_SEND_FIRE_FORGET:
            if (expr->child_count >= 2) {
                ASTNode* target = expr->children[0];
                ASTNode* message = expr->children[1];
                
                if (message && message->type == AST_MESSAGE_CONSTRUCTOR) {
                    MessageDef* msg_def = lookup_message(gen->message_registry, message->value);
                    if (msg_def) {
                        const char* single_int = get_single_int_field(msg_def);
                        if (single_int) {
                            fprintf(gen->output, "{ Message _imsg = {%d, 0, ", msg_def->message_id);
                            for (int i = 0; i < message->child_count; i++) {
                                ASTNode* field_init = message->children[i];
                                if (field_init && field_init->type == AST_FIELD_INIT && field_init->child_count > 0) {
                                    // Actor refs passed in int message fields need intptr_t cast
                                    // to avoid pointer-to-int conversion errors in payload_int.
                                    ASTNode* val = field_init->children[0];
                                    int is_actor_ref = val->node_type && val->node_type->kind == TYPE_ACTOR_REF;
                                    if (is_actor_ref) fprintf(gen->output, "(intptr_t)");
                                    generate_expression(gen, val);
                                    break;
                                }
                            }
                            fprintf(gen->output, ", NULL, {NULL, 0, 0}}; ");

                            if (gen->in_main_loop) {
                                // Main thread loop: batch sends to reduce atomics N→num_cores
                                fprintf(gen->output, "scheduler_send_batch_add(");
                                emit_send_target(gen, target, "ActorBase*");
                                fprintf(gen->output, ", _imsg); }");
                            } else if (gen->current_actor == NULL) {
                                // Main thread, non-loop: current_core_id is always -1, local path
                                // is never taken — emit scheduler_send_remote directly (no dead branch)
                                fprintf(gen->output, "scheduler_send_remote(");
                                emit_send_target(gen, target, "ActorBase*");
                                fprintf(gen->output, ", _imsg, current_core_id); }");
                            } else {
                                // Inside an actor handler: same-core vs cross-core branch is live.
                                // Store target in temp to avoid triple-evaluation of side-effecting expressions.
                                fprintf(gen->output, "ActorBase* _send_target = ");
                                emit_send_target(gen, target, "ActorBase*");
                                fprintf(gen->output, "; ");
                                fprintf(gen->output, "if (current_core_id >= 0 && current_core_id == _send_target->assigned_core) { ");
                                fprintf(gen->output, "scheduler_send_local(_send_target, _imsg); } else { ");
                                fprintf(gen->output, "scheduler_send_remote(_send_target, _imsg, current_core_id); } }");
                            }
                        } else {
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
                            emit_send_target(gen, target, "void*");
                            fprintf(gen->output, ", &_msg, sizeof(%s)); }", message->value);
                        }
                    } else {
                        fprintf(gen->output, "/* ERROR: unknown message type %s */", message->value ? message->value : "<?>");
                    }
                }
            }
            break;

        case AST_SEND_ASK:
            if (expr->child_count >= 2) {
                ASTNode* target = expr->children[0];
                ASTNode* message = expr->children[1];
                
                if (message && message->type == AST_MESSAGE_CONSTRUCTOR) {
                    MessageDef* msg_def = lookup_message(gen->message_registry, message->value);
                    if (msg_def) {
                        // Look up the reply message type from the pre-built map
                        const char* reply_msg_name = NULL;
                        for (int r = 0; r < gen->reply_type_count; r++) {
                            if (strcmp(gen->reply_type_map[r].request_msg, message->value) == 0) {
                                reply_msg_name = gen->reply_type_map[r].reply_msg;
                                break;
                            }
                        }

                        // Find the first non-_message_id field of the reply message
                        const char* reply_field = NULL;
                        int reply_field_type = TYPE_INT;
                        if (reply_msg_name) {
                            MessageDef* reply_def = lookup_message(gen->message_registry, reply_msg_name);
                            if (reply_def && reply_def->fields) {
                                reply_field = reply_def->fields->name;
                                reply_field_type = reply_def->fields->type_kind;
                            }
                        }

                        int timeout_ms = 5000;
                        if (expr->child_count >= 3 && expr->children[2] &&
                            expr->children[2]->value) {
                            timeout_ms = atoi(expr->children[2]->value);
                        }

                        // Emit the ask expression with GCC/MSVC guards
                        fprintf(gen->output, "\n#if AETHER_GCC_COMPAT\n");
                        // GCC/Clang: statement expression
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

                        fprintf(gen->output, " }; void* _ask_r = scheduler_ask_message(");
                        emit_send_target(gen, target, "ActorBase*");
                        fprintf(gen->output, ", &_msg, sizeof(%s), %d); ", message->value, timeout_ms);

                        if (reply_msg_name && reply_field) {
                            const char* c_type = "int";
                            const char* c_zero = "0";
                            switch (reply_field_type) {
                                case TYPE_FLOAT:   c_type = "double"; c_zero = "0.0"; break;
                                case TYPE_BOOL:    c_type = "int";    c_zero = "0";   break;
                                case TYPE_STRING:  c_type = "const char*"; c_zero = "NULL"; break;
                                case TYPE_INT64:   c_type = "int64_t"; c_zero = "0";  break;
                                case TYPE_UINT64:  c_type = "uint64_t"; c_zero = "0"; break;
                                case TYPE_PTR:     c_type = "void*";  c_zero = "NULL"; break;
                                default:           c_type = "int";    c_zero = "0";   break;
                            }
                            fprintf(gen->output, "%s _ask_val = _ask_r ? ((%s*)_ask_r)->%s : %s; ",
                                    c_type, reply_msg_name, reply_field, c_zero);
                            fprintf(gen->output, "free(_ask_r); _ask_val; })");
                        } else {
                            // Fallback: return raw pointer as intptr_t
                            fprintf(gen->output, "intptr_t _ask_val = (intptr_t)(uintptr_t)_ask_r; _ask_val; })");
                        }

                        fprintf(gen->output, "\n#else\n");
                        // MSVC: use _aether_ask helper + compound literal
                        gen->ask_temp_counter++;
                        fprintf(gen->output, "_aether_ask_helper(");
                        emit_send_target(gen, target, "ActorBase*");
                        fprintf(gen->output, ", &(%s){ ._message_id = %d",
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
                        fprintf(gen->output, " }, sizeof(%s), %d, ", message->value, timeout_ms);
                        if (reply_msg_name && reply_field) {
                            fprintf(gen->output, "offsetof(%s, %s), sizeof(", reply_msg_name, reply_field);
                            // Emit the field type size based on reply_field_type
                            switch (reply_field_type) {
                                case TYPE_FLOAT:   fprintf(gen->output, "double"); break;
                                case TYPE_INT64:   fprintf(gen->output, "int64_t"); break;
                                case TYPE_UINT64:  fprintf(gen->output, "uint64_t"); break;
                                case TYPE_PTR:     fprintf(gen->output, "void*"); break;
                                case TYPE_STRING:  fprintf(gen->output, "const char*"); break;
                                default:           fprintf(gen->output, "int"); break;
                            }
                            fprintf(gen->output, "))");
                        } else {
                            fprintf(gen->output, "0, sizeof(intptr_t))");
                        }
                        fprintf(gen->output, "\n#endif\n");
                    } else {
                        fprintf(gen->output, "/* ERROR: unknown message type %s */", message->value ? message->value : "<?>");
                    }
                }
            }
            break;

        case AST_CLOSURE: {
            // Emit inline closure construction
            // The closure's value field was set to its ID by discover_closures
            int id = expr->value ? atoi(expr->value) : 0;
            int cap_count = 0;
            char** captures = NULL;
            // Find this closure's info
            for (int ci = 0; ci < gen->closure_count; ci++) {
                if (gen->closures[ci].id == id) {
                    cap_count = gen->closures[ci].capture_count;
                    captures = gen->closures[ci].captures;
                    break;
                }
            }
            if (cap_count == 0) {
                // Zero-capture closure: NULL env is safe
                fprintf(gen->output, "(_AeClosure){ .fn = (void(*)(void))_closure_fn_%d, .env = NULL }",
                        id);
            } else {
                // Heap-allocate the environment (portable, no use-after-free)
                fprintf(gen->output, "\n#if AETHER_GCC_COMPAT\n");
                fprintf(gen->output, "({ _closure_env_%d* _e = malloc(sizeof(_closure_env_%d)); ", id, id);
                for (int i = 0; i < cap_count; i++) {
                    fprintf(gen->output, "_e->%s = %s; ", safe_c_name(captures[i]), safe_c_name(captures[i]));
                }
                fprintf(gen->output, "(_AeClosure){ .fn = (void(*)(void))_closure_fn_%d, .env = _e }; })", id);
                fprintf(gen->output, "\n#else\n");
                // MSVC: use _aether_make_closure helper (emitted in preamble)
                fprintf(gen->output, "_aether_make_closure_%d(", id);
                for (int i = 0; i < cap_count; i++) {
                    if (i > 0) fprintf(gen->output, ", ");
                    fprintf(gen->output, "%s", safe_c_name(captures[i]));
                }
                fprintf(gen->output, ")");
                fprintf(gen->output, "\n#endif\n");
            }
            break;
        }

        case AST_CLOSURE_PARAM:
            // Should not be generated directly
            break;

        default:
            for (int i = 0; i < expr->child_count; i++) {
                generate_expression(gen, expr->children[i]);
            }
            break;
    }
}
