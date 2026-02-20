#include "codegen_internal.h"

void generate_expression(CodeGenerator* gen, ASTNode* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case AST_LITERAL:
            if (expr->node_type && expr->node_type->kind == TYPE_STRING) {
                fprintf(gen->output, "\"");
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
            if (expr->child_count > 0) {
                ASTNode* child = expr->children[0];

                int needs_atomic = 0;
                if (child->node_type && child->node_type->kind == TYPE_ACTOR_REF) {
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

                if (!skip_parens) fprintf(gen->output, "(");

                if (is_assignment) {
                    gen->generating_lvalue = 1;
                }
                generate_expression(gen, expr->children[0]);
                if (is_assignment) {
                    gen->generating_lvalue = 0;
                }

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
                        // print("Hello ${name}!") — delegate to AST_STRING_INTERP codegen
                        generate_expression(gen, expr->children[0]);
                    } else
                    if (expr->child_count == 1 && expr->children[0]->node_type) {
                        ASTNode* arg = expr->children[0];
                        Type* arg_type = arg->node_type;

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
                    } else {
                        fprintf(gen->output, "printf(");
                        for (int i = 0; i < expr->child_count; i++) {
                            if (i > 0) fprintf(gen->output, ", ");
                            generate_expression(gen, expr->children[i]);
                        }
                        fprintf(gen->output, ")");
                    }
                }
                else if (strcmp(func_name, "println") == 0) {
                    // println(x) = print(x) then putchar('\n')
                    // Special case: println("...${expr}...") — generate interp then add \n
                    if (expr->child_count == 1 && expr->children[0]->type == AST_STRING_INTERP) {
                        generate_expression(gen, expr->children[0]);
                        fprintf(gen->output, "; putchar('\\n')");
                    } else
                    if (expr->child_count == 1 && expr->children[0]->node_type) {
                        ASTNode* arg = expr->children[0];
                        Type* arg_type = arg->node_type;
                        if (arg_type->kind == TYPE_INT) {
                            fprintf(gen->output, "printf(\"%%d\\n\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_FLOAT) {
                            fprintf(gen->output, "printf(\"%%f\\n\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else if (arg_type->kind == TYPE_STRING) {
                            fprintf(gen->output, "printf(\"%%s\\n\", ");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
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
                    } else {
                        fprintf(gen->output, "printf(");
                        for (int i = 0; i < expr->child_count; i++) {
                            if (i > 0) fprintf(gen->output, ", ");
                            generate_expression(gen, expr->children[i]);
                        }
                        fprintf(gen->output, "); putchar('\\n')");
                    }
                }
                else if (strcmp(func_name, "wait_for_idle") == 0) {
                    fprintf(gen->output, "scheduler_wait()");
                }
                else if (strcmp(func_name, "sleep") == 0 && expr->child_count == 1) {
                    fprintf(gen->output, "usleep(1000 * (");
                    generate_expression(gen, expr->children[0]);
                    fprintf(gen->output, "))");
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
                else if (strcmp(func_name, "clock_ns") == 0 && expr->child_count == 0) {
                    fprintf(gen->output, "({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC, &_ts); (int64_t)_ts.tv_sec * 1000000000LL + _ts.tv_nsec; })");
                }
                else {
                    char c_func_name[256];
                    strncpy(c_func_name, func_name, sizeof(c_func_name) - 1);
                    c_func_name[sizeof(c_func_name) - 1] = '\0';
                    for (char* p = c_func_name; *p; p++) {
                        if (*p == '.') *p = '_';
                    }
                    fprintf(gen->output, "%s(", c_func_name);
                    for (int i = 0; i < expr->child_count; i++) {
                        if (i > 0) fprintf(gen->output, ", ");
                        ASTNode* arg = expr->children[i];
                        // Cast int→void* when the extern param expects void* (TYPE_PTR).
                        // Uses (void*)(intptr_t) which is the well-defined C idiom.
                        TypeKind expected = lookup_extern_param_kind(gen, c_func_name, i);
                        if (expected == TYPE_PTR && arg->node_type &&
                            (arg->node_type->kind == TYPE_INT || arg->node_type->kind == TYPE_BOOL)) {
                            fprintf(gen->output, "(void*)(intptr_t)(");
                            generate_expression(gen, arg);
                            fprintf(gen->output, ")");
                        } else {
                            generate_expression(gen, arg);
                        }
                    }
                    fprintf(gen->output, ")");
                }
            }
            break;

        case AST_STRING_INTERP: {
            // Generate a single printf call for "Hello ${name}, count=${n}"
            // Children alternate: AST_LITERAL (string) and expression nodes.
            // Build format string, then emit arguments.
            fprintf(gen->output, "printf(\"");
            for (int i = 0; i < expr->child_count; i++) {
                ASTNode* ch = expr->children[i];
                if (ch->type == AST_LITERAL && ch->node_type && ch->node_type->kind == TYPE_STRING) {
                    // Literal segment: emit content, re-escaping special chars for C string
                    const char* s = ch->value ? ch->value : "";
                    for (; *s; s++) {
                        switch (*s) {
                            case '\n': fprintf(gen->output, "\\n");   break;
                            case '\t': fprintf(gen->output, "\\t");   break;
                            case '\r': fprintf(gen->output, "\\r");   break;
                            case '"':  fprintf(gen->output, "\\\"");  break;
                            case '\\': fprintf(gen->output, "\\\\");  break;
                            case '%':  fprintf(gen->output, "%%%%");  break;
                            default:   fputc(*s, gen->output);        break;
                        }
                    }
                } else {
                    // Expression: pick format specifier based on inferred type
                    TypeKind tk = (ch->node_type) ? ch->node_type->kind : TYPE_UNKNOWN;
                    switch (tk) {
                        case TYPE_INT:    fprintf(gen->output, "%%d");  break;
                        case TYPE_INT64:  fprintf(gen->output, "%%lld"); break;
                        case TYPE_UINT64: fprintf(gen->output, "%%llu"); break;
                        case TYPE_FLOAT:  fprintf(gen->output, "%%g");  break;
                        case TYPE_BOOL:   fprintf(gen->output, "%%s");  break;
                        case TYPE_STRING: fprintf(gen->output, "%%s");  break;
                        default:          fprintf(gen->output, "%%d");  break; // unknown/void: default to int like print()
                    }
                }
            }
            fprintf(gen->output, "\"");
            // Emit arguments (expression nodes only, not literals)
            for (int i = 0; i < expr->child_count; i++) {
                ASTNode* ch = expr->children[i];
                if (ch->type == AST_LITERAL && ch->node_type && ch->node_type->kind == TYPE_STRING)
                    continue;
                fprintf(gen->output, ", ");
                TypeKind tk = ch->node_type ? ch->node_type->kind : TYPE_UNKNOWN;
                if (tk == TYPE_BOOL) {
                    generate_expression(gen, ch);
                    fprintf(gen->output, " ? \"true\" : \"false\"");
                } else {
                    generate_expression(gen, ch);
                }
            }
            fprintf(gen->output, ")");
            break;
        }

        case AST_ACTOR_REF:
            if (strcmp(expr->value, "self") == 0) {
                fprintf(gen->output, "aether_self()");
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
                                    generate_expression(gen, field_init->children[0]);
                                    break;
                                }
                            }
                            fprintf(gen->output, ", NULL, {NULL, 0, 0}}; ");

                            if (gen->in_main_loop) {
                                // Main thread loop: batch sends to reduce atomics N→num_cores
                                fprintf(gen->output, "scheduler_send_batch_add((ActorBase*)(");
                                generate_expression(gen, target);
                                fprintf(gen->output, "), _imsg); }");
                            } else if (gen->current_actor == NULL) {
                                // Main thread, non-loop: current_core_id is always -1, local path
                                // is never taken — emit scheduler_send_remote directly (no dead branch)
                                fprintf(gen->output, "scheduler_send_remote((ActorBase*)(");
                                generate_expression(gen, target);
                                fprintf(gen->output, "), _imsg, current_core_id); }");
                            } else {
                                // Inside an actor handler: same-core vs cross-core branch is live
                                fprintf(gen->output, "if (current_core_id >= 0 && current_core_id == ((ActorBase*)(");
                                generate_expression(gen, target);
                                fprintf(gen->output, "))->assigned_core) { scheduler_send_local((ActorBase*)(");
                                generate_expression(gen, target);
                                fprintf(gen->output, "), _imsg); } else { scheduler_send_remote((ActorBase*)(");
                                generate_expression(gen, target);
                                fprintf(gen->output, "), _imsg, current_core_id); } }");
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
                            fprintf(gen->output, "(void*)"); generate_expression(gen, target);
                            fprintf(gen->output, ", &_msg, sizeof(%s)); }", message->value);
                        }
                    } else {
                        fprintf(gen->output, "/* ERROR: unknown message type %s */", message->value);
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
                        
                        fprintf(gen->output, " }; void* _ask_r = scheduler_ask_message((ActorBase*)(");
                        generate_expression(gen, target);
                        // Cast void* → intptr_t to avoid pointer-to-integer error; NULL check still works.
                        fprintf(gen->output, "), &_msg, sizeof(%s), 5000); (intptr_t)(uintptr_t)_ask_r; })", message->value);
                    } else {
                        fprintf(gen->output, "/* ERROR: unknown message type %s */", message->value);
                    }
                }
            }
            break;
            
        default:
            for (int i = 0; i < expr->child_count; i++) {
                generate_expression(gen, expr->children[i]);
            }
            break;
    }
}
