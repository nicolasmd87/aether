#include "type_inference.h"
#include "../aether_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_INFERENCE_ITERATIONS 100

// Create inference context
InferenceContext* create_inference_context(SymbolTable* table) {
    InferenceContext* ctx = (InferenceContext*)malloc(sizeof(InferenceContext));
    ctx->constraints = NULL;
    ctx->constraint_count = 0;
    ctx->constraint_capacity = 0;
    ctx->symbols = table;
    ctx->iteration_count = 0;
    return ctx;
}

void free_inference_context(InferenceContext* ctx) {
    if (!ctx) return;
    
    for (int i = 0; i < ctx->constraint_count; i++) {
        if (ctx->constraints[i].required_type) {
            free_type(ctx->constraints[i].required_type);
        }
    }
    
    if (ctx->constraints) {
        free(ctx->constraints);
    }
    
    free(ctx);
}

// Add constraint
void add_constraint(InferenceContext* ctx, ASTNode* node, Type* type, const char* reason) {
    if (!ctx || !node || !type) return;
    
    // Grow array if needed
    if (ctx->constraint_count >= ctx->constraint_capacity) {
        int new_capacity = ctx->constraint_capacity == 0 ? 16 : ctx->constraint_capacity * 2;
        TypeConstraint* new_constraints = (TypeConstraint*)realloc(ctx->constraints, 
                                                     new_capacity * sizeof(TypeConstraint));
        if (!new_constraints) return;
        ctx->constraints = new_constraints;
        ctx->constraint_capacity = new_capacity;
    }
    
    TypeConstraint* constraint = &ctx->constraints[ctx->constraint_count++];
    constraint->node = node;
    constraint->required_type = clone_type(type);
    constraint->reason = reason;
    constraint->line = node->line;
    constraint->column = node->column;
    constraint->resolved = 0;
}

// Check if type needs inference
int is_type_inferrable(Type* type) {
    return type && type->kind == TYPE_UNKNOWN;
}

// Infer type from literal value
Type* infer_from_literal(const char* value) {
    if (!value) return create_type(TYPE_UNKNOWN);
    
    // Check if it's a number
    int is_float = 0;
    int is_number = 1;
    
    for (const char* p = value; *p; p++) {
        if (*p == '.') {
            is_float = 1;
        } else if (!isdigit(*p) && *p != '-' && *p != '+') {
            is_number = 0;
            break;
        }
    }
    
    if (is_number) {
        return create_type(is_float ? TYPE_FLOAT : TYPE_INT);
    }
    
    // Check if it's a string literal (starts with quote)
    if (value[0] == '"' || value[0] == '\'') {
        return create_type(TYPE_STRING);
    }
    
    // Check for boolean
    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
        return create_type(TYPE_BOOL);
    }
    
    return create_type(TYPE_UNKNOWN);
}

// Infer type from binary operation
Type* infer_from_binary_op(Type* left, Type* right, const char* operator) {
    if (!left || !right) return create_type(TYPE_UNKNOWN);
    
    // Arithmetic operators: +, -, *, /, %
    if (strcmp(operator, "+") == 0 || strcmp(operator, "-") == 0 ||
        strcmp(operator, "*") == 0 || strcmp(operator, "/") == 0 ||
        strcmp(operator, "%") == 0) {
        // If either is int64 (long), promote to int64
        if (left->kind == TYPE_INT64 || right->kind == TYPE_INT64) {
            return create_type(TYPE_INT64);
        }
        // If both are int, result is int
        if (left->kind == TYPE_INT && right->kind == TYPE_INT) {
            return create_type(TYPE_INT);
        }
        // If either is float, result is float
        if (left->kind == TYPE_FLOAT || right->kind == TYPE_FLOAT) {
            return create_type(TYPE_FLOAT);
        }
        // String concatenation for +
        if (strcmp(operator, "+") == 0 && 
            (left->kind == TYPE_STRING || right->kind == TYPE_STRING)) {
            return create_type(TYPE_STRING);
        }
    }
    
    // Comparison operators: ==, !=, <, <=, >, >=
    if (strcmp(operator, "==") == 0 || strcmp(operator, "!=") == 0 ||
        strcmp(operator, "<") == 0 || strcmp(operator, "<=") == 0 ||
        strcmp(operator, ">") == 0 || strcmp(operator, ">=") == 0) {
        return create_type(TYPE_BOOL);
    }
    
    // Logical operators: &&, ||
    if (strcmp(operator, "&&") == 0 || strcmp(operator, "||") == 0) {
        return create_type(TYPE_BOOL);
    }
    
    return create_type(TYPE_UNKNOWN);
}

