#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tokens.h"

#define MAX_IDENTIFIER_LENGTH 256

static const char* source;
static int source_length;
static int current_pos;
static int current_line;
static int current_column;

void lexer_init(const char* src) {
    source = src;
    source_length = strlen(src);
    current_pos = 0;
    current_line = 1;
    current_column = 1;
}

char peek() {
    if (current_pos >= source_length) return '\0';
    return source[current_pos];
}

char advance() {
    if (current_pos >= source_length) return '\0';
    char c = source[current_pos++];
    if (c == '\n') {
        current_line++;
        current_column = 1;
    } else {
        current_column++;
    }
    return c;
}

void skip_whitespace() {
    while (current_pos < source_length) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            advance();
        } else {
            break;
        }
    }
}

void skip_comment() {
    if (peek() == '/' && current_pos + 1 < source_length && source[current_pos + 1] == '/') {
        // Single line comment
        while (current_pos < source_length && peek() != '\n') {
            advance();
        }
    } else if (peek() == '/' && current_pos + 1 < source_length && source[current_pos + 1] == '*') {
        // Multi-line comment
        advance(); // skip /
        advance(); // skip *
        while (current_pos < source_length) {
            if (peek() == '*' && current_pos + 1 < source_length && source[current_pos + 1] == '/') {
                advance(); // skip *
                advance(); // skip /
                break;
            }
            advance();
        }
    }
}

Token* read_string() {
    advance(); // skip opening quote
    char* buffer = malloc(MAX_IDENTIFIER_LENGTH);
    int i = 0;
    
    while (current_pos < source_length && peek() != '"') {
        if (peek() == '\\') {
            advance(); // skip backslash
            char c = advance();
            switch (c) {
                case 'n': buffer[i++] = '\n'; break;
                case 't': buffer[i++] = '\t'; break;
                case 'r': buffer[i++] = '\r'; break;
                case '\\': buffer[i++] = '\\'; break;
                case '"': buffer[i++] = '"'; break;
                default: buffer[i++] = c; break;
            }
        } else {
            buffer[i++] = advance();
        }
    }
    
    if (peek() == '"') {
        advance(); // skip closing quote
    }
    
    buffer[i] = '\0';
    return create_token(TOKEN_STRING_LITERAL, buffer, current_line, current_column);
}

Token* read_number() {
    char* buffer = malloc(MAX_IDENTIFIER_LENGTH);
    int i = 0;
    
    while (current_pos < source_length && (isdigit(peek()) || peek() == '.')) {
        buffer[i++] = advance();
    }
    
    buffer[i] = '\0';
    return create_token(TOKEN_NUMBER, buffer, current_line, current_column);
}

