#ifndef TOKENS_H
#define TOKENS_H

typedef enum {
    // Keywords
    TOKEN_ACTOR,
    TOKEN_MAIN,
    TOKEN_FUNC,
    TOKEN_LET,
    TOKEN_VAR,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_WHILE,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_RETURN,
    TOKEN_MATCH,
    TOKEN_RECEIVE,
    TOKEN_SEND,
    TOKEN_SPAWN_ACTOR,
    TOKEN_SPAWN,
    TOKEN_SELF,
    TOKEN_STATE,
    TOKEN_STRUCT,
    
    // Types
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_BOOL,
    TOKEN_STRING,
    TOKEN_ACTOR_REF,
    TOKEN_MESSAGE,
    
    // Literals
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING_LITERAL,
    TOKEN_TRUE,
    TOKEN_FALSE,
    
    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_MODULO,
    TOKEN_ASSIGN,
    TOKEN_EQUALS,
    TOKEN_NOT_EQUALS,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_INCREMENT,
    TOKEN_DECREMENT,
    
    // Delimiters
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_COLON,
    TOKEN_ARROW,
    
    // Special
    TOKEN_PRINT,
    TOKEN_EOF,
    TOKEN_ERROR
} AeTokenType;

typedef struct {
    AeTokenType type;
    char* value;
    int line;
    int column;
} Token;

// Lexer functions
void lexer_init(const char* src);
Token* next_token(void);

// Token functions
Token* create_token(AeTokenType type, const char* value, int line, int column);
void free_token(Token* token);
const char* token_type_to_string(AeTokenType type);

#endif
