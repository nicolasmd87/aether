#include "codegen_internal.h"
#include "optimizer.h"

// ============================================================================
// ARITHMETIC SERIES LOOP COLLAPSE
//
// Detects while loops of the form:
//   while counter < bound {
//       acc1 = acc1 + invariant_expr1    // any number of accumulators
//       acc2 = acc2 + invariant_expr2
//       counter = counter + step         // must be a positive literal step
//   }
//
// And replaces them with closed-form O(1) expressions:
//   acc1 = acc1 + invariant_expr1 * (bound - counter);
//   acc2 = acc2 + invariant_expr2 * (bound - counter);
//   counter = bound;
//
// Works for any starting value of counter and any bound expression (even
// runtime variables) — the formula (bound - counter) computes remaining
// iterations correctly regardless of initial state.
//
// Also handles "counter <= bound" (adds one extra iteration).
// Also handles step != 1 via division.
// ============================================================================

#define MAX_SERIES_ACCUMULATORS 16

// Returns 1 if the expression tree references the named variable.
static int expr_references_var(ASTNode* node, const char* var_name) {
    if (!node || !var_name) return 0;
    if (node->type == AST_IDENTIFIER && node->value &&
        strcmp(node->value, var_name) == 0) return 1;
    for (int i = 0; i < node->child_count; i++) {
        if (expr_references_var(node->children[i], var_name)) return 1;
    }
    return 0;
}

// Returns 1 if the expression has any side effects (function calls, sends).
static int expr_has_side_effects(ASTNode* node) {
    if (!node) return 0;
    if (node->type == AST_FUNCTION_CALL ||
        node->type == AST_SEND_FIRE_FORGET ||
        node->type == AST_SEND_ASK) return 1;
    for (int i = 0; i < node->child_count; i++) {
        if (expr_has_side_effects(node->children[i])) return 1;
    }
    return 0;
}

