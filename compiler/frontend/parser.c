#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

Parser* create_parser(Token** tokens, int token_count) {
    Parser* parser = malloc(sizeof(Parser));
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->current_token = 0;
    parser->suppress_errors = 0;  // By default, show errors
    return parser;
}

void free_parser(Parser* parser) {
    if (parser) {
        free(parser);
    }
}

Token* peek_token(Parser* parser) {
    if (parser->current_token >= parser->token_count) {
        return NULL;
    }
    return parser->tokens[parser->current_token];
}

Token* peek_ahead(Parser* parser, int offset) {
    int pos = parser->current_token + offset;
    if (pos >= parser->token_count) {
        return NULL;
    }
    return parser->tokens[pos];
}

Token* advance_token(Parser* parser) {
    if (parser->current_token >= parser->token_count) {
        return NULL;
    }
    return parser->tokens[parser->current_token++];
}

Token* expect_token(Parser* parser, AeTokenType expected) {
    Token* token = peek_token(parser);
    if (!token || token->type != expected) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "Expected %s, got %s", 
                token_type_to_string(expected),
                token ? token_type_to_string(token->type) : "EOF");
        parser_error(parser, error_msg);
        return NULL;
    }
    return advance_token(parser);
}

int is_at_end(Parser* parser) {
    return parser->current_token >= parser->token_count || 
           peek_token(parser)->type == TOKEN_EOF;
}

int match_token(Parser* parser, AeTokenType type) {
    if (is_at_end(parser)) return 0;
    if (peek_token(parser)->type == type) {
        advance_token(parser);
        return 1;
    }
    return 0;
}

void parser_error(Parser* parser, const char* message) {
    if (parser->suppress_errors) {
        return;  // Silently return if error suppression is enabled
    }
    
    Token* token = peek_token(parser);
    if (token) {
        fprintf(stderr, "Parse error at line %d, column %d: %s", 
                token->line, token->column, message);
    } else {
        fprintf(stderr, "Parse error at end of file: %s", message);
    }
}

// Helper to print warnings and errors (respects suppress_errors flag)
static void parser_message(Parser* parser, const char* message) {
    if (!parser->suppress_errors) {
        fprintf(stderr, "%s", message);
    }
}

Type* parse_type(Parser* parser) {
    Token* token = peek_token(parser);
    if (!token) return NULL;
    
    Type* type = NULL;
    
    switch (token->type) {
        case TOKEN_INT:
            advance_token(parser);
            type = create_type(TYPE_INT);
            break;
        case TOKEN_FLOAT:
            advance_token(parser);
            type = create_type(TYPE_FLOAT);
            break;
        case TOKEN_BOOL:
            advance_token(parser);
            type = create_type(TYPE_BOOL);
            break;
        case TOKEN_STRING:
            advance_token(parser);
            type = create_type(TYPE_STRING);
            break;
        case TOKEN_MESSAGE:
            advance_token(parser);
            type = create_type(TYPE_MESSAGE);
            break;
        case TOKEN_PTR:
            advance_token(parser);
            type = create_type(TYPE_PTR);
            break;
        case TOKEN_IDENTIFIER: {
            // Could be a struct type
            advance_token(parser);
            type = create_type(TYPE_STRUCT);
            type->struct_name = strdup(token->value);
            break;
        }
        case TOKEN_ACTOR_REF:
            advance_token(parser);
            if (!expect_token(parser, TOKEN_LEFT_BRACKET)) return NULL;
            Type* actor_type = parse_type(parser);
            if (!expect_token(parser, TOKEN_RIGHT_BRACKET)) return NULL;
            type = create_actor_ref_type(actor_type);
            break;
        default:
            return NULL;
    }
    
    // Check for array type
    if (match_token(parser, TOKEN_LEFT_BRACKET)) {
        if (match_token(parser, TOKEN_RIGHT_BRACKET)) {
            // Dynamic array
            type = create_array_type(type, -1);
        } else {
            // Fixed-size array
            Token* size_token = expect_token(parser, TOKEN_NUMBER);
            if (size_token) {
                int size = atoi(size_token->value);
                if (!expect_token(parser, TOKEN_RIGHT_BRACKET)) return NULL;
                type = create_array_type(type, size);
            }
        }
    }
    
    return type;
}

ASTNode* parse_primary_expression(Parser* parser) {
    Token* token = peek_token(parser);
    if (!token) return NULL;
    
    switch (token->type) {
        case TOKEN_NUMBER:
        case TOKEN_STRING_LITERAL:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return create_literal_node(advance_token(parser));
            
        case TOKEN_IDENTIFIER: {
            // Could be identifier or struct literal
            Token* next = parser->tokens[parser->current_token + 1];
            if (next && next->type == TOKEN_LEFT_BRACE) {
                // Struct literal: TypeName{ field: value, ... }
                char* struct_name = strdup(token->value);
                int line = token->line;
                int column = token->column;
                advance_token(parser); // consume identifier
                advance_token(parser); // consume '{'
                
                ASTNode* struct_lit = create_ast_node(AST_STRUCT_LITERAL, struct_name, line, column);
                
                // Parse field initializers
                if (!match_token(parser, TOKEN_RIGHT_BRACE)) {
                    do {
                        // Parse field name
                        Token* field_name = expect_token(parser, TOKEN_IDENTIFIER);
                        if (!field_name) {
                            free_ast_node(struct_lit);
                            return NULL;
                        }
                        
                        // Expect colon
                        if (!expect_token(parser, TOKEN_COLON)) {
                            free_ast_node(struct_lit);
                            return NULL;
                        }
                        
                        // Parse field value
                        ASTNode* value_expr = parse_expression(parser);
                        if (!value_expr) {
                            free_ast_node(struct_lit);
                            return NULL;
                        }
                        
                        // Create field init node
                        ASTNode* field_init = create_ast_node(AST_ASSIGNMENT, field_name->value,
                                                              field_name->line, field_name->column);
                        add_child(field_init, value_expr);
                        add_child(struct_lit, field_init);
                        
                    } while (match_token(parser, TOKEN_COMMA));
                    
                    if (!expect_token(parser, TOKEN_RIGHT_BRACE)) {
                        free_ast_node(struct_lit);
                        return NULL;
                    }
                }
                
                return struct_lit;
            } else {
                // Regular identifier
                return create_identifier_node(advance_token(parser));
            }
        }
            
        case TOKEN_LEFT_PAREN: {
            advance_token(parser);
            ASTNode* expr = parse_expression(parser);
            if (!expect_token(parser, TOKEN_RIGHT_PAREN)) return NULL;
            return expr;
        }
        
        case TOKEN_LEFT_BRACKET: {
            // Array literal: [1, 2, 3]
            int line = token->line;
            int column = token->column;
            advance_token(parser); // consume '['
            
            ASTNode* array_lit = create_ast_node(AST_ARRAY_LITERAL, NULL, line, column);
            
            // Parse array elements
            if (!match_token(parser, TOKEN_RIGHT_BRACKET)) {
                do {
                    ASTNode* element = parse_expression(parser);
                    if (element) {
                        add_child(array_lit, element);
                    }
                } while (match_token(parser, TOKEN_COMMA));
                
                if (!expect_token(parser, TOKEN_RIGHT_BRACKET)) {
                    free_ast_node(array_lit);
                    return NULL;
                }
            }
            
            return array_lit;
        }
        
        case TOKEN_SELF:
            advance_token(parser);
            return create_ast_node(AST_ACTOR_REF, "self", token->line, token->column);
        
        case TOKEN_MAKE: {
            // make([]type, size) for dynamic arrays
            int line = token->line;
            int column = token->column;
            advance_token(parser); // consume 'make'

            if (!expect_token(parser, TOKEN_LEFT_PAREN)) return NULL;

            // Parse []type syntax
            if (!expect_token(parser, TOKEN_LEFT_BRACKET)) return NULL;
            if (!expect_token(parser, TOKEN_RIGHT_BRACKET)) return NULL;

            // Parse element type
            Type* element_type = parse_type(parser);
            if (!element_type) {
                parser_error(parser, "Expected type after [] in make");
                return NULL;
            }

            // Parse comma
            if (!expect_token(parser, TOKEN_COMMA)) return NULL;

            // Parse size expression
            ASTNode* size_expr = parse_expression(parser);
            if (!size_expr) {
                parser_error(parser, "Expected size expression in make");
                return NULL;
            }

            if (!expect_token(parser, TOKEN_RIGHT_PAREN)) return NULL;

            // Create a function call node: malloc(size * sizeof(type))
            // We'll transform this in codegen
            ASTNode* make_node = create_ast_node(AST_FUNCTION_CALL, "make", line, column);
            make_node->node_type = create_array_type(element_type, -1); // Dynamic array
            add_child(make_node, size_expr);

            return make_node;
        }

        case TOKEN_SPAWN: {
            // spawn(ActorName()) - spawn as function-call syntax
            int line = token->line;
            int column = token->column;
            advance_token(parser); // consume 'spawn'

            // Expect opening paren: spawn(...)
            if (!expect_token(parser, TOKEN_LEFT_PAREN)) {
                parser_error(parser, "Expected '(' after 'spawn'");
                return NULL;
            }

            Token* actor_name = expect_token(parser, TOKEN_IDENTIFIER);
            if (!actor_name) {
                parser_error(parser, "Expected actor name inside spawn(...)");
                return NULL;
            }

            // Expect () after actor name (constructor args)
            if (!expect_token(parser, TOKEN_LEFT_PAREN)) return NULL;
            if (!expect_token(parser, TOKEN_RIGHT_PAREN)) return NULL;

            // Expect closing paren for spawn(...)
            if (!expect_token(parser, TOKEN_RIGHT_PAREN)) return NULL;

            // Internal representation unchanged: AST_FUNCTION_CALL with spawn_ActorName
            char func_name[256];
            snprintf(func_name, sizeof(func_name), "spawn_%s", actor_name->value);

            ASTNode* spawn_call = create_ast_node(AST_FUNCTION_CALL, func_name, line, column);
            return spawn_call;
        }

        default:
            return NULL;
    }
}

