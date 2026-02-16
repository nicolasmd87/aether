#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "typechecker.h"
#include "type_inference.h"

static int error_count = 0;
static int warning_count = 0;

// Symbol table functions
SymbolTable* create_symbol_table(SymbolTable* parent) {
    SymbolTable* table = malloc(sizeof(SymbolTable));
    table->symbols = NULL;
    table->parent = parent;
    return table;
}

void free_symbol_table(SymbolTable* table) {
    if (!table) return;

    Symbol* current = table->symbols;
    while (current) {
        Symbol* next = current->next;
        if (current->name) free(current->name);
        if (current->type) free_type(current->type);
        if (current->alias_target) free(current->alias_target);
        free(current);
        current = next;
    }

    free(table);
}

void add_symbol(SymbolTable* table, const char* name, Type* type, int is_actor, int is_function, int is_state) {
    Symbol* symbol = malloc(sizeof(Symbol));
    symbol->name = strdup(name);
    symbol->type = type;
    symbol->is_actor = is_actor;
    symbol->is_function = is_function;
    symbol->is_state = is_state;
    symbol->is_module_alias = 0;
    symbol->alias_target = NULL;
    symbol->node = NULL;  // Initialize to NULL
    symbol->next = table->symbols;
    table->symbols = symbol;
}

Symbol* lookup_symbol(SymbolTable* table, const char* name) {
    Symbol* symbol = lookup_symbol_local(table, name);
    if (symbol) return symbol;
    
    if (table->parent) {
        return lookup_symbol(table->parent, name);
    }
    
    return NULL;
}

