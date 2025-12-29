#ifndef AETHER_ERROR_H
#define AETHER_ERROR_H

#include <stddef.h>

// Error codes for documentation lookup
typedef enum {
    AETHER_ERR_NONE = 0,
    AETHER_ERR_SYNTAX = 100,
    AETHER_ERR_TYPE_MISMATCH = 200,
    AETHER_ERR_UNDEFINED_VAR = 300,
    AETHER_ERR_UNDEFINED_FUNC = 301,
    AETHER_ERR_UNDEFINED_TYPE = 302,
    AETHER_ERR_REDEFINITION = 400,
    AETHER_ERR_INVALID_OPERAND = 500,
    AETHER_ERR_ACTOR_ERROR = 600,
} AetherErrorCode;

// Error reporting with source context
typedef struct {
    const char* filename;
    const char* source_code;
    int line;
    int column;
    const char* message;
    const char* suggestion;
    const char* context;  // "in actor definition", "in function call", etc.
    AetherErrorCode code;
} AetherError;

// Error reporting functions
void aether_error_init(const char* filename, const char* source);
void aether_error_report(AetherError* error);
void aether_warning_report(AetherError* warning);

// Helper functions
void aether_error_simple(const char* message, int line, int column);
void aether_error_with_suggestion(const char* message, int line, int column, const char* suggestion);
void aether_error_in_context(const char* message, int line, int column, const char* context);
void aether_error_with_code(const char* message, int line, int column, AetherErrorCode code);

// Terminal colors (ANSI escape codes)
#define AETHER_COLOR_RESET "\033[0m"
#define AETHER_COLOR_RED "\033[31m"
#define AETHER_COLOR_YELLOW "\033[33m"
#define AETHER_COLOR_BLUE "\033[34m"
#define AETHER_COLOR_CYAN "\033[36m"
#define AETHER_COLOR_BOLD "\033[1m"

// Error statistics
int aether_error_count();
int aether_warning_count();
void aether_error_reset_counts();

#endif // AETHER_ERROR_H