ASTNode* parse_expression(Parser* parser) {
    return parse_binary_expression(parser, 0);
}

ASTNode* parse_binary_expression(Parser* parser, int precedence) {
    ASTNode* left = parse_unary_expression(parser);
    if (!left) return NULL;
    
    int iteration_count = 0;
    const int MAX_BINARY_OPS = 1000;
    
    while (1) {
        if (++iteration_count > MAX_BINARY_OPS) {
            parser_message(parser, "Error: Expression too complex (max %d binary operators)");
            break;
        }
        
        Token* operator = peek_token(parser);
        if (!operator) break;
        
        int op_precedence = get_operator_precedence(operator->type);
        if (op_precedence < 0) break;  // Not an operator
        if (op_precedence < precedence) break;  // Lower precedence, stop
        
        advance_token(parser);
        ASTNode* right = parse_binary_expression(parser, op_precedence + 1);  // Left-associative
        if (!right) return NULL;
        
        left = create_binary_expression(left, right, operator);
    }
    
    return left;
}

// Parse postfix expressions like i++ / i-- / obj.field
static ASTNode* parse_postfix_expression(Parser* parser) {
    ASTNode* expr = parse_primary_expression(parser);
    if (!expr) return NULL;
    
    int iteration_count = 0;
    const int MAX_POSTFIX_OPS = 100;
    
    while (1) {
        if (++iteration_count > MAX_POSTFIX_OPS) {
            parser_message(parser, "Error: Too many postfix operations (max 100)");
            break;
        }
        
        Token* op = peek_token(parser);
        if (!op) break;
        
        if (op->type == TOKEN_INCREMENT || op->type == TOKEN_DECREMENT) {
            advance_token(parser);
            expr = create_unary_expression(expr, op);
            continue;
        }
        
        if (op->type == TOKEN_DOT) {
            // Member access: expr.field
            advance_token(parser);
            Token* field = expect_token(parser, TOKEN_IDENTIFIER);
            if (!field) return NULL;
            
            ASTNode* member_access = create_ast_node(AST_MEMBER_ACCESS, field->value, op->line, op->column);
            add_child(member_access, expr);
            expr = member_access;
            continue;
        }
        
        if (op->type == TOKEN_LEFT_BRACKET) {
            // Array indexing: expr[index]
            advance_token(parser); // consume '['
            ASTNode* index = parse_expression(parser);
            if (!index) return NULL;
            if (!expect_token(parser, TOKEN_RIGHT_BRACKET)) return NULL;
            
            ASTNode* array_access = create_ast_node(AST_ARRAY_ACCESS, NULL, op->line, op->column);
            add_child(array_access, expr);  // array expression
            add_child(array_access, index); // index expression
            expr = array_access;
            continue;
        }
        
        if (op->type == TOKEN_LEFT_PAREN) {
            // Function call: expr(arg1, arg2, ...)
            // Extract function name if expr is an identifier
            const char* func_name = NULL;
            if (expr && expr->type == AST_IDENTIFIER && expr->value) {
                func_name = strdup(expr->value);
            }
            
            advance_token(parser); // consume '('
            
            ASTNode* func_call = create_ast_node(AST_FUNCTION_CALL, func_name, op->line, op->column);
            
            // Parse arguments
            if (!match_token(parser, TOKEN_RIGHT_PAREN)) {
                do {
                    ASTNode* arg = parse_expression(parser);
                    if (!arg) {
                        free_ast_node(func_call);
                        return NULL;
                    }
                    add_child(func_call, arg);
                } while (match_token(parser, TOKEN_COMMA));
                
                if (!expect_token(parser, TOKEN_RIGHT_PAREN)) {
                    free_ast_node(func_call);
                    return NULL;
                }
            }
            
            // Free the original identifier node since we've copied its name
            if (expr) free_ast_node(expr);
            
            expr = func_call;
            continue;
        }
        
        // Actor V2 - Fire-and-forget operator: actor ! Message { ... }
        if (op->type == TOKEN_EXCLAIM) {
            advance_token(parser); // consume '!'
            
            ASTNode* message = parse_message_constructor(parser);
            if (!message) return NULL;
            
            ASTNode* send_op = create_ast_node(AST_SEND_FIRE_FORGET, NULL, op->line, op->column);
            add_child(send_op, expr);     // actor reference
            add_child(send_op, message);  // message to send
            expr = send_op;
            continue;
        }
        
        // Actor V2 - Ask operator: result = actor ? Message { ... }
        if (op->type == TOKEN_QUESTION) {
            advance_token(parser); // consume '?'
            
            ASTNode* message = parse_message_constructor(parser);
            if (!message) return NULL;
            
            ASTNode* ask_op = create_ast_node(AST_SEND_ASK, NULL, op->line, op->column);
            add_child(ask_op, expr);     // actor reference
            add_child(ask_op, message);  // message to send
            expr = ask_op;
            continue;
        }
        
        break;
    }
    
    return expr;
}

ASTNode* parse_unary_expression(Parser* parser) {
    Token* operator = peek_token(parser);
    if (!operator) return NULL;
    
    if (operator->type == TOKEN_EXCLAIM || operator->type == TOKEN_MINUS ||
        operator->type == TOKEN_INCREMENT || operator->type == TOKEN_DECREMENT) {
        advance_token(parser);
        ASTNode* operand = parse_unary_expression(parser);
        if (!operand) return NULL;
        return create_unary_expression(operand, operator);
    }
    
    return parse_postfix_expression(parser);
}

int get_operator_precedence(AeTokenType type) {
    switch (type) {
        case TOKEN_ASSIGN: return 0;  // Lowest precedence (right-associative)
        case TOKEN_OR: return 1;
        case TOKEN_AND: return 2;
        case TOKEN_EQUALS:
        case TOKEN_NOT_EQUALS: return 3;
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL: return 4;
        case TOKEN_PLUS:
        case TOKEN_MINUS: return 5;
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
        case TOKEN_MODULO: return 6;
        case TOKEN_INCREMENT:
        case TOKEN_DECREMENT: return 7;
        default: return -1;  // Not an operator
    }
}

