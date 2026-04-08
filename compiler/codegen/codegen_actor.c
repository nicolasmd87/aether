#include "codegen_internal.h"

void generate_actor_definition(CodeGenerator* gen, ASTNode* actor) {
    if (!actor || actor->type != AST_ACTOR_DEFINITION) return;
    
    gen->current_actor = strdup(actor->value);
    gen->state_var_count = 0;
    gen->actor_state_vars = NULL;
    
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_STATE_DECLARATION) {
            char** new_svars = realloc(gen->actor_state_vars,
                                       (gen->state_var_count + 1) * sizeof(char*));
            if (!new_svars) continue;
            gen->actor_state_vars = new_svars;
            gen->actor_state_vars[gen->state_var_count] = strdup(child->value);
            gen->state_var_count++;
        }
    }
    
    // Generate cache-aligned actor struct with optimized field layout
    print_line(gen, "#ifdef _MSC_VER");
    print_line(gen, "__declspec(align(64))");
    print_line(gen, "#endif");
    print_line(gen, "typedef struct");
    print_line(gen, "#if defined(__GNUC__) || defined(__clang__)");
    print_line(gen, "__attribute__((aligned(64)))");
    print_line(gen, "#endif");
    print_line(gen, "%s {", actor->value);
    indent(gen);
    
    // Hot fields (accessed every message) - first cache line
    print_line(gen, "atomic_int active;       // Hot: checked every loop iteration");
    print_line(gen, "int id;                  // Hot: used for identification");
    print_line(gen, "Mailbox mailbox;         // Hot: message queue");
    print_line(gen, "void (*step)(void*);     // Hot: message handler");
    
    // Warm fields (accessed occasionally)
    print_line(gen, "pthread_t thread;        // Warm: thread handle");
    print_line(gen, "int auto_process;        // Warm: auto-processing flag");
    print_line(gen, "atomic_int assigned_core; // Cold: core assignment (atomic for work-stealing)");
    print_line(gen, "atomic_int migrate_to;    // Cold: affinity hint (-1 = none)");
    print_line(gen, "atomic_int main_thread_only; // Cold: scheduler skip flag");
    print_line(gen, "SPSCQueue* spsc_queue;   // Lock-free same-core messaging (lazy alloc)");
    print_line(gen, "_Atomic(ActorReplySlot*) reply_slot; // Non-NULL only during ask/reply");
    print_line(gen, "atomic_flag step_lock;   // Prevents concurrent step() during work-steal handoff");
    print_line(gen, "uint64_t timeout_ns;     // Receive timeout (0 = none)");
    print_line(gen, "uint64_t last_activity_ns; // Idle start timestamp (0 = not idle)");
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
                // Use atomic types for numeric fields to enable safe concurrent access
                if (child->node_type && child->node_type->kind == TYPE_INT) {
                    fprintf(gen->output, "atomic_int %s;\n", child->value);
                } else if (child->node_type && child->node_type->kind == TYPE_INT64) {
                    fprintf(gen->output, "_Atomic int64_t %s;\n", child->value);
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
                        print_line(gen, "static AETHER_HOT void %s_handle_%s(%s* self, void* _msg_data) {",
                                  actor->value, pattern->value, actor->value);
                        indent(gen);
                        print_line(gen, "%s* _pattern = (%s*)_msg_data;", pattern->value, pattern->value);

                        // Extract pattern fields with correct types from message definition.
                        // Single-int-field messages use intptr_t (matches payload_int width).
                        const char* single_int_name = get_single_int_field(msg_def);
                        for (int k = 0; k < pattern->child_count; k++) {
                            ASTNode* field = pattern->children[k];
                            if (field->type == AST_PATTERN_FIELD) {
                                const char* c_type = "int";
                                if (msg_def && msg_def->fields) {
                                    MessageFieldDef* fdef = msg_def->fields;
                                    while (fdef) {
                                        if (strcmp(fdef->name, field->value) == 0) {
                                            if (single_int_name && fdef->type_kind == TYPE_INT) {
                                                c_type = "intptr_t";
                                            } else {
                                                Type temp_type = { .kind = fdef->type_kind, .element_type = NULL, .array_size = 0, .struct_name = NULL };
                                                c_type = get_c_type(&temp_type);
                                            }
                                            break;
                                        }
                                        fdef = fdef->next;
                                    }
                                }
                                const char* var_name = field->value;
                                if (field->child_count > 0 && field->children[0] &&
                                    field->children[0]->type == AST_PATTERN_VARIABLE && field->children[0]->value) {
                                    var_name = field->children[0]->value;
                                }
                                print_line(gen, "%s %s = _pattern->%s;", c_type, var_name, field->value);
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
    
    // Check if any receive block has a timeout arm
    ASTNode* timeout_arm = NULL;
    for (int i = 0; i < actor->child_count && !timeout_arm; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_RECEIVE_STATEMENT) {
            for (int j = 0; j < child->child_count; j++) {
                if (child->children[j]->type == AST_TIMEOUT_ARM) {
                    timeout_arm = child->children[j];
                    break;
                }
            }
        }
    }

    print_line(gen, "void %s_step(%s* self) {", actor->value, actor->value);
    indent(gen);

    // Timeout check — fires if idle longer than timeout_ns
    if (timeout_arm) {
        print_line(gen, "// Timeout check");
        print_line(gen, "if (self->timeout_ns > 0 && self->last_activity_ns > 0) {");
        indent(gen);
        print_line(gen, "uint64_t _now = (uint64_t)_aether_clock_ns();");
        print_line(gen, "if ((_now - self->last_activity_ns) >= self->timeout_ns) {");
        indent(gen);
        print_line(gen, "self->timeout_ns = 0;  // one-shot");
        print_line(gen, "self->last_activity_ns = 0;");
        // Generate the timeout body
        if (timeout_arm->child_count >= 2 && timeout_arm->children[1]) {
            ASTNode* tbody = timeout_arm->children[1];
            if (tbody->type == AST_BLOCK) {
                for (int j = 0; j < tbody->child_count; j++) {
                    generate_statement(gen, tbody->children[j]);
                }
            } else {
                generate_statement(gen, tbody);
            }
        }
        print_line(gen, "return;");
        unindent(gen);
        print_line(gen, "}");
        unindent(gen);
        print_line(gen, "}");
        print_line(gen, "");
    }

    print_line(gen, "Message msg;");
    print_line(gen, "");
    print_line(gen, "if (unlikely(!mailbox_receive(&self->mailbox, &msg))) {");
    indent(gen);
    if (timeout_arm) {
        // Start timeout countdown when mailbox is empty
        print_line(gen, "if (self->timeout_ns > 0 && self->last_activity_ns == 0) {");
        indent(gen);
        print_line(gen, "self->last_activity_ns = (uint64_t)_aether_clock_ns();");
        unindent(gen);
        print_line(gen, "}");
    }
    print_line(gen, "atomic_store_explicit(&self->active, 0, memory_order_relaxed);");
    print_line(gen, "return;");
    unindent(gen);
    print_line(gen, "}");
    if (timeout_arm) {
        // Message received — cancel timeout (one-shot: fire only if no messages ever arrive)
        print_line(gen, "self->timeout_ns = 0;");
        print_line(gen, "self->last_activity_ns = 0;");
    }
    print_line(gen, "g_current_reply_slot = msg._reply_slot;");
    print_line(gen, "");
    
    if (pattern_count > 0) {
        print_line(gen, "void* _msg_data = msg.payload_ptr;");
        print_line(gen, "int _msg_id = msg.type;");
        print_line(gen, "");
        print_line(gen, "#if AETHER_GCC_COMPAT");
        print_line(gen, "// COMPUTED GOTO DISPATCH - 15-30%% faster than switch");
        print_line(gen, "static void* dispatch_table[256] = {");
        indent(gen);

        // Generate dispatch table with labels (GCC/Clang path)
        for (int i = 0; i < actor->child_count; i++) {
            ASTNode* child = actor->children[i];
            if (child->type == AST_RECEIVE_STATEMENT && child->child_count > 0) {
                for (int j = 0; j < child->child_count; j++) {
                    ASTNode* arm = child->children[j];
                    ASTNode* pattern = NULL;

                    if (arm->type == AST_RECEIVE_ARM && arm->child_count >= 1) {
                        pattern = arm->children[0];
                    }
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
        print_line(gen, "if (likely(_msg_id >= 0 && _msg_id < 256 && dispatch_table[_msg_id])) {");
        indent(gen);
        print_line(gen, "goto *dispatch_table[_msg_id];");
        unindent(gen);
        print_line(gen, "}");
        print_line(gen, "#else");
        print_line(gen, "// MSVC: switch-case dispatch fallback");
        print_line(gen, "switch (_msg_id) {");

        // Generate switch cases (MSVC path)
        for (int i = 0; i < actor->child_count; i++) {
            ASTNode* child = actor->children[i];
            if (child->type == AST_RECEIVE_STATEMENT && child->child_count > 0) {
                for (int j = 0; j < child->child_count; j++) {
                    ASTNode* arm = child->children[j];
                    ASTNode* pattern = NULL;

                    if (arm->type == AST_RECEIVE_ARM && arm->child_count >= 1) {
                        pattern = arm->children[0];
                    }
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
                            print_line(gen, "case %d: goto handle_%s;", msg_def->message_id, pattern->value);
                        }
                    }
                }
            }
        }

        print_line(gen, "default: break;");
        print_line(gen, "}");
        print_line(gen, "#endif");
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
                            // Single-field inline: reconstruct on stack from payload_int
                            const char* cast = "";
                            if (msg_def->fields) {
                                int fk = msg_def->fields->type_kind;
                                if (fk == TYPE_INT64) cast = "(int64_t)";
                                else if (fk == TYPE_PTR || fk == TYPE_ACTOR_REF) cast = "(void*)";
                            }
                            print_line(gen, "if (_msg_data) {");
                            indent(gen);
                            print_line(gen, "%s_handle_%s(self, _msg_data);", actor->value, pattern->value);
                            print_line(gen, "aether_free_message(_msg_data);");
                            unindent(gen);
                            print_line(gen, "} else {");
                            indent(gen);
                            print_line(gen, "%s _msg_val = { ._message_id = msg.type, .%s = %smsg.payload_int };",
                                      pattern->value, single_int, cast);
                            print_line(gen, "%s_handle_%s(self, &_msg_val);", actor->value, pattern->value);
                            unindent(gen);
                            print_line(gen, "}");
                        } else {
                            // Two-field inline: reconstruct from payload_int + payload_ptr
                            print_line(gen, "%s_handle_%s(self, _msg_data);", actor->value, pattern->value);
                            print_line(gen, "aether_free_message(_msg_data);");
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
    
    print_line(gen, "%s* spawn_%s(int preferred_core) {", actor->value, actor->value);
    indent(gen);
    print_line(gen, "// AETHER_SINGLE_CORE=1 forces all actors to core 0 (eliminates cross-core overhead)");
    print_line(gen, "static int _single_core_cached = -1;");
    print_line(gen, "if (_single_core_cached < 0) _single_core_cached = (getenv(\"AETHER_SINGLE_CORE\") != NULL);");
    print_line(gen, "int core = (preferred_core >= 0) ? preferred_core : (_single_core_cached ? 0 : -1);");
    print_line(gen, "%s* actor = (%s*)scheduler_spawn_pooled(core, (void (*)(void*))%s_step, sizeof(%s));",
               actor->value, actor->value, actor->value, actor->value);
    print_line(gen, "if (!actor) {");
    indent(gen);
    print_line(gen, "// Fallback to aligned allocation if pool exhausted");
    print_line(gen, "actor = aether_aligned_alloc(64, sizeof(%s));", actor->value);
    print_line(gen, "if (!actor) return NULL;");
    print_line(gen, "actor->id = atomic_fetch_add(&next_actor_id, 1);");
    print_line(gen, "atomic_init(&actor->assigned_core, -1);");
    print_line(gen, "actor->step = (void (*)(void*))%s_step;", actor->value);
    print_line(gen, "mailbox_init(&actor->mailbox);");
    print_line(gen, "atomic_flag_clear_explicit(&actor->step_lock, memory_order_relaxed);");
    print_line(gen, "scheduler_register_actor((ActorBase*)actor, -1);");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "atomic_init(&actor->active, 0);  // inactive until first message send");
    print_line(gen, "atomic_init(&actor->migrate_to, -1);");
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

    // Auto-initialize "my_ref" to the actor's own pointer so it is valid
    // immediately after spawn — no Setup message needed.  This eliminates
    // the race window where my_ref is 0 if Spawn arrives before Setup on
    // a different core.
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        if (child->type == AST_STATE_DECLARATION &&
            strcmp(child->value, "my_ref") == 0) {
            print_line(gen, "actor->my_ref = (void*)actor;  // self-ref available immediately (no Setup needed)");
            break;
        }
    }

    print_line(gen, "");
    print_line(gen, "#if AETHER_HAS_THREADS");
    print_line(gen, "if (actor->auto_process) {");
    indent(gen);
    print_line(gen, "pthread_create(&actor->thread, NULL, (void*(*)(void*))aether_actor_thread, actor);");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "#endif");

    // Set timeout if actor has a receive ... after N clause
    if (timeout_arm && timeout_arm->child_count >= 1) {
        print_line(gen, "// Receive timeout (milliseconds -> nanoseconds)");
        print_indent(gen);
        fprintf(gen->output, "actor->timeout_ns = (uint64_t)(");
        generate_expression(gen, timeout_arm->children[0]);
        fprintf(gen->output, ") * 1000000ULL;\n");
        print_line(gen, "actor->last_activity_ns = (uint64_t)_aether_clock_ns();  // Start timeout countdown at spawn");
        print_line(gen, "atomic_store_explicit(&actor->active, 1, memory_order_release);  // Activate for timeout polling");
    }

    print_line(gen, "");
    print_line(gen, "return actor;");
    unindent(gen);
    print_line(gen, "}");
    print_line(gen, "");

    print_line(gen, "void send_%s(%s* actor, int type, int payload) {", actor->value, actor->value);
    indent(gen);
    print_line(gen, "Message msg = {type, 0, payload, NULL};");
    print_line(gen, "if (atomic_load_explicit(&actor->assigned_core, memory_order_relaxed) == current_core_id) {");
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
