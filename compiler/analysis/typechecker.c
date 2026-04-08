#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "typechecker.h"
#include "type_inference.h"
#include "../parser/lexer.h"
#include "../parser/parser.h"
#include "../aether_error.h"
#include "../aether_module.h"

static int error_count = 0;
static int warning_count = 0;

// Get the last component of a module path for namespace
// "mypackage.utils" -> "utils"
static const char* get_namespace_from_path(const char* module_path) {
    const char* last_dot = strrchr(module_path, '.');
    if (last_dot) {
        return last_dot + 1;
    }
    return module_path;
}

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

// Track imported namespaces for qualified function calls
static char* imported_namespaces[64];
static int namespace_count = 0;

void register_namespace(const char* ns) {
    if (namespace_count < 64) {
        // Check if already registered
        for (int i = 0; i < namespace_count; i++) {
            if (strcmp(imported_namespaces[i], ns) == 0) return;
        }
        imported_namespaces[namespace_count++] = strdup(ns);
    }
}

// Check if a symbol is blocked by export visibility.
// Returns 1 if blocked (module has exports and symbol isn't one), 0 if allowed.
static int is_export_blocked(const char* namespace, const char* symbol) {
    if (!global_module_registry) return 0;
    AetherModule* mod = module_find(namespace);
    return (mod && mod->export_count > 0 && !module_is_exported(mod, symbol));
}

int is_imported_namespace(const char* name) {
    for (int i = 0; i < namespace_count; i++) {
        if (strcmp(imported_namespaces[i], name) == 0) return 1;
    }
    return 0;
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

        // Check if prefix is an imported namespace (e.g., "string" from import std.string)
        // Convert string.new -> string_new
        if (is_imported_namespace(prefix)) {
            // Enforce export visibility
            if (is_export_blocked(prefix, suffix)) {
                free(name_copy);
                return NULL;
            }
            char c_func_name[512];
            snprintf(c_func_name, sizeof(c_func_name), "%s_%s", prefix, suffix);
            Symbol* sym = lookup_symbol(table, c_func_name);
            free(name_copy);
            return sym;
        }
    }

    free(name_copy);
    return lookup_symbol(table, qualified_name);
}

void type_error(const char* message, int line, int column) {
    AetherErrorCode code = AETHER_ERR_TYPE_MISMATCH;
    if (strstr(message, "not exported")) code = AETHER_ERR_NOT_EXPORTED;
    else if (strstr(message, "Undefined variable")) code = AETHER_ERR_UNDEFINED_VAR;
    else if (strstr(message, "Undefined function") || strstr(message, "Unknown function"))
        code = AETHER_ERR_UNDEFINED_FUNC;
    else if (strstr(message, "Undefined type") || strstr(message, "Unknown type"))
        code = AETHER_ERR_UNDEFINED_TYPE;
    else if (strstr(message, "Redefinition") || strstr(message, "redefinition"))
        code = AETHER_ERR_REDEFINITION;
    aether_error_with_code(message, line, column, code);
    error_count++;
}

void type_warning(const char* message, int line, int column) {
    AetherError w = {
        .filename = NULL, .source_code = NULL,
        .line = line, .column = column,
        .message = message, .suggestion = NULL,
        .context = NULL, .code = AETHER_ERR_NONE
    };
    aether_warning_report(&w);
    warning_count++;
}

// Return a human-readable type name (static buffer — for error messages only)
static const char* type_name(Type* t) {
    if (!t) return "unknown";
    switch (t->kind) {
        case TYPE_INT:      return "int";
        case TYPE_INT64:    return "long";
        case TYPE_UINT64:   return "uint64";
        case TYPE_FLOAT:    return "float";
        case TYPE_BOOL:     return "bool";
        case TYPE_STRING:   return "string";
        case TYPE_VOID:     return "void";
        case TYPE_PTR:      return "ptr";
        case TYPE_ACTOR_REF: return "actor_ref";
        case TYPE_MESSAGE:  return "message";
        case TYPE_ARRAY:    return "array";
        case TYPE_STRUCT:   return t->struct_name ? t->struct_name : "struct";
        case TYPE_FUNCTION:  return "closure";
        case TYPE_TUPLE:    return "tuple";
        case TYPE_UNKNOWN:  return "unknown";
        default:            return "unknown";
    }
}