ASTNode* parse_statement(Parser* parser) {
    Token* token = peek_token(parser);
    if (!token) return NULL;
    
    switch (token->type) {
        case TOKEN_LET:
        case TOKEN_VAR:
            // Optional 'let' or 'var' - skip it and parse as Python-style
            advance_token(parser);
            return parse_python_style_declaration(parser);
            
        case TOKEN_INT:
        case TOKEN_STRING:
        case TOKEN_FLOAT:
        case TOKEN_BOOL:
            // Explicit type: int x = 42;
            return parse_variable_declaration(parser);
            
        case TOKEN_IF:
            return parse_if_statement(parser);
            
        case TOKEN_FOR:
            return parse_for_loop(parser);
            
        case TOKEN_WHILE:
            return parse_while_loop(parser);
            
        case TOKEN_SWITCH:
            return parse_switch_statement(parser);
            
        case TOKEN_MATCH:
            return parse_match_statement(parser);
            
        case TOKEN_RETURN:
            return parse_return_statement(parser);
            
        case TOKEN_REPLY:
            return parse_reply_statement(parser);
            
        case TOKEN_BREAK:
            advance_token(parser);
            expect_token(parser, TOKEN_SEMICOLON);
            return create_ast_node(AST_BREAK_STATEMENT, NULL, token->line, token->column);
            
        case TOKEN_CONTINUE:
            advance_token(parser);
            expect_token(parser, TOKEN_SEMICOLON);
            return create_ast_node(AST_CONTINUE_STATEMENT, NULL, token->line, token->column);
            
        case TOKEN_DEFER:
            return parse_defer_statement(parser);
            
        case TOKEN_PRINT:
            return parse_print_statement(parser);
            
        case TOKEN_SEND:
            return parse_send_statement(parser);
            
        case TOKEN_SPAWN_ACTOR:
            return parse_spawn_actor_statement(parser);
            
        case TOKEN_LEFT_BRACE:
            return parse_block(parser);
            
        case TOKEN_IDENTIFIER: {
            // Check if this is: identifier = expression (Python-style)
            Token* next = peek_ahead(parser, 1);
            if (next && next->type == TOKEN_ASSIGN) {
                // This is: x = value (could be declaration or assignment)
                return parse_python_style_declaration(parser);
            }
            // Otherwise fall through to expression statement
            ASTNode* expr = parse_expression(parser);
            if (expr) {
                expect_token(parser, TOKEN_SEMICOLON);
                ASTNode* stmt = create_ast_node(AST_EXPRESSION_STATEMENT, NULL, token->line, token->column);
                add_child(stmt, expr);
                return stmt;
            }
            return NULL;
        }
            
        default: {
            ASTNode* expr = parse_expression(parser);
            if (expr) {
                expect_token(parser, TOKEN_SEMICOLON);
                ASTNode* stmt = create_ast_node(AST_EXPRESSION_STATEMENT, NULL, token->line, token->column);
                add_child(stmt, expr);
                return stmt;
            }
            return NULL;
        }
    }
}

ASTNode* parse_variable_declaration(Parser* parser) {
    return parse_variable_declaration_with_semicolon(parser, true);
}

ASTNode* parse_variable_declaration_with_semicolon(Parser* parser, bool expect_semicolon) {
    // Token is already positioned at type token (int, string, etc.)
    Type* type = parse_type(parser);  // parse_type will advance past type
    Token* name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name) return NULL;
    
    ASTNode* decl = create_ast_node(AST_VARIABLE_DECLARATION, name->value, name->line, name->column);
    decl->node_type = type;
    
    if (match_token(parser, TOKEN_ASSIGN)) {
        ASTNode* value = parse_expression(parser);
        if (value) {
            add_child(decl, value);
        }
    }
    
    if (expect_semicolon) {
        expect_token(parser, TOKEN_SEMICOLON);
    }
    return decl;
}

// Python-style variable declaration: x = 42 (no 'let', type inferred)
ASTNode* parse_python_style_declaration(Parser* parser) {
    Token* name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name) return NULL;
    
    // Create declaration node with TYPE_UNKNOWN (will be inferred)
    ASTNode* decl = create_ast_node(AST_VARIABLE_DECLARATION, name->value, name->line, name->column);
    decl->node_type = create_type(TYPE_UNKNOWN);
    
    if (match_token(parser, TOKEN_ASSIGN)) {
        ASTNode* value = parse_expression(parser);
        if (value) {
            add_child(decl, value);
        }
    }
    
    expect_token(parser, TOKEN_SEMICOLON);
    return decl;
}

ASTNode* parse_if_statement(Parser* parser) {
    advance_token(parser); // if
    ASTNode* condition = parse_expression(parser);
    if (!condition) return NULL;
    
    ASTNode* then_branch = parse_statement(parser);
    if (!then_branch) return NULL;
    
    ASTNode* if_stmt = create_ast_node(AST_IF_STATEMENT, NULL, 0, 0);
    add_child(if_stmt, condition);
    add_child(if_stmt, then_branch);
    
    if (match_token(parser, TOKEN_ELSE)) {
        ASTNode* else_branch = parse_statement(parser);
        if (else_branch) {
            add_child(if_stmt, else_branch);
        }
    }
    
    return if_stmt;
}

ASTNode* parse_for_loop(Parser* parser) {
    advance_token(parser); // for
    expect_token(parser, TOKEN_LEFT_PAREN);
    
    ASTNode* init = NULL;
    Token* token = peek_token(parser);
    
    // Check if init is a variable declaration (int i = 1) or expression (i = 1)
    if (token && (token->type == TOKEN_INT || token->type == TOKEN_STRING ||
                  token->type == TOKEN_FLOAT || token->type == TOKEN_BOOL)) {
        init = parse_variable_declaration_with_semicolon(parser, false);
        expect_token(parser, TOKEN_SEMICOLON);
    } else if (token && token->type == TOKEN_IDENTIFIER) {
        // Check for Python-style: i = 0 (treat as variable declaration)
        Token* next = peek_ahead(parser, 1);
        if (next && next->type == TOKEN_ASSIGN) {
            // Parse as variable declaration without consuming semicolon
            Token* name = expect_token(parser, TOKEN_IDENTIFIER);
            init = create_ast_node(AST_VARIABLE_DECLARATION, name->value, name->line, name->column);
            init->node_type = create_type(TYPE_UNKNOWN);
            if (match_token(parser, TOKEN_ASSIGN)) {
                ASTNode* value = parse_expression(parser);
                if (value) {
                    add_child(init, value);
                }
            }
        } else {
            init = parse_expression(parser);
        }
        expect_token(parser, TOKEN_SEMICOLON);
    } else if (!match_token(parser, TOKEN_SEMICOLON)) {
        init = parse_expression(parser);
        expect_token(parser, TOKEN_SEMICOLON);
    }
    
    ASTNode* condition = NULL;
    if (!match_token(parser, TOKEN_SEMICOLON)) {
        condition = parse_expression(parser);
        expect_token(parser, TOKEN_SEMICOLON);
    }
    
    ASTNode* increment = NULL;
    if (!match_token(parser, TOKEN_RIGHT_PAREN)) {
        increment = parse_expression(parser);
        expect_token(parser, TOKEN_RIGHT_PAREN);
    }
    
    ASTNode* body = parse_statement(parser);
    if (!body) return NULL;
    
    ASTNode* for_loop = create_ast_node(AST_FOR_LOOP, NULL, 0, 0);
    // Reserve 4 slots for init, condition, increment, body
    for_loop->children = malloc(4 * sizeof(ASTNode*));
    for_loop->child_count = 4;
    for_loop->children[0] = init;
    for_loop->children[1] = condition;
    for_loop->children[2] = increment;
    for_loop->children[3] = body;
    
    return for_loop;
}

