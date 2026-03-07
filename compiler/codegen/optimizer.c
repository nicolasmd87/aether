#include "optimizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

OptimizationStats global_opt_stats = {0, 0, 0, 0, 0};

void reset_optimization_stats() {
    global_opt_stats.constants_folded = 0;
    global_opt_stats.dead_code_removed = 0;
    global_opt_stats.tail_calls_detected = 0;
    global_opt_stats.series_loops_collapsed = 0;
    global_opt_stats.linear_loops_collapsed = 0;
}

void print_optimization_stats() {
    printf("Optimization Statistics:\n");
    printf("  Constants folded: %d\n", global_opt_stats.constants_folded);
    printf("  Dead code removed: %d\n", global_opt_stats.dead_code_removed);
    printf("  Tail calls detected: %d (optimized by C backend at -O2)\n", global_opt_stats.tail_calls_detected);
    printf("  Series loops collapsed: %d\n", global_opt_stats.series_loops_collapsed);
    printf("  Linear loops collapsed: %d\n", global_opt_stats.linear_loops_collapsed);
}

// Helper: check if node is a numeric literal constant (not string/bool)
static int is_constant(ASTNode* node) {
    if (!node || node->type != AST_LITERAL || !node->value) return 0;
    // Exclude booleans and strings from numeric constant folding
    if (node->node_type) {
        if (node->node_type->kind == TYPE_STRING) return 0;
        if (node->node_type->kind == TYPE_BOOL) return 0;
    }
    // Also check by value for untyped literals
    if (strcmp(node->value, "true") == 0 || strcmp(node->value, "false") == 0) return 0;
    return 1;
}

// Helper: get numeric value from literal
static double get_constant_value(ASTNode* node) {
    if (!node || !node->value) return 0.0;
    // Handle boolean literals: "true" = 1.0, "false" = 0.0
    if (strcmp(node->value, "true") == 0) return 1.0;
    if (strcmp(node->value, "false") == 0) return 0.0;
    return atof(node->value);
}

// Helper: create a literal node with a numeric value
static ASTNode* create_numeric_literal(double value, int is_int, int line, int column) {
    char buffer[64];
    if (is_int && value == (double)(long long)value) {
        snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    } else {
        snprintf(buffer, sizeof(buffer), "%.10g", value);
    }
    ASTNode* node = create_ast_node(AST_LITERAL, buffer, line, column);
    node->node_type = create_type(is_int ? TYPE_INT : TYPE_FLOAT);
    return node;
}

// Constant folding for binary expressions
static ASTNode* fold_binary_expression(ASTNode* node) {
    if (!node || node->type != AST_BINARY_EXPRESSION) return node;
    if (node->child_count < 2) return node;
    
    ASTNode* left = node->children[0];
    ASTNode* right = node->children[1];
    
    // Recursively fold children first
    left = optimize_constant_folding(left);
    right = optimize_constant_folding(right);
    node->children[0] = left;
    node->children[1] = right;
    
    // If both operands are constants, fold the expression
    if (is_constant(left) && is_constant(right)) {
        double left_val = get_constant_value(left);
        double right_val = get_constant_value(right);
        double result = 0.0;
        int can_fold = 1;
        
        const char* op = node->value;
        if (strcmp(op, "+") == 0) {
            result = left_val + right_val;
        } else if (strcmp(op, "-") == 0) {
            result = left_val - right_val;
        } else if (strcmp(op, "*") == 0) {
            result = left_val * right_val;
        } else if (strcmp(op, "/") == 0) {
            if (right_val != 0.0) {
                result = left_val / right_val;
            } else {
                can_fold = 0; // Division by zero, can't fold
            }
        } else if (strcmp(op, "%") == 0) {
            if (right_val != 0.0) {
                result = fmod(left_val, right_val);
            } else {
                can_fold = 0;
            }
        } else {
            can_fold = 0; // Unknown operator
        }
        
        if (can_fold) {
            global_opt_stats.constants_folded++;
            // Preserve integer type if both operands are integers
            int both_int = (left->node_type && left->node_type->kind == TYPE_INT) &&
                           (right->node_type && right->node_type->kind == TYPE_INT);
            ASTNode* folded = create_numeric_literal(result, both_int, node->line, node->column);
            
            // Free old node (but not the original structure, return new one)
            free(node->value);
            free_type(node->node_type);
            free(node->children);
            free(node);
            
            return folded;
        }
    }
    
    return node;
}

// Constant folding main function
ASTNode* optimize_constant_folding(ASTNode* node) {
    if (!node) return NULL;
    
    // Handle binary expressions
    if (node->type == AST_BINARY_EXPRESSION) {
        return fold_binary_expression(node);
    }
    
    // Recursively optimize children
    for (int i = 0; i < node->child_count; i++) {
        node->children[i] = optimize_constant_folding(node->children[i]);
    }
    
    return node;
}