// Collect constraints from literals
void collect_literal_constraints(ASTNode* node, InferenceContext* ctx) {
    if (!node || node->type != AST_LITERAL) return;
    
    if (node->node_type && is_type_inferrable(node->node_type)) {
        Type* inferred = infer_from_literal(node->value);
        if (inferred->kind != TYPE_UNKNOWN) {
            add_constraint(ctx, node, inferred, "literal type inference");
            node->node_type = clone_type(inferred);
        }
        free_type(inferred);
    }
}

// Collect constraints from expressions
void collect_expression_constraints(ASTNode* node, InferenceContext* ctx) {
    if (!node) return;
    
    switch (node->type) {
        case AST_BINARY_EXPRESSION:
            if (node->child_count >= 2) {
                collect_constraints(node->children[0], ctx);
                collect_constraints(node->children[1], ctx);
                
                Type* left_type = node->children[0]->node_type;
                Type* right_type = node->children[1]->node_type;
                
                if (left_type && right_type && node->value) {
                    Type* result_type = infer_from_binary_op(left_type, right_type, node->value);
                    if (result_type->kind != TYPE_UNKNOWN) {
                        if (node->node_type) free_type(node->node_type);
                        node->node_type = result_type;
                    } else {
                        free_type(result_type);
                    }
                }
            }
            break;
            
        case AST_COMPOUND_ASSIGNMENT:
            // children[0] = operator, children[1] = RHS expression
            if (node->child_count >= 2) {
                collect_constraints(node->children[1], ctx);
                // Infer type from existing variable
                if (node->value && ctx->symbols) {
                    Symbol* sym = lookup_symbol(ctx->symbols, node->value);
                    if (sym && sym->type && sym->type->kind != TYPE_UNKNOWN) {
                        if (!node->node_type || node->node_type->kind == TYPE_UNKNOWN) {
                            if (node->node_type) free_type(node->node_type);
                            node->node_type = clone_type(sym->type);
                        }
                    }
                }
            }
            break;

        case AST_VARIABLE_DECLARATION:
        case AST_STATE_DECLARATION:
            // Always process initializer if present (even with explicit types)
            if (node->child_count > 0) {
                collect_constraints(node->children[0], ctx);
                
                // If declaration type is unknown, infer it from initializer
                if (is_type_inferrable(node->node_type)) {
                    Type* init_type = node->children[0]->node_type;
                    if (init_type && init_type->kind != TYPE_UNKNOWN) {
                        node->node_type = clone_type(init_type);
                        add_constraint(ctx, node, init_type, "variable initialization");
                    }
                }
            }
            
            // Add variable to symbol table for later lookups (member access, etc.)
            if (node->value && node->node_type && node->node_type->kind != TYPE_UNKNOWN && ctx->symbols) {
                Symbol* existing = lookup_symbol_local(ctx->symbols, node->value);
                if (existing) {
                    // Update existing symbol's type
                    if (existing->type) free_type(existing->type);
                    existing->type = clone_type(node->node_type);
                } else {
                    // Add new symbol
                    add_symbol(ctx->symbols, node->value, clone_type(node->node_type), 0, 0, 0);
                }
            }
            break;
            
        case AST_IDENTIFIER:
            // Look up in symbol table
            if (ctx->symbols) {
                Symbol* sym = lookup_symbol(ctx->symbols, node->value);
                if (sym && sym->type && sym->type->kind != TYPE_UNKNOWN) {
                    if (!node->node_type || is_type_inferrable(node->node_type)) {
                        node->node_type = clone_type(sym->type);
                    }
                }
            }
            break;
            
        case AST_FUNCTION_CALL:
            // Process argument expressions
            for (int i = 0; i < node->child_count; i++) {
                collect_constraints(node->children[i], ctx);
            }
            
            // Look up function definition to get return type
            if (node->value) {
                Symbol* func_sym = lookup_symbol(ctx->symbols, node->value);
                if (func_sym && func_sym->type) {
                    // Function call inherits the function's return type
                    if (!node->node_type || node->node_type->kind == TYPE_UNKNOWN) {
                        node->node_type = clone_type(func_sym->type);
                    }
                }
            }
            break;
            
        case AST_RETURN_STATEMENT:
            // Infer from return expression
            if (node->child_count > 0) {
                collect_constraints(node->children[0], ctx);
                if (!node->node_type || is_type_inferrable(node->node_type)) {
                    Type* expr_type = node->children[0]->node_type;
                    if (expr_type && expr_type->kind != TYPE_UNKNOWN) {
                        node->node_type = clone_type(expr_type);
                    }
                }
            }
            break;
            
        case AST_MEMBER_ACCESS:
            // Member access: expr.field
            // Infer type from the struct field or Message type
            if (node->child_count > 0 && node->value) {
                ASTNode* base_expr = node->children[0];
                collect_constraints(base_expr, ctx);
                
                // Get the base expression's type
                Type* base_type = base_expr->node_type;
                
                // Handle Message type member access
                if (base_type && base_type->kind == TYPE_MESSAGE) {
                    // Message has fields: type (int), sender_id (int), payload_int (int), payload_ptr (void*)
                    if (strcmp(node->value, "type") == 0 || 
                        strcmp(node->value, "sender_id") == 0 || 
                        strcmp(node->value, "payload_int") == 0) {
                        node->node_type = create_type(TYPE_INT);
                    } else if (strcmp(node->value, "payload_ptr") == 0) {
                        node->node_type = create_type(TYPE_VOID); // void* represented as void
                    }
                }
                // Handle struct type member access
                else if (base_type && base_type->kind == TYPE_STRUCT && ctx->symbols) {
                    // Look up the struct definition
                    Symbol* struct_sym = lookup_symbol(ctx->symbols, base_type->struct_name);
                    
                    if (struct_sym && struct_sym->node) {
                        ASTNode* struct_def = struct_sym->node;
                        // Find the field in the struct definition
                        for (int i = 0; i < struct_def->child_count; i++) {
                            ASTNode* field = struct_def->children[i];
                            if (field && field->type == AST_STRUCT_FIELD && 
                                field->value && strcmp(field->value, node->value) == 0) {
                                // Found matching field - use its type
                                if (field->node_type) {
                                    node->node_type = clone_type(field->node_type);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            break;
            
        case AST_ARRAY_ACCESS:
            // Array access: arr[index]
            // Infer element type from array type
            if (node->child_count >= 2) {
                ASTNode* array_expr = node->children[0];
                ASTNode* index_expr = node->children[1];
                
                collect_constraints(array_expr, ctx);
                collect_constraints(index_expr, ctx);
                
                // Get array type and extract element type
                Type* array_type = array_expr->node_type;
                if (array_type && array_type->kind == TYPE_ARRAY && array_type->element_type) {
                    node->node_type = clone_type(array_type->element_type);
                }
            }
            break;
            
        case AST_STRUCT_LITERAL:
            // Struct literal: StructName{ field: value, ... }
            // Look up struct definition to propagate field types
            if (node->value && ctx->symbols) {
                Symbol* struct_sym = lookup_symbol(ctx->symbols, node->value);
                
                if (struct_sym && struct_sym->type && struct_sym->type->kind == TYPE_STRUCT) {
                    // Set the struct literal's type to the struct type
                    node->node_type = clone_type(struct_sym->type);
                }
                
                // Process each field initializer and propagate types to struct definition
                for (int i = 0; i < node->child_count; i++) {
                    ASTNode* field_init = node->children[i];
                    if (field_init && field_init->type == AST_ASSIGNMENT && field_init->child_count > 0) {
                        // Collect constraints from the value expression
                        collect_constraints(field_init->children[0], ctx);
                        
                        Type* val_type = field_init->children[0]->node_type;
                        
                        // Propagate field type back to struct definition
                        if (struct_sym && struct_sym->node && val_type && val_type->kind != TYPE_UNKNOWN) {
                            ASTNode* struct_def = struct_sym->node;
                            const char* field_name = field_init->value;
                            
                            // Find matching field in struct definition
                            for (int j = 0; j < struct_def->child_count; j++) {
                                ASTNode* field = struct_def->children[j];
                                if (field && field->type == AST_STRUCT_FIELD && 
                                    field->value && strcmp(field->value, field_name) == 0) {
                                    // Update field type if it's unknown
                                    if (!field->node_type || field->node_type->kind == TYPE_UNKNOWN) {
                                        if (field->node_type) free_type(field->node_type);
                                        field->node_type = clone_type(val_type);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            break;
            
        default:
            // Recursively collect from children
            for (int i = 0; i < node->child_count; i++) {
                collect_constraints(node->children[i], ctx);
            }
            break;
    }
}

// Infer return type from return statements in function body.
// Called with is_top_level=true only from infer_function_return_types; the
// recursive descent into control-flow children uses is_top_level=false.
// Given an identifier name, scan preceding siblings in a block for the
// variable declaration of that name and return the type of its initializer.
static Type* resolve_local_var_type(const char* name, ASTNode* block, int before_index, SymbolTable* symbols) {
    if (!name || !block) return NULL;
    for (int i = before_index - 1; i >= 0; i--) {
        ASTNode* stmt = block->children[i];
        if (!stmt) continue;
        if (stmt->type == AST_VARIABLE_DECLARATION && stmt->value &&
            strcmp(stmt->value, name) == 0 && stmt->child_count > 0) {
            ASTNode* init = stmt->children[0];
            // Use node_type if already set
            if (init->node_type && init->node_type->kind != TYPE_UNKNOWN) {
                return clone_type(init->node_type);
            }
            // Recognize common expression types directly
            if (init->type == AST_STRING_INTERP) return create_type(TYPE_STRING);
            if (init->type == AST_NULL_LITERAL) return create_type(TYPE_PTR);
            if (init->type == AST_LITERAL && init->node_type)
                return clone_type(init->node_type);
            if (init->type == AST_ARRAY_LITERAL)
                return init->node_type ? clone_type(init->node_type) : create_type(TYPE_ARRAY);
            if (init->type == AST_STRUCT_LITERAL && init->node_type)
                return clone_type(init->node_type);
            // Function call — look up function return type in symbol table
            if (init->type == AST_FUNCTION_CALL && init->value && symbols) {
                Symbol* func_sym = lookup_symbol(symbols, init->value);
                if (func_sym && func_sym->type && func_sym->type->kind != TYPE_UNKNOWN)
                    return clone_type(func_sym->type);
            }
            break;
        }
    }
    return NULL;
}

static Type* infer_return_type_impl(ASTNode* body, SymbolTable* symbols, bool is_top_level) {
    if (!body) return NULL;

    if (body->type == AST_RETURN_STATEMENT && body->child_count > 0) {
        ASTNode* return_expr = body->children[0];
        // Unwrap AST_EXPRESSION_STATEMENT (created by implicit return wrapping)
        if (return_expr->type == AST_EXPRESSION_STATEMENT && return_expr->child_count > 0) {
            return_expr = return_expr->children[0];
        }
        if (return_expr->node_type && return_expr->node_type->kind != TYPE_UNKNOWN) {
            return clone_type(return_expr->node_type);
        }
        // node_type may not be set on identifiers — resolve via symbol table
        if (return_expr->type == AST_IDENTIFIER && return_expr->value && symbols) {
            Symbol* sym = lookup_symbol(symbols, return_expr->value);
            if (sym && sym->type && sym->type->kind != TYPE_UNKNOWN) {
                return clone_type(sym->type);
            }
        }
        // String interpolation always returns a string (ptr)
        if (return_expr->type == AST_STRING_INTERP) {
            return create_type(TYPE_STRING);
        }
    }

    // Arrow function: the body IS the return expression (not a block).
    // Only applies at the top level (direct child of the function node).
    if (is_top_level &&
        body->type != AST_BLOCK && body->type != AST_RETURN_STATEMENT) {
        if (body->node_type && body->node_type->kind != TYPE_UNKNOWN &&
            body->node_type->kind != TYPE_VOID) {
            return clone_type(body->node_type);
        }
        // Resolve identifier types via symbol table
        if (body->type == AST_IDENTIFIER && body->value && symbols) {
            Symbol* sym = lookup_symbol(symbols, body->value);
            if (sym && sym->type && sym->type->kind != TYPE_UNKNOWN) {
                return clone_type(sym->type);
            }
        }
        if (body->type == AST_STRING_INTERP) {
            return create_type(TYPE_STRING);
        }
    }

    // Only descend into control-flow nodes that may contain return statements.
    // This avoids mistaking a string literal inside print() for a return type.
    switch (body->type) {
        case AST_BLOCK:
            for (int i = 0; i < body->child_count; i++) {
                ASTNode* child = body->children[i];
                if (!child) continue;
                // For return statements in a block, resolve local variable types
                // from preceding siblings before recursing.
                if (child->type == AST_RETURN_STATEMENT && child->child_count > 0) {
                    ASTNode* ret_expr = child->children[0];
                    // Unwrap AST_EXPRESSION_STATEMENT (from implicit return)
                    if (ret_expr->type == AST_EXPRESSION_STATEMENT && ret_expr->child_count > 0)
                        ret_expr = ret_expr->children[0];
                    if (ret_expr->type == AST_IDENTIFIER && ret_expr->value &&
                        (!ret_expr->node_type || ret_expr->node_type->kind == TYPE_UNKNOWN)) {
                        Type* local_type = resolve_local_var_type(ret_expr->value, body, i, symbols);
                        if (local_type) return local_type;
                    }
                }
                Type* rt = infer_return_type_impl(child, symbols, false);
                if (rt) return rt;
            }
            break;
        case AST_IF_STATEMENT:
        case AST_FOR_LOOP:
        case AST_WHILE_LOOP:
        case AST_SWITCH_STATEMENT:
        case AST_MATCH_STATEMENT:
        case AST_MATCH_ARM:
        case AST_DEFER_STATEMENT:
            for (int i = 0; i < body->child_count; i++) {
                if (!body->children[i]) continue;
                Type* rt = infer_return_type_impl(body->children[i], symbols, false);
                if (rt) return rt;
            }
            break;
        default:
            break;
    }

    return NULL;
}

Type* infer_return_type_from_body(ASTNode* body, SymbolTable* symbols) {
    return infer_return_type_impl(body, symbols, true);
}

// Collect constraints from function
void collect_function_constraints(ASTNode* node, InferenceContext* ctx) {
    if (!node || node->type != AST_FUNCTION_DEFINITION) return;
    
    // Add parameters to symbol table so identifiers in function body can look them up
    int body_index = node->child_count - 1;
    for (int i = 0; i < body_index; i++) {
        ASTNode* param = node->children[i];
        if (param && param->value && param->node_type &&
            (param->type == AST_VARIABLE_DECLARATION || param->type == AST_PATTERN_VARIABLE)) {
            // Check if parameter already exists in symbol table
            Symbol* existing = lookup_symbol(ctx->symbols, param->value);
            if (existing) {
                // Update existing symbol's type if we now have a more specific type
                if (param->node_type->kind != TYPE_UNKNOWN) {
                    if (existing->type) free_type(existing->type);
                    existing->type = clone_type(param->node_type);
                }
            } else {
                // Add new symbol
                add_symbol(ctx->symbols, param->value, clone_type(param->node_type), 0, 0, 0);
            }
        }
    }
    
    // Collect constraints from function body
    if (body_index >= 0 && body_index < node->child_count) {
        collect_constraints(node->children[body_index], ctx);
    }
}

// Main constraint collection
void collect_constraints(ASTNode* node, InferenceContext* ctx) {
    if (!node) return;
    
    switch (node->type) {
        case AST_LITERAL:
            collect_literal_constraints(node, ctx);
            break;

        case AST_NULL_LITERAL:
            if (!node->node_type) node->node_type = create_type(TYPE_PTR);
            break;

        case AST_ARRAY_LITERAL:
            // Infer array type from first element
            if (node->child_count > 0) {
                collect_constraints(node->children[0], ctx);
                Type* elem_type = node->children[0]->node_type;
                if (elem_type && elem_type->kind != TYPE_UNKNOWN) {
                    // Create array type with dynamic size (-1)
                    Type* array_type = create_array_type(clone_type(elem_type), node->child_count);
                    node->node_type = array_type;
                    add_constraint(ctx, node, array_type, "array literal type inference");
                }
                // Collect constraints for all elements
                for (int i = 1; i < node->child_count; i++) {
                    collect_constraints(node->children[i], ctx);
                }
            }
            break;
            
        case AST_FUNCTION_DEFINITION:
            collect_function_constraints(node, ctx);
            break;
            
        case AST_STRUCT_DEFINITION:
            // Struct fields with initializers
            for (int i = 0; i < node->child_count; i++) {
                collect_constraints(node->children[i], ctx);
            }
            break;
            
        default:
            collect_expression_constraints(node, ctx);
            break;
    }
}

// Check if there are unresolved types
int has_unresolved_types(InferenceContext* ctx) {
    if (!ctx || !ctx->constraints) return 0;
    for (int i = 0; i < ctx->constraint_count; i++) {
        if (!ctx->constraints[i].resolved) {
            return 1;
        }
    }
    return 0;
}

// Propagate known types through constraint graph
void propagate_known_types(InferenceContext* ctx) {
    // Track progress for iterative propagation
    int progress = 0;
    (void)progress;  // Reserved for future iterative algorithm
    
    for (int i = 0; i < ctx->constraint_count; i++) {
        TypeConstraint* constraint = &ctx->constraints[i];
        
        if (constraint->resolved) continue;
        
        ASTNode* node = constraint->node;
        Type* required_type = constraint->required_type;
        
        // If node doesn't have a type or has unknown type, apply constraint
        if (!node->node_type || is_type_inferrable(node->node_type)) {
            if (node->node_type) {
                free_type(node->node_type);
            }
            node->node_type = clone_type(required_type);
            constraint->resolved = 1;
            progress = 1;
        }
        // If types match, mark as resolved
        else if (types_equal(node->node_type, required_type)) {
            constraint->resolved = 1;
            progress = 1;
        }
    }
}

// Solve constraints iteratively
int solve_constraints(InferenceContext* ctx) {
    ctx->iteration_count = 0;
    
    while (has_unresolved_types(ctx) && ctx->iteration_count < MAX_INFERENCE_ITERATIONS) {
        propagate_known_types(ctx);
        ctx->iteration_count++;
    }
    
    if (ctx->iteration_count >= MAX_INFERENCE_ITERATIONS) {
        report_ambiguous_types(ctx);
        return 0;
    }
    
    return 1;
}

// Report ambiguous types
void report_ambiguous_types(InferenceContext* ctx) {
    for (int i = 0; i < ctx->constraint_count; i++) {
        if (!ctx->constraints[i].resolved) {
            TypeConstraint* c = &ctx->constraints[i];
            aether_error_with_suggestion(
                c->reason ? c->reason : "cannot infer type",
                c->line, c->column,
                "add an explicit type annotation, e.g. x: int = ...");
        }
    }
}

// Propagate types from function call sites to function definitions
int propagate_function_call_types(ASTNode* program, SymbolTable* table);
int propagate_call_types_in_tree(ASTNode* tree, const char* func_name, ASTNode* func_def, int param_count);

// Helper to recursively find function calls and propagate types
int propagate_call_types_in_tree(ASTNode* tree, const char* func_name, ASTNode* func_def, int param_count) {
    if (!tree || !func_name) return 0;
    int changed = 0;

    // For function definitions: skip parameter nodes but recurse into the body
    if (tree->type == AST_FUNCTION_DEFINITION) {
        int body_idx = tree->child_count - 1;
        if (body_idx >= 0 && tree->children[body_idx]) {
            changed += propagate_call_types_in_tree(tree->children[body_idx], func_name, func_def, param_count);
        }
        return changed;
    }
    
    // Check if this is a function call to our target function
    if (tree->type == AST_FUNCTION_CALL && tree->value && strcmp(tree->value, func_name) == 0) {
        // This is a call to our function - propagate argument types to parameters
        int arg_count = tree->child_count;
        for (int i = 0; i < arg_count && i < param_count; i++) {
            ASTNode* arg = tree->children[i];
            ASTNode* param = func_def->children[i];

            if (arg && param &&
                (param->type == AST_VARIABLE_DECLARATION || param->type == AST_PATTERN_VARIABLE)) {
                // If parameter type is unknown and argument type is known, propagate it
                if ((!param->node_type || param->node_type->kind == TYPE_UNKNOWN) &&
                    arg->node_type && arg->node_type->kind != TYPE_UNKNOWN) {
                    if (param->node_type) free_type(param->node_type);
                    param->node_type = clone_type(arg->node_type);
                    changed++;
                }
            }
        }
    }

    // Recursively process all children
    for (int i = 0; i < tree->child_count; i++) {
        changed += propagate_call_types_in_tree(tree->children[i], func_name, func_def, param_count);
    }
    return changed;
}

// Propagate types from function call sites to function definitions.
// Returns the number of type updates made (0 = stable, nothing changed).
int propagate_function_call_types(ASTNode* program, SymbolTable* table) {
    (void)table;  // Unused for now
    if (!program) return 0;
    int total_changed = 0;

    // Find all function calls and match them with definitions
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* node = program->children[i];
        if (!node) continue;

        // Look for function definitions
        if (node->type == AST_FUNCTION_DEFINITION && node->value) {
            const char* func_name = node->value;
            int param_count = node->child_count - 1; // Last child is body

            // Search every other top-level node (including other function bodies)
            for (int j = 0; j < program->child_count; j++) {
                if (i != j) {
                    total_changed += propagate_call_types_in_tree(program->children[j], func_name, node, param_count);
                }
            }
        }
    }
    return total_changed;
}

// Infer return types for all functions
void infer_function_return_types(ASTNode* program, SymbolTable* table) {
    if (!program) return;
    
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* node = program->children[i];
        if (!node || node->type != AST_FUNCTION_DEFINITION) continue;
        
        // Infer return type from return statements
        int body_index = node->child_count - 1;
        if (body_index >= 0 && body_index < node->child_count) {
            if (!node->node_type || node->node_type->kind == TYPE_UNKNOWN) {
                Type* return_type = infer_return_type_from_body(node->children[body_index], table);
                if (return_type) {
                    node->node_type = return_type;
                } else {
                    // No explicit return, assume void
                    node->node_type = create_type(TYPE_VOID);
                }
            }
        }
    }
}

// Main inference function
int infer_all_types(ASTNode* program, SymbolTable* table) {
    if (!program) return 0;
    
    InferenceContext* ctx = create_inference_context(table);
    
    // Phase 1: Collect constraints from the entire program
    collect_constraints(program, ctx);
    
    // Phase 2: Solve basic constraints
    int success = solve_constraints(ctx);
    
    // Phase 3-5: Interleaved propagation + constraint solving.
    // Each pass: propagate call-site types into parameter definitions,
    // then re-collect and re-solve so that identifier references inside
    // function bodies pick up the newly resolved parameter types.
    // This handles deep call chains (a->b->c->d) where each level needs
    // one propagation pass followed by one constraint-solve pass.
    for (int pass = 0; pass < MAX_INFERENCE_ITERATIONS; pass++) {
        int changed = propagate_function_call_types(program, table);

        free_inference_context(ctx);
        ctx = create_inference_context(table);
        collect_constraints(program, ctx);
        success = solve_constraints(ctx);

        if (changed == 0) break;
    }
    
    // Phase 6: Infer function return types (now that return expressions have types)
    infer_function_return_types(program, table);
    
    free_inference_context(ctx);
    
    return success;
}