Token* read_identifier() {
    char* buffer = malloc(MAX_IDENTIFIER_LENGTH);
    int i = 0;
    
    while (current_pos < source_length && (isalnum(peek()) || peek() == '_')) {
        buffer[i++] = advance();
    }
    
    buffer[i] = '\0';
    
    // Check if it's a keyword
    if (strcmp(buffer, "actor") == 0) return create_token(TOKEN_ACTOR, buffer, current_line, current_column);
    if (strcmp(buffer, "main") == 0) return create_token(TOKEN_MAIN, buffer, current_line, current_column);
    if (strcmp(buffer, "func") == 0) return create_token(TOKEN_FUNC, buffer, current_line, current_column);
    if (strcmp(buffer, "let") == 0) return create_token(TOKEN_LET, buffer, current_line, current_column);
    if (strcmp(buffer, "var") == 0) return create_token(TOKEN_VAR, buffer, current_line, current_column);
    if (strcmp(buffer, "if") == 0) return create_token(TOKEN_IF, buffer, current_line, current_column);
    if (strcmp(buffer, "else") == 0) return create_token(TOKEN_ELSE, buffer, current_line, current_column);
    if (strcmp(buffer, "for") == 0) return create_token(TOKEN_FOR, buffer, current_line, current_column);
    if (strcmp(buffer, "while") == 0) return create_token(TOKEN_WHILE, buffer, current_line, current_column);
    if (strcmp(buffer, "switch") == 0) return create_token(TOKEN_SWITCH, buffer, current_line, current_column);
    if (strcmp(buffer, "case") == 0) return create_token(TOKEN_CASE, buffer, current_line, current_column);
    if (strcmp(buffer, "default") == 0) return create_token(TOKEN_DEFAULT, buffer, current_line, current_column);
    if (strcmp(buffer, "break") == 0) return create_token(TOKEN_BREAK, buffer, current_line, current_column);
    if (strcmp(buffer, "continue") == 0) return create_token(TOKEN_CONTINUE, buffer, current_line, current_column);
    if (strcmp(buffer, "return") == 0) return create_token(TOKEN_RETURN, buffer, current_line, current_column);
    if (strcmp(buffer, "match") == 0) return create_token(TOKEN_MATCH, buffer, current_line, current_column);
    if (strcmp(buffer, "receive") == 0) return create_token(TOKEN_RECEIVE, buffer, current_line, current_column);
    if (strcmp(buffer, "send") == 0) return create_token(TOKEN_SEND, buffer, current_line, current_column);
    if (strcmp(buffer, "spawn_actor") == 0) return create_token(TOKEN_SPAWN_ACTOR, buffer, current_line, current_column);
    if (strcmp(buffer, "spawn") == 0) return create_token(TOKEN_SPAWN, buffer, current_line, current_column);
    if (strcmp(buffer, "self") == 0) return create_token(TOKEN_SELF, buffer, current_line, current_column);
    if (strcmp(buffer, "state") == 0) return create_token(TOKEN_STATE, buffer, current_line, current_column);
    if (strcmp(buffer, "struct") == 0) return create_token(TOKEN_STRUCT, buffer, current_line, current_column);
    if (strcmp(buffer, "int") == 0) return create_token(TOKEN_INT, buffer, current_line, current_column);
    if (strcmp(buffer, "float") == 0) return create_token(TOKEN_FLOAT, buffer, current_line, current_column);
    if (strcmp(buffer, "bool") == 0) return create_token(TOKEN_BOOL, buffer, current_line, current_column);
    if (strcmp(buffer, "string") == 0) return create_token(TOKEN_STRING, buffer, current_line, current_column);
    if (strcmp(buffer, "ActorRef") == 0) return create_token(TOKEN_ACTOR_REF, buffer, current_line, current_column);
    if (strcmp(buffer, "Message") == 0) return create_token(TOKEN_MESSAGE, buffer, current_line, current_column);
    if (strcmp(buffer, "true") == 0) return create_token(TOKEN_TRUE, buffer, current_line, current_column);
    if (strcmp(buffer, "false") == 0) return create_token(TOKEN_FALSE, buffer, current_line, current_column);
    if (strcmp(buffer, "print") == 0) return create_token(TOKEN_PRINT, buffer, current_line, current_column);
    
    return create_token(TOKEN_IDENTIFIER, buffer, current_line, current_column);
}