ASTNode* parse_while_loop(Parser* parser) {
    advance_token(parser); // while
    ASTNode* condition = parse_expression(parser);
    if (!condition) return NULL;
    
    ASTNode* body = parse_statement(parser);
    if (!body) return NULL;
    
    ASTNode* while_loop = create_ast_node(AST_WHILE_LOOP, NULL, 0, 0);
    add_child(while_loop, condition);
    add_child(while_loop, body);
    
    return while_loop;
}

ASTNode* parse_switch_statement(Parser* parser) {
    advance_token(parser);
    ASTNode* expression = parse_expression(parser);
    if (!expression) return NULL;
    
    expect_token(parser, TOKEN_LEFT_BRACE);
    
    ASTNode* switch_stmt = create_ast_node(AST_SWITCH_STATEMENT, NULL, 0, 0);
    add_child(switch_stmt, expression);
    
    int iteration_count = 0;
    const int MAX_CASES = 1000;
    
    while (!match_token(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
        if (++iteration_count > MAX_CASES) {
            parser_message(parser, "Error: Too many cases in switch statement (max 100)");
            return switch_stmt;
        }
        
        ASTNode* case_stmt = parse_case_statement(parser);
        if (case_stmt) {
            add_child(switch_stmt, case_stmt);
        } else {
            fprintf(stderr, "Parse error: Expected 'case' or 'default' in switch statement");
            advance_token(parser);
        }
    }
    
    return switch_stmt;
}

ASTNode* parse_case_statement(Parser* parser) {
    if (match_token(parser, TOKEN_DEFAULT)) {
        expect_token(parser, TOKEN_COLON);
        
        ASTNode* case_stmt = create_ast_node(AST_CASE_STATEMENT, "default", 0, 0);
        
        int iteration_count = 0;
        const int MAX_CASE_STMTS = 1000;
        
        while (!is_at_end(parser)) {
            if (++iteration_count > MAX_CASE_STMTS) {
                parser_message(parser, "Error: Too many statements in case block (max %d)");
                break;
            }
            
            Token* next = peek_token(parser);
            if (!next || next->type == TOKEN_CASE || next->type == TOKEN_DEFAULT || next->type == TOKEN_RIGHT_BRACE) {
                break;
            }
            ASTNode* stmt = parse_statement(parser);
            if (stmt) {
                add_child(case_stmt, stmt);
            } else {
                advance_token(parser);
            }
        }
        return case_stmt;
    }
    
    if (match_token(parser, TOKEN_CASE)) {
        ASTNode* value = parse_expression(parser);
        expect_token(parser, TOKEN_COLON);
        
        ASTNode* case_stmt = create_ast_node(AST_CASE_STATEMENT, NULL, 0, 0);
        add_child(case_stmt, value);
        
        int iteration_count = 0;
        const int MAX_CASE_STMTS = 1000;
        
        while (!is_at_end(parser)) {
            if (++iteration_count > MAX_CASE_STMTS) {
                parser_message(parser, "Error: Too many statements in case block (max %d)");
                break;
            }
            
            Token* next = peek_token(parser);
            if (!next || next->type == TOKEN_CASE || next->type == TOKEN_DEFAULT || next->type == TOKEN_RIGHT_BRACE) {
                break;
            }
            ASTNode* stmt = parse_statement(parser);
            if (stmt) {
                add_child(case_stmt, stmt);
            } else {
                advance_token(parser);
            }
        }
        return case_stmt;
    }
    
    return NULL;
}

// Parse match statement (pattern matching)
// Syntax:
//   match (expr) {
//     pattern => expression
//     pattern => { statements }
//     _ => default_case
//   }
ASTNode* parse_match_statement(Parser* parser) {
    advance_token(parser); // consume 'match'
    
    // Parse the expression to match on
    expect_token(parser, TOKEN_LEFT_PAREN);
    ASTNode* expression = parse_expression(parser);
    if (!expression) return NULL;
    expect_token(parser, TOKEN_RIGHT_PAREN);
    
    expect_token(parser, TOKEN_LEFT_BRACE);
    
    ASTNode* match_stmt = create_ast_node(AST_MATCH_STATEMENT, NULL, 0, 0);
    add_child(match_stmt, expression);
    
    int iteration_count = 0;
    const int MAX_CASES = 1000;
    
    // Parse match arms
    while (!match_token(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
        if (++iteration_count > MAX_CASES) {
            parser_message(parser, "Error: Too many match arms (max 1000)");
            return match_stmt;
        }
        
        ASTNode* match_arm = parse_match_case(parser);
        if (match_arm) {
            add_child(match_stmt, match_arm);
        } else {
            parser_message(parser, "Parse error: Expected match arm in match statement");
            advance_token(parser);
        }
    }
    
    return match_stmt;
}

// Parse a single match arm
// pattern => expression
// pattern => { block }
ASTNode* parse_match_case(Parser* parser) {
    Token* current = peek_token(parser);
    if (!current) return NULL;

    // Parse pattern: wildcard, list pattern, or expression
    ASTNode* pattern = NULL;

    if (current->type == TOKEN_IDENTIFIER && strcmp(current->value, "_") == 0) {
        // Wildcard pattern
        advance_token(parser);
        pattern = create_ast_node(AST_LITERAL, "_", current->line, current->column);
        pattern->node_type = create_type(TYPE_WILDCARD);
    } else if (current->type == TOKEN_LEFT_BRACKET) {
        // List pattern: [], [x], [x, y], [h|t]
        pattern = parse_pattern(parser);
        if (!pattern) return NULL;
    } else {
        // Expression pattern (literal, identifier, etc.)
        pattern = parse_expression(parser);
        if (!pattern) return NULL;
    }
    
    // Expect => arrow
    expect_token(parser, TOKEN_ARROW);
    
    // Parse the result (expression or block)
    ASTNode* result = NULL;
    Token* next = peek_token(parser);
    
    if (next && next->type == TOKEN_LEFT_BRACE) {
        // Block result
        result = parse_block(parser);
    } else {
        // Expression result
        result = parse_expression(parser);
    }
    
    if (!result) return NULL;
    
    // Optional comma or newline
    Token* separator = peek_token(parser);
    if (separator && separator->type == TOKEN_COMMA) {
        advance_token(parser);
    }
    
    // Create match arm node
    ASTNode* match_arm = create_ast_node(AST_MATCH_ARM, NULL, 0, 0);
    add_child(match_arm, pattern);
    add_child(match_arm, result);
    
    return match_arm;
}

// Parse module declaration
// Syntax: module name.subname
ASTNode* parse_module_declaration(Parser* parser) {
    Token* module_token = advance_token(parser);  // consume 'module'
    
    Token* name_token = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name_token) return NULL;
    
    // Build full module name (handle dotted notation)
    char module_name[256] = {0};
    strncpy(module_name, name_token->value, sizeof(module_name) - 1);
    
    while (match_token(parser, TOKEN_DOT)) {
        Token* part = expect_token(parser, TOKEN_IDENTIFIER);
        if (!part) break;
        strncat(module_name, ".", sizeof(module_name) - strlen(module_name) - 1);
        strncat(module_name, part->value, sizeof(module_name) - strlen(module_name) - 1);
    }
    
    ASTNode* module_decl = create_ast_node(AST_MODULE_DECLARATION, module_name, 
                                          module_token->line, module_token->column);
    return module_decl;
}

// Parse import statement
// Syntax: import module.name
// Syntax: import module.name (symbol1, symbol2)
// Syntax: import module.name as alias
ASTNode* parse_import_statement(Parser* parser) {
    Token* import_token = advance_token(parser);  // consume 'import'
    
    Token* name_token = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name_token) return NULL;
    
    // Build module name (handle dotted notation)
    char module_name[256] = {0};
    strncpy(module_name, name_token->value, sizeof(module_name) - 1);
    
    while (match_token(parser, TOKEN_DOT)) {
        Token* part = expect_token(parser, TOKEN_IDENTIFIER);
        if (!part) break;
        strncat(module_name, ".", sizeof(module_name) - strlen(module_name) - 1);
        strncat(module_name, part->value, sizeof(module_name) - strlen(module_name) - 1);
    }
    
    ASTNode* import_stmt = create_ast_node(AST_IMPORT_STATEMENT, module_name,
                                          import_token->line, import_token->column);
    
    // Check for selective import: import mod (a, b, c)
    if (match_token(parser, TOKEN_LEFT_PAREN)) {
        do {
            Token* symbol = expect_token(parser, TOKEN_IDENTIFIER);
            if (!symbol) break;
            
            ASTNode* symbol_node = create_ast_node(AST_IDENTIFIER, symbol->value, 
                                                  symbol->line, symbol->column);
            add_child(import_stmt, symbol_node);
        } while (match_token(parser, TOKEN_COMMA));
        
        expect_token(parser, TOKEN_RIGHT_PAREN);
    }
    
    // Check for alias: import mod as alias
    Token* next = peek_token(parser);
    if (next && next->type == TOKEN_AS) {
        advance_token(parser);  // consume 'as'
        Token* alias = expect_token(parser, TOKEN_IDENTIFIER);
        if (alias) {
            ASTNode* alias_node = create_ast_node(AST_IDENTIFIER, alias->value,
                                                 alias->line, alias->column);
            // Store alias as last child
            add_child(import_stmt, alias_node);
        }
    }
    
    return import_stmt;
}

