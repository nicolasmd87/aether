#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "typechecker.h"

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
    // For now, assume all types are callable if they have a function call AST node
    return type != NULL;
}

// Type inference functions
Type* infer_type(ASTNode* expr, SymbolTable* table) {
    if (!expr) return NULL;
    
    switch (expr->type) {
        case AST_LITERAL:
            return clone_type(expr->node_type);
            
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
            
        default:
            return create_type(TYPE_UNKNOWN);
    }
}

Type* infer_binary_type(ASTNode* left, ASTNode* right, AeTokenType operator) {
    Type* left_type = left ? left->node_type : NULL;
    Type* right_type = right ? right->node_type : NULL;
    
    if (!left_type || !right_type) return create_type(TYPE_UNKNOWN);
    
    switch (operator) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
        case TOKEN_MODULO:
            // Numeric operations
            if (left_type->kind == TYPE_FLOAT || right_type->kind == TYPE_FLOAT) {
                return create_type(TYPE_FLOAT);
            }
            if (left_type->kind == TYPE_INT && right_type->kind == TYPE_INT) {
                return create_type(TYPE_INT);
            }
            break;
            
        case TOKEN_EQUALS:
        case TOKEN_NOT_EQUALS:
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_AND:
        case TOKEN_OR:
            return create_type(TYPE_BOOL);
            
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
    
    // First pass: collect all declarations
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        
        switch (child->type) {
            case AST_ACTOR_DEFINITION: {
                Type* actor_type = create_type(TYPE_UNKNOWN);
                add_symbol(global_table, child->value, actor_type, 1, 0, 0);
                break;
            }
            case AST_FUNCTION_DEFINITION: {
                add_symbol(global_table, child->value, clone_type(child->node_type), 0, 1, 0);
                break;
            }
            case AST_STRUCT_DEFINITION: {
                Type* struct_type = create_type(TYPE_STRUCT);
                struct_type->struct_name = strdup(child->value);
                add_symbol(global_table, child->value, struct_type, 0, 0, 0);
                break;
            }
            case AST_MAIN_FUNCTION:
                // Main function doesn't need to be in symbol table
                break;
            default:
                break;
        }
    }
    
    // Second pass: type check all nodes
    for (int i = 0; i < program->child_count; i++) {
        typecheck_node(program->children[i], global_table);
    }
    
    free_symbol_table(global_table);
    
    if (error_count > 0) {
        fprintf(stderr, "Type checking failed with %d errors and %d warnings\n", error_count, warning_count);
        return 0;
    }
    
    if (warning_count > 0) {
        fprintf(stderr, "Type checking completed with %d warnings\n", warning_count);
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
        case AST_STRUCT_DEFINITION:
            return typecheck_struct_definition(node, table);
        case AST_MAIN_FUNCTION:
            return typecheck_statement(node, table);
        default:
            return typecheck_statement(node, table);
    }
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
        if (param->type == AST_VARIABLE_DECLARATION) {
            add_symbol(func_table, param->value, clone_type(param->node_type), 0, 0, 0);
        }
    }
    
    // Type check function body
    ASTNode* body = func->children[func->child_count - 1];
    typecheck_statement(body, func_table);
    
    free_symbol_table(func_table);
    return 1;
}

int typecheck_struct_definition(ASTNode* struct_def, SymbolTable* table) {
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
                
                if (!is_assignable(init_type, stmt->node_type)) {
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
        if (result_type->kind == TYPE_UNKNOWN) {
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
