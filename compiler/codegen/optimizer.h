#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "../ast.h"

// Compiler optimization passes

// Constant folding: evaluate constant expressions at compile time
// Example: 2 + 3 * 4 -> 14
ASTNode* optimize_constant_folding(ASTNode* node);

// Dead code elimination: remove unreachable code
// Example: if (true) { A } else { B } -> { A }
ASTNode* optimize_dead_code(ASTNode* node);

// Tail call optimization: convert tail-recursive calls to loops
// Example: factorial(n, acc) -> while loop
ASTNode* optimize_tail_calls(ASTNode* node);

// Apply all optimizations
ASTNode* optimize_ast(ASTNode* node);

// Optimization statistics
typedef struct {
    int constants_folded;
    int dead_code_removed;
    int tail_calls_detected;
} OptimizationStats;

extern OptimizationStats global_opt_stats;

void reset_optimization_stats();
void print_optimization_stats();

#endif // OPTIMIZER_H