// Count the number of formal parameters of a function definition node
// Skips _ctx parameters (auto-injected by builder context)
static int count_function_params(ASTNode* func) {
    if (!func || func->child_count == 0) return 0;
    int count = 0;
    // Last child is the function body; everything before it may be params or a guard
    for (int i = 0; i < func->child_count - 1; i++) {
        ASTNode* child = func->children[i];
        if (child->type == AST_VARIABLE_DECLARATION ||
            child->type == AST_PATTERN_VARIABLE ||
            child->type == AST_PATTERN_LITERAL) {
            // Skip _ctx parameters — they're auto-injected by the builder system
            if (child->value && strcmp(child->value, "_ctx") == 0 &&
                child->node_type && child->node_type->kind == TYPE_PTR) {
                continue;
            }
            count++;
        }
        // AST_GUARD_CLAUSE is skipped (not a parameter)
    }
    return count;
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
    // int promotes to long without loss
    if (from->kind == TYPE_INT && to->kind == TYPE_INT64) return 1;
    if (from->kind == TYPE_INT64 && to->kind == TYPE_INT) return 1;
    // long <-> float compatibility
    if (from->kind == TYPE_INT64 && to->kind == TYPE_FLOAT) return 1;
    if (from->kind == TYPE_FLOAT && to->kind == TYPE_INT64) return 1;
    
    // Array compatibility
    if (from->kind == TYPE_ARRAY && to->kind == TYPE_ARRAY) {
        return is_type_compatible(from->element_type, to->element_type);
    }
    
    // Actor reference compatibility
    // Bare actor_ref (no type parameter) is compatible with any actor_ref
    if (from->kind == TYPE_ACTOR_REF && to->kind == TYPE_ACTOR_REF) {
        if (!from->element_type || !to->element_type) return 1;
        return is_type_compatible(from->element_type, to->element_type);
    }

    // Actor refs stored in int/ptr state fields (common wiring pattern: state ref = 0)
    if (from->kind == TYPE_ACTOR_REF &&
        (to->kind == TYPE_INT || to->kind == TYPE_INT64 || to->kind == TYPE_PTR)) return 1;
    if (to->kind == TYPE_ACTOR_REF &&
        (from->kind == TYPE_INT || from->kind == TYPE_INT64 || from->kind == TYPE_PTR)) return 1;

    // int ↔ ptr compatibility (e.g. x = 0 then x = ptr_func(), or passing 0 to ptr param)
    if (from->kind == TYPE_INT && to->kind == TYPE_PTR) return 1;
    if (from->kind == TYPE_PTR && to->kind == TYPE_INT) return 1;

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

        case AST_NULL_LITERAL:
            return create_type(TYPE_PTR);

        case AST_IF_EXPRESSION:
            // Type is the type of the then-branch expression
            if (expr->child_count >= 2) {
                return infer_type(expr->children[1], table);
            }
            return create_type(TYPE_UNKNOWN);

        case AST_STRING_INTERP:
            return create_type(TYPE_STRING);

        case AST_MATCH_STATEMENT:
            // Return type of first arm's result expression
            if (expr->node_type && expr->node_type->kind != TYPE_UNKNOWN) {
                return clone_type(expr->node_type);
            }
            if (expr->child_count >= 2) {
                ASTNode* first_arm = expr->children[1];
                if (first_arm && first_arm->child_count >= 2) {
                    return infer_type(first_arm->children[1], table);
                }
            }
            return create_type(TYPE_UNKNOWN);

        case AST_ARRAY_LITERAL:
            // Return the inferred array type
            return expr->node_type ? clone_type(expr->node_type) : create_type(TYPE_UNKNOWN);
            
        case AST_IDENTIFIER: {
            Symbol* symbol = lookup_symbol(table, expr->value);
            return (symbol && symbol->type) ? clone_type(symbol->type) : create_type(TYPE_UNKNOWN);
        }
        
        case AST_BINARY_EXPRESSION:
            return infer_binary_type(expr->children[0], expr->children[1], 
                                   get_token_type_from_string(expr->value));
            
        case AST_UNARY_EXPRESSION:
            return infer_unary_type(expr->children[0], 
                                  get_token_type_from_string(expr->value));
            
        case AST_FUNCTION_CALL: {
            Symbol* symbol = lookup_qualified_symbol(table, expr->value);
            if (symbol && symbol->is_function && symbol->type
                && symbol->type->kind != TYPE_VOID
                && symbol->type->kind != TYPE_UNKNOWN) {
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
            // Enforce export visibility before resolving
            if (expr->child_count > 0 && expr->children[0] &&
                expr->children[0]->type == AST_IDENTIFIER && expr->children[0]->value &&
                is_imported_namespace(expr->children[0]->value) && expr->value &&
                is_export_blocked(expr->children[0]->value, expr->value)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "'%s' is not exported from module '%s'",
                         expr->value, expr->children[0]->value);
                type_error(msg, expr->line, expr->column);
                return create_type(TYPE_UNKNOWN);
            }
            // If node_type already set, use it
            if (expr->node_type && expr->node_type->kind != TYPE_UNKNOWN)
                return clone_type(expr->node_type);
            // Namespace-qualified constant access: mymath.PI_APPROX -> mymath_PI_APPROX
            if (expr->child_count > 0 && expr->children[0] &&
                expr->children[0]->type == AST_IDENTIFIER && expr->children[0]->value &&
                is_imported_namespace(expr->children[0]->value) && expr->value) {
                char qualified[512];
                snprintf(qualified, sizeof(qualified), "%s_%s",
                         expr->children[0]->value, expr->value);
                Symbol* sym = lookup_symbol(table, qualified);
                if (sym && sym->type) {
                    // Rewrite node in-place for codegen
                    expr->type = AST_IDENTIFIER;
                    free(expr->value);
                    expr->value = strdup(qualified);
                    expr->node_type = clone_type(sym->type);
                    return clone_type(sym->type);
                }
            }
            // Look up the struct/actor type and find the field type
            if (expr->child_count > 0 && expr->children[0]) {
                Type* base_type = infer_type(expr->children[0], table);
                // Struct field lookup
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
                // Actor ref field lookup — look up state declarations in the actor definition
                if (base_type && base_type->kind == TYPE_ACTOR_REF && base_type->element_type &&
                    base_type->element_type->kind == TYPE_STRUCT && base_type->element_type->struct_name) {
                    Symbol* actor_sym = lookup_symbol(table, base_type->element_type->struct_name);
                    if (actor_sym && actor_sym->node) {
                        ASTNode* actor_def = actor_sym->node;
                        for (int fi = 0; fi < actor_def->child_count; fi++) {
                            ASTNode* field = actor_def->children[fi];
                            if (field && field->type == AST_STATE_DECLARATION &&
                                field->value && strcmp(field->value, expr->value) == 0) {
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
            // ptr arithmetic: ptr +-*/ int → int (Aether C interop: list.get returns ptr holding int)
            if ((left_type->kind == TYPE_PTR && right_type->kind == TYPE_INT) ||
                (left_type->kind == TYPE_INT && right_type->kind == TYPE_PTR) ||
                (left_type->kind == TYPE_PTR && right_type->kind == TYPE_PTR)) {
                return create_type(TYPE_INT);
            }
            // Promote to int64 if either operand is long/int64
            if ((left_type->kind == TYPE_INT64 || left_type->kind == TYPE_INT) &&
                (right_type->kind == TYPE_INT64 || right_type->kind == TYPE_INT)) {
                return create_type(TYPE_INT64);
            }
            if (left_type->kind == TYPE_STRING && right_type->kind == TYPE_STRING) {
                return create_type(TYPE_STRING);
            }
            break;
            
        case TOKEN_AMPERSAND:
        case TOKEN_PIPE:
        case TOKEN_CARET:
        case TOKEN_LSHIFT:
        case TOKEN_RSHIFT:
            // Bitwise operations: integer operands, result matches wider type
            if (left_type->kind == TYPE_UNKNOWN || right_type->kind == TYPE_UNKNOWN) {
                return create_type(TYPE_UNKNOWN);
            }
            if (left_type->kind == TYPE_INT && right_type->kind == TYPE_INT) {
                return create_type(TYPE_INT);
            }
            if ((left_type->kind == TYPE_INT64 || left_type->kind == TYPE_INT) &&
                (right_type->kind == TYPE_INT64 || right_type->kind == TYPE_INT)) {
                return create_type(TYPE_INT64);
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
            
        case TOKEN_TILDE:
            return clone_type(operand_type); // Bitwise NOT: same integer type

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
    if (strcmp(str, "&") == 0) return TOKEN_AMPERSAND;
    if (strcmp(str, "|") == 0) return TOKEN_PIPE;
    if (strcmp(str, "^") == 0) return TOKEN_CARET;
    if (strcmp(str, "~") == 0) return TOKEN_TILDE;
    if (strcmp(str, "<<") == 0) return TOKEN_LSHIFT;
    if (strcmp(str, ">>") == 0) return TOKEN_RSHIFT;

    return TOKEN_ERROR;
}

// --- Unused variable analysis ---

#define MAX_TRACKED_VARS 256

typedef struct {
    const char* name;
    int line;
    int col;
    int used;
} TrackedVar;

// Collect all AST_IDENTIFIER references in a subtree (excluding declarations)
static void collect_references(ASTNode* node, TrackedVar* vars, int var_count) {
    if (!node) return;

    // An identifier in expression position is a reference
    if (node->type == AST_IDENTIFIER && node->value) {
        for (int i = 0; i < var_count; i++) {
            if (strcmp(vars[i].name, node->value) == 0) {
                vars[i].used = 1;
            }
        }
    }

    // Match statements with list patterns implicitly reference <expr>_len variables
    // (the codegen generates: int _match_len = <expr>_len;)
    if (node->type == AST_MATCH_STATEMENT && node->child_count > 0) {
        ASTNode* match_expr = node->children[0];
        if (match_expr && match_expr->type == AST_IDENTIFIER && match_expr->value) {
            char len_name[256];
            snprintf(len_name, sizeof(len_name), "%s_len", match_expr->value);
            for (int i = 0; i < var_count; i++) {
                if (strcmp(vars[i].name, len_name) == 0) {
                    vars[i].used = 1;
                }
            }
        }
    }

    // For variable declarations, the RHS is a reference but the name itself is not
    if (node->type == AST_VARIABLE_DECLARATION) {
        // Only walk children (RHS expression), not the declaration name
        for (int i = 0; i < node->child_count; i++) {
            collect_references(node->children[i], vars, var_count);
        }
        return;
    }

    for (int i = 0; i < node->child_count; i++) {
        collect_references(node->children[i], vars, var_count);
    }
}

// Collect variable declarations from a block (non-recursive into nested functions)
static int collect_declarations(ASTNode* node, TrackedVar* vars, int var_count) {
    if (!node || var_count >= MAX_TRACKED_VARS) return var_count;

    if (node->type == AST_VARIABLE_DECLARATION && node->value) {
        // Skip _ prefixed names (intentional discard)
        if (node->value[0] != '_') {
            vars[var_count].name = node->value;
            vars[var_count].line = node->line;
            vars[var_count].col = node->column;
            vars[var_count].used = 0;
            var_count++;
        }
    }

    // Don't recurse into nested function definitions or actor definitions
    if (node->type == AST_FUNCTION_DEFINITION || node->type == AST_ACTOR_DEFINITION) {
        return var_count;
    }

    for (int i = 0; i < node->child_count; i++) {
        var_count = collect_declarations(node->children[i], vars, var_count);
    }
    return var_count;
}

static void check_unused_variables(ASTNode* body) {
    if (!body) return;

    TrackedVar vars[MAX_TRACKED_VARS];
    int var_count = 0;

    // Collect declarations
    for (int i = 0; i < body->child_count; i++) {
        var_count = collect_declarations(body->children[i], vars, var_count);
    }

    if (var_count == 0) return;

    // Collect references
    for (int i = 0; i < body->child_count; i++) {
        collect_references(body->children[i], vars, var_count);
    }

    // Warn about unused
    for (int i = 0; i < var_count; i++) {
        if (!vars[i].used) {
            char msg[256];
            snprintf(msg, sizeof(msg), "unused variable '%s'", vars[i].name);
            AetherError warn = {
                .filename = NULL,
                .source_code = NULL,
                .line = vars[i].line,
                .column = vars[i].col,
                .message = msg,
                .suggestion = "prefix with '_' to suppress this warning",
                .context = NULL,
                .code = AETHER_WARN_UNUSED_VAR
            };
            aether_warning_report(&warn);
            warning_count++;
        }
    }
}

// --- Unreachable code analysis ---

// Check if a statement is a return or exit() call
static int is_terminating(ASTNode* node) {
    if (!node) return 0;
    if (node->type == AST_RETURN_STATEMENT) return 1;
    // Unwrap expression statement to check inner call
    if (node->type == AST_EXPRESSION_STATEMENT && node->child_count > 0) {
        return is_terminating(node->children[0]);
    }
    if (node->type == AST_FUNCTION_CALL && node->value &&
        strcmp(node->value, "exit") == 0) return 1;
    // if/else where BOTH branches terminate
    if (node->type == AST_IF_STATEMENT && node->child_count >= 3) {
        ASTNode* then_branch = node->children[1];
        ASTNode* else_branch = node->children[2];
        // Check last statement of each branch
        if (then_branch && else_branch) {
            int then_terminates = 0;
            int else_terminates = 0;
            if (then_branch->child_count > 0)
                then_terminates = is_terminating(then_branch->children[then_branch->child_count - 1]);
            else
                then_terminates = is_terminating(then_branch);
            if (else_branch->child_count > 0)
                else_terminates = is_terminating(else_branch->children[else_branch->child_count - 1]);
            else
                else_terminates = is_terminating(else_branch);
            return then_terminates && else_terminates;
        }
    }
    return 0;
}

static void check_unreachable_code(ASTNode* body) {
    if (!body) return;

    for (int i = 0; i < body->child_count; i++) {
        ASTNode* stmt = body->children[i];
        if (is_terminating(stmt) && i + 1 < body->child_count) {
            // Next statement is unreachable
            ASTNode* unreachable = body->children[i + 1];
            if (unreachable) {
                char msg[256];
                snprintf(msg, sizeof(msg), "unreachable code after %s",
                         stmt->type == AST_RETURN_STATEMENT ? "return" :
                         (stmt->type == AST_FUNCTION_CALL ? "exit()" : "terminating block"));
                AetherError warn = {
                    .filename = NULL,
                    .source_code = NULL,
                    .line = unreachable->line,
                    .column = unreachable->column,
                    .message = msg,
                    .suggestion = "remove unreachable code or restructure control flow",
                    .context = NULL,
                    .code = AETHER_WARN_UNREACHABLE
                };
                aether_warning_report(&warn);
                warning_count++;
            }
            break;  // Only warn once per block
        }

        // Recurse into blocks (if/else bodies, while bodies, etc.)
        if (stmt->type == AST_IF_STATEMENT) {
            for (int j = 1; j < stmt->child_count; j++) {
                check_unreachable_code(stmt->children[j]);
            }
        } else if (stmt->type == AST_WHILE_LOOP && stmt->child_count > 1) {
            check_unreachable_code(stmt->children[1]);
        }
    }
}

// Type checking functions
int typecheck_program(ASTNode* program) {
    if (!program || program->type != AST_PROGRAM) return 0;

    error_count = 0;
    warning_count = 0;
    namespace_count = 0;  // Reset imported namespaces

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

    // Timing builtin — returns nanoseconds as int64 (int32 overflows after ~2.1 seconds)
    Type* clock_ns_type = create_type(TYPE_INT64);
    add_symbol(global_table, "clock_ns", clock_ns_type, 0, 1, 0);

    // Output builtins
    Type* println_type = create_type(TYPE_VOID);
    add_symbol(global_table, "println", println_type, 0, 1, 0);
    Type* print_char_type = create_type(TYPE_VOID);
    add_symbol(global_table, "print_char", print_char_type, 0, 1, 0);

    // Platform selection builtin
    Type* select_type = create_type(TYPE_UNKNOWN);
    add_symbol(global_table, "select", select_type, 0, 1, 0);

    // Process control builtins
    Type* exit_type = create_type(TYPE_VOID);
    add_symbol(global_table, "exit", exit_type, 0, 1, 0);

    // Memory builtins
    Type* free_builtin_type = create_type(TYPE_VOID);
    add_symbol(global_table, "free", free_builtin_type, 0, 1, 0);

    // Array/collection builtins
    Type* make_type = create_type(TYPE_PTR);  // returns allocated memory
    add_symbol(global_table, "make", make_type, 0, 1, 0);

    // Closure/iteration builtins
    Type* each_type = create_type(TYPE_VOID);
    add_symbol(global_table, "each", each_type, 0, 1, 0);
    Type* call_type = create_type(TYPE_INT);  // return type depends on closure
    add_symbol(global_table, "call", call_type, 0, 1, 0);
    Type* read_char_type = create_type(TYPE_INT);
    add_symbol(global_table, "read_char", read_char_type, 0, 1, 0);
    Type* char_at_type = create_type(TYPE_INT);
    add_symbol(global_table, "char_at", char_at_type, 0, 1, 0);
    Type* box_closure_type = create_type(TYPE_PTR);
    add_symbol(global_table, "box_closure", box_closure_type, 0, 1, 0);
    Type* unbox_closure_type = create_type(TYPE_FUNCTION);
    add_symbol(global_table, "unbox_closure", unbox_closure_type, 0, 1, 0);
    Type* ref_type = create_type(TYPE_PTR);
    add_symbol(global_table, "ref", ref_type, 0, 1, 0);
    Type* ref_get_type = create_type(TYPE_INT);
    add_symbol(global_table, "ref_get", ref_get_type, 0, 1, 0);
    Type* ref_set_type = create_type(TYPE_VOID);
    add_symbol(global_table, "ref_set", ref_set_type, 0, 1, 0);
    Type* ref_free_type = create_type(TYPE_VOID);
    add_symbol(global_table, "ref_free", ref_free_type, 0, 1, 0);
    // Lazy evaluation builtins
    Type* lazy_type = create_type(TYPE_PTR);
    add_symbol(global_table, "lazy", lazy_type, 0, 1, 0);
    Type* force_type = create_type(TYPE_INT);  // default int; C returns intptr_t, implicit conversion
    add_symbol(global_table, "force", force_type, 0, 1, 0);
    Type* thunk_free_type = create_type(TYPE_VOID);
    add_symbol(global_table, "thunk_free", thunk_free_type, 0, 1, 0);
    Type* str_eq_type = create_type(TYPE_INT);
    add_symbol(global_table, "str_eq", str_eq_type, 0, 1, 0);
    Type* raw_mode_type = create_type(TYPE_VOID);
    add_symbol(global_table, "raw_mode", raw_mode_type, 0, 1, 0);
    Type* cooked_mode_type = create_type(TYPE_VOID);
    add_symbol(global_table, "cooked_mode", cooked_mode_type, 0, 1, 0);
    Type* spawn_sandboxed_type = create_type(TYPE_INT);
    add_symbol(global_table, "spawn_sandboxed", spawn_sandboxed_type, 0, 1, 0);
    // num_cores: runtime global (extern int num_cores in multicore_scheduler.h)
    Type* num_cores_type = create_type(TYPE_INT);
    add_symbol(global_table, "num_cores", num_cores_type, 0, 0, 0);
    Type* aether_push_type = create_type(TYPE_VOID);
    add_symbol(global_table, "sandbox_push", aether_push_type, 0, 1, 0);
    Type* aether_pop_type = create_type(TYPE_VOID);
    add_symbol(global_table, "sandbox_pop", aether_pop_type, 0, 1, 0);
    Type* sandbox_install_type = create_type(TYPE_VOID);
    add_symbol(global_table, "sandbox_install", sandbox_install_type, 0, 1, 0);
    Type* sandbox_uninstall_type = create_type(TYPE_VOID);
    add_symbol(global_table, "sandbox_uninstall", sandbox_uninstall_type, 0, 1, 0);
    Type* builder_ctx_type = create_type(TYPE_PTR);
    add_symbol(global_table, "builder_context", builder_ctx_type, 0, 1, 0);
    Type* builder_depth_type = create_type(TYPE_INT);
    add_symbol(global_table, "builder_depth", builder_depth_type, 0, 1, 0);

    // First pass: collect all declarations
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        
        switch (child->type) {
            case AST_ACTOR_DEFINITION: {
                // Create actor struct type
                Type* actor_type = create_type(TYPE_STRUCT);
                actor_type->struct_name = strdup(child->value);
                add_symbol(global_table, child->value, actor_type, 1, 0, 0);
                // Store AST node so state field types can be looked up
                Symbol* actor_sym_node = lookup_symbol(global_table, child->value);
                if (actor_sym_node) actor_sym_node->node = child;
                
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
                // Store AST node so arity can be verified at call sites
                Symbol* func_sym = lookup_symbol(global_table, child->value);
                if (func_sym) func_sym->node = child;
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
            case AST_CONST_DECLARATION: {
                // Register constant in symbol table
                Type* ctype = child->node_type ? clone_type(child->node_type) : create_type(TYPE_UNKNOWN);
                // Infer type from the value expression if unknown
                if (ctype->kind == TYPE_UNKNOWN && child->child_count > 0 && child->children[0]->node_type) {
                    free_type(ctype);
                    ctype = clone_type(child->children[0]->node_type);
                }
                add_symbol(global_table, child->value, ctype, 0, 0, 0);
                break;
            }
            case AST_MAIN_FUNCTION:
                // Main function doesn't need to be in symbol table
                break;
            case AST_IMPORT_STATEMENT: {
                // Process import and register alias if present
                const char* module_path = child->value;

                // Check if this import has an alias (last child is identifier)
                if (child->child_count > 0) {
                    ASTNode* last_child = child->children[child->child_count - 1];
                    // Check if last child is the alias (an identifier node)
                    if (last_child && last_child->type == AST_IDENTIFIER) {
                        const char* alias = last_child->value;
                        // Register the alias in symbol table
                        add_module_alias(global_table, alias, module_path);
                    }
                }

                // Handle stdlib imports: import std.X
                if (strncmp(module_path, "std.", 4) == 0) {
                    const char* module_name = module_path + 4;  // "fs", "string", etc.

                    // Register namespace for qualified calls (e.g., string.new)
                    register_namespace(module_name);

                    // Look up cached module from orchestrator
                    AetherModule* mod = module_find(module_path);
                    ASTNode* mod_ast = mod ? mod->ast : NULL;
                    if (mod_ast) {
                        // Build prefix for stripping: "math_" from module "math"
                        char prefix[128];
                        snprintf(prefix, sizeof(prefix), "%s_", module_name);
                        int prefix_len = (int)strlen(prefix);

                        // Extract extern declarations from the module
                        for (int j = 0; j < mod_ast->child_count; j++) {
                            ASTNode* decl = mod_ast->children[j];
                            if (decl->type == AST_EXTERN_FUNCTION && decl->value) {
                                // Check if selective import - only import specified functions
                                int should_import = 1;
                                if (child->child_count > 0) {
                                    ASTNode* first = child->children[0];
                                    if (first && first->type == AST_IDENTIFIER) {
                                        should_import = 0;
                                        // Strip module prefix for comparison:
                                        // decl->value is "math_sqrt", sel->value is "sqrt"
                                        const char* short_name = decl->value;
                                        if (strncmp(decl->value, prefix, prefix_len) == 0) {
                                            short_name = decl->value + prefix_len;
                                        }
                                        for (int k = 0; k < child->child_count; k++) {
                                            ASTNode* sel = child->children[k];
                                            if (sel && sel->type == AST_IDENTIFIER &&
                                                strcmp(sel->value, short_name) == 0) {
                                                should_import = 1;
                                                break;
                                            }
                                        }
                                    }
                                }

                                if (should_import) {
                                    if (!lookup_symbol_local(global_table, decl->value)) {
                                        add_symbol(global_table, decl->value,
                                                   clone_type(decl->node_type), 0, 1, 0);
                                    }
                                }
                            }
                        }
                        // NOTE: do NOT free mod_ast — registry owns it
                    }
                } else {
                    // Handle local package imports: import mypackage.utils
                    const char* namespace = get_namespace_from_path(module_path);
                    register_namespace(namespace);

                    // Look up cached module from orchestrator
                    AetherModule* mod = module_find(module_path);
                    ASTNode* mod_ast = mod ? mod->ast : NULL;
                    if (mod_ast) {
                        for (int j = 0; j < mod_ast->child_count; j++) {
                            ASTNode* decl = mod_ast->children[j];
                            if (decl->type == AST_EXTERN_FUNCTION && decl->value) {
                                // Check if selective import - only import specified functions
                                int should_import = 1;
                                if (child->child_count > 0) {
                                    ASTNode* first = child->children[0];
                                    if (first && first->type == AST_IDENTIFIER) {
                                        should_import = 0;
                                        for (int k = 0; k < child->child_count; k++) {
                                            ASTNode* sel = child->children[k];
                                            if (sel && sel->type == AST_IDENTIFIER &&
                                                strcmp(sel->value, decl->value) == 0) {
                                                should_import = 1;
                                                break;
                                            }
                                        }
                                    }
                                }

                                if (should_import) {
                                    if (!lookup_symbol_local(global_table, decl->value)) {
                                        add_symbol(global_table, decl->value,
                                                   clone_type(decl->node_type), 0, 1, 0);
                                    }
                                }
                            }
                            // AST_FUNCTION_DEFINITION handled by module_merge_into_program()
                        }
                        // NOTE: do NOT free mod_ast — registry owns it
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
        free_symbol_table(global_table);
        // Clean up namespace strings to avoid leaks on re-runs
        for (int ns = 0; ns < namespace_count; ns++) free(imported_namespaces[ns]);
        namespace_count = 0;
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

    // Third pass: unused variable + unreachable code analysis
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_FUNCTION_DEFINITION && child->child_count > 0) {
            ASTNode* body = child->children[child->child_count - 1];
            check_unused_variables(body);
            check_unreachable_code(body);
        } else if (child->type == AST_MAIN_FUNCTION && child->child_count > 0) {
            // main() has a BLOCK child containing the actual statements
            ASTNode* main_body = child->children[0];
            check_unused_variables(main_body);
            check_unreachable_code(main_body);
        }
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
    
    // Clean up namespace strings
    for (int ns = 0; ns < namespace_count; ns++) free(imported_namespaces[ns]);
    namespace_count = 0;

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

// Validate that message constructor field values match declared field types
static void typecheck_message_constructor(ASTNode* constructor, SymbolTable* table) {
    if (!constructor || constructor->type != AST_MESSAGE_CONSTRUCTOR || !constructor->value) return;
    const char* msg_name = constructor->value;
    Symbol* msg_sym = lookup_symbol(table, msg_name);
    if (!msg_sym || !msg_sym->node || msg_sym->node->type != AST_MESSAGE_DEFINITION) return;

    for (int i = 0; i < constructor->child_count; i++) {
        ASTNode* field_init = constructor->children[i];
        if (!field_init || field_init->type != AST_FIELD_INIT || !field_init->value) continue;
        if (field_init->child_count == 0) continue;

        Type* declared = lookup_message_field_type(table, msg_name, field_init->value);
        if (!declared) continue;

        ASTNode* value_expr = field_init->children[0];
        typecheck_expression(value_expr, table);
        Type* actual = infer_type(value_expr, table);

        if (actual && actual->kind != TYPE_UNKNOWN &&
            declared->kind != TYPE_UNKNOWN &&
            !is_type_compatible(actual, declared)) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Type mismatch in field '%s' of message '%s': expected %s, got %s",
                     field_init->value, msg_name, type_name(declared), type_name(actual));
            type_error(buf, field_init->line, field_init->column);
        }
        free_type(actual);
        free_type(declared);
    }
}

int typecheck_actor_definition(ASTNode* actor, SymbolTable* table) {
    if (!actor || actor->type != AST_ACTOR_DEFINITION) return 0;
    
    SymbolTable* actor_table = create_symbol_table(table);
    
    // Type check actor body
    for (int i = 0; i < actor->child_count; i++) {
        ASTNode* child = actor->children[i];
        
        if (child->type == AST_STATE_DECLARATION) {
            if ((!child->node_type || child->node_type->kind == TYPE_UNKNOWN)
                && child->child_count > 0 && child->children[0]) {
                ASTNode* init = child->children[0];
                if (init->type == AST_FUNCTION_CALL && init->value) {
                    Symbol* fn = lookup_qualified_symbol(actor_table, init->value);
                    if (fn && fn->type) {
                        child->node_type = clone_type(fn->type);
                    }
                }
            }
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
                } else if (arm->type == AST_TIMEOUT_ARM && arm->child_count >= 2) {
                    // Timeout arm: after N -> { body }
                    typecheck_expression(arm->children[0], receive_table);  // timeout expr
                    typecheck_statement(arm->children[1], receive_table);   // body
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
        case AST_TUPLE_DESTRUCTURE: {
            // a, b = func() — last child is RHS, others are variable declarations
            if (stmt->child_count < 2) {
                type_error("Invalid tuple destructuring", stmt->line, stmt->column);
                return 0;
            }
            int var_count = stmt->child_count - 1;
            ASTNode* rhs = stmt->children[var_count];  // Last child is RHS

            // Typecheck the RHS
            typecheck_expression(rhs, table);
            Type* rhs_type = infer_type(rhs, table);

            // Verify RHS is a tuple with matching element count
            if (!rhs_type || rhs_type->kind != TYPE_TUPLE) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "cannot destructure — '%s' returns '%s', not a tuple",
                    rhs->value ? rhs->value : "expression",
                    type_to_string(rhs_type));
                aether_error_with_suggestion(msg, stmt->line, stmt->column,
                    "use single assignment instead, or ensure the function returns multiple values");
                error_count++;
                if (rhs_type) free_type(rhs_type);
                return 0;
            }

            if (rhs_type->tuple_count != var_count) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "tuple destructuring count mismatch — %d variables, but expression returns %d values",
                    var_count, rhs_type->tuple_count);
                aether_error_with_suggestion(msg, stmt->line, stmt->column,
                    "match the number of variables to the number of returned values");
                error_count++;
                free_type(rhs_type);
                return 0;
            }

            // Assign types to each variable and add to symbol table
            for (int j = 0; j < var_count; j++) {
                ASTNode* var = stmt->children[j];
                if (var->node_type) free_type(var->node_type);
                var->node_type = clone_type(rhs_type->tuple_types[j]);
                // Don't register _ (discard) in symbol table
                if (var->value && strcmp(var->value, "_") != 0) {
                    add_symbol(table, var->value, clone_type(var->node_type), 0, 0, 0);
                }
            }

            free_type(rhs_type);
            return 1;
        }

        case AST_CONST_DECLARATION:
        case AST_VARIABLE_DECLARATION: {
            if (stmt->child_count > 0) {
                // Has initializer
                ASTNode* init = stmt->children[0];
                // Match-as-expression: typecheck as statement, then use its type
                if (init->type == AST_MATCH_STATEMENT) {
                    typecheck_statement(init, table);
                } else {
                    typecheck_expression(init, table);
                }
                Type* init_type = infer_type(init, table);

                // If variable has no explicit type (TYPE_UNKNOWN), use initializer's type
                if (!stmt->node_type || stmt->node_type->kind == TYPE_UNKNOWN) {
                    if (stmt->node_type) free_type(stmt->node_type);
                    stmt->node_type = clone_type(init_type);
                } else if (!is_assignable(init_type, stmt->node_type)) {
                    // Has explicit type but initializer doesn't match
                    free_type(init_type);
                    type_error("Type mismatch in variable initialization", stmt->line, stmt->column);
                    return 0;
                }
                free_type(init_type);
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
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg), "Undefined variable '%s'", left->value ? left->value : "?");
                    type_error(error_msg, left->line, left->column);
                    return 0;
                }

                Type* right_type = infer_type(right, table);
                if (!is_assignable(right_type, symbol->type)) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Type mismatch in assignment to '%s': expected %s, got %s",
                             left->value ? left->value : "?",
                             type_name(symbol->type), type_name(right_type));
                    free_type(right_type);
                    type_error(error_msg, stmt->line, stmt->column);
                    return 0;
                }
                free_type(right_type);
            }
            return 1;
        }

        case AST_COMPOUND_ASSIGNMENT: {
            // node->value = variable name, children[0] = operator, children[1] = RHS
            if (stmt->child_count >= 2) {
                Symbol* symbol = lookup_symbol(table, stmt->value);
                if (!symbol) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg), "Undefined variable '%s'", stmt->value ? stmt->value : "?");
                    type_error(error_msg, stmt->line, stmt->column);
                    return 0;
                }
                ASTNode* rhs = stmt->children[1];
                typecheck_expression(rhs, table);
                { Type* _t = infer_type(rhs, table); free_type(_t); }
                if (stmt->node_type && stmt->node_type->kind == TYPE_UNKNOWN && symbol->type) {
                    free_type(stmt->node_type);
                    stmt->node_type = clone_type(symbol->type);
                }
            }
            return 1;
        }

        case AST_IF_STATEMENT: {
            if (stmt->child_count >= 1) {
                ASTNode* condition = stmt->children[0];
                typecheck_expression(condition, table);
                Type* cond_type = infer_type(condition, table);

                if (cond_type && cond_type->kind != TYPE_BOOL) {
                    free_type(cond_type);
                    type_error("If condition must be boolean", condition->line, condition->column);
                    return 0;
                }
                free_type(cond_type);
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
                    free_type(cond_type);
                    type_error("For loop condition must be boolean", stmt->line, stmt->column);
                    free_symbol_table(loop_table);
                    return 0;
                }
                free_type(cond_type);
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
                    free_type(cond_type);
                    type_error("Loop condition must be boolean", condition->line, condition->column);
                    return 0;
                }
                free_type(cond_type);
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

        case AST_FUNCTION_CALL:
            // Function call used as a statement (e.g. println(...), user_fn(...))
            return typecheck_function_call(stmt, table);

        case AST_PRINT_STATEMENT: {
            for (int i = 0; i < stmt->child_count; i++) {
                typecheck_expression(stmt->children[i], table);
            }
            if (stmt->child_count >= 2 &&
                stmt->children[0]->type == AST_LITERAL &&
                stmt->children[0]->node_type &&
                stmt->children[0]->node_type->kind == TYPE_STRING &&
                stmt->children[0]->value) {
                const char* fmt = stmt->children[0]->value;
                int arg_idx = 1;
                for (int fi = 0; fmt[fi]; fi++) {
                    if (fmt[fi] != '%' || !fmt[fi + 1]) continue;
                    fi++;
                    while (fmt[fi] == '-' || fmt[fi] == '+' || fmt[fi] == ' ' ||
                           fmt[fi] == '#' || fmt[fi] == '0') fi++;
                    while (fmt[fi] >= '0' && fmt[fi] <= '9') fi++;
                    if (fmt[fi] == '.') { fi++; while (fmt[fi] >= '0' && fmt[fi] <= '9') fi++; }
                    if (fmt[fi] == '%') continue;
                    if (arg_idx >= stmt->child_count) break;
                    Type* atype = infer_type(stmt->children[arg_idx], table);
                    TypeKind ak = atype ? atype->kind : TYPE_UNKNOWN;
                    char spec = fmt[fi];
                    int mismatch = 0;
                    if ((spec == 's') && ak != TYPE_STRING && ak != TYPE_PTR) mismatch = 1;
                    if ((spec == 'd' || spec == 'i') && ak != TYPE_INT && ak != TYPE_INT64 && ak != TYPE_BOOL) mismatch = 1;
                    if ((spec == 'f' || spec == 'g' || spec == 'e') && ak != TYPE_FLOAT) mismatch = 1;
                    if (mismatch) {
                        char wbuf[256];
                        snprintf(wbuf, sizeof(wbuf),
                            "Format specifier '%%%c' does not match argument type '%s' (auto-corrected)",
                            spec, type_name(atype));
                        type_warning(wbuf, stmt->children[arg_idx]->line, stmt->children[arg_idx]->column);
                    }
                    if (atype) free_type(atype);
                    arg_idx++;
                }
            }
            return 1;
        }
        
        case AST_SEND_STATEMENT: {
            if (stmt->child_count >= 2) {
                ASTNode* actor_ref = stmt->children[0];
                ASTNode* message = stmt->children[1];

                Type* actor_type = infer_type(actor_ref, table);
                if (actor_type && actor_type->kind != TYPE_ACTOR_REF) {
                    free_type(actor_type);
                    type_error("First argument to send must be an actor reference", actor_ref->line, actor_ref->column);
                    return 0;
                }
                free_type(actor_type);

                typecheck_expression(message, table);
            }
            return 1;
        }

        case AST_SEND_FIRE_FORGET: {
            // actor ! MessageType { fields... }
            if (stmt->child_count >= 2) {
                ASTNode* actor_ref = stmt->children[0];
                ASTNode* message = stmt->children[1];

                // Validate actor reference type
                typecheck_expression(actor_ref, table);
                Type* actor_type = infer_type(actor_ref, table);
                if (actor_type && actor_type->kind != TYPE_ACTOR_REF && actor_type->kind != TYPE_UNKNOWN) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Cannot send to '%s': expected an actor reference",
                             actor_ref->value ? actor_ref->value : "expression");
                    free_type(actor_type);
                    type_error(error_msg, actor_ref->line, actor_ref->column);
                    return 0;
                }
                free_type(actor_type);

                // Validate that the message type is a registered message definition
                if (message->type == AST_MESSAGE_CONSTRUCTOR && message->value) {
                    Symbol* msg_sym = lookup_symbol(table, message->value);
                    if (!msg_sym || !msg_sym->type || msg_sym->type->kind != TYPE_MESSAGE) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Undefined message type '%s'", message->value);
                        type_error(error_msg, message->line, message->column);
                        return 0;
                    }
                }

                // Validate field value types match declared field types
                typecheck_message_constructor(message, table);
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

                // Propagate arm result type to the match node (for match-as-expression)
                if (!stmt->node_type || stmt->node_type->kind == TYPE_UNKNOWN) {
                    if (body->node_type && body->node_type->kind != TYPE_UNKNOWN) {
                        stmt->node_type = clone_type(body->node_type);
                    }
                }

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
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "Undefined variable '%s'", expr->value ? expr->value : "?");
                type_error(error_msg, expr->line, expr->column);
                return 0;
            }
            expr->node_type = symbol->type ? clone_type(symbol->type) : create_type(TYPE_UNKNOWN);
            return 1;
        }

        case AST_LITERAL:
            // Literals are already typed
            return 1;

        case AST_IF_EXPRESSION:
            // Typecheck all children (condition, then-expr, else-expr)
            for (int i = 0; i < expr->child_count; i++) {
                typecheck_expression(expr->children[i], table);
            }
            if (expr->child_count >= 2) {
                Type* then_type = infer_type(expr->children[1], table);
                if (!expr->node_type || expr->node_type->kind == TYPE_UNKNOWN) {
                    if (expr->node_type) free_type(expr->node_type);
                    expr->node_type = clone_type(then_type);
                }
                free_type(then_type);
            }
            return 1;

        case AST_NULL_LITERAL:
            // null is always TYPE_PTR
            if (!expr->node_type) expr->node_type = create_type(TYPE_PTR);
            return 1;

        case AST_ARRAY_LITERAL:
            // Type check all array elements
            for (int i = 0; i < expr->child_count; i++) {
                typecheck_expression(expr->children[i], table);
            }
            return 1;

        case AST_NAMED_ARG:
            // Named argument: type check the value expression
            if (expr->child_count > 0) {
                typecheck_expression(expr->children[0], table);
                expr->node_type = expr->children[0]->node_type
                    ? clone_type(expr->children[0]->node_type)
                    : create_type(TYPE_UNKNOWN);
            }
            return 1;

        case AST_STRING_INTERP:
            // Type check all sub-expressions inside the interpolation
            for (int i = 0; i < expr->child_count; i++) {
                typecheck_expression(expr->children[i], table);
            }
            expr->node_type = create_type(TYPE_STRING);
            return 1;

        case AST_CLOSURE: {
            // Create a child scope for the closure's parameters
            SymbolTable* closure_scope = create_symbol_table(table);

            // Register closure parameters in the child scope
            for (int i = 0; i < expr->child_count; i++) {
                ASTNode* child = expr->children[i];
                if (child && child->type == AST_CLOSURE_PARAM && child->value) {
                    Type* ptype = child->node_type ? clone_type(child->node_type)
                                                   : create_type(TYPE_INT); // default to int
                    add_symbol(closure_scope, child->value, ptype, 0, 0, 0);
                }
            }

            // Type check the body block in the closure scope
            // Also register variable declarations so later statements can see them
            for (int i = 0; i < expr->child_count; i++) {
                ASTNode* child = expr->children[i];
                if (child && child->type == AST_BLOCK) {
                    for (int j = 0; j < child->child_count; j++) {
                        ASTNode* stmt = child->children[j];
                        typecheck_expression(stmt, closure_scope);
                        // Register variable declarations in the closure scope
                        if (stmt && stmt->type == AST_VARIABLE_DECLARATION && stmt->value) {
                            Type* vtype = stmt->node_type ? clone_type(stmt->node_type)
                                                          : create_type(TYPE_INT);
                            // Try to infer from initializer
                            if (vtype->kind == TYPE_UNKNOWN && stmt->child_count > 0 &&
                                stmt->children[0] && stmt->children[0]->node_type) {
                                free_type(vtype);
                                vtype = clone_type(stmt->children[0]->node_type);
                            }
                            add_symbol(closure_scope, stmt->value, vtype, 0, 0, 0);
                        }
                    }
                }
            }

            free_symbol_table(closure_scope);

            if (!expr->node_type) {
                expr->node_type = create_type(TYPE_FUNCTION);
            }
            return 1;
        }

        case AST_ARRAY_ACCESS:
            // Type check array access — validate index is integer
            if (expr->child_count >= 2) {
                typecheck_expression(expr->children[0], table);
                typecheck_expression(expr->children[1], table);
                Type* idx_type = infer_type(expr->children[1], table);
                if (idx_type && idx_type->kind != TYPE_INT && idx_type->kind != TYPE_INT64
                    && idx_type->kind != TYPE_UNKNOWN) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Array index must be an integer, got %s",
                             type_name(idx_type));
                    type_error(error_msg, expr->line, expr->column);
                }
                if (idx_type) free_type(idx_type);
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
            // Namespace-qualified constant access: mymath.PI_APPROX -> mymath_PI_APPROX
            // Rewrite AST to AST_IDENTIFIER so codegen emits the C variable name directly
            if (expr->child_count > 0 && expr->children[0] &&
                expr->children[0]->type == AST_IDENTIFIER && expr->children[0]->value &&
                is_imported_namespace(expr->children[0]->value) && expr->value) {
                // Enforce export visibility for constants
                if (is_export_blocked(expr->children[0]->value, expr->value)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "'%s' is not exported from module '%s'",
                             expr->value, expr->children[0]->value);
                    type_error(msg, expr->line, expr->column);
                    return 0;
                }
                char qualified[512];
                snprintf(qualified, sizeof(qualified), "%s_%s",
                         expr->children[0]->value, expr->value);
                Symbol* sym = lookup_symbol(table, qualified);
                if (sym && sym->type) {
                    // Rewrite node in-place
                    expr->type = AST_IDENTIFIER;
                    free(expr->value);
                    expr->value = strdup(qualified);
                    expr->node_type = clone_type(sym->type);
                    return 1;
                }
            }
            // Type check member access (e.g., msg.type, struct.field)
            if (expr->child_count > 0) {
                ASTNode* base = expr->children[0];
                typecheck_expression(base, table);

                Type* base_type = infer_type(base, table);

                // Reject member access on primitive types — catch the error in Aether, not C
                if (base_type && (base_type->kind == TYPE_INT || base_type->kind == TYPE_FLOAT ||
                                  base_type->kind == TYPE_BOOL || base_type->kind == TYPE_STRING)) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Type '%s' has no field '%s'",
                             type_name(base_type), expr->value ? expr->value : "?");
                    free_type(base_type);
                    type_error(error_msg, expr->line, expr->column);
                    return 0;
                }

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
                // Handle actor ref member access — look up state field type from actor definition
                else if (base_type && base_type->kind == TYPE_ACTOR_REF && base_type->element_type &&
                         base_type->element_type->kind == TYPE_STRUCT && base_type->element_type->struct_name) {
                    Symbol* actor_sym2 = lookup_symbol(table, base_type->element_type->struct_name);
                    if (actor_sym2 && actor_sym2->node) {
                        ASTNode* actor_def2 = actor_sym2->node;
                        for (int fi = 0; fi < actor_def2->child_count; fi++) {
                            ASTNode* field = actor_def2->children[fi];
                            if (field && field->type == AST_STATE_DECLARATION &&
                                field->value && strcmp(field->value, expr->value) == 0) {
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
                // Handle struct member access — look up field type from definition
                else if (base_type && base_type->kind == TYPE_STRUCT && base_type->struct_name) {
                    Symbol* struct_sym = lookup_symbol(table, base_type->struct_name);
                    if (struct_sym && struct_sym->node) {
                        ASTNode* struct_def = struct_sym->node;
                        int found = 0;
                        for (int fi = 0; fi < struct_def->child_count; fi++) {
                            ASTNode* field = struct_def->children[fi];
                            if (field && field->value && strcmp(field->value, expr->value) == 0) {
                                if (field->node_type && field->node_type->kind != TYPE_UNKNOWN) {
                                    expr->node_type = clone_type(field->node_type);
                                }
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg),
                                     "Struct '%s' has no field '%s'",
                                     base_type->struct_name, expr->value ? expr->value : "?");
                            free_type(base_type);
                            type_error(error_msg, expr->line, expr->column);
                            return 0;
                        }
                    }
                    // Fallback to general inference
                    if (!expr->node_type || expr->node_type->kind == TYPE_UNKNOWN) {
                        expr->node_type = infer_type(expr, table);
                    }
                }
                free_type(base_type);
            }
            return 1;
        }
            
        case AST_SEND_FIRE_FORGET: {
            // actor ! MessageType { fields... }  — validate both operands
            if (expr->child_count >= 2) {
                ASTNode* actor_ref = expr->children[0];
                ASTNode* message   = expr->children[1];

                typecheck_expression(actor_ref, table);

                // Validate that the message type is a registered message definition
                if (message->type == AST_MESSAGE_CONSTRUCTOR && message->value) {
                    Symbol* msg_sym = lookup_symbol(table, message->value);
                    if (!msg_sym || !msg_sym->type || msg_sym->type->kind != TYPE_MESSAGE) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Undefined message type '%s'", message->value);
                        type_error(error_msg, message->line, message->column);
                        return 0;
                    }
                }

                // Validate field value types match declared field types
                typecheck_message_constructor(message, table);
            }
            expr->node_type = create_type(TYPE_VOID);
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
            free_type(left_type);
            free_type(right_type);
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
            free_type(left_type);
            free_type(right_type);
            free_type(result_type);
            type_error("Invalid operation for given types", expr->line, expr->column);
            return 0;
        }
        expr->node_type = result_type;
    }

    free_type(left_type);
    free_type(right_type);
    return 1;
}