// Helper: check if condition is a compile-time known constant (numeric or boolean)
static int is_constant_condition(ASTNode* node, int* is_truthy) {
    if (!node || !node->value) return 0;
    if (node->type == AST_LITERAL) {
        // Boolean literals
        if (strcmp(node->value, "true") == 0) { *is_truthy = 1; return 1; }
        if (strcmp(node->value, "false") == 0) { *is_truthy = 0; return 1; }
        // Numeric literals (exclude strings)
        if (node->node_type && node->node_type->kind == TYPE_STRING) return 0;
        double val = atof(node->value);
        *is_truthy = (val != 0.0);
        return 1;
    }
    return 0;
}

// Dead code elimination
ASTNode* optimize_dead_code(ASTNode* node) {
    if (!node) return NULL;

    // If statement with constant condition
    if (node->type == AST_IF_STATEMENT && node->child_count >= 2) {
        ASTNode* condition = node->children[0];
        int truthy = 0;
        if (is_constant_condition(condition, &truthy)) {
            if (truthy) {
                // Condition is always true, replace with then-branch
                global_opt_stats.dead_code_removed++;
                ASTNode* then_branch = node->children[1];
                
                // Detach then-branch from parent
                for (int i = 1; i < node->child_count - 1; i++) {
                    node->children[i] = node->children[i + 1];
                }
                node->child_count--;
                
                // Free the original if node and return then-branch
                free(node->value);
                free_type(node->node_type);
                free(node->children);
                free(node);
                
                return optimize_dead_code(then_branch);
            } else if (node->child_count >= 3) {
                // Condition is always false, replace with else-branch
                global_opt_stats.dead_code_removed++;
                ASTNode* else_branch = node->children[2];
                
                // Similar detachment and cleanup
                free(node->value);
                free_type(node->node_type);
                free(node->children);
                free(node);
                
                return optimize_dead_code(else_branch);
            } else {
                // No else branch, remove entire if
                global_opt_stats.dead_code_removed++;
                free_ast_node(node);
                return NULL;
            }
        }
    }
    
    // While loop with constant false condition
    if (node->type == AST_WHILE_LOOP && node->child_count >= 1) {
        ASTNode* condition = node->children[0];
        int while_truthy = 0;
        if (is_constant_condition(condition, &while_truthy)) {
            if (!while_truthy) {
                // Loop never executes
                global_opt_stats.dead_code_removed++;
                free_ast_node(node);
                return NULL;
            }
        }
    }
    
    // Recursively optimize children
    int new_count = 0;
    for (int i = 0; i < node->child_count; i++) {
        ASTNode* optimized = optimize_dead_code(node->children[i]);
        if (optimized) {
            node->children[new_count++] = optimized;
        }
    }
    node->child_count = new_count;
    
    return node;
}

// Tail call optimization helper: check if call is in tail position
static int is_tail_call(ASTNode* function, ASTNode* call) {
    if (!call || call->type != AST_FUNCTION_CALL) return 0;
    if (!call->value || !function->value) return 0;
    
    // Check if calling the same function
    return strcmp(call->value, function->value) == 0;
}

// Tail call optimization
ASTNode* optimize_tail_calls(ASTNode* node) {
    if (!node) return NULL;
    
    // Look for function definitions
    if (node->type == AST_FUNCTION_DEFINITION && node->child_count > 0) {
        // Find return statements in the function body
        ASTNode* body = NULL;
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i] && node->children[i]->type == AST_BLOCK) {
                body = node->children[i];
                break;
            }
        }
        
        if (body && body->child_count > 0) {
            // Check last statement for tail call
            ASTNode* last_stmt = body->children[body->child_count - 1];
            if (!last_stmt) goto recurse_children;

            if (last_stmt->type == AST_RETURN_STATEMENT &&
                last_stmt->child_count > 0 &&
                is_tail_call(node, last_stmt->children[0])) {
                
                global_opt_stats.tail_calls_detected++;
                // GCC/Clang optimize tail calls into loops at -O2.
                // ae build uses -O2, so no source-level transformation needed.
            }
        }
    }
    
recurse_children:
    // Recursively optimize children
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i]) {
            node->children[i] = optimize_tail_calls(node->children[i]);
        }
    }

    return node;
}

// Apply all optimizations
ASTNode* optimize_ast(ASTNode* node) {
    if (!node) return NULL;
    
    reset_optimization_stats();
    
    // Apply optimizations in order
    node = optimize_constant_folding(node);
    node = optimize_dead_code(node);
    node = optimize_tail_calls(node);
    
    return node;
}