// Parse export statement
// Syntax: export func_name
// Syntax: export struct Point { ... }
// Syntax: export actor Worker { ... }
ASTNode* parse_export_statement(Parser* parser) {
    Token* export_token = advance_token(parser);  // consume 'export'
    
    ASTNode* export_stmt = create_ast_node(AST_EXPORT_STATEMENT, NULL,
                                          export_token->line, export_token->column);
    
    Token* next = peek_token(parser);
    if (!next) return NULL;
    
    ASTNode* exported_item = NULL;
    
    switch (next->type) {
        case TOKEN_FUNC:
            advance_token(parser);
            exported_item = parse_function_definition(parser);
            break;
        case TOKEN_STRUCT:
            exported_item = parse_struct_definition(parser);
            break;
        case TOKEN_ACTOR:
            exported_item = parse_actor_definition(parser);
            break;
        case TOKEN_IDENTIFIER:
            // Export existing symbol: export my_func
            exported_item = create_ast_node(AST_IDENTIFIER, next->value,
                                          next->line, next->column);
            advance_token(parser);
            break;
        default:
            parser_error(parser, "Expected function, struct, actor, or identifier after 'export'");
            return NULL;
    }
    
    if (exported_item) {
        add_child(export_stmt, exported_item);
    }
    
    return export_stmt;
}

ASTNode* parse_return_statement(Parser* parser) {
    advance_token(parser); // return
    ASTNode* value = NULL;
    if (!match_token(parser, TOKEN_SEMICOLON)) {
        value = parse_expression(parser);
        expect_token(parser, TOKEN_SEMICOLON);
    }
    
    ASTNode* return_stmt = create_ast_node(AST_RETURN_STATEMENT, NULL, 0, 0);
    if (value) {
        add_child(return_stmt, value);
    }
    
    return return_stmt;
}

ASTNode* parse_defer_statement(Parser* parser) {
    Token* defer_token = peek_token(parser);
    advance_token(parser);
    
    ASTNode* deferred_stmt = parse_statement(parser);
    if (!deferred_stmt) {
        parser_error(parser, "Expected statement after 'defer'");
        return NULL;
    }
    
    ASTNode* defer_node = create_ast_node(AST_DEFER_STATEMENT, NULL, defer_token->line, defer_token->column);
    add_child(defer_node, deferred_stmt);
    
    return defer_node;
}

// Actor V2 - Message Definition Parsing
// Syntax: message MessageName { field1: type1, field2: type2 }
ASTNode* parse_message_definition(Parser* parser) {
    Token* message_token = peek_token(parser);
    advance_token(parser); // consume 'message'
    
    Token* name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name) return NULL;
    
    expect_token(parser, TOKEN_LEFT_BRACE);
    
    ASTNode* msg_def = create_ast_node(AST_MESSAGE_DEFINITION, name->value, message_token->line, message_token->column);
    
    // Parse fields: name: type
    while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
        if (is_at_end(parser)) {
            parser_message(parser, "Error: Unexpected end of file in message definition");
            return NULL;
        }
        
        Token* field_name = expect_token(parser, TOKEN_IDENTIFIER);
        if (!field_name) break;
        
        expect_token(parser, TOKEN_COLON);
        
        Type* field_type = parse_type(parser);
        if (!field_type) {
            parser_message(parser, "Error: Expected type for message field");
            break;
        }
        
        ASTNode* field = create_ast_node(AST_MESSAGE_FIELD, field_name->value, field_name->line, field_name->column);
        field->node_type = field_type;
        add_child(msg_def, field);
        
        // Optional comma
        if (peek_token(parser) && peek_token(parser)->type == TOKEN_COMMA) {
            advance_token(parser);
        }
    }
    
    return msg_def;
}

// Parse message pattern in receive block
// Syntax: MessageName(field1, field2) or MessageName(field1: var1, field2)
ASTNode* parse_message_pattern(Parser* parser) {
    Token* msg_name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!msg_name) return NULL;

    ASTNode* pattern = create_ast_node(AST_MESSAGE_PATTERN, msg_name->value, msg_name->line, msg_name->column);

    // Check for field destructuring
    if (match_token(parser, TOKEN_LEFT_PAREN)) {
        // Parse pattern fields
        while (!match_token(parser, TOKEN_RIGHT_PAREN)) {
            if (is_at_end(parser)) {
                parser_message(parser, "Error: Unexpected end in message pattern");
                return NULL;
            }
            
            Token* field_name = expect_token(parser, TOKEN_IDENTIFIER);
            if (!field_name) break;

            ASTNode* field_pattern = create_ast_node(AST_PATTERN_FIELD, field_name->value, field_name->line, field_name->column);

            // Check for explicit binding: field: variable
            if (match_token(parser, TOKEN_COLON)) {
                Token* var_name = expect_token(parser, TOKEN_IDENTIFIER);
                if (var_name) {
                    ASTNode* var_node = create_ast_node(AST_PATTERN_VARIABLE, var_name->value, var_name->line, var_name->column);
                    add_child(field_pattern, var_node);
                }
            } else {
                // Implicit binding: use field name as variable name
                ASTNode* var_node = create_ast_node(AST_PATTERN_VARIABLE, field_name->value, field_name->line, field_name->column);
                add_child(field_pattern, var_node);
            }

            add_child(pattern, field_pattern);
            
            if (peek_token(parser) && peek_token(parser)->type == TOKEN_COMMA) {
                advance_token(parser);
            }
        }
    }
    
    return pattern;
}

// Parse reply statement
// Syntax: reply MessageName { field1: expr1, field2: expr2 }
ASTNode* parse_reply_statement(Parser* parser) {
    Token* reply_token = peek_token(parser);
    advance_token(parser); // consume 'reply'
    
    Token* msg_name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!msg_name) return NULL;
    
    ASTNode* reply_stmt = create_ast_node(AST_REPLY_STATEMENT, msg_name->value, reply_token->line, reply_token->column);
    
    // Parse message constructor
    if (match_token(parser, TOKEN_LEFT_BRACE)) {
        while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
            if (is_at_end(parser)) {
                parser_message(parser, "Error: Unexpected end in reply statement");
                return NULL;
            }
            
            Token* field_name = expect_token(parser, TOKEN_IDENTIFIER);
            if (!field_name) break;
            
            expect_token(parser, TOKEN_COLON);
            
            ASTNode* field_expr = parse_expression(parser);
            if (!field_expr) break;
            
            ASTNode* field_init = create_ast_node(AST_FIELD_INIT, field_name->value, field_name->line, field_name->column);
            add_child(field_init, field_expr);
            add_child(reply_stmt, field_init);
            
            if (peek_token(parser) && peek_token(parser)->type == TOKEN_COMMA) {
                advance_token(parser);
            }
        }
    }
    
    return reply_stmt;
}