Symbol* lookup_symbol_local(SymbolTable* table, const char* name) {
    Symbol* current = table->symbols;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Module alias functions
void add_module_alias(SymbolTable* table, const char* alias, const char* module_name) {
    Symbol* symbol = malloc(sizeof(Symbol));
    symbol->name = strdup(alias);
    symbol->type = NULL;  // Modules don't have types
    symbol->is_actor = 0;
    symbol->is_function = 0;
    symbol->is_state = 0;
    symbol->is_module_alias = 1;
    symbol->alias_target = strdup(module_name);
    symbol->node = NULL;
    symbol->next = table->symbols;
    table->symbols = symbol;
}

Symbol* resolve_module_alias(SymbolTable* table, const char* name) {
    Symbol* symbol = lookup_symbol(table, name);
    if (symbol && symbol->is_module_alias) {
        return symbol;
    }
    return NULL;
}

Symbol* lookup_qualified_symbol(SymbolTable* table, const char* qualified_name) {
    // Split qualified name on '.'
    char* name_copy = strdup(qualified_name);
    char* dot = strchr(name_copy, '.');

    if (dot) {
        *dot = '\0';
        const char* prefix = name_copy;
        const char* suffix = dot + 1;

        // Check if prefix is a module alias
        Symbol* alias_sym = resolve_module_alias(table, prefix);
        if (alias_sym && alias_sym->alias_target) {
            // Reconstruct with actual module name
            char resolved_name[512];
            snprintf(resolved_name, sizeof(resolved_name), "%s.%s",
                    alias_sym->alias_target, suffix);
            free(name_copy);
            return lookup_symbol(table, resolved_name);
        }
    }

    free(name_copy);
    return lookup_symbol(table, qualified_name);
}

// Error reporting
void type_error(const char* message, int line, int column) {
    fprintf(stderr, "Type error at line %d, column %d: %s\n", line, column, message);
    error_count++;
}

void type_warning(const char* message, int line, int column) {
    fprintf(stderr, "Type warning at line %d, column %d: %s\n", line, column, message);
    warning_count++;
}

// Type compatibility functions
int is_type_compatible(Type* from, Type* to) {
    if (!from || !to) return 0;
    
    // Unknown types match anything (for inference)
    if (from->kind == TYPE_UNKNOWN || to->kind == TYPE_UNKNOWN) return 1;
    
    // Exact match
    if (types_equal(from, to)) return 1;
    
    // Numeric conversions
    if (from->kind == TYPE_INT && to->kind == TYPE_FLOAT) return 1;
    if (from->kind == TYPE_FLOAT && to->kind == TYPE_INT) return 1;
    
    // Array compatibility
    if (from->kind == TYPE_ARRAY && to->kind == TYPE_ARRAY) {
        return is_type_compatible(from->element_type, to->element_type);
    }
    
    // Actor reference compatibility
    if (from->kind == TYPE_ACTOR_REF && to->kind == TYPE_ACTOR_REF) {
        return is_type_compatible(from->element_type, to->element_type);
    }
    
    return 0;
}

int is_assignable(Type* from, Type* to) {
    return is_type_compatible(from, to);
}

int is_callable(Type* type) {
    if (!type) return 0;
    switch (type->kind) {
        case TYPE_INT:
        case TYPE_INT64:
        case TYPE_UINT64:
        case TYPE_FLOAT:
        case TYPE_BOOL:
        case TYPE_STRING:
        case TYPE_VOID:
        case TYPE_ARRAY:
        case TYPE_WILDCARD:
        case TYPE_PTR:
            return 0;
        default:
            return 1;
    }
}

// Type inference functions
Type* infer_type(ASTNode* expr, SymbolTable* table) {
    if (!expr) return NULL;
    
    switch (expr->type) {
        case AST_LITERAL:
            return clone_type(expr->node_type);
            
        case AST_ARRAY_LITERAL:
            // Return the inferred array type
            return expr->node_type ? clone_type(expr->node_type) : create_type(TYPE_UNKNOWN);
            
        case AST_IDENTIFIER: {
            Symbol* symbol = lookup_symbol(table, expr->value);
            return symbol ? symbol->type : create_type(TYPE_UNKNOWN);
        }
        
        case AST_BINARY_EXPRESSION:
            return infer_binary_type(expr->children[0], expr->children[1], 
                                   get_token_type_from_string(expr->value));
            
        case AST_UNARY_EXPRESSION:
            return infer_unary_type(expr->children[0], 
                                  get_token_type_from_string(expr->value));
            
        case AST_FUNCTION_CALL: {
            Symbol* symbol = lookup_symbol(table, expr->value);
            if (symbol && symbol->is_function) {
                return clone_type(symbol->type);
            }
            return create_type(TYPE_UNKNOWN);
        }
        
        case AST_ACTOR_REF:
            return create_type(TYPE_ACTOR_REF);
            
        case AST_STRUCT_LITERAL:
            // Return the struct type from node_type (set during type inference)
            return expr->node_type ? clone_type(expr->node_type) : create_type(TYPE_UNKNOWN);
            
        case AST_ARRAY_ACCESS:
            // Return the element type from array access (set during type inference)
            return expr->node_type ? clone_type(expr->node_type) : create_type(TYPE_UNKNOWN);

        case AST_MEMBER_ACCESS: {
            // If node_type already set, use it
            if (expr->node_type && expr->node_type->kind != TYPE_UNKNOWN)
                return clone_type(expr->node_type);
            // Look up the struct type and find the field type
            if (expr->child_count > 0 && expr->children[0]) {
                Type* base_type = infer_type(expr->children[0], table);
                if (base_type && base_type->kind == TYPE_STRUCT && base_type->struct_name) {
                    Symbol* struct_sym = lookup_symbol(table, base_type->struct_name);
                    if (struct_sym && struct_sym->node) {
                        ASTNode* struct_def = struct_sym->node;
                        for (int fi = 0; fi < struct_def->child_count; fi++) {
                            ASTNode* field = struct_def->children[fi];
                            if (field && field->value && strcmp(field->value, expr->value) == 0) {
                                if (field->node_type && field->node_type->kind != TYPE_UNKNOWN)
                                    return clone_type(field->node_type);
                                break;
                            }
                        }
                    }
                }
            }
            return create_type(TYPE_UNKNOWN);
        }

        default:
            return create_type(TYPE_UNKNOWN);
    }
}

Type* infer_binary_type(ASTNode* left, ASTNode* right, AeTokenType operator) {
    Type* left_type = left ? left->node_type : NULL;
    Type* right_type = right ? right->node_type : NULL;

    // Comparison and logical operators always produce bool, even with unknown operands
    switch (operator) {
        case TOKEN_EQUALS:
        case TOKEN_NOT_EQUALS:
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_AND:
        case TOKEN_OR:
            return create_type(TYPE_BOOL);
        default:
            break;
    }

    if (!left_type || !right_type) return create_type(TYPE_UNKNOWN);

    switch (operator) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
        case TOKEN_MODULO:
            // Numeric operations
            if (left_type->kind == TYPE_UNKNOWN || right_type->kind == TYPE_UNKNOWN) {
                // If either type is unknown (e.g., unresolved parameter), allow it
                return create_type(TYPE_UNKNOWN);
            }
            if (left_type->kind == TYPE_FLOAT || right_type->kind == TYPE_FLOAT) {
                return create_type(TYPE_FLOAT);
            }
            if (left_type->kind == TYPE_INT && right_type->kind == TYPE_INT) {
                return create_type(TYPE_INT);
            }
            if (left_type->kind == TYPE_STRING && right_type->kind == TYPE_STRING) {
                return create_type(TYPE_STRING);
            }
            break;
            
        case TOKEN_ASSIGN:
            return clone_type(right_type);
            
        default:
            break;
    }
    
    return create_type(TYPE_UNKNOWN);
}