// Try to detect and emit a collapsed arithmetic series loop.
// Returns 1 if the loop was collapsed and emitted; 0 otherwise (caller emits normally).
static int try_emit_series_collapse(CodeGenerator* gen, ASTNode* while_node) {
    if (!while_node || while_node->child_count < 2) return 0;

    ASTNode* condition = while_node->children[0];
    ASTNode* body      = while_node->children[1];

    // 1. Condition must be "counter < bound" or "counter <= bound"
    if (!condition || condition->type != AST_BINARY_EXPRESSION || !condition->value) return 0;
    int is_lt  = strcmp(condition->value, "<")  == 0;
    int is_lte = strcmp(condition->value, "<=") == 0;
    if (!is_lt && !is_lte) return 0;
    if (condition->child_count < 2) return 0;

    ASTNode* cond_left  = condition->children[0];   // the counter
    ASTNode* cond_right = condition->children[1];   // the bound

    if (!cond_left || cond_left->type != AST_IDENTIFIER || !cond_left->value) return 0;
    const char* counter_var = cond_left->value;

    // Bound must not have side effects
    if (expr_has_side_effects(cond_right)) return 0;

    // 2. Body: get statement list
    ASTNode** stmts;
    int stmt_count;
    if (!body) return 0;
    if (body->type == AST_BLOCK && body->child_count == 1 &&
        body->children[0] && body->children[0]->type == AST_BLOCK) {
        body = body->children[0];
    }
    if (body->type == AST_BLOCK) {
        stmts      = body->children;
        stmt_count = body->child_count;
    } else {
        stmts      = &body;
        stmt_count = 1;
    }
    if (stmt_count == 0) return 0;

    // 3. Parse each statement
    const char* acc_vars[MAX_SERIES_ACCUMULATORS];
    ASTNode*    acc_addends[MAX_SERIES_ACCUMULATORS];
    int         acc_is_linear[MAX_SERIES_ACCUMULATORS];   // 1 = addend is counter (linear sum)
    double      acc_linear_scale[MAX_SERIES_ACCUMULATORS]; // scale for counter*C pattern
    int         acc_count        = 0;
    int         found_counter    = 0;
    double      counter_step     = 1.0;

    // Also collect the set of target variable names for later checks.
    const char* stmt_targets[MAX_SERIES_ACCUMULATORS + 1];  // +1 for counter
    int stmt_target_count = 0;

    for (int i = 0; i < stmt_count; i++) {
        ASTNode* s = stmts[i];
        if (!s) return 0;

        // Every statement must be an assignment of the form: target = target + expr
        // The parser emits AST_VARIABLE_DECLARATION for all "x = expr" statements:
        //   s->value      = target variable name
        //   s->children[0] = RHS expression
        if (s->type != AST_VARIABLE_DECLARATION) return 0;
        if (!s->value || s->child_count < 1) return 0;

        const char* target = s->value;
        ASTNode*    rhs    = s->children[0];

        if (!rhs || rhs->type != AST_BINARY_EXPRESSION) return 0;
        if (!rhs->value || strcmp(rhs->value, "+") != 0) return 0;
        if (rhs->child_count < 2) return 0;

        ASTNode* rhs_left  = rhs->children[0];
        ASTNode* rhs_right = rhs->children[1];

        // Identify the "self" side and the "addend" side
        int left_is_self  = rhs_left  && rhs_left->type  == AST_IDENTIFIER &&
                            rhs_left->value  && strcmp(rhs_left->value,  target) == 0;
        int right_is_self = rhs_right && rhs_right->type == AST_IDENTIFIER &&
                            rhs_right->value && strcmp(rhs_right->value, target) == 0;
        if (!left_is_self && !right_is_self) return 0;

        ASTNode* addend = left_is_self ? rhs_right : rhs_left;

        // Track this target for bound-mutation check later
        if (stmt_target_count < MAX_SERIES_ACCUMULATORS + 1)
            stmt_targets[stmt_target_count++] = target;

        if (strcmp(target, counter_var) == 0) {
            // Counter increment: must be a positive literal step
            if (addend->type != AST_LITERAL || !addend->value) return 0;
            counter_step = atof(addend->value);
            if (counter_step <= 0.0) return 0;
            found_counter = 1;
        } else {
            // Accumulator: addend is either loop-invariant (constant series)
            // or the counter variable itself / counter*C (linear sum: Σ i = n*(n-1)/2).
            if (acc_count >= MAX_SERIES_ACCUMULATORS) return 0;

            int addend_is_counter = 0;
            double linear_scale = 1.0;

            if (addend->type == AST_IDENTIFIER && addend->value &&
                strcmp(addend->value, counter_var) == 0) {
                // Plain counter addend: acc = acc + i
                addend_is_counter = 1;
            } else if (addend->type == AST_BINARY_EXPRESSION && addend->value &&
                       strcmp(addend->value, "*") == 0 && addend->child_count >= 2) {
                // Possibly scaled counter: acc = acc + i * C  or  acc = acc + C * i
                ASTNode* ml = addend->children[0];
                ASTNode* mr = addend->children[1];
                if (ml && ml->type == AST_IDENTIFIER && ml->value &&
                    strcmp(ml->value, counter_var) == 0 &&
                    mr && mr->type == AST_LITERAL && mr->value) {
                    addend_is_counter = 1;
                    linear_scale = atof(mr->value);
                } else if (mr && mr->type == AST_IDENTIFIER && mr->value &&
                           strcmp(mr->value, counter_var) == 0 &&
                           ml && ml->type == AST_LITERAL && ml->value) {
                    addend_is_counter = 1;
                    linear_scale = atof(ml->value);
                }
            }

            if (addend_is_counter) {
                acc_vars[acc_count]          = target;
                acc_addends[acc_count]       = addend;
                acc_is_linear[acc_count]     = 1;
                acc_linear_scale[acc_count]  = linear_scale;
            } else {
                // Regular invariant addend: must not reference counter
                if (expr_references_var(addend, counter_var)) return 0;
                if (expr_has_side_effects(addend)) return 0;
                acc_vars[acc_count]          = target;
                acc_addends[acc_count]       = addend;
                acc_is_linear[acc_count]     = 0;
                acc_linear_scale[acc_count]  = 0.0;
            }
            acc_count++;
        }
    }

    if (!found_counter) return 0;

    // Linear sums require step = 1 (the triangular formula doesn't generalize cleanly to other steps).
    for (int i = 0; i < acc_count; i++) {
        if (acc_is_linear[i] && counter_step != 1.0) return 0;
    }

    // 3b. Bound-mutation check: if any loop body statement assigns to a variable
    // referenced in the bound expression, the bound changes per-iteration.
    for (int i = 0; i < stmt_target_count; i++) {
        if (expr_references_var(cond_right, stmt_targets[i])) return 0;
    }

    // 3c. Addend invariance check: verify no addend references a variable modified
    // by any other statement in the loop body.
    // Skip for linear accumulators — their "addend" is the counter itself, which is
    // expected to be in the write-set; the formula accounts for that by design.
    for (int i = 0; i < acc_count; i++) {
        if (acc_is_linear[i]) continue;
        for (int j = 0; j < stmt_target_count; j++) {
            if (expr_references_var(acc_addends[i], stmt_targets[j])) return 0;
        }
    }

    // 4. Emit collapsed form, wrapped in a guard matching the original condition.
    // The guard is needed so that when counter >= bound (loop would not execute
    // at all), the accumulators are left unchanged — without it, the formula
    // (bound - counter) is zero or negative and could corrupt the accumulator.
    print_indent(gen);
    fprintf(gen->output, "if ((%s) %s (", counter_var, is_lte ? "<=" : "<");
    generate_expression(gen, cond_right);
    fprintf(gen->output, ")) {\n");
    indent(gen);

    // Emit each accumulator update.
    // Constant addend: acc = acc + addend * trip_count
    // Linear addend:   acc = acc + scale * (bound*(bound±1)/2 - counter*(counter-1)/2)
    int emitted_linear = 0;
    for (int i = 0; i < acc_count; i++) {
        print_indent(gen);
        if (acc_is_linear[i]) {
            // Triangular-number closed form:
            //   Σ(j = counter .. bound-1) j  =  bound*(bound-1)/2 - counter*(counter-1)/2
            //   Σ(j = counter .. bound)   j  =  bound*(bound+1)/2 - counter*(counter-1)/2
            if (acc_linear_scale[i] != 1.0) {
                fprintf(gen->output, "%s = %s + %g * (", acc_vars[i], acc_vars[i], acc_linear_scale[i]);
            } else {
                fprintf(gen->output, "%s = %s + (", acc_vars[i], acc_vars[i]);
            }
            fprintf(gen->output, "(");
            generate_expression(gen, cond_right);
            if (is_lte) {
                fprintf(gen->output, ") * ((");
                generate_expression(gen, cond_right);
                fprintf(gen->output, ") + 1)");
            } else {
                fprintf(gen->output, ") * ((");
                generate_expression(gen, cond_right);
                fprintf(gen->output, ") - 1)");
            }
            fprintf(gen->output, " / 2 - %s * (%s - 1) / 2);\n", counter_var, counter_var);
            emitted_linear = 1;
        } else {
            // Constant addend: multiply by trip count
            fprintf(gen->output, "%s = %s + (", acc_vars[i], acc_vars[i]);
            generate_expression(gen, acc_addends[i]);
            fprintf(gen->output, ") * (");
            if (counter_step == 1.0) {
                fprintf(gen->output, "(");
                generate_expression(gen, cond_right);
                fprintf(gen->output, ") - %s", counter_var);
            } else {
                fprintf(gen->output, "((");
                generate_expression(gen, cond_right);
                fprintf(gen->output, ") - %s) / %g", counter_var, counter_step);
            }
            if (is_lte) {
                fprintf(gen->output, " + 1");
            }
            fprintf(gen->output, ");\n");
        }
    }

    // counter = bound (or bound + step for <=)
    print_indent(gen);
    fprintf(gen->output, "%s = (", counter_var);
    generate_expression(gen, cond_right);
    if (is_lte) {
        fprintf(gen->output, ") + %g;\n", counter_step);
    } else {
        fprintf(gen->output, ");\n");
    }

    unindent(gen);
    print_indent(gen);
    fprintf(gen->output, "}\n");

    if (emitted_linear) {
        global_opt_stats.linear_loops_collapsed++;
    } else {
        global_opt_stats.series_loops_collapsed++;
    }
    return 1;
}