// Parse message constructor (for send operations)
// Syntax: MessageName { field1: expr1, field2: expr2 }
ASTNode* parse_message_constructor(Parser* parser) {
    Token* msg_name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!msg_name) return NULL;
    
    ASTNode* constructor = create_ast_node(AST_MESSAGE_CONSTRUCTOR, msg_name->value, msg_name->line, msg_name->column);
    
    if (match_token(parser, TOKEN_LEFT_BRACE)) {
        while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
            if (is_at_end(parser)) {
                parser_message(parser, "Error: Unexpected end in message constructor");
                return NULL;
            }
            
            Token* field_name = expect_token(parser, TOKEN_IDENTIFIER);
            if (!field_name) break;
            
            expect_token(parser, TOKEN_COLON);
            
            ASTNode* field_expr = parse_expression(parser);
            if (!field_expr) break;
            
            ASTNode* field_init = create_ast_node(AST_FIELD_INIT, field_name->value, field_name->line, field_name->column);
            add_child(field_init, field_expr);
            add_child(constructor, field_init);
            
            if (peek_token(parser) && peek_token(parser)->type == TOKEN_COMMA) {
                advance_token(parser);
            }
        }
    }
    
    return constructor;
}

ASTNode* parse_print_statement(Parser* parser) {
    advance_token(parser); // print
    expect_token(parser, TOKEN_LEFT_PAREN);
    
    ASTNode* print_stmt = create_ast_node(AST_PRINT_STATEMENT, NULL, 0, 0);
    
    if (!match_token(parser, TOKEN_RIGHT_PAREN)) {
        do {
            ASTNode* arg = parse_expression(parser);
            if (arg) {
                add_child(print_stmt, arg);
            }
        } while (match_token(parser, TOKEN_COMMA));
        
        expect_token(parser, TOKEN_RIGHT_PAREN);
    }
    
    expect_token(parser, TOKEN_SEMICOLON);
    return print_stmt;
}

ASTNode* parse_send_statement(Parser* parser) {
    advance_token(parser); // send
    expect_token(parser, TOKEN_LEFT_PAREN);
    
    ASTNode* actor_ref = parse_expression(parser);
    if (!actor_ref) return NULL;
    
    expect_token(parser, TOKEN_COMMA);
    ASTNode* message = parse_expression(parser);
    if (!message) return NULL;
    
    expect_token(parser, TOKEN_RIGHT_PAREN);
    expect_token(parser, TOKEN_SEMICOLON);
    
    ASTNode* send_stmt = create_ast_node(AST_SEND_STATEMENT, NULL, 0, 0);
    add_child(send_stmt, actor_ref);
    add_child(send_stmt, message);
    
    return send_stmt;
}

ASTNode* parse_spawn_actor_statement(Parser* parser) {
    advance_token(parser); // spawn_actor
    expect_token(parser, TOKEN_LEFT_PAREN);
    
    ASTNode* actor_type = parse_expression(parser);
    if (!actor_type) return NULL;
    
    expect_token(parser, TOKEN_RIGHT_PAREN);
    expect_token(parser, TOKEN_SEMICOLON);
    
    ASTNode* spawn_stmt = create_ast_node(AST_SPAWN_ACTOR_STATEMENT, NULL, 0, 0);
    add_child(spawn_stmt, actor_type);
    
    return spawn_stmt;
}

ASTNode* parse_block(Parser* parser) {
    expect_token(parser, TOKEN_LEFT_BRACE);
    
    ASTNode* block = create_ast_node(AST_BLOCK, NULL, 0, 0);
    
    while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
        int start_token = parser->current_token;
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            add_child(block, stmt);
        } else {
            // Prevent infinite loops on unexpected tokens inside blocks.
            parser_error(parser, "Expected statement in block");
            if (parser->current_token == start_token) {
                advance_token(parser);
            }
        }
        
        if (is_at_end(parser)) break;
    }
    
    return block;
}

ASTNode* parse_actor_definition(Parser* parser) {
    advance_token(parser); // actor
    Token* name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name) return NULL;
    
    expect_token(parser, TOKEN_LEFT_BRACE);
    
    ASTNode* actor = create_ast_node(AST_ACTOR_DEFINITION, name->value, name->line, name->column);
    
    int iteration_count = 0;
    const int MAX_ACTOR_BODY = 1000;
    
    while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
        if (++iteration_count > MAX_ACTOR_BODY) {
            parser_message(parser, "Error: Too many statements in actor definition (max %d)");
            break;
        }
        
        if (is_at_end(parser)) {
            parser_message(parser, "Error: Unexpected end of file in actor definition");
            break;
        }
        
        if (match_token(parser, TOKEN_STATE)) {
            // Check if there's an explicit type or Python-style
            Token* next_tok = peek_token(parser);
            ASTNode* state_decl = NULL;
            
            if (next_tok && (next_tok->type == TOKEN_INT || next_tok->type == TOKEN_FLOAT || 
                            next_tok->type == TOKEN_STRING || next_tok->type == TOKEN_BOOL)) {
                // Explicit type: state int count = 0
                state_decl = parse_variable_declaration_with_semicolon(parser, false);
            } else if (next_tok && next_tok->type == TOKEN_IDENTIFIER) {
                // Python-style: state count = 0 (no semicolon required in actor)
                Token* name = expect_token(parser, TOKEN_IDENTIFIER);
                if (name) {
                    state_decl = create_ast_node(AST_VARIABLE_DECLARATION, name->value, name->line, name->column);
                    state_decl->node_type = create_type(TYPE_UNKNOWN);
                    
                    if (match_token(parser, TOKEN_ASSIGN)) {
                        ASTNode* value = parse_expression(parser);
                        if (value) {
                            add_child(state_decl, value);
                        }
                    }
                }
            }
            
            if (state_decl) {
                state_decl->type = AST_STATE_DECLARATION;
                add_child(actor, state_decl);
                // Consume optional semicolon after state declaration
                match_token(parser, TOKEN_SEMICOLON);
            }
        } else if (match_token(parser, TOKEN_RECEIVE)) {
            ASTNode* receive_stmt = parse_receive_statement(parser);
            if (receive_stmt) {
                add_child(actor, receive_stmt);
            }
        } else {
            ASTNode* stmt = parse_statement(parser);
            if (stmt) {
                add_child(actor, stmt);
            } else {
                // If we can't parse a statement, advance to avoid infinite loop
                Token* tok = peek_token(parser);
                fprintf(stderr, "Warning: Skipping unexpected token in actor body: %s (type=%d)\n",
                        tok ? tok->value : "NULL", tok ? tok->type : -1);
                advance_token(parser);
            }
        }
    }
    
    return actor;
}