Type* infer_unary_type(ASTNode* operand, AeTokenType operator) {
    Type* operand_type = operand ? operand->node_type : NULL;
    if (!operand_type) return create_type(TYPE_UNKNOWN);
    
    switch (operator) {
        case TOKEN_NOT:
            return create_type(TYPE_BOOL);
            
        case TOKEN_MINUS:
        case TOKEN_INCREMENT:
        case TOKEN_DECREMENT:
            return clone_type(operand_type); // Same type as operand
            
        default:
            return create_type(TYPE_UNKNOWN);
    }
}

AeTokenType get_token_type_from_string(const char* str) {
    if (!str) return TOKEN_ERROR;
    
    if (strcmp(str, "+") == 0) return TOKEN_PLUS;
    if (strcmp(str, "-") == 0) return TOKEN_MINUS;
    if (strcmp(str, "*") == 0) return TOKEN_MULTIPLY;
    if (strcmp(str, "/") == 0) return TOKEN_DIVIDE;
    if (strcmp(str, "%") == 0) return TOKEN_MODULO;
    if (strcmp(str, "==") == 0) return TOKEN_EQUALS;
    if (strcmp(str, "!=") == 0) return TOKEN_NOT_EQUALS;
    if (strcmp(str, "<") == 0) return TOKEN_LESS;
    if (strcmp(str, "<=") == 0) return TOKEN_LESS_EQUAL;
    if (strcmp(str, ">") == 0) return TOKEN_GREATER;
    if (strcmp(str, ">=") == 0) return TOKEN_GREATER_EQUAL;
    if (strcmp(str, "&&") == 0) return TOKEN_AND;
    if (strcmp(str, "||") == 0) return TOKEN_OR;
    if (strcmp(str, "=") == 0) return TOKEN_ASSIGN;
    if (strcmp(str, "!") == 0) return TOKEN_NOT;
    if (strcmp(str, "++") == 0) return TOKEN_INCREMENT;
    if (strcmp(str, "--") == 0) return TOKEN_DECREMENT;
    
    return TOKEN_ERROR;
}