static void generate_list_pattern_condition(CodeGenerator* gen, ASTNode* pattern,
                                            const char* len_name) {
    if (!pattern) return;

    if (pattern->type == AST_PATTERN_LIST) {
        if (pattern->child_count == 0) {
            fprintf(gen->output, "%s == 0", len_name);
        } else {
            fprintf(gen->output, "%s == %d", len_name, pattern->child_count);
        }
    } else if (pattern->type == AST_PATTERN_CONS) {
        fprintf(gen->output, "%s >= 1", len_name);
    }
}

static void generate_list_pattern_bindings(CodeGenerator* gen, ASTNode* pattern,
                                           const char* array_name, const char* len_name) {
    if (!pattern) return;

    if (pattern->type == AST_PATTERN_LIST && pattern->child_count > 0) {
        for (int i = 0; i < pattern->child_count; i++) {
            ASTNode* elem = pattern->children[i];
            if (elem && elem->type == AST_PATTERN_VARIABLE && elem->value) {
                print_line(gen, "int %s = %s[%d];", elem->value, array_name, i);
            }
        }
    } else if (pattern->type == AST_PATTERN_CONS && pattern->child_count >= 2) {
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

// Destructor registry: maps constructor function names to their free functions.
// Used by auto-free mode to inject scope-exit cleanup for stdlib allocations.
static const char* lookup_destructor(ASTNode* init_expr) {
    if (!init_expr || init_expr->type != AST_FUNCTION_CALL || !init_expr->value) return NULL;

    static const struct { const char* fn; const char* free_fn; } REGISTRY[] = {
        { "map_new",    "map_free"      },
        { "list_new",   "list_free"     },
        { "map_keys",   "map_keys_free" },
        { "dir_list",   "dir_list_free" },
        { "string_new", "string_free"   },
        { NULL,         NULL            }
    };

    for (int i = 0; REGISTRY[i].fn; i++) {
        if (strcmp(init_expr->value, REGISTRY[i].fn) == 0) {
            return REGISTRY[i].free_fn;
        }
    }
    return NULL;
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

                    // Auto-free: if in auto mode, the variable is not @manual, and
                    // the initializer calls a registered *_new() function, push a
                    // scope-exit defer to call the corresponding *_free() function.
                    if (!gen->no_auto_free && !stmt->is_manual &&
                        stmt->child_count > 0 && stmt->value) {
                        const char* free_fn = lookup_destructor(stmt->children[0]);
                        if (free_fn) {
                            push_auto_defer(gen, free_fn, stmt->value);
                        }
                    }
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
            
        case AST_WHILE_LOOP: {
            // OPTIMIZATION: Try to collapse arithmetic series loops into O(1) expressions.
            // Only attempt when not inside actors and no sends (sends need batch treatment).
            int has_sends = contains_send_expression(stmt);
            if (!has_sends && try_emit_series_collapse(gen, stmt)) {
                break;  // collapsed — done
            }

            // Batch optimization: only in main() (not inside actors)
            // Uses queue_enqueue_batch to reduce atomics from N to num_cores
            if (has_sends && gen->current_actor == NULL) {
                print_line(gen, "scheduler_send_batch_start();");
                gen->in_main_loop = 1;
            }

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

            if (has_sends && gen->current_actor == NULL) {
                print_line(gen, "scheduler_send_batch_flush();");
                gen->in_main_loop = 0;
            }
            break;
        }
            
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
            
        case AST_RETURN_STATEMENT: {
            // In main(), all returns go through main_exit so scheduler_wait() always runs
            if (gen->in_main_function) {
                if (gen->defer_count > 0) {
                    emit_all_defers(gen);
                }
                print_indent(gen);
                if (stmt->child_count > 0 && stmt->children[0] &&
                    stmt->children[0]->type != AST_PRINT_STATEMENT) {
                    fprintf(gen->output, "main_exit_ret = ");
                    generate_expression(gen, stmt->children[0]);
                    fprintf(gen->output, "; goto main_exit;\n");
                } else {
                    if (stmt->child_count > 0 && stmt->children[0] &&
                        stmt->children[0]->type == AST_PRINT_STATEMENT) {
                        generate_statement(gen, stmt->children[0]);
                        print_indent(gen);
                    }
                    print_line(gen, "goto main_exit;");
                }
                break;
            }
            // Emit ALL defers before return (unwind entire function)
            if (gen->defer_count > 0) {
                // For return with value, save to temp first
                if (stmt->child_count > 0 && stmt->children[0] &&
                    stmt->children[0]->type != AST_PRINT_STATEMENT) {
                    print_indent(gen);
                    // Determine return type from expression
                    Type* ret_type = stmt->children[0]->node_type;
                    if (ret_type && ret_type->kind == TYPE_STRING) {
                        fprintf(gen->output, "const char* _defer_ret = ");
                    } else if (ret_type && ret_type->kind == TYPE_FLOAT) {
                        fprintf(gen->output, "double _defer_ret = ");
                    } else {
                        fprintf(gen->output, "int _defer_ret = ");
                    }
                    generate_expression(gen, stmt->children[0]);
                    fprintf(gen->output, ";\n");
                    emit_all_defers(gen);
                    print_line(gen, "return _defer_ret;");
                } else if (stmt->child_count > 0 && stmt->children[0] &&
                           stmt->children[0]->type == AST_PRINT_STATEMENT) {
                    emit_all_defers(gen);
                    generate_statement(gen, stmt->children[0]);
                    print_line(gen, "return;");
                } else {
                    emit_all_defers(gen);
                    print_line(gen, "return;");
                }
            } else {
                // No defers - original behavior
                if (stmt->child_count > 0 && stmt->children[0] &&
                    stmt->children[0]->type == AST_PRINT_STATEMENT) {
                    generate_statement(gen, stmt->children[0]);
                    print_line(gen, "return;");
                } else {
                    print_indent(gen);
                    fprintf(gen->output, "return");
                    if (stmt->child_count > 0) {
                        fprintf(gen->output, " ");
                        generate_expression(gen, stmt->children[0]);
                    }
                    fprintf(gen->output, ";\n");
                }
            }
            break;
        }
            
        case AST_BREAK_STATEMENT:
            // Emit defers for current scope before break
            emit_defers_for_scope(gen);
            print_line(gen, "break;");
            break;

        case AST_CONTINUE_STATEMENT:
            // Emit defers for current scope before continue
            emit_defers_for_scope(gen);
            print_line(gen, "continue;");
            break;

        case AST_DEFER_STATEMENT:
            // Push deferred statement to stack - will be executed at scope exit
            if (stmt->child_count > 0) {
                push_defer(gen, stmt->children[0]);
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

                // Interpolated string: delegate directly to expression codegen (emits printf(...))
                if (stmt->child_count == 1 && first_arg->type == AST_STRING_INTERP) {
                    generate_expression(gen, first_arg);
                    fprintf(gen->output, ";\n");
                    break;
                }

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
            enter_scope(gen);  // Track defer scope
            for (int i = 0; i < stmt->child_count; i++) {
                generate_statement(gen, stmt->children[i]);
            }
            exit_scope(gen);  // Emit defers and pop scope
            unindent(gen);
            print_line(gen, "}");
            break;
        
        // Actor V2 - Reply statement
        // NOTE: Reply constructs the message but scheduler-based actors don't have
        // the request-tracking infrastructure yet. The reply message is logged but
        // not actually sent back to caller. Full ask/reply needs ActorBase changes.
        case AST_REPLY_STATEMENT:
            if (stmt->child_count > 0) {
                ASTNode* reply_expr = stmt->children[0];

                if (reply_expr->type == AST_MESSAGE_CONSTRUCTOR) {
                    MessageDef* msg_def = lookup_message(gen->message_registry, reply_expr->value);
                    if (msg_def) {
                        print_indent(gen);
                        // Construct the reply message (validates fields at compile time)
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

                        // Send reply back to the waiting asker via the scheduler reply slot.
                        fprintf(gen->output, " }; scheduler_reply((ActorBase*)self, &_reply, sizeof(%s)); }\n",
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