Token* next_token() {
    skip_whitespace();
    
    if (current_pos >= source_length) {
        return create_token(TOKEN_EOF, NULL, current_line, current_column);
    }
    
    char c = peek();
    
    // Handle comments
    if (c == '/') {
        skip_comment();
        return next_token(); // Recursively get next token after comment
    }
    
    // Handle string literals
    if (c == '"') {
        return read_string();
    }
    
    // Handle numbers
    if (isdigit(c)) {
        return read_number();
    }
    
    // Handle identifiers and keywords
    if (isalpha(c) || c == '_') {
        return read_identifier();
    }
    
    // Handle operators and delimiters
    switch (c) {
        case '+': 
            advance();
            if (peek() == '+') {
                advance();
                return create_token(TOKEN_INCREMENT, "++", current_line, current_column);
            }
            return create_token(TOKEN_PLUS, "+", current_line, current_column);
        case '-': 
            advance();
            if (peek() == '>') {
                advance();
                return create_token(TOKEN_ARROW, "->", current_line, current_column);
            }
            if (peek() == '-') {
                advance();
                return create_token(TOKEN_DECREMENT, "--", current_line, current_column);
            }
            return create_token(TOKEN_MINUS, "-", current_line, current_column);
        case '*': advance(); return create_token(TOKEN_MULTIPLY, "*", current_line, current_column);
        case '/': advance(); return create_token(TOKEN_DIVIDE, "/", current_line, current_column);
        case '%': advance(); return create_token(TOKEN_MODULO, "%", current_line, current_column);
        case '=':
            advance();
            if (peek() == '=') {
                advance();
                return create_token(TOKEN_EQUALS, "==", current_line, current_column);
            }
            return create_token(TOKEN_ASSIGN, "=", current_line, current_column);
        case '!':
            advance();
            if (peek() == '=') {
                advance();
                return create_token(TOKEN_NOT_EQUALS, "!=", current_line, current_column);
            }
            return create_token(TOKEN_NOT, "!", current_line, current_column);
        case '<':
            advance();
            if (peek() == '=') {
                advance();
                return create_token(TOKEN_LESS_EQUAL, "<=", current_line, current_column);
            }
            return create_token(TOKEN_LESS, "<", current_line, current_column);
        case '>':
            advance();
            if (peek() == '=') {
                advance();
                return create_token(TOKEN_GREATER_EQUAL, ">=", current_line, current_column);
            }
            return create_token(TOKEN_GREATER, ">", current_line, current_column);
        case '&':
            advance();
            if (peek() == '&') {
                advance();
                return create_token(TOKEN_AND, "&&", current_line, current_column);
            }
            return create_token(TOKEN_ERROR, "&", current_line, current_column);
        case '|':
            advance();
            if (peek() == '|') {
                advance();
                return create_token(TOKEN_OR, "||", current_line, current_column);
            }
            return create_token(TOKEN_ERROR, "|", current_line, current_column);
        case '(': advance(); return create_token(TOKEN_LEFT_PAREN, "(", current_line, current_column);
        case ')': advance(); return create_token(TOKEN_RIGHT_PAREN, ")", current_line, current_column);
        case '{': advance(); return create_token(TOKEN_LEFT_BRACE, "{", current_line, current_column);
        case '}': advance(); return create_token(TOKEN_RIGHT_BRACE, "}", current_line, current_column);
        case '[': advance(); return create_token(TOKEN_LEFT_BRACKET, "[", current_line, current_column);
        case ']': advance(); return create_token(TOKEN_RIGHT_BRACKET, "]", current_line, current_column);
        case ';': advance(); return create_token(TOKEN_SEMICOLON, ";", current_line, current_column);
        case ',': advance(); return create_token(TOKEN_COMMA, ",", current_line, current_column);
        case '.': advance(); return create_token(TOKEN_DOT, ".", current_line, current_column);
        case ':': advance(); return create_token(TOKEN_COLON, ":", current_line, current_column);
        default:
            advance();
            return create_token(TOKEN_ERROR, &c, current_line, current_column);
    }
}

Token* create_token(AeTokenType type, const char* value, int line, int column) {
    Token* token = malloc(sizeof(Token));
    token->type = type;
    token->line = line;
    token->column = column;
    if (value) {
        token->value = malloc(strlen(value) + 1);
        strcpy(token->value, value);
    } else {
        token->value = NULL;
    }
    return token;
}

void free_token(Token* token) {
    if (token) {
        if (token->value) {
            free(token->value);
        }
        free(token);
    }
}

const char* token_type_to_string(AeTokenType type) {
    switch (type) {
        case TOKEN_ACTOR: return "ACTOR";
        case TOKEN_MAIN: return "MAIN";
        case TOKEN_FUNC: return "FUNC";
        case TOKEN_LET: return "LET";
        case TOKEN_VAR: return "VAR";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_SWITCH: return "SWITCH";
        case TOKEN_CASE: return "CASE";
        case TOKEN_DEFAULT: return "DEFAULT";
        case TOKEN_BREAK: return "BREAK";
        case TOKEN_CONTINUE: return "CONTINUE";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_MATCH: return "MATCH";
        case TOKEN_RECEIVE: return "RECEIVE";
        case TOKEN_SEND: return "SEND";
        case TOKEN_SPAWN_ACTOR: return "SPAWN_ACTOR";
        case TOKEN_SPAWN: return "SPAWN";
        case TOKEN_SELF: return "SELF";
        case TOKEN_STATE: return "STATE";
        case TOKEN_STRUCT: return "STRUCT";
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_BOOL: return "BOOL";
        case TOKEN_STRING: return "STRING";
        case TOKEN_ACTOR_REF: return "ACTOR_REF";
        case TOKEN_MESSAGE: return "MESSAGE";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING_LITERAL: return "STRING_LITERAL";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_MULTIPLY: return "MULTIPLY";
        case TOKEN_DIVIDE: return "DIVIDE";
        case TOKEN_MODULO: return "MODULO";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_EQUALS: return "EQUALS";
        case TOKEN_NOT_EQUALS: return "NOT_EQUALS";
        case TOKEN_LESS: return "LESS";
        case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_INCREMENT: return "INCREMENT";
        case TOKEN_DECREMENT: return "DECREMENT";
        case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TOKEN_LEFT_BRACKET: return "LEFT_BRACKET";
        case TOKEN_RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_DOT: return "DOT";
        case TOKEN_COLON: return "COLON";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_PRINT: return "PRINT";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