// Type checking functions
int typecheck_program(ASTNode* program) {
    if (!program || program->type != AST_PROGRAM) return 0;
    
    error_count = 0;
    warning_count = 0;
    
    SymbolTable* global_table = create_symbol_table(NULL);
    
    // Add builtin functions
    // Signature: add_symbol(table, name, type, is_actor, is_function, is_state)
    Type* typeof_type = create_type(TYPE_STRING);
    add_symbol(global_table, "typeof", typeof_type, 0, 1, 0);

    Type* is_type_type = create_type(TYPE_BOOL);
    add_symbol(global_table, "is_type", is_type_type, 0, 1, 0);

    Type* convert_type_type = create_type(TYPE_UNKNOWN);  // Returns any type
    add_symbol(global_table, "convert_type", convert_type_type, 0, 1, 0);

    // Scheduler/concurrency builtins
    Type* wait_idle_type = create_type(TYPE_VOID);
    add_symbol(global_table, "wait_for_idle", wait_idle_type, 0, 1, 0);

    Type* sleep_type = create_type(TYPE_VOID);
    add_symbol(global_table, "sleep", sleep_type, 0, 1, 0);

    // Environment variable builtins
    Type* getenv_type = create_type(TYPE_STRING);  // Returns string (or null)
    add_symbol(global_table, "getenv", getenv_type, 0, 1, 0);

    Type* atoi_type = create_type(TYPE_INT);  // Returns int
    add_symbol(global_table, "atoi", atoi_type, 0, 1, 0);

    // Timing builtin
    Type* clock_ns_type = create_type(TYPE_INT);  // Returns nanoseconds as int
    add_symbol(global_table, "clock_ns", clock_ns_type, 0, 1, 0);

    // First pass: collect all declarations
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        
        switch (child->type) {
            case AST_ACTOR_DEFINITION: {
                // Create actor struct type
                Type* actor_type = create_type(TYPE_STRUCT);
                actor_type->struct_name = strdup(child->value);
                add_symbol(global_table, child->value, actor_type, 1, 0, 0);
                
                // Add generated spawn_ActorName() function - returns pointer to actor
                // Use TYPE_ACTOR_REF to represent pointer type
                char spawn_name[256];
                snprintf(spawn_name, sizeof(spawn_name), "spawn_%s", child->value);
                Type* spawn_return_type = create_type(TYPE_ACTOR_REF);
                spawn_return_type->element_type = clone_type(actor_type);
                add_symbol(global_table, spawn_name, spawn_return_type, 0, 1, 0);
                
                // Add generated send_ActorName() function - returns void
                char send_name[256];
                snprintf(send_name, sizeof(send_name), "send_%s", child->value);
                Type* send_type = create_type(TYPE_VOID);
                add_symbol(global_table, send_name, send_type, 0, 1, 0);
                
                // Add generated ActorName_step() function - returns void
                char step_name[256];
                snprintf(step_name, sizeof(step_name), "%s_step", child->value);
                Type* step_type = create_type(TYPE_VOID);
                add_symbol(global_table, step_name, step_type, 0, 1, 0);
                break;
            }
            case AST_FUNCTION_DEFINITION: {
                add_symbol(global_table, child->value, clone_type(child->node_type), 0, 1, 0);
                break;
            }
            case AST_EXTERN_FUNCTION: {
                // Register extern C function in symbol table
                add_symbol(global_table, child->value, clone_type(child->node_type), 0, 1, 0);
                break;
            }
            case AST_STRUCT_DEFINITION: {
                Type* struct_type = create_type(TYPE_STRUCT);
                struct_type->struct_name = strdup(child->value);
                add_symbol(global_table, child->value, struct_type, 0, 0, 0);
                // Store AST node in symbol for later field type updates
                Symbol* struct_sym = lookup_symbol(global_table, child->value);
                if (struct_sym) {
                    struct_sym->node = child;
                }
                break;
            }
            case AST_MESSAGE_DEFINITION: {
                // Register message type so receive patterns can look up field types
                Type* msg_type = create_type(TYPE_MESSAGE);
                add_symbol(global_table, child->value, msg_type, 0, 0, 0);
                Symbol* msg_sym = lookup_symbol(global_table, child->value);
                if (msg_sym) {
                    msg_sym->node = child;
                }
                break;
            }
            case AST_MAIN_FUNCTION:
                // Main function doesn't need to be in symbol table
                break;
            case AST_IMPORT_STATEMENT: {
                // Process import and register alias if present
                const char* module_name = child->value;

                // Check if this import has an alias (last child is identifier)
                if (child->child_count > 0) {
                    ASTNode* last_child = child->children[child->child_count - 1];
                    // Check if last child is the alias (an identifier node)
                    if (last_child && last_child->type == AST_IDENTIFIER) {
                        const char* alias = last_child->value;
                        // Register the alias in symbol table
                        add_module_alias(global_table, alias, module_name);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    
    // NEW: Run type inference before type checking
    if (!infer_all_types(program, global_table)) {
        fprintf(stderr, "Type inference failed\n");
        free_symbol_table(global_table);
        return 0;
    }
    
    // Update symbol table with inferred types
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_FUNCTION_DEFINITION && child->value && child->node_type) {
            Symbol* func_sym = lookup_symbol(global_table, child->value);
            if (func_sym) {
                if (func_sym->type) free_type(func_sym->type);
                func_sym->type = clone_type(child->node_type);
            }
        }
    }
    
    // Second pass: type check all nodes
    for (int i = 0; i < program->child_count; i++) {
        typecheck_node(program->children[i], global_table);
    }
    
    free_symbol_table(global_table);
    
    // Report errors and warnings
    if (error_count > 0) {
        fprintf(stderr, "Type checking failed with %d error(s)\n", error_count);
        return 0;  // Block compilation on errors
    }
    
    if (warning_count > 0) {
        fprintf(stderr, "Type checking completed with %d warning(s)\n", warning_count);
    }
    
    return 1;
}

int typecheck_node(ASTNode* node, SymbolTable* table) {
    if (!node) return 0;
    
    switch (node->type) {
        case AST_ACTOR_DEFINITION:
            return typecheck_actor_definition(node, table);
        case AST_FUNCTION_DEFINITION:
            return typecheck_function_definition(node, table);
        case AST_EXTERN_FUNCTION:
            // Extern functions have no body to check - just a declaration
            return 1;
        case AST_STRUCT_DEFINITION:
            return typecheck_struct_definition(node, table);
        case AST_MAIN_FUNCTION:
            return typecheck_statement(node, table);
        default:
            return typecheck_statement(node, table);
    }
}

// Look up the type of a specific field in a message definition
static Type* lookup_message_field_type(SymbolTable* table, const char* message_name, const char* field_name) {
    Symbol* msg_sym = lookup_symbol(table, message_name);
    if (!msg_sym || !msg_sym->node || msg_sym->node->type != AST_MESSAGE_DEFINITION) {
        return NULL;
    }
    ASTNode* msg_def = msg_sym->node;
    for (int i = 0; i < msg_def->child_count; i++) {
        ASTNode* field = msg_def->children[i];
        if (field->type == AST_MESSAGE_FIELD && field->value && strcmp(field->value, field_name) == 0) {
            return field->node_type ? clone_type(field->node_type) : NULL;
        }
    }
    return NULL;
}

int typecheck_actor_definition(ASTNode* actor, SymbolTable* table) {
    if (!actor || actor->type != AST_ACTOR_DEFINITION) return 0;
    
    SymbolTable* actor_table = create_symbol_table(table);
    
    // Type check actor body
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        
        if (child->type == AST_STATE_DECLARATION) {
            // Add state variables to actor's symbol table
            add_symbol(actor_table, child->value, clone_type(child->node_type), 0, 0, 1);
        } else if (child->type == AST_RECEIVE_STATEMENT) {
            // Handle receive statement
            SymbolTable* receive_table = create_symbol_table(actor_table);

            // V1 syntax: receive(msg) { ... } has child->value set
            // V2 syntax: receive { Pattern -> ... } has child->value = NULL
            if (child->value) {
                Type* msg_type = create_type(TYPE_MESSAGE);
                add_symbol(receive_table, child->value, msg_type, 0, 0, 0);
            }

            // Type check the receive body/arms
            for (int j = 0; j < child->child_count; j++) {
                ASTNode* arm = child->children[j];

                // For V2 receive arms, extract pattern variables
                if (arm->type == AST_RECEIVE_ARM && arm->child_count >= 2) {
                    ASTNode* pattern = arm->children[0];
                    ASTNode* arm_body = arm->children[1];

                    // Add pattern variables to scope
                    if (pattern->type == AST_MESSAGE_PATTERN) {
                        for (int k = 0; k < pattern->child_count; k++) {
                            ASTNode* field = pattern->children[k];
                            if (field->type == AST_PATTERN_FIELD) {
                                // Look up actual field type from message definition
                                Type* field_type = lookup_message_field_type(table, pattern->value, field->value);
                                if (!field_type) {
                                    field_type = create_type(TYPE_UNKNOWN);
                                }
                                // Use pattern variable name if present (field: var), else field name
                                const char* var_name = field->value;
                                if (field->child_count > 0 && field->children[0] &&
                                    field->children[0]->type == AST_PATTERN_VARIABLE && field->children[0]->value) {
                                    var_name = field->children[0]->value;
                                }
                                add_symbol(receive_table, var_name, field_type, 0, 0, 0);
                            }
                        }
                    }

                    // Type check arm body
                    typecheck_statement(arm_body, receive_table);
                } else {
                    typecheck_statement(arm, receive_table);
                }
            }

            free_symbol_table(receive_table);
            continue;
        }
        
        typecheck_node(child, actor_table);
    }
    
    free_symbol_table(actor_table);
    return 1;
}

int typecheck_function_definition(ASTNode* func, SymbolTable* table) {
    if (!func || func->type != AST_FUNCTION_DEFINITION) return 0;
    
    SymbolTable* func_table = create_symbol_table(table);
    
    // Add parameters to function's symbol table
    for (int i = 0; i < func->child_count - 1; i++) { // Last child is body
        ASTNode* param = func->children[i];
        if (param->type == AST_VARIABLE_DECLARATION || param->type == AST_PATTERN_VARIABLE) {
            Type* param_type = param->node_type ? clone_type(param->node_type) : create_type(TYPE_UNKNOWN);
            add_symbol(func_table, param->value, param_type, 0, 0, 0);
        }
    }
    
    // Type check function body
    ASTNode* body = func->children[func->child_count - 1];
    typecheck_statement(body, func_table);
    
    free_symbol_table(func_table);
    return 1;
}

int typecheck_struct_definition(ASTNode* struct_def, SymbolTable* table) {
    (void)table;  // Unused for now
    if (!struct_def || struct_def->type != AST_STRUCT_DEFINITION) return 0;
    
    // Type check all fields
    for (int i = 0; i < struct_def->child_count; i++) {
        ASTNode* field = struct_def->children[i];
        
        if (field->type != AST_STRUCT_FIELD) {
            type_error("Invalid struct field", field->line, field->column);
            return 0;
        }
        
        // Verify field type is valid
        if (!field->node_type) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), 
                    "Struct field '%s' has no type", field->value);
            type_error(error_msg, field->line, field->column);
            return 0;
        }
        
        // Check for duplicate field names
        for (int j = 0; j < i; j++) {
            ASTNode* other_field = struct_def->children[j];
            if (strcmp(field->value, other_field->value) == 0) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), 
                        "Duplicate field name '%s' in struct '%s'", 
                        field->value, struct_def->value);
                type_error(error_msg, field->line, field->column);
                return 0;
            }
        }
    }
    
    return 1;
}