int typecheck_function_call(ASTNode* call, SymbolTable* table) {
    if (!call || call->type != AST_FUNCTION_CALL) return 0;

    // Use qualified lookup to handle namespaced calls like string.new -> string_new
    Symbol* symbol = lookup_qualified_symbol(table, call->value);
    if (!symbol || !symbol->is_function) {
        char error_msg[256];
        // Check if this is a visibility rejection (not-exported) rather than truly undefined
        if (call->value && strchr(call->value, '.') && global_module_registry) {
            char* tmp = strdup(call->value);
            char* dot = strchr(tmp, '.');
            *dot = '\0';
            if (is_export_blocked(tmp, dot + 1)) {
                snprintf(error_msg, sizeof(error_msg),
                         "'%s' is not exported from module '%s'", dot + 1, tmp);
            } else {
                snprintf(error_msg, sizeof(error_msg),
                         "Undefined function '%s'", call->value);
            }
            free(tmp);
        } else {
            snprintf(error_msg, sizeof(error_msg),
                     "Undefined function '%s'", call->value ? call->value : "?");
        }
        type_error(error_msg, call->line, call->column);
        return 0;
    }

    // Arity check: user-defined functions have their AST node stored
    if (symbol->node && symbol->node->type == AST_FUNCTION_DEFINITION) {
        int expected = count_function_params(symbol->node);
        int got = call->child_count;
        // If mismatch, try excluding trailing closures (for functions that
        // don't accept fn params but have trailing blocks for DSL syntax)
        if (got != expected) {
            int non_closure = 0;
            for (int i = 0; i < call->child_count; i++) {
                if (call->children[i] && call->children[i]->type != AST_CLOSURE) {
                    non_closure++;
                }
            }
            if (non_closure == expected) {
                got = non_closure; // trailing closures are DSL blocks, not args
            }
        }
        if (got != expected) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg),
                     "Function '%s' expects %d argument(s), got %d",
                     call->value, expected, got);
            type_error(error_msg, call->line, call->column);
            return 0;
        }
    }

    // Type check arguments and validate types against parameters
    for (int i = 0; i < call->child_count; i++) {
        typecheck_expression(call->children[i], table);
    }

    // Validate argument types for extern functions (which always have typed params)
    // Skip for user-defined functions since type inference may not have set param types yet
    if (symbol->node && symbol->node->type == AST_EXTERN_FUNCTION) {
        int param_idx = 0;
        for (int i = 0; i < symbol->node->child_count - 1 && param_idx < call->child_count; i++) {
            ASTNode* param = symbol->node->children[i];
            if (!param) { param_idx++; continue; }
            if (param->type == AST_VARIABLE_DECLARATION || param->type == AST_PATTERN_VARIABLE) {
                Type* param_type = param->node_type;
                if (param_type && param_type->kind != TYPE_UNKNOWN &&
                    call->children[param_idx] != NULL) {
                    Type* arg_type = infer_type(call->children[param_idx], table);
                    if (arg_type && arg_type->kind != TYPE_UNKNOWN &&
                        !is_type_compatible(arg_type, param_type)) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg),
                                 "Argument %d of '%s': expected %s, got %s",
                                 param_idx + 1, call->value,
                                 type_name(param_type), type_name(arg_type));
                        type_error(error_msg, call->children[param_idx]->line,
                                   call->children[param_idx]->column);
                    }
                    if (arg_type) free_type(arg_type);
                }
                param_idx++;
            }
        }
    }

    call->node_type = symbol->type ? clone_type(symbol->type) : create_type(TYPE_UNKNOWN);

    // select() infers its type from the first named arg's value
    if (call->value && strcmp(call->value, "select") == 0 &&
        (!call->node_type || call->node_type->kind == TYPE_UNKNOWN)) {
        for (int i = 0; i < call->child_count; i++) {
            ASTNode* arg = call->children[i];
            if (arg && arg->type == AST_NAMED_ARG &&
                arg->child_count > 0 && arg->children[0] &&
                arg->children[0]->node_type &&
                arg->children[0]->node_type->kind != TYPE_UNKNOWN) {
                if (call->node_type) free_type(call->node_type);
                call->node_type = clone_type(arg->children[0]->node_type);
                break;
            }
        }
    }

    return 1;
}