ASTNode* parse_receive_statement(Parser* parser) {
    // Note: TOKEN_RECEIVE has already been consumed by the caller
    Token* current = peek_token(parser);
    if (!current) return NULL;
    
    // Check for V1 syntax: receive(msg) { ... }
    if (current->type == TOKEN_LEFT_PAREN) {
        // V1 syntax - backward compatibility
        expect_token(parser, TOKEN_LEFT_PAREN);
        Token* param = expect_token(parser, TOKEN_IDENTIFIER);
        if (!param) return NULL;
        expect_token(parser, TOKEN_RIGHT_PAREN);
        
        ASTNode* body = parse_block(parser);
        if (!body) return NULL;
        
        ASTNode* receive_stmt = create_ast_node(AST_RECEIVE_STATEMENT, param->value, param->line, param->column);
        add_child(receive_stmt, body);
        
        return receive_stmt;
    }
    
    // V2 syntax: receive { Pattern -> block, ... }
    if (current->type != TOKEN_LEFT_BRACE) {
        parser_error(parser, "Expected '(' or '{' after 'receive'");
        return NULL;
    }
    
    expect_token(parser, TOKEN_LEFT_BRACE);
    
    ASTNode* receive_stmt = create_ast_node(AST_RECEIVE_STATEMENT, NULL, current->line, current->column);
    
    // Parse receive arms (pattern matching)
    while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
        if (is_at_end(parser)) {
            parser_message(parser, "Error: Unexpected end in receive statement");
            return NULL;
        }
        
        // Parse pattern (message pattern or wildcard)
        ASTNode* pattern = NULL;
        Token* pattern_token = peek_token(parser);
        
        if (pattern_token && pattern_token->type == TOKEN_IDENTIFIER && 
            strcmp(pattern_token->value, "_") == 0) {
            // Wildcard pattern: _
            advance_token(parser);
            pattern = create_ast_node(AST_WILDCARD_PATTERN, "_", pattern_token->line, pattern_token->column);
        } else {
            // Message pattern: MessageName { fields }
            pattern = parse_message_pattern(parser);
            if (!pattern) break;
        }
        
        expect_token(parser, TOKEN_ARROW);
        
        // Parse arm body
        ASTNode* arm_body = NULL;
        Token* body_start = peek_token(parser);
        
        if (body_start && body_start->type == TOKEN_LEFT_BRACE) {
            arm_body = parse_block(parser);
        } else {
            // Single expression or statement
            ASTNode* stmt = parse_statement(parser);
            if (stmt) {
                arm_body = create_ast_node(AST_BLOCK, NULL, body_start->line, body_start->column);
                add_child(arm_body, stmt);
            }
        }
        
        if (!arm_body || !pattern || !pattern_token) break;

        // Create receive arm node
        ASTNode* arm = create_ast_node(AST_RECEIVE_ARM, NULL, pattern_token->line, pattern_token->column);
        add_child(arm, pattern);
        add_child(arm, arm_body);
        add_child(receive_stmt, arm);
        
        // Optional comma between arms
        if (peek_token(parser) && peek_token(parser)->type == TOKEN_COMMA) {
            advance_token(parser);
        }
    }
    
    return receive_stmt;
}

// Parse extern C function declaration
// Syntax: extern name(param: type, ...) -> return_type
//         extern name(param: type, ...)   (void return)
ASTNode* parse_extern_declaration(Parser* parser) {
    Token* extern_token = expect_token(parser, TOKEN_EXTERN);
    if (!extern_token) return NULL;

    Token* name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name) return NULL;

    expect_token(parser, TOKEN_LEFT_PAREN);

    ASTNode* extern_func = create_ast_node(AST_EXTERN_FUNCTION, name->value,
                                           extern_token->line, extern_token->column);

    // Parse parameters with types: param: type, param2: type
    if (!match_token(parser, TOKEN_RIGHT_PAREN)) {
        do {
            Token* param_name = expect_token(parser, TOKEN_IDENTIFIER);
            if (!param_name) break;

            ASTNode* param = create_ast_node(AST_IDENTIFIER, param_name->value,
                                            param_name->line, param_name->column);

            // Require type annotation for extern: param: type
            if (match_token(parser, TOKEN_COLON)) {
                Type* param_type = parse_type(parser);
                if (param_type) {
                    param->node_type = param_type;
                } else {
                    parser_error(parser, "Expected type after ':' in extern parameter");
                    param->node_type = create_type(TYPE_INT);  // Fallback for error recovery
                }
            } else {
                // Type annotation required for extern functions
                parser_error(parser, "Type annotation required for extern parameter (use param: type)");
                param->node_type = create_type(TYPE_INT);  // Fallback for error recovery
            }

            add_child(extern_func, param);
        } while (match_token(parser, TOKEN_COMMA));

        expect_token(parser, TOKEN_RIGHT_PAREN);
    }

    // Parse optional return type: -> type
    if (match_token(parser, TOKEN_ARROW)) {
        Type* return_type = parse_type(parser);
        if (return_type) {
            extern_func->node_type = return_type;
        } else {
            extern_func->node_type = create_type(TYPE_INT);
        }
    } else {
        // No return type = void
        extern_func->node_type = create_type(TYPE_VOID);
    }

    return extern_func;
}

ASTNode* parse_function_definition(Parser* parser) {
    // Erlang-style pattern matching functions!
    // Syntax: 
    //   fib(0) -> 1
    //   fib(1) -> 1
    //   fib(n) when n > 1 -> fib(n-1) + fib(n-2)
    // Or traditional:
    //   name(param1, param2) { ... }
    
    Token* name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name) return NULL;
    
    expect_token(parser, TOKEN_LEFT_PAREN);
    
    ASTNode* func = create_ast_node(AST_FUNCTION_DEFINITION, name->value, name->line, name->column);
    
    // Parse parameters - can be patterns!
    if (!match_token(parser, TOKEN_RIGHT_PAREN)) {
        do {
            ASTNode* param = parse_pattern(parser);
            if (!param) break;
            add_child(func, param);
        } while (match_token(parser, TOKEN_COMMA));
        
        expect_token(parser, TOKEN_RIGHT_PAREN);
    }
    
    // Check for guard clause: when condition
    ASTNode* guard = NULL;
    if (match_token(parser, TOKEN_WHEN)) {
        ASTNode* guard_expr = parse_expression(parser);
        if (guard_expr) {
            guard = create_ast_node(AST_GUARD_CLAUSE, NULL, 0, 0);
            add_child(guard, guard_expr);
            add_child(func, guard);
        }
    }
    
    // Optional return type annotation: -> type (before arrow body)
    Type* return_type = create_type(TYPE_UNKNOWN);
    Token* next = peek_token(parser);
    if (next && next->type == TOKEN_COLON) {
        advance_token(parser);  // consume ':'
        Type* parsed_type = parse_type(parser);
        if (parsed_type) {
            free_type(return_type);
            return_type = parsed_type;
        }
        next = peek_token(parser);
    }
    func->node_type = return_type;
    
    // Check for Erlang-style arrow body: -> expr
    if (match_token(parser, TOKEN_ARROW)) {
        ASTNode* body_expr = parse_expression(parser);
        if (body_expr) {
            // Wrap in a return statement
            ASTNode* return_stmt = create_ast_node(AST_RETURN_STATEMENT, NULL, 0, 0);
            add_child(return_stmt, body_expr);
            
            ASTNode* body_block = create_ast_node(AST_BLOCK, NULL, 0, 0);
            add_child(body_block, return_stmt);
            add_child(func, body_block);
        }
    } else {
        // Traditional block body
        ASTNode* body = parse_block(parser);
        if (body) {
            add_child(func, body);
        }
    }
    
    return func;
}

// Parse pattern for function parameters and match expressions
// Supports: literals (0, "foo"), variables (n), wildcards (_), structs
ASTNode* parse_pattern(Parser* parser) {
    Token* token = peek_token(parser);
    if (!token) return NULL;
    
    switch (token->type) {
        case TOKEN_NUMBER: {
            advance_token(parser);
            ASTNode* pattern = create_ast_node(AST_PATTERN_LITERAL, token->value, 
                                              token->line, token->column);
            pattern->node_type = create_type(TYPE_INT);
            return pattern;
        }
        
        case TOKEN_STRING_LITERAL: {
            advance_token(parser);
            ASTNode* pattern = create_ast_node(AST_PATTERN_LITERAL, token->value,
                                              token->line, token->column);
            pattern->node_type = create_type(TYPE_STRING);
            return pattern;
        }
        
        case TOKEN_TRUE:
        case TOKEN_FALSE: {
            advance_token(parser);
            ASTNode* pattern = create_ast_node(AST_PATTERN_LITERAL, token->value,
                                              token->line, token->column);
            pattern->node_type = create_type(TYPE_BOOL);
            return pattern;
        }
        
        case TOKEN_IDENTIFIER: {
            // Check if it's a wildcard _
            if (strcmp(token->value, "_") == 0) {
                advance_token(parser);
                ASTNode* pattern = create_ast_node(AST_PATTERN_LITERAL, "_", 
                                                  token->line, token->column);
                pattern->node_type = create_type(TYPE_WILDCARD);
                return pattern;
            }
            
            // Check if it's a struct pattern: Point{x: 0, y: 0}
            Token* next = peek_ahead(parser, 1);
            if (next && next->type == TOKEN_LEFT_BRACE) {
                return parse_struct_pattern(parser);
            }
            
            // Regular variable pattern
            advance_token(parser);
            ASTNode* pattern = create_ast_node(AST_PATTERN_VARIABLE, token->value,
                                              token->line, token->column);
            
            // Optional type annotation: param: type
            if (match_token(parser, TOKEN_COLON)) {
                Type* param_type = parse_type(parser);
                if (param_type) {
                    pattern->node_type = param_type;
                } else {
                    pattern->node_type = create_type(TYPE_UNKNOWN);
                }
            } else {
                pattern->node_type = create_type(TYPE_UNKNOWN);
            }
            
            return pattern;
        }
        
        case TOKEN_LEFT_BRACKET: {
            // List pattern: [], [x], [H|T]
            return parse_list_pattern(parser);
        }
        
        default:
            // Fallback to expression
            return parse_expression(parser);
    }
}

