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
            // Cast to int64_t to prevent overflow for large N.
            // e.g., N=100000: N*(N-1)/2 = 4999950000 which exceeds int32 max.
            fprintf(gen->output, "(int64_t)(");
            generate_expression(gen, cond_right);
            if (is_lte) {
                fprintf(gen->output, ") * ((int64_t)(");
                generate_expression(gen, cond_right);
                fprintf(gen->output, ") + 1)");
            } else {
                fprintf(gen->output, ") * ((int64_t)(");
                generate_expression(gen, cond_right);
                fprintf(gen->output, ") - 1)");
            }
            fprintf(gen->output, " / 2 - (int64_t)%s * ((int64_t)%s - 1) / 2);\n", counter_var, counter_var);
            emitted_linear = 1;
        } else {
            // Constant addend: multiply by trip count (int64 to prevent overflow)
            fprintf(gen->output, "%s = %s + (int64_t)(", acc_vars[i], acc_vars[i]);
            generate_expression(gen, acc_addends[i]);
            fprintf(gen->output, ") * (");
            if (counter_step == 1.0) {
                fprintf(gen->output, "(int64_t)(");
                generate_expression(gen, cond_right);
                fprintf(gen->output, ") - %s", counter_var);
            } else {
                fprintf(gen->output, "((int64_t)(");
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

// Check if any binding in the pattern is actually used by the arm body
static int pattern_needs_array(ASTNode* pattern, ASTNode* body) {
    if (!pattern || !body) return 0;
    if (pattern->type == AST_PATTERN_LIST) {
        for (int i = 0; i < pattern->child_count; i++) {
            ASTNode* elem = pattern->children[i];
            if (elem && elem->type == AST_PATTERN_VARIABLE && elem->value &&
                expr_references_var(body, elem->value)) return 1;
        }
    } else if (pattern->type == AST_PATTERN_CONS && pattern->child_count >= 2) {
        ASTNode* head = pattern->children[0];
        ASTNode* tail = pattern->children[1];
        if (head && head->type == AST_PATTERN_VARIABLE && head->value &&
            expr_references_var(body, head->value)) return 1;
        if (tail && tail->type == AST_PATTERN_VARIABLE && tail->value &&
            expr_references_var(body, tail->value)) return 1;
    }
    return 0;
}

static void generate_list_pattern_bindings(CodeGenerator* gen, ASTNode* pattern,
                                           ASTNode* match_expr, const char* len_name,
                                           ASTNode* body) {
    if (!pattern) return;

    // Only declare the array pointer if this arm actually uses element bindings
    int needs_arr = pattern_needs_array(pattern, body);
    if (needs_arr) {
        print_indent(gen);
        fprintf(gen->output, "int* _match_arr = ");
        generate_expression(gen, match_expr);
        fprintf(gen->output, ";\n");
    }

    if (pattern->type == AST_PATTERN_LIST && pattern->child_count > 0) {
        for (int i = 0; i < pattern->child_count; i++) {
            ASTNode* elem = pattern->children[i];
            if (elem && elem->type == AST_PATTERN_VARIABLE && elem->value) {
                if (expr_references_var(body, elem->value)) {
                    print_line(gen, "int %s = _match_arr[%d];", elem->value, i);
                }
            }
        }
    } else if (pattern->type == AST_PATTERN_CONS && pattern->child_count >= 2) {
        ASTNode* head = pattern->children[0];
        ASTNode* tail = pattern->children[1];

        if (head && head->type == AST_PATTERN_VARIABLE && head->value) {
            if (expr_references_var(body, head->value)) {
                print_line(gen, "int %s = _match_arr[0];", head->value);
            }
        }
        if (tail && tail->type == AST_PATTERN_VARIABLE && tail->value) {
            if (expr_references_var(body, tail->value)) {
                print_line(gen, "int* %s = &_match_arr[1];", tail->value);
                print_line(gen, "int %s_len = %s - 1;", tail->value, len_name);
            }
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

void generate_statement(CodeGenerator* gen, ASTNode* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case AST_CONST_DECLARATION: {
            // Local constant: const <type> <name> = <value>;
            if (stmt->value && stmt->child_count > 0) {
                mark_var_declared(gen, stmt->value);
                Type* var_type = stmt->node_type;
                if ((!var_type || var_type->kind == TYPE_VOID || var_type->kind == TYPE_UNKNOWN)
                    && stmt->children[0] && stmt->children[0]->node_type) {
                    var_type = stmt->children[0]->node_type;
                }
                // STRING already emits "const char*", skip extra const qualifier
                if (var_type && var_type->kind == TYPE_STRING) {
                    generate_type(gen, var_type);
                } else {
                    fprintf(gen->output, "const ");
                    generate_type(gen, var_type);
                }
                fprintf(gen->output, " %s = ", stmt->value);
                generate_expression(gen, stmt->children[0]);
                fprintf(gen->output, ";\n");
            }
            break;
        }
        case AST_TUPLE_DESTRUCTURE: {
            // a, b = func() — last child is RHS, others are variable declarations
            if (stmt->child_count < 2) break;
            int var_count = stmt->child_count - 1;
            ASTNode* rhs = stmt->children[var_count];

            // Infer tuple type from RHS
            Type* rhs_type = rhs->node_type;
            if (rhs_type && rhs_type->kind == TYPE_TUPLE) {
                ensure_tuple_typedef(gen, rhs_type);
            }

            // Generate: _tuple_X_Y _tmp = func();
            const char* tuple_type_name = rhs_type ? get_c_type(rhs_type) : "_tuple_unknown";
            static int tuple_tmp_counter = 0;
            int tmp_id = tuple_tmp_counter++;
            print_indent(gen);
            fprintf(gen->output, "%s _tup%d = ", tuple_type_name, tmp_id);
            generate_expression(gen, rhs);
            fprintf(gen->output, ";\n");

            // Generate: type a = _tmp._0; type b = _tmp._1; ...
            for (int j = 0; j < var_count; j++) {
                ASTNode* var = stmt->children[j];
                if (var->value && strcmp(var->value, "_") == 0) continue;  // Skip discard

                // Prefer tuple element type over var's node_type (may be UNKNOWN)
                const char* var_type;
                if (rhs_type && rhs_type->kind == TYPE_TUPLE && j < rhs_type->tuple_count &&
                    rhs_type->tuple_types[j]->kind != TYPE_UNKNOWN) {
                    var_type = get_c_type(rhs_type->tuple_types[j]);
                } else {
                    var_type = get_c_type(var->node_type);
                }
                print_indent(gen);
                if (is_var_declared(gen, var->value)) {
                    fprintf(gen->output, "%s = _tup%d._%d;\n", var->value, tmp_id, j);
                } else {
                    mark_var_declared(gen, var->value);
                    fprintf(gen->output, "%s %s = _tup%d._%d;\n", var_type, var->value, tmp_id, j);
                }
            }
            break;
        }

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
                // Match-as-expression: x = match val { ... }
                if (stmt->child_count > 0 && stmt->children[0] &&
                    stmt->children[0]->type == AST_MATCH_STATEMENT) {
                    if (!is_var_declared(gen, stmt->value)) {
                        mark_var_declared(gen, stmt->value);
                        // Infer type from first match arm result
                        const char* c_type = get_c_type(stmt->node_type);
                        ASTNode* match_node = stmt->children[0];
                        if ((!stmt->node_type || stmt->node_type->kind == TYPE_UNKNOWN) &&
                            match_node->child_count >= 2) {
                            ASTNode* first_arm = match_node->children[1];
                            if (first_arm && first_arm->child_count >= 2 && first_arm->children[1]) {
                                Type* arm_type = first_arm->children[1]->node_type;
                                if (arm_type) c_type = get_c_type(arm_type);
                            }
                        }
                        print_indent(gen);
                        fprintf(gen->output, "%s %s;\n", c_type, stmt->value);
                    }
                    // Generate match with result assignment
                    gen->match_result_var = stmt->value;
                    generate_statement(gen, stmt->children[0]);
                    gen->match_result_var = NULL;
                    break;
                }

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
                                    if (fn && (fn->type == AST_FUNCTION_DEFINITION || fn->type == AST_BUILDER_FUNCTION)
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
                        // Check if this is a builder function with trailing block —
                        // if so, just declare the variable; the builder handler assigns later
                        int defer_with_trailing = 0;
                        if (stmt->children[0] && stmt->children[0]->type == AST_FUNCTION_CALL &&
                            stmt->children[0]->value && is_builder_func_reg(gen, stmt->children[0]->value)) {
                            for (int dtc = 0; dtc < stmt->children[0]->child_count; dtc++) {
                                ASTNode* dtarg = stmt->children[0]->children[dtc];
                                if (dtarg && dtarg->type == AST_CLOSURE &&
                                    dtarg->value && strcmp(dtarg->value, "trailing") == 0) {
                                    defer_with_trailing = 1;
                                    break;
                                }
                            }
                        }
                        if (defer_with_trailing) {
                            // Just declare — defer trailing block handler will assign
                            fprintf(gen->output, " = 0");
                        } else if (is_array_init && stmt->children[0]->child_count == 0) {
                            // Empty array literal gets NULL, not {}
                            fprintf(gen->output, " = NULL");
                        } else {
                            fprintf(gen->output, " = ");
                            generate_expression(gen, stmt->children[0]);
                        }
                    }

                    fprintf(gen->output, ";\n");
                    // Record variable→closure mapping for closure invocation
                    if (stmt->child_count > 0 && stmt->children[0] &&
                        stmt->children[0]->type == AST_CLOSURE &&
                        stmt->children[0]->value && stmt->value) {
                        int cid = atoi(stmt->children[0]->value);
                        if (gen->closure_var_count >= gen->closure_var_capacity) {
                            gen->closure_var_capacity = gen->closure_var_capacity ? gen->closure_var_capacity * 2 : 16;
                            gen->closure_var_map = realloc(gen->closure_var_map,
                                gen->closure_var_capacity * sizeof(gen->closure_var_map[0]));
                        }
                        gen->closure_var_map[gen->closure_var_count].var_name = strdup(stmt->value);
                        gen->closure_var_map[gen->closure_var_count].closure_id = cid;
                        gen->closure_var_count++;

                        // Emit deferred free for heap-allocated closure environments
                        for (int ci = 0; ci < gen->closure_count; ci++) {
                            if (gen->closures[ci].id == cid && gen->closures[ci].capture_count > 0) {
                                // Create a synthetic defer: free(var.env)
                                ASTNode* free_call = create_ast_node(AST_FUNCTION_CALL, "free",
                                    stmt->line, stmt->column);
                                char env_access[256];
                                snprintf(env_access, sizeof(env_access), "%s.env", safe_c_name(stmt->value));
                                ASTNode* env_arg = create_ast_node(AST_IDENTIFIER, env_access,
                                    stmt->line, stmt->column);
                                add_child(free_call, env_arg);
                                // Wrap in expression statement so generate_statement handles it
                                ASTNode* expr_stmt = create_ast_node(AST_EXPRESSION_STATEMENT, NULL,
                                    stmt->line, stmt->column);
                                add_child(expr_stmt, free_call);
                                push_defer(gen, expr_stmt);
                                break;
                            }
                        }
                    }
                    // Suppress unused-variable warning for arrays used with list
                    // pattern matching — the paired _len variable may be the only
                    // one used when patterns only check size ([], [_], wildcard).
                    if (is_array_init || (stmt->node_type && stmt->node_type->kind == TYPE_ARRAY)) {
                        print_line(gen, "(void)%s;", stmt->value);
                    }

                    // Handle trailing blocks on function calls used as initializers
                    // e.g., root = make_container("root") { ... }
                    if (stmt->child_count > 0 && stmt->children[0] &&
                        stmt->children[0]->type == AST_FUNCTION_CALL) {
                        ASTNode* init_call = stmt->children[0];
                        int init_is_defer = init_call->value &&
                            is_builder_func_reg(gen, init_call->value);

                        if (init_is_defer) {
                            // DEFER PATTERN for assignment: block first, then call
                            // The variable was already declared with func(args, (void*)0)
                            // We need to redo it: create config, run block, reassign with config
                            for (int tc = 0; tc < init_call->child_count; tc++) {
                                ASTNode* trailing = init_call->children[tc];
                                if (trailing && trailing->type == AST_CLOSURE &&
                                    trailing->value && strcmp(trailing->value, "trailing") == 0) {
                                    for (int bi = 0; bi < trailing->child_count; bi++) {
                                        if (trailing->children[bi] &&
                                            trailing->children[bi]->type == AST_BLOCK) {
                                            // Open block scope for _bcfg
                                            print_indent(gen);
                                            fprintf(gen->output, "{\n");
                                            gen->indent_level++;
                                            print_indent(gen);
                                            fprintf(gen->output, "void* _bcfg = %s();\n",
                                                    get_builder_factory(gen, init_call->value));
                                            print_indent(gen);
                                            fprintf(gen->output, "_aether_ctx_push(_bcfg);\n");
                                            // Run trailing block
                                            print_indent(gen);
                                            fprintf(gen->output, "{\n");
                                            gen->indent_level++;
                                            gen->in_trailing_block++;
                                            ASTNode* body = trailing->children[bi];
                                            for (int si = 0; si < body->child_count; si++) {
                                                generate_statement(gen, body->children[si]);
                                            }
                                            gen->in_trailing_block--;
                                            gen->indent_level--;
                                            print_indent(gen);
                                            fprintf(gen->output, "}\n");
                                            print_indent(gen);
                                            fprintf(gen->output, "_aether_ctx_pop();\n");
                                            // Reassign variable with defer config
                                            print_indent(gen);
                                            char c_dfn[256];
                                            strncpy(c_dfn, safe_c_name(init_call->value), sizeof(c_dfn) - 1);
                                            c_dfn[sizeof(c_dfn) - 1] = '\0';
                                            for (char* p = c_dfn; *p; p++) { if (*p == '.') *p = '_'; }
                                            fprintf(gen->output, "%s = %s(",
                                                    safe_c_name(stmt->value), c_dfn);
                                            int darg = 0;
                                            for (int ai = 0; ai < init_call->child_count; ai++) {
                                                ASTNode* arg = init_call->children[ai];
                                                if (arg && arg->type == AST_CLOSURE &&
                                                    arg->value && strcmp(arg->value, "trailing") == 0) {
                                                    continue;
                                                }
                                                if (darg > 0) fprintf(gen->output, ", ");
                                                generate_expression(gen, arg);
                                                darg++;
                                            }
                                            if (darg > 0) fprintf(gen->output, ", ");
                                            fprintf(gen->output, "_bcfg);\n");
                                            gen->indent_level--;
                                            print_indent(gen);
                                            fprintf(gen->output, "}\n");
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }
                        } else {
                            // REGULAR PATTERN: function already called, push result as context
                            for (int tc = 0; tc < init_call->child_count; tc++) {
                                ASTNode* trailing = init_call->children[tc];
                                if (trailing && trailing->type == AST_CLOSURE &&
                                    trailing->value && strcmp(trailing->value, "trailing") == 0) {
                                    for (int bi = 0; bi < trailing->child_count; bi++) {
                                        if (trailing->children[bi] &&
                                            trailing->children[bi]->type == AST_BLOCK) {
                                            // Push the variable's value as builder context
                                            print_indent(gen);
                                            fprintf(gen->output, "_aether_ctx_push((void*)(intptr_t)%s);\n",
                                                    safe_c_name(stmt->value));
                                            print_indent(gen);
                                            fprintf(gen->output, "{\n");
                                            gen->indent_level++;
                                            gen->in_trailing_block++;
                                            ASTNode* body = trailing->children[bi];
                                            for (int si = 0; si < body->child_count; si++) {
                                                generate_statement(gen, body->children[si]);
                                            }
                                            gen->in_trailing_block--;
                                            gen->indent_level--;
                                            print_indent(gen);
                                            fprintf(gen->output, "}\n");
                                            print_indent(gen);
                                            fprintf(gen->output, "_aether_ctx_pop();\n");
                                            break;
                                        }
                                    }
                                }
                            }
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

        case AST_COMPOUND_ASSIGNMENT: {
            // node->value = variable name, children[0] = operator literal, children[1] = RHS
            if (stmt->child_count >= 2 && stmt->value && stmt->children[0] && stmt->children[0]->value) {
                const char* op = stmt->children[0]->value;  // "+=", "-=", etc.

                // Check if this is a state variable in an actor
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
                    fprintf(gen->output, "self->%s %s ", stmt->value, op);
                } else {
                    fprintf(gen->output, "%s %s ", stmt->value, op);
                }
                generate_expression(gen, stmt->children[1]);
                fprintf(gen->output, ";\n");
            }
            break;
        }

        case AST_IF_STATEMENT:
            fprintf(gen->output, "if (");
            if (stmt->child_count > 0) {
                gen->in_condition = 1;
                generate_expression(gen, stmt->children[0]);
                gen->in_condition = 0;
            }
            fprintf(gen->output, ") {\n");

            {
                // Save declared_var_count before if-body.  Variables declared
                // inside if/else blocks live in separate C scopes and must not
                // leak to sibling statements (fixes Issue #2: sibling if blocks
                // re-using the same variable name).
                int saved_var_count = gen->declared_var_count;

                indent(gen);
                if (stmt->child_count > 1) {
                    generate_statement(gen, stmt->children[1]);
                }
                unindent(gen);

                if (stmt->child_count > 2) {
                    // Restore: else-branch sees only pre-if declarations.
                    gen->declared_var_count = saved_var_count;

                    print_line(gen, "} else {");
                    indent(gen);
                    generate_statement(gen, stmt->children[2]);
                    unindent(gen);
                }

                // Restore after entire if/else: variables declared inside
                // if/else blocks do not leak to subsequent sibling statements.
                gen->declared_var_count = saved_var_count;
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
            if (gen->preempt_loops) {
                print_line(gen, "if (--_aether_reductions <= 0) { _aether_reductions = 10000; sched_yield(); }");
            }
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
            // Cooperative preemption: yield to OS at loop back-edges
            if (gen->preempt_loops) {
                print_line(gen, "if (--_aether_reductions <= 0) { _aether_reductions = 10000; sched_yield(); }");
            }
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
            // becomes: { T _match_val = x; if (_match_val == 1) { a; } else if ... }
            // Using a temp variable avoids re-evaluating the match expression per arm.
            if (stmt->child_count > 0) {
                ASTNode* match_expr = stmt->children[0];

                // Check if any arm uses list patterns
                int uses_list_patterns = has_list_patterns(stmt);
                char len_name[64] = "_match_len";

                // Wrap match in a block and store the match expression in a temp
                // to avoid evaluating it multiple times (could have side effects).
                print_line(gen, "{");
                indent(gen);

                // If using list patterns, generate length variable for conditions
                if (uses_list_patterns) {
                    print_indent(gen);
                    fprintf(gen->output, "int %s = ", len_name);
                    generate_expression(gen, match_expr);
                    fprintf(gen->output, "_len;\n");
                } else {
                    // Emit temp variable for the match expression value
                    Type* mexpr_type = match_expr->node_type;
                    const char* match_c_type = "int";
                    if (mexpr_type) {
                        if (mexpr_type->kind == TYPE_STRING || mexpr_type->kind == TYPE_PTR)
                            match_c_type = "const char*";
                        else if (mexpr_type->kind == TYPE_FLOAT)
                            match_c_type = "double";
                        else if (mexpr_type->kind == TYPE_INT64)
                            match_c_type = "int64_t";
                        else if (mexpr_type->kind == TYPE_BOOL)
                            match_c_type = "bool";
                    }
                    print_indent(gen);
                    fprintf(gen->output, "%s _match_val = ", match_c_type);
                    generate_expression(gen, match_expr);
                    fprintf(gen->output, ";\n");
                }

                for (int i = 1; i < stmt->child_count; i++) {
                    ASTNode* match_arm = stmt->children[i];
                    if (!match_arm || match_arm->type != AST_MATCH_ARM || match_arm->child_count < 2) continue;

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
                        // Use _match_val (temp) instead of re-evaluating match_expr
                        Type* mexpr_type = match_expr->node_type;
                        if (mexpr_type && mexpr_type->kind == TYPE_STRING) {
                            // NULL-safe strcmp: guard with _match_val != NULL
                            fprintf(gen->output, "_match_val && strcmp(_match_val, ");
                            generate_expression(gen, pattern);
                            fprintf(gen->output, ") == 0) {\n");
                        } else {
                            fprintf(gen->output, "_match_val == ");
                            generate_expression(gen, pattern);
                            fprintf(gen->output, ") {\n");
                        }
                    }

                    indent(gen);

                    // Generate list pattern bindings if needed
                    if (is_list_pattern) {
                        generate_list_pattern_bindings(gen, pattern, match_expr, len_name, result);
                    }

                    if (result->type == AST_BLOCK) {
                        // Already a block, generate its statements
                        for (int j = 0; j < result->child_count; j++) {
                            generate_statement(gen, result->children[j]);
                        }
                    } else if (result->type == AST_PRINT_STATEMENT
                            || result->type == AST_RETURN_STATEMENT
                            || result->type == AST_VARIABLE_DECLARATION) {
                        // Statement-level node (e.g. print, return)
                        generate_statement(gen, result);
                    } else {
                        // Single expression — assign to result var or emit as statement
                        print_indent(gen);
                        if (gen->match_result_var) {
                            fprintf(gen->output, "%s = ", gen->match_result_var);
                        }
                        generate_expression(gen, result);
                        fprintf(gen->output, ";\n");
                    }
                    unindent(gen);
                    print_line(gen, "}");
                }

                // Close the match scoping block
                unindent(gen);
                print_line(gen, "}");
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
                    // Determine return type from expression (fall back to int if untyped)
                    Type* ret_type = stmt->children[0]->node_type;
                    const char* ret_c_type = (ret_type && ret_type->kind != TYPE_VOID && ret_type->kind != TYPE_UNKNOWN)
                                             ? get_c_type(ret_type) : "int";
                    fprintf(gen->output, "%s _builder_ret = ", ret_c_type);
                    generate_expression(gen, stmt->children[0]);
                    fprintf(gen->output, ";\n");
                    emit_all_defers(gen);
                    print_line(gen, "return _builder_ret;");
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
                } else if (stmt->child_count > 1) {
                    // Multi-value return: return a, b → return (_tuple_X_Y){a, b}
                    print_indent(gen);
                    // Use the function's known return type if it's a tuple
                    // (avoids UNKNOWN types from unresolved identifiers)
                    Type* tuple = NULL;
                    int owned = 0;
                    if (gen->current_func_return_type &&
                        gen->current_func_return_type->kind == TYPE_TUPLE) {
                        tuple = gen->current_func_return_type;
                    } else {
                        // Fallback: build from expression types
                        tuple = create_type(TYPE_TUPLE);
                        tuple->tuple_count = stmt->child_count;
                        tuple->tuple_types = malloc(stmt->child_count * sizeof(Type*));
                        for (int j = 0; j < stmt->child_count; j++) {
                            tuple->tuple_types[j] = stmt->children[j]->node_type
                                ? clone_type(stmt->children[j]->node_type)
                                : create_type(TYPE_INT);
                        }
                        owned = 1;
                    }
                    ensure_tuple_typedef(gen, tuple);
                    const char* tname = get_c_type(tuple);
                    fprintf(gen->output, "return (%s){", tname);
                    for (int j = 0; j < stmt->child_count; j++) {
                        if (j > 0) fprintf(gen->output, ", ");
                        generate_expression(gen, stmt->children[j]);
                    }
                    fprintf(gen->output, "};\n");
                    if (owned) free_type(tuple);
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
                ASTNode* inner = stmt->children[0];

                // Check if this function call has a trailing block
                int has_trailing = 0;
                if (inner && inner->type == AST_FUNCTION_CALL) {
                    for (int tc = 0; tc < inner->child_count; tc++) {
                        if (inner->children[tc] && inner->children[tc]->type == AST_CLOSURE &&
                            inner->children[tc]->value &&
                            strcmp(inner->children[tc]->value, "trailing") == 0) {
                            has_trailing = 1;
                            break;
                        }
                    }
                }

                // Check if this is a builder function call with trailing block
                int is_builder_call = has_trailing && inner->value &&
                    is_builder_func_reg(gen, inner->value);

                if (has_trailing && is_builder_call) {
                    // BUILDER PATTERN: block configures first, then function executes
                    // Wrap in block scope so _bcfg doesn't collide with other builder calls
                    print_indent(gen);
                    fprintf(gen->output, "{\n");
                    gen->indent_level++;

                    // 1. Create config object and push as context
                    print_indent(gen);
                    fprintf(gen->output, "void* _bcfg = %s();\n",
                            get_builder_factory(gen, inner->value));
                    print_indent(gen);
                    fprintf(gen->output, "_aether_ctx_push(_bcfg);\n");

                    // 2. Run trailing block (fills config via builder functions)
                    for (int tc = 0; tc < inner->child_count; tc++) {
                        ASTNode* trailing = inner->children[tc];
                        if (trailing && trailing->type == AST_CLOSURE &&
                            trailing->value && strcmp(trailing->value, "trailing") == 0) {
                            for (int bi = 0; bi < trailing->child_count; bi++) {
                                if (trailing->children[bi] &&
                                    trailing->children[bi]->type == AST_BLOCK) {
                                    print_indent(gen);
                                    fprintf(gen->output, "{\n");
                                    gen->indent_level++;
                                    gen->in_trailing_block++;
                                    ASTNode* body = trailing->children[bi];
                                    for (int si = 0; si < body->child_count; si++) {
                                        generate_statement(gen, body->children[si]);
                                    }
                                    gen->in_trailing_block--;
                                    gen->indent_level--;
                                    print_indent(gen);
                                    fprintf(gen->output, "}\n");
                                    break;
                                }
                            }
                        }
                    }

                    // 3. Pop context
                    print_indent(gen);
                    fprintf(gen->output, "_aether_ctx_pop();\n");

                    // 4. Call function with config as extra last arg
                    print_indent(gen);
                    char c_builder_name[256];
                    strncpy(c_builder_name, safe_c_name(inner->value), sizeof(c_builder_name) - 1);
                    c_builder_name[sizeof(c_builder_name) - 1] = '\0';
                    for (char* p = c_builder_name; *p; p++) { if (*p == '.') *p = '_'; }
                    fprintf(gen->output, "%s(", c_builder_name);
                    int arg_printed = 0;
                    for (int ai = 0; ai < inner->child_count; ai++) {
                        ASTNode* arg = inner->children[ai];
                        if (arg && arg->type == AST_CLOSURE &&
                            arg->value && strcmp(arg->value, "trailing") == 0) {
                            continue; // skip trailing block
                        }
                        if (arg_printed > 0) fprintf(gen->output, ", ");
                        generate_expression(gen, arg);
                        arg_printed++;
                    }
                    if (arg_printed > 0) fprintf(gen->output, ", ");
                    fprintf(gen->output, "_bcfg);\n");

                    gen->indent_level--;
                    print_indent(gen);
                    fprintf(gen->output, "}\n");

                } else if (has_trailing) {
                    // REGULAR PATTERN: function runs first, block decorates
                    // Check if function returns void (no return value to capture)
                    int returns_void = 1;
                    if (inner->node_type && inner->node_type->kind != TYPE_VOID &&
                        inner->node_type->kind != TYPE_UNKNOWN) {
                        returns_void = 0;
                    }
                    // Also check if function has return statements
                    if (inner->value) {
                        for (int fi = 0; fi < gen->program->child_count; fi++) {
                            ASTNode* fdef = gen->program->children[fi];
                            if (fdef && (fdef->type == AST_FUNCTION_DEFINITION || fdef->type == AST_BUILDER_FUNCTION) &&
                                fdef->value && strcmp(fdef->value, inner->value) == 0) {
                                if (has_return_value(fdef)) returns_void = 0;
                                break;
                            }
                        }
                    }

                    if (!returns_void) {
                        // Capture return value and push as context
                        print_indent(gen);
                        fprintf(gen->output, "_aether_ctx_push((void*)(intptr_t)");
                        generate_expression(gen, inner);
                        fprintf(gen->output, ");\n");
                    } else {
                        // Void function — just call it, push NULL context
                        generate_expression(gen, inner);
                        fprintf(gen->output, ";\n");
                        print_indent(gen);
                        fprintf(gen->output, "_aether_ctx_push((void*)0);\n");
                    }
                } else {
                    generate_expression(gen, inner);
                    fprintf(gen->output, ";\n");
                }

                // Trailing blocks for non-defer: emit closure body as inline statements after the call
                if (inner && inner->type == AST_FUNCTION_CALL && !is_builder_call) {
                    for (int tc = 0; tc < inner->child_count; tc++) {
                        ASTNode* trailing = inner->children[tc];
                        if (trailing && trailing->type == AST_CLOSURE &&
                            trailing->value && strcmp(trailing->value, "trailing") == 0) {
                            for (int bi = 0; bi < trailing->child_count; bi++) {
                                if (trailing->children[bi] &&
                                    trailing->children[bi]->type == AST_BLOCK) {
                                    print_indent(gen);
                                    fprintf(gen->output, "{\n");
                                    gen->indent_level++;
                                    gen->in_trailing_block++;
                                    ASTNode* body = trailing->children[bi];
                                    for (int si = 0; si < body->child_count; si++) {
                                        generate_statement(gen, body->children[si]);
                                    }
                                    gen->in_trailing_block--;
                                    gen->indent_level--;
                                    print_indent(gen);
                                    fprintf(gen->output, "}\n");

                                    // Pop the builder context
                                    if (has_trailing) {
                                        print_indent(gen);
                                        fprintf(gen->output, "_aether_ctx_pop();\n");
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            break;
            
        case AST_PRINT_STATEMENT:
            // Generate printf call with all arguments
            if (stmt->child_count > 0) {
                ASTNode* first_arg = stmt->children[0];

                // Interpolated string: delegate directly to expression codegen (emits printf(...))
                if (stmt->child_count == 1 && first_arg->type == AST_STRING_INTERP) {
                    gen->interp_as_printf = 1;
                    generate_expression(gen, first_arg);
                    gen->interp_as_printf = 0;
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
                        // NULL-safe via helper (no double-evaluation)
                        fprintf(gen->output, "printf(\"%%s\", _aether_safe_str(");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, "));\n");
                    } else if (arg_type->kind == TYPE_BOOL) {
                        fprintf(gen->output, "printf(\"%%s\", ");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, " ? \"true\" : \"false\");\n");
                    } else if (arg_type->kind == TYPE_INT64) {
                        fprintf(gen->output, "printf(\"%%lld\", (long long)");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, ");\n");
                    } else if (arg_type->kind == TYPE_PTR) {
                        // NULL-safe via helper (no double-evaluation)
                        fprintf(gen->output, "printf(\"%%s\", _aether_safe_str(");
                        generate_expression(gen, first_arg);
                        fprintf(gen->output, "));\n");
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
                    // Auto-fix format specifiers based on argument types to prevent
                    // undefined behavior (e.g. print("Test: %s", 201) would crash)
                    ASTNode* fmt_arg = stmt->children[0];
                    if (fmt_arg->type == AST_LITERAL && fmt_arg->node_type &&
                        fmt_arg->node_type->kind == TYPE_STRING && fmt_arg->value) {
                        // Parse format string and replace specifiers with type-correct ones
                        const char* fmt = fmt_arg->value;
                        fprintf(gen->output, "printf(\"");
                        int arg_idx = 1;  // index into stmt->children for arguments
                        for (int fi = 0; fmt[fi]; fi++) {
                            if (fmt[fi] == '%' && fmt[fi + 1]) {
                                fi++;
                                // Skip flags, width, precision
                                while (fmt[fi] == '-' || fmt[fi] == '+' || fmt[fi] == ' ' ||
                                       fmt[fi] == '#' || fmt[fi] == '0') fi++;
                                while (fmt[fi] >= '0' && fmt[fi] <= '9') fi++;
                                if (fmt[fi] == '.') {
                                    fi++;
                                    while (fmt[fi] >= '0' && fmt[fi] <= '9') fi++;
                                }
                                if (fmt[fi] == '%') {
                                    // Literal %%
                                    fprintf(gen->output, "%%%%");
                                } else if (arg_idx < stmt->child_count) {
                                    // Replace with type-correct specifier
                                    ASTNode* arg = stmt->children[arg_idx];
                                    Type* atype = arg->node_type;
                                    if (atype && atype->kind == TYPE_FLOAT) {
                                        fprintf(gen->output, "%%f");
                                    } else if (atype && atype->kind == TYPE_INT64) {
                                        fprintf(gen->output, "%%lld");
                                    } else if (atype && (atype->kind == TYPE_STRING || atype->kind == TYPE_PTR)) {
                                        fprintf(gen->output, "%%s");
                                    } else if (atype && atype->kind == TYPE_BOOL) {
                                        fprintf(gen->output, "%%s");
                                    } else {
                                        fprintf(gen->output, "%%d");
                                    }
                                    arg_idx++;
                                } else {
                                    // More specifiers than args — keep original
                                    fprintf(gen->output, "%%%c", fmt[fi]);
                                }
                            } else {
                                // Re-escape special characters for C string output
                                switch (fmt[fi]) {
                                    case '\n': fprintf(gen->output, "\\n");  break;
                                    case '\t': fprintf(gen->output, "\\t");  break;
                                    case '\r': fprintf(gen->output, "\\r");  break;
                                    case '\0': fprintf(gen->output, "\\0");  break;
                                    case '\\': fprintf(gen->output, "\\\\"); break;
                                    case '"':  fprintf(gen->output, "\\\""); break;
                                    default:   fprintf(gen->output, "%c", fmt[fi]); break;
                                }
                            }
                        }
                        fprintf(gen->output, "\", ");
                        // Emit arguments with type-safe wrappers
                        for (int i = 1; i < stmt->child_count; i++) {
                            if (i > 1) fprintf(gen->output, ", ");
                            ASTNode* arg = stmt->children[i];
                            Type* atype = arg->node_type;
                            if (atype && atype->kind == TYPE_INT64) {
                                fprintf(gen->output, "(long long)");
                                generate_expression(gen, arg);
                            } else if (atype && atype->kind == TYPE_BOOL) {
                                generate_expression(gen, arg);
                                fprintf(gen->output, " ? \"true\" : \"false\"");
                            } else if (atype && (atype->kind == TYPE_STRING || atype->kind == TYPE_PTR)) {
                                fprintf(gen->output, "_aether_safe_str(");
                                generate_expression(gen, arg);
                                fprintf(gen->output, ")");
                            } else {
                                generate_expression(gen, arg);
                            }
                        }
                        fprintf(gen->output, ");\n");
                    } else {
                        // Non-literal format string — use %s to prevent format injection
                        fprintf(gen->output, "printf(\"%%s\", ");
                        generate_expression(gen, stmt->children[0]);
                        fprintf(gen->output, ");\n");
                    }
                }
                // Flush stdout so partial-line output appears immediately
                // (without this, print(".") in a loop won't show until \n)
                fprintf(gen->output, "fflush(stdout);\n");
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
        
        case AST_REPLY_STATEMENT:
            if (stmt->child_count > 0) {
                ASTNode* reply_expr = stmt->children[0];

                if (reply_expr->type == AST_MESSAGE_CONSTRUCTOR && reply_expr->value) {
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