int typecheck_statement(ASTNode* stmt, SymbolTable* table) {
    if (!stmt) return 0;
    
    switch (stmt->type) {
        case AST_VARIABLE_DECLARATION: {
            if (stmt->child_count > 0) {
                // Has initializer
                ASTNode* init = stmt->children[0];
                typecheck_expression(init, table);
                Type* init_type = infer_type(init, table);

                // If variable has no explicit type (TYPE_UNKNOWN), use initializer's type
                if (!stmt->node_type || stmt->node_type->kind == TYPE_UNKNOWN) {
                    if (stmt->node_type) free_type(stmt->node_type);
                    stmt->node_type = clone_type(init_type);
                } else if (!is_assignable(init_type, stmt->node_type)) {
                    // Has explicit type but initializer doesn't match
                    type_error("Type mismatch in variable initialization", stmt->line, stmt->column);
                    return 0;
                }
            }

            // Add to symbol table
            add_symbol(table, stmt->value, clone_type(stmt->node_type), 0, 0, 0);
            return 1;
        }
        
        case AST_ASSIGNMENT: {
            if (stmt->child_count >= 2) {
                ASTNode* left = stmt->children[0];
                ASTNode* right = stmt->children[1];
                
                Symbol* symbol = lookup_symbol(table, left->value);
                if (!symbol) {
                    type_error("Undefined variable", left->line, left->column);
                    return 0;
                }
                
                Type* right_type = infer_type(right, table);
                if (!is_assignable(right_type, symbol->type)) {
                    type_error("Type mismatch in assignment", stmt->line, stmt->column);
                    return 0;
                }
            }
            return 1;
        }
        
        case AST_IF_STATEMENT: {
            if (stmt->child_count >= 1) {
                ASTNode* condition = stmt->children[0];
                typecheck_expression(condition, table);
                Type* cond_type = infer_type(condition, table);
                
                if (cond_type->kind != TYPE_BOOL) {
                    type_error("If condition must be boolean", condition->line, condition->column);
                    return 0;
                }
            }
            
            // Type check then and else branches
            for (int i = 1; i < stmt->child_count; i++) {
                typecheck_statement(stmt->children[i], table);
            }
            return 1;
        }
        
        case AST_FOR_LOOP: {
            SymbolTable* loop_table = create_symbol_table(table);
            
            // Type check init (child 0)
            if (stmt->child_count > 0 && stmt->children[0]) {
                typecheck_statement(stmt->children[0], loop_table);
            }
            
            // Type check condition (child 1)
            if (stmt->child_count > 1 && stmt->children[1]) {
                typecheck_expression(stmt->children[1], loop_table);
                Type* cond_type = infer_type(stmt->children[1], loop_table);
                if (cond_type && cond_type->kind != TYPE_BOOL) {
                    type_error("For loop condition must be boolean", stmt->line, stmt->column);
                    free_symbol_table(loop_table);
                    return 0;
                }
            }
            
            // Type check increment (child 2)
            if (stmt->child_count > 2 && stmt->children[2]) {
                typecheck_expression(stmt->children[2], loop_table);
            }
            
            // Type check body (child 3)
            if (stmt->child_count > 3 && stmt->children[3]) {
                typecheck_statement(stmt->children[3], loop_table);
            }
            
            free_symbol_table(loop_table);
            return 1;
        }
        
        case AST_WHILE_LOOP: {
            if (stmt->child_count >= 1) {
                ASTNode* condition = stmt->children[0];
                typecheck_expression(condition, table);
                Type* cond_type = infer_type(condition, table);
                
                if (cond_type && cond_type->kind != TYPE_BOOL) {
                    type_error("Loop condition must be boolean", condition->line, condition->column);
                    return 0;
                }
            }
            
            // Type check loop body
            for (int i = 1; i < stmt->child_count; i++) {
                typecheck_statement(stmt->children[i], table);
            }
            return 1;
        }
        
        case AST_BLOCK: {
            SymbolTable* block_table = create_symbol_table(table);
            
            for (int i = 0; i < stmt->child_count; i++) {
                typecheck_statement(stmt->children[i], block_table);
            }
            
            free_symbol_table(block_table);
            return 1;
        }
        
        case AST_EXPRESSION_STATEMENT: {
            if (stmt->child_count > 0) {
                typecheck_expression(stmt->children[0], table);
            }
            return 1;
        }
        
        case AST_PRINT_STATEMENT: {
            for (int i = 0; i < stmt->child_count; i++) {
                typecheck_expression(stmt->children[i], table);
            }
            return 1;
        }
        
        case AST_SEND_STATEMENT: {
            if (stmt->child_count >= 2) {
                ASTNode* actor_ref = stmt->children[0];
                ASTNode* message = stmt->children[1];
                
                Type* actor_type = infer_type(actor_ref, table);
                if (actor_type->kind != TYPE_ACTOR_REF) {
                    type_error("First argument to send must be an actor reference", actor_ref->line, actor_ref->column);
                    return 0;
                }
                
                typecheck_expression(message, table);
            }
            return 1;
        }
        
        case AST_SPAWN_ACTOR_STATEMENT: {
            if (stmt->child_count > 0) {
                typecheck_expression(stmt->children[0], table);
            }
            return 1;
        }
        
        case AST_MATCH_STATEMENT: {
            // Type check the match expression
            Type* match_expr_type = NULL;
            Type* element_type = NULL;
            if (stmt->child_count > 0) {
                typecheck_expression(stmt->children[0], table);
                match_expr_type = stmt->children[0]->node_type;
                // Extract element type if matching on an array
                if (match_expr_type && match_expr_type->kind == TYPE_ARRAY && match_expr_type->element_type) {
                    element_type = match_expr_type->element_type;
                }
            }
            // Default to int if we couldn't determine the element type
            if (!element_type) {
                element_type = create_type(TYPE_INT);
            }

            // Type check each match arm
            for (int i = 1; i < stmt->child_count; i++) {
                ASTNode* arm = stmt->children[i];
                if (!arm || arm->type != AST_MATCH_ARM || arm->child_count < 2) continue;

                ASTNode* pattern = arm->children[0];
                ASTNode* body = arm->children[1];

                // Create a new scope for pattern variables
                SymbolTable* arm_table = create_symbol_table(table);

                // Register pattern variables from list patterns using the actual element type
                if (pattern->type == AST_PATTERN_LIST) {
                    for (int j = 0; j < pattern->child_count; j++) {
                        ASTNode* elem = pattern->children[j];
                        if (elem && elem->type == AST_PATTERN_VARIABLE && elem->value) {
                            add_symbol(arm_table, elem->value, clone_type(element_type), 0, 0, 0);
                        }
                    }
                } else if (pattern->type == AST_PATTERN_CONS) {
                    // [h|t] - register head and tail with proper types
                    if (pattern->child_count >= 1) {
                        ASTNode* head = pattern->children[0];
                        if (head && head->type == AST_PATTERN_VARIABLE && head->value) {
                            add_symbol(arm_table, head->value, clone_type(element_type), 0, 0, 0);
                        }
                    }
                    if (pattern->child_count >= 2) {
                        ASTNode* tail = pattern->children[1];
                        if (tail && tail->type == AST_PATTERN_VARIABLE && tail->value) {
                            Type* tail_type = create_type(TYPE_ARRAY);
                            tail_type->element_type = clone_type(element_type);
                            add_symbol(arm_table, tail->value, tail_type, 0, 0, 0);
                        }
                    }
                }

                // Type check the arm body in the new scope
                typecheck_statement(body, arm_table);

                free_symbol_table(arm_table);
            }
            return 1;
        }

        default:
            // Type check all children
            for (int i = 0; i < stmt->child_count; i++) {
                typecheck_node(stmt->children[i], table);
            }
            return 1;
    }
}

