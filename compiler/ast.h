#ifndef AST_H
#define AST_H

#include "frontend/tokens.h"

typedef enum {
    // Program structure
    AST_PROGRAM,
    AST_MODULE_DECLARATION,
    AST_IMPORT_STATEMENT,
    AST_EXPORT_STATEMENT,
    AST_ACTOR_DEFINITION,
    AST_FUNCTION_DEFINITION,
    AST_FUNCTION_CLAUSE,
    AST_MAIN_FUNCTION,
    AST_STRUCT_DEFINITION,
    AST_STRUCT_FIELD,
    AST_EXTERN_FUNCTION,      // External C function declaration

    // Statements
    AST_BLOCK,
    AST_VARIABLE_DECLARATION,
    AST_ASSIGNMENT,
    AST_IF_STATEMENT,
    AST_FOR_LOOP,
    AST_WHILE_LOOP,
    AST_SWITCH_STATEMENT,
    AST_CASE_STATEMENT,
    AST_RETURN_STATEMENT,
    AST_BREAK_STATEMENT,
    AST_CONTINUE_STATEMENT,
    AST_DEFER_STATEMENT,
    AST_EXPRESSION_STATEMENT,
    AST_MATCH_STATEMENT,
    AST_MATCH_ARM,
    AST_PATTERN_LITERAL,
    AST_PATTERN_VARIABLE,
    AST_PATTERN_STRUCT,
    AST_PATTERN_LIST,
    AST_PATTERN_CONS,
    AST_GUARD_CLAUSE,
    AST_RECEIVE_STATEMENT,
    AST_SEND_STATEMENT,
    AST_SPAWN_ACTOR_STATEMENT,
    AST_STATE_DECLARATION,
    
    // Actor V2 - Message system
    AST_MESSAGE_DEFINITION,
    AST_MESSAGE_FIELD,
    AST_RECEIVE_ARM,
    AST_MESSAGE_PATTERN,
    AST_PATTERN_FIELD,
    AST_WILDCARD_PATTERN,
    AST_REPLY_STATEMENT,
    AST_MESSAGE_CONSTRUCTOR,
    AST_FIELD_INIT,
    AST_SEND_FIRE_FORGET,
    AST_SEND_ASK,
    
    // Expressions
    AST_BINARY_EXPRESSION,
    AST_UNARY_EXPRESSION,
    AST_FUNCTION_CALL,
    AST_ACTOR_REF,
    AST_IDENTIFIER,
    AST_LITERAL,
    AST_ARRAY_LITERAL,
    AST_ARRAY_ACCESS,
    AST_MEMBER_ACCESS,
    AST_STRUCT_LITERAL,
    
    // Types
    AST_TYPE_ANNOTATION,
    AST_ACTOR_REF_TYPE,
    AST_ARRAY_TYPE,
    
    // Special
    AST_PRINT_STATEMENT
} ASTNodeType;

typedef enum {
    TYPE_INT,
    TYPE_INT64,
    TYPE_UINT64,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_ACTOR_REF,
    TYPE_MESSAGE,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_VOID,
    TYPE_PTR,           // void* for C interop
    TYPE_WILDCARD,
    TYPE_UNKNOWN
} TypeKind;

typedef struct Type {
    TypeKind kind;
    struct Type* element_type; // For arrays and actor refs
    int array_size; // For fixed-size arrays
    char* struct_name; // For struct types
} Type;

typedef struct ASTNode {
    ASTNodeType type;
    char* value;                // For literals, identifiers, etc.
    Type* node_type;           // Type information for this node
    struct ASTNode** children;  // Array of child nodes
    int child_count;
    int line;
    int column;
} ASTNode;

// Type functions
Type* create_type(TypeKind kind);
Type* create_array_type(Type* element_type, int size);
Type* create_actor_ref_type(Type* actor_type);
void free_type(Type* type);
const char* type_to_string(Type* type);
int types_equal(Type* a, Type* b);
Type* clone_type(Type* type);

// AST Node functions
ASTNode* create_ast_node(ASTNodeType type, const char* value, int line, int column);
void add_child(ASTNode* parent, ASTNode* child);
void free_ast_node(ASTNode* node);
void print_ast(ASTNode* node, int indent);
const char* ast_node_type_to_string(ASTNodeType type);

// Utility functions
ASTNode* create_literal_node(Token* token);
ASTNode* create_identifier_node(Token* token);
ASTNode* create_binary_expression(ASTNode* left, ASTNode* right, Token* operator);
ASTNode* create_unary_expression(ASTNode* operand, Token* operator);

#endif
