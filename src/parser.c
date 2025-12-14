#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

Parser* create_parser(Token** tokens, int token_count) {
    Parser* parser = malloc(sizeof(Parser));
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->current_token = 0;
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

Token* advance_token(Parser* parser) {
    if (parser->current_token >= parser->token_count) {
        return NULL;
    }
    return parser->tokens[parser->current_token++];
}

Token* expect_token(Parser* parser, TokenType expected) {
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

int match_token(Parser* parser, TokenType type) {
    if (is_at_end(parser)) return 0;
    if (peek_token(parser)->type == type) {
        advance_token(parser);
        return 1;
    }
    return 0;
}

void parser_error(Parser* parser, const char* message) {
    Token* token = peek_token(parser);
    if (token) {
        fprintf(stderr, "Parse error at line %d, column %d: %s\n", 
                token->line, token->column, message);
    } else {
        fprintf(stderr, "Parse error at end of file: %s\n", message);
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
            
        case TOKEN_IDENTIFIER:
            return create_identifier_node(advance_token(parser));
            
        case TOKEN_LEFT_PAREN: {
            advance_token(parser);
            ASTNode* expr = parse_expression(parser);
            if (!expect_token(parser, TOKEN_RIGHT_PAREN)) return NULL;
            return expr;
        }
        
        case TOKEN_SELF:
            advance_token(parser);
            return create_ast_node(AST_ACTOR_REF, "self", token->line, token->column);
            
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
    
    while (1) {
        Token* operator = peek_token(parser);
        if (!operator) break;
        
        int op_precedence = get_operator_precedence(operator->type);
        if (op_precedence <= precedence) break;
        
        advance_token(parser);
        ASTNode* right = parse_binary_expression(parser, op_precedence);
        if (!right) return NULL;
        
        left = create_binary_expression(left, right, operator);
    }
    
    return left;
}

// Parse postfix expressions like i++ / i--
static ASTNode* parse_postfix_expression(Parser* parser) {
    ASTNode* expr = parse_primary_expression(parser);
    if (!expr) return NULL;
    
    while (1) {
        Token* op = peek_token(parser);
        if (!op) break;
        
        if (op->type == TOKEN_INCREMENT || op->type == TOKEN_DECREMENT) {
            advance_token(parser);
            // We currently represent both prefix and postfix as AST_UNARY_EXPRESSION.
            // Codegen emits prefix form (e.g. ++i), which is acceptable for now.
            expr = create_unary_expression(expr, op);
            continue;
        }
        
        break;
    }
    
    return expr;
}

ASTNode* parse_unary_expression(Parser* parser) {
    Token* operator = peek_token(parser);
    if (!operator) return NULL;
    
    if (operator->type == TOKEN_NOT || operator->type == TOKEN_MINUS ||
        operator->type == TOKEN_INCREMENT || operator->type == TOKEN_DECREMENT) {
        advance_token(parser);
        ASTNode* operand = parse_unary_expression(parser);
        if (!operand) return NULL;
        return create_unary_expression(operand, operator);
    }
    
    return parse_postfix_expression(parser);
}

int get_operator_precedence(TokenType type) {
    switch (type) {
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
        default: return 0;
    }
}

ASTNode* parse_statement(Parser* parser) {
    Token* token = peek_token(parser);
    if (!token) return NULL;
    
    switch (token->type) {
        case TOKEN_LET:
        case TOKEN_VAR:
        case TOKEN_INT:
        case TOKEN_STRING:
        case TOKEN_FLOAT:
        case TOKEN_BOOL:
            return parse_variable_declaration(parser);
            
        case TOKEN_IF:
            return parse_if_statement(parser);
            
        case TOKEN_FOR:
            return parse_for_loop(parser);
            
        case TOKEN_WHILE:
            return parse_while_loop(parser);
            
        case TOKEN_SWITCH:
            return parse_switch_statement(parser);
            
        case TOKEN_RETURN:
            return parse_return_statement(parser);
            
        case TOKEN_BREAK:
            advance_token(parser);
            expect_token(parser, TOKEN_SEMICOLON);
            return create_ast_node(AST_BREAK_STATEMENT, NULL, token->line, token->column);
            
        case TOKEN_CONTINUE:
            advance_token(parser);
            expect_token(parser, TOKEN_SEMICOLON);
            return create_ast_node(AST_CONTINUE_STATEMENT, NULL, token->line, token->column);
            
        case TOKEN_PRINT:
            return parse_print_statement(parser);
            
        case TOKEN_SEND:
            return parse_send_statement(parser);
            
        case TOKEN_SPAWN_ACTOR:
            return parse_spawn_actor_statement(parser);
            
        case TOKEN_LEFT_BRACE:
            return parse_block(parser);
            
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
    advance_token(parser); // switch
    ASTNode* expression = parse_expression(parser);
    if (!expression) return NULL;
    
    expect_token(parser, TOKEN_LEFT_BRACE);
    
    ASTNode* switch_stmt = create_ast_node(AST_SWITCH_STATEMENT, NULL, 0, 0);
    add_child(switch_stmt, expression);
    
    while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
        ASTNode* case_stmt = parse_case_statement(parser);
        if (case_stmt) {
            add_child(switch_stmt, case_stmt);
        }
    }
    
    return switch_stmt;
}

ASTNode* parse_case_statement(Parser* parser) {
    if (match_token(parser, TOKEN_DEFAULT)) {
        expect_token(parser, TOKEN_COLON);
        ASTNode* body = parse_statement(parser);
        ASTNode* case_stmt = create_ast_node(AST_CASE_STATEMENT, "default", 0, 0);
        add_child(case_stmt, body);
        return case_stmt;
    }
    
    if (match_token(parser, TOKEN_CASE)) {
        ASTNode* value = parse_expression(parser);
        expect_token(parser, TOKEN_COLON);
        ASTNode* body = parse_statement(parser);
        
        ASTNode* case_stmt = create_ast_node(AST_CASE_STATEMENT, NULL, 0, 0);
        add_child(case_stmt, value);
        add_child(case_stmt, body);
        return case_stmt;
    }
    
    return NULL;
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
    
    while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
        if (match_token(parser, TOKEN_STATE)) {
            ASTNode* state_decl = parse_variable_declaration(parser);
            if (state_decl) {
                state_decl->type = AST_STATE_DECLARATION;
                add_child(actor, state_decl);
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
            }
        }
    }
    
    return actor;
}

ASTNode* parse_receive_statement(Parser* parser) {
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

ASTNode* parse_function_definition(Parser* parser) {
    advance_token(parser); // func
    Token* name = expect_token(parser, TOKEN_IDENTIFIER);
    if (!name) return NULL;
    
    expect_token(parser, TOKEN_LEFT_PAREN);
    
    ASTNode* func = create_ast_node(AST_FUNCTION_DEFINITION, name->value, name->line, name->column);
    
    // Parse parameters
    while (!match_token(parser, TOKEN_RIGHT_PAREN)) {
        Type* param_type = parse_type(parser);
        Token* param_name = expect_token(parser, TOKEN_IDENTIFIER);
        if (param_type && param_name) {
            ASTNode* param = create_ast_node(AST_VARIABLE_DECLARATION, param_name->value, param_name->line, param_name->column);
            param->node_type = param_type;
            add_child(func, param);
        }
        
        if (!match_token(parser, TOKEN_COMMA)) break;
    }
    
    // Parse return type
    Type* return_type = create_type(TYPE_VOID);
    if (match_token(parser, TOKEN_COLON)) {
        Type* parsed_type = parse_type(parser);
        if (parsed_type) {
            free_type(return_type);
            return_type = parsed_type;
        }
    }
    
    func->node_type = return_type;
    
    ASTNode* body = parse_block(parser);
    if (body) {
        add_child(func, body);
    }
    
    return func;
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

ASTNode* parse_program(Parser* parser) {
    ASTNode* program = create_ast_node(AST_PROGRAM, NULL, 0, 0);
    
    int safety_counter = 0;
    const int MAX_ITERATIONS = 10000;
    
    while (!is_at_end(parser) && safety_counter < MAX_ITERATIONS) {
        safety_counter++;
        
        Token* token = peek_token(parser);
        if (!token) break;
        
        ASTNode* node = NULL;
        
        switch (token->type) {
            case TOKEN_ACTOR:
                node = parse_actor_definition(parser);
                break;
            case TOKEN_FUNC:
                node = parse_function_definition(parser);
                break;
            case TOKEN_MAIN:
                node = parse_main_function(parser);
                break;
            default:
                parser_error(parser, "Expected actor, func, or main");
                advance_token(parser);
                continue;
        }
        
        if (node) {
            add_child(program, node);
        }
    }
    
    if (safety_counter >= MAX_ITERATIONS) {
        fprintf(stderr, "ERROR: Parser safety limit reached - possible infinite loop\n");
        return NULL;
    }
    
    return program;
}