int typecheck_expression(ASTNode* expr, SymbolTable* table) {
    if (!expr) return 0;
    
    switch (expr->type) {
        case AST_BINARY_EXPRESSION:
            return typecheck_binary_expression(expr, table);
            
        case AST_UNARY_EXPRESSION: {
            if (expr->child_count > 0) {
                typecheck_expression(expr->children[0], table);
                expr->node_type = infer_unary_type(expr->children[0], 
                                                 get_token_type_from_string(expr->value));
            }
            return 1;
        }
        
        case AST_FUNCTION_CALL:
            return typecheck_function_call(expr, table);
            
        case AST_IDENTIFIER: {
            Symbol* symbol = lookup_symbol(table, expr->value);
            if (!symbol) {
                type_error("Undefined variable", expr->line, expr->column);
                return 0;
            }
            expr->node_type = clone_type(symbol->type);
            return 1;
        }
        
        case AST_LITERAL:
            // Literals are already typed
            return 1;
            
        case AST_ARRAY_LITERAL:
            // Type check all array elements
            for (int i = 0; i < expr->child_count; i++) {
                typecheck_expression(expr->children[i], table);
            }
            return 1;
            
        case AST_STRUCT_LITERAL:
            // Type check struct literal field initializers
            for (int i = 0; i < expr->child_count; i++) {
                ASTNode* field_init = expr->children[i];
                if (field_init && field_init->type == AST_ASSIGNMENT && field_init->child_count > 0) {
                    typecheck_expression(field_init->children[0], table);
                }
            }
            // Struct literal type is already set during type inference
            return 1;
            
        case AST_MEMBER_ACCESS: {
            // Type check member access (e.g., msg.type, struct.field)
            if (expr->child_count > 0) {
                ASTNode* base = expr->children[0];
                typecheck_expression(base, table);
                
                Type* base_type = infer_type(base, table);
                
                // Handle Message type member access
                if (base_type && base_type->kind == TYPE_MESSAGE) {
                    if (strcmp(expr->value, "type") == 0 || 
                        strcmp(expr->value, "sender_id") == 0 || 
                        strcmp(expr->value, "payload_int") == 0) {
                        expr->node_type = create_type(TYPE_INT);
                    } else if (strcmp(expr->value, "payload_ptr") == 0) {
                        expr->node_type = create_type(TYPE_VOID);
                    }
                }
                // Handle struct member access — look up field type from definition
                else if (base_type && base_type->kind == TYPE_STRUCT && base_type->struct_name) {
                    Symbol* struct_sym = lookup_symbol(table, base_type->struct_name);
                    if (struct_sym && struct_sym->node) {
                        ASTNode* struct_def = struct_sym->node;
                        for (int fi = 0; fi < struct_def->child_count; fi++) {
                            ASTNode* field = struct_def->children[fi];
                            if (field && field->value && strcmp(field->value, expr->value) == 0) {
                                if (field->node_type && field->node_type->kind != TYPE_UNKNOWN) {
                                    expr->node_type = clone_type(field->node_type);
                                }
                                break;
                            }
                        }
                    }
                    // Fallback to general inference
                    if (!expr->node_type || expr->node_type->kind == TYPE_UNKNOWN) {
                        expr->node_type = infer_type(expr, table);
                    }
                }
            }
            return 1;
        }
            
        default:
            // Type check all children
            for (int i = 0; i < expr->child_count; i++) {
                typecheck_expression(expr->children[i], table);
            }
            return 1;
    }
}

