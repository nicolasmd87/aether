#include "codegen_internal.h"

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
            
        case AST_WHILE_LOOP: {
            // Check if loop body contains sends (for batch optimization)
            int has_sends = contains_send_expression(stmt);

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
            
        case AST_RETURN_STATEMENT:
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