// Parse struct pattern: Point{x: 0, y: _}
ASTNode* parse_struct_pattern(Parser* parser) {
    Token* name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name) return NULL;
    
    expect_token(parser, TOKEN_LEFT_BRACE);
    
    ASTNode* pattern = create_ast_node(AST_PATTERN_STRUCT, name->value, 
                                      name->line, name->column);
    
    if (!match_token(parser, TOKEN_RIGHT_BRACE)) {
        do {
            Token* field = expect_token(parser, TOKEN_IDENTIFIER);
            if (!field) break;
            
            expect_token(parser, TOKEN_COLON);
            
            ASTNode* field_pattern = parse_pattern(parser);
            if (field_pattern) {
                // Store field name in pattern
                ASTNode* field_node = create_ast_node(AST_ASSIGNMENT, field->value,
                                                     field->line, field->column);
                add_child(field_node, field_pattern);
                add_child(pattern, field_node);
            }
        } while (match_token(parser, TOKEN_COMMA));
        
        expect_token(parser, TOKEN_RIGHT_BRACE);
    }
    
    return pattern;
}

// Parse list pattern: [], [x], [x, y], [H|T]
ASTNode* parse_list_pattern(Parser* parser) {
    Token* bracket = expect_token(parser, TOKEN_LEFT_BRACKET);
    if (!bracket) return NULL;
    
    // Empty list: []
    if (match_token(parser, TOKEN_RIGHT_BRACKET)) {
        ASTNode* pattern = create_ast_node(AST_PATTERN_LIST, "[]", 
                                          bracket->line, bracket->column);
        pattern->node_type = create_array_type(create_type(TYPE_UNKNOWN), -1);
        return pattern;
    }
    
    // Parse first element
    ASTNode* first = parse_pattern(parser);
    if (!first) return NULL;
    
    // Check for cons pattern: [H|T]
    if (match_token(parser, TOKEN_PIPE)) {
        ASTNode* tail = parse_pattern(parser);
        if (!tail) return NULL;
        
        expect_token(parser, TOKEN_RIGHT_BRACKET);
        
        // Create cons pattern node
        ASTNode* cons = create_ast_node(AST_PATTERN_CONS, "[|]", 
                                       bracket->line, bracket->column);
        cons->node_type = create_array_type(create_type(TYPE_UNKNOWN), -1);
        add_child(cons, first);   // Head
        add_child(cons, tail);    // Tail
        return cons;
        }
    
    // Regular list pattern: [x, y, z]
    ASTNode* list = create_ast_node(AST_PATTERN_LIST, "[]",
                                   bracket->line, bracket->column);
    list->node_type = create_array_type(create_type(TYPE_UNKNOWN), -1);
    add_child(list, first);
    
    while (match_token(parser, TOKEN_COMMA)) {
        ASTNode* elem = parse_pattern(parser);
        if (!elem) break;
        add_child(list, elem);
    }
    
    expect_token(parser, TOKEN_RIGHT_BRACKET);
    return list;
}

ASTNode* parse_main_function(Parser* parser) {
    advance_token(parser); // main
    expect_token(parser, TOKEN_LEFT_PAREN);
    expect_token(parser, TOKEN_RIGHT_PAREN);

    ASTNode* main = create_ast_node(AST_MAIN_FUNCTION, "main", 0, 0);
    main->node_type = create_type(TYPE_VOID);

    ASTNode* body = parse_block(parser);
    if (body) {
        add_child(main, body);
    }

    return main;
}

ASTNode* parse_struct_definition(Parser* parser) {
    Token* struct_token = advance_token(parser); // consume 'struct'
    
    Token* name_token = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name_token) return NULL;
    
    ASTNode* struct_def = create_ast_node(AST_STRUCT_DEFINITION, name_token->value, 
                                         struct_token->line, struct_token->column);
    
    if (!expect_token(parser, TOKEN_LEFT_BRACE)) return NULL;
    
    // Parse fields (types optional - will be inferred!)
    while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
        if (is_at_end(parser)) {
            parser_error(parser, "Unexpected end of struct definition");
            return NULL;
        }
        
        Token* field_name = expect_token(parser, TOKEN_IDENTIFIER);
        if (!field_name) return NULL;
        
        // Create field node
        ASTNode* field = create_ast_node(AST_STRUCT_FIELD, field_name->value, 
                                        field_name->line, field_name->column);
        
        // Optional type annotation: name: type
        if (match_token(parser, TOKEN_COLON)) {
            Type* field_type = parse_type(parser);
            if (field_type) {
                field->node_type = field_type;
            }
        } else {
            // No type - will be inferred from usage
            field->node_type = create_type(TYPE_UNKNOWN);
        }
        
        add_child(struct_def, field);
        
        // Optional comma or semicolon
        if (!match_token(parser, TOKEN_COMMA)) {
            match_token(parser, TOKEN_SEMICOLON);  // Optional semicolon
        }
    }
    
    return struct_def;
}

ASTNode* parse_program(Parser* parser) {
    ASTNode* program = create_ast_node(AST_PROGRAM, NULL, 0, 0);
    
    int safety_counter = 0;
    // Safety limit to prevent infinite loops on malformed input
    const int MAX_ITERATIONS = 10000;
    
    while (!is_at_end(parser) && safety_counter < MAX_ITERATIONS) {
        safety_counter++;
        
        Token* token = peek_token(parser);
        if (!token) break;
        
        ASTNode* node = NULL;
        
        switch (token->type) {
            case TOKEN_MODULE:
                node = parse_module_declaration(parser);
                break;
            case TOKEN_IMPORT:
                node = parse_import_statement(parser);
                break;
            case TOKEN_EXPORT:
                node = parse_export_statement(parser);
                break;
            case TOKEN_ACTOR:
                node = parse_actor_definition(parser);
                break;
            case TOKEN_MESSAGE_KEYWORD:
                node = parse_message_definition(parser);
                break;
            case TOKEN_FUNC:
                // 'func' keyword is optional but still supported
                advance_token(parser);
                node = parse_function_definition(parser);
                break;
            case TOKEN_STRUCT:
                node = parse_struct_definition(parser);
                break;
            case TOKEN_EXTERN:
                node = parse_extern_declaration(parser);
                break;
            case TOKEN_MAIN:
                node = parse_main_function(parser);
                break;
            case TOKEN_IDENTIFIER: {
                // Check if this is a function: identifier(...)
                Token* next = parser->tokens[parser->current_token + 1];
                if (next && next->type == TOKEN_LEFT_PAREN) {
                    // Function without 'func' keyword
                    node = parse_function_definition(parser);
                } else {
                    parser_error(parser, "Unexpected identifier at top level (expected actor, struct, or function)");
                    advance_token(parser);
                    continue;
                }
                break;
            }
            default:
                parser_error(parser, "Expected actor, struct, function, or main");
                advance_token(parser);
                continue;
        }
        
        if (node) {
            add_child(program, node);
        }
    }
    
    if (safety_counter >= MAX_ITERATIONS) {
        parser_message(parser, "Error: Parser safety limit reached - possible infinite loop");
        return NULL;
    }
    
    return program;
}