int typecheck_binary_expression(ASTNode* expr, SymbolTable* table) {
    if (!expr || expr->type != AST_BINARY_EXPRESSION || expr->child_count < 2) return 0;
    
    ASTNode* left = expr->children[0];
    ASTNode* right = expr->children[1];
    
    typecheck_expression(left, table);
    typecheck_expression(right, table);
    
    Type* left_type = infer_type(left, table);
    Type* right_type = infer_type(right, table);
    
    AeTokenType operator = get_token_type_from_string(expr->value);
    
    if (operator == TOKEN_ASSIGN) {
        if (!is_assignable(right_type, left_type)) {
            type_error("Type mismatch in assignment", expr->line, expr->column);
            return 0;
        }
        expr->node_type = clone_type(left_type);
    } else {
        Type* result_type = infer_binary_type(left, right, operator);
        if (result_type->kind == TYPE_UNKNOWN &&
            left_type && left_type->kind != TYPE_UNKNOWN &&
            right_type && right_type->kind != TYPE_UNKNOWN) {
            // Only error if both types are known but incompatible
            type_error("Invalid operation for given types", expr->line, expr->column);
            return 0;
        }
        expr->node_type = result_type;
    }
    
    return 1;
}

int typecheck_function_call(ASTNode* call, SymbolTable* table) {
    if (!call || call->type != AST_FUNCTION_CALL) return 0;

    Symbol* symbol = lookup_symbol(table, call->value);
    if (!symbol || !symbol->is_function) {
        type_error("Undefined function", call->line, call->column);
        return 0;
    }
    
    // Type check arguments
    for (int i = 0; i < call->child_count; i++) {
        typecheck_expression(call->children[i], table);
    }
    
    call->node_type = clone_type(symbol->type);
    return 1;
}
