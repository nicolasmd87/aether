#ifndef LEXER_H
#define LEXER_H

#include "tokens.h"

// Lexer functions
void lexer_init(const char* src);
Token* next_token(void);

// Internal lexer functions (exposed for testing)
char peek(void);
char advance(void);
void skip_whitespace(void);
void skip_comment(void);
Token* read_string(void);
Token* read_number(void);
Token* read_identifier(void);

#endif // LEXER_H

