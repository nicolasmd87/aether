#include "aether_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* current_filename = NULL;
static const char* current_source = NULL;
static int error_count_global = 0;
static int warning_count_global = 0;

void aether_error_init(const char* filename, const char* source) {
    current_filename = filename;
    current_source = source;
    error_count_global = 0;
    warning_count_global = 0;
}

// Get a specific line from source code
static const char* get_line(const char* source, int line_num, int* line_length) {
    if (!source) return NULL;
    
    int current_line = 1;
    const char* line_start = source;
    const char* p = source;
    
    while (*p && current_line < line_num) {
        if (*p == '\n') {
            current_line++;
            line_start = p + 1;
        }
        p++;
    }
    
    if (current_line != line_num) return NULL;
    
    // Find end of line
    const char* line_end = line_start;
    while (*line_end && *line_end != '\n') line_end++;
    
    *line_length = line_end - line_start;
    return line_start;
}

void aether_error_report(AetherError* error) {
    if (!error) return;
    
    error_count_global++;
    
    // Print error header
    fprintf(stderr, "%serror%s: %s\n", 
            AETHER_COLOR_RED AETHER_COLOR_BOLD,
            AETHER_COLOR_RESET,
            error->message);
    
    // Print location
    if (error->filename) {
        fprintf(stderr, "  %s-->%s %s:%d:%d\n",
                AETHER_COLOR_BLUE,
                AETHER_COLOR_RESET,
                error->filename,
                error->line,
                error->column);
    }
    
    // Print source context
    if (error->source_code && error->line > 0) {
        int line_length;
        const char* line = get_line(error->source_code, error->line, &line_length);
        
        if (line) {
            // Line number width
            int line_num_width = snprintf(NULL, 0, "%d", error->line);
            
            // Print line with line number
            fprintf(stderr, "%s%*d |%s ", 
                    AETHER_COLOR_BLUE,
                    line_num_width, error->line,
                    AETHER_COLOR_RESET);
            fwrite(line, 1, line_length, stderr);
            fprintf(stderr, "\n");
            
            // Print caret (^) pointing to error column
            fprintf(stderr, "%*s |%s ", line_num_width, "", AETHER_COLOR_BLUE);
            for (int i = 0; i < error->column - 1; i++) {
                fprintf(stderr, " ");
            }
            fprintf(stderr, "%s^%s", AETHER_COLOR_RED AETHER_COLOR_BOLD, AETHER_COLOR_RESET);
            
            // Print suggestion on same line if available
            if (error->suggestion) {
                fprintf(stderr, " %shelp:%s %s",
                        AETHER_COLOR_CYAN,
                        AETHER_COLOR_RESET,
                        error->suggestion);
            }
            fprintf(stderr, "\n");
        }
    }
    
    // Print context if available
    if (error->context) {
        fprintf(stderr, "   %s= note:%s %s\n",
                AETHER_COLOR_CYAN,
                AETHER_COLOR_RESET,
                error->context);
    }
    
    fprintf(stderr, "\n");
}

void aether_warning_report(AetherError* warning) {
    if (!warning) return;
    
    warning_count_global++;
    
    // Print warning header (yellow instead of red)
    fprintf(stderr, "%swarning%s: %s\n",
            AETHER_COLOR_YELLOW AETHER_COLOR_BOLD,
            AETHER_COLOR_RESET,
            warning->message);
    
    // Print location
    if (warning->filename) {
        fprintf(stderr, "  %s-->%s %s:%d:%d\n",
                AETHER_COLOR_BLUE,
                AETHER_COLOR_RESET,
                warning->filename,
                warning->line,
                warning->column);
    }
    
    // Print source context (same as error)
    if (warning->source_code && warning->line > 0) {
        int line_length;
        const char* line = get_line(warning->source_code, warning->line, &line_length);
        
        if (line) {
            int line_num_width = snprintf(NULL, 0, "%d", warning->line);
            
            fprintf(stderr, "%s%*d |%s ",
                    AETHER_COLOR_BLUE,
                    line_num_width, warning->line,
                    AETHER_COLOR_RESET);
            fwrite(line, 1, line_length, stderr);
            fprintf(stderr, "\n");
            
            fprintf(stderr, "%*s |%s ", line_num_width, "", AETHER_COLOR_BLUE);
            for (int i = 0; i < warning->column - 1; i++) {
                fprintf(stderr, " ");
            }
            fprintf(stderr, "%s^%s", AETHER_COLOR_YELLOW, AETHER_COLOR_RESET);
            
            if (warning->suggestion) {
                fprintf(stderr, " %shelp:%s %s",
                        AETHER_COLOR_CYAN,
                        AETHER_COLOR_RESET,
                        warning->suggestion);
            }
            fprintf(stderr, "\n");
        }
    }
    
    fprintf(stderr, "\n");
}

// Helper functions
void aether_error_simple(const char* message, int line, int column) {
    AetherError error = {
        .filename = current_filename,
        .source_code = current_source,
        .line = line,
        .column = column,
        .message = message,
        .suggestion = NULL,
        .context = NULL
    };
    aether_error_report(&error);
}

void aether_error_with_suggestion(const char* message, int line, int column, const char* suggestion) {
    AetherError error = {
        .filename = current_filename,
        .source_code = current_source,
        .line = line,
        .column = column,
        .message = message,
        .suggestion = suggestion,
        .context = NULL
    };
    aether_error_report(&error);
}

void aether_error_in_context(const char* message, int line, int column, const char* context) {
    AetherError error = {
        .filename = current_filename,
        .source_code = current_source,
        .line = line,
        .column = column,
        .message = message,
        .suggestion = NULL,
        .context = context
    };
    aether_error_report(&error);
}

// Error statistics
int aether_error_count() {
    return error_count_global;
}

int aether_warning_count() {
    return warning_count_global;
}

void aether_error_reset_counts() {
    error_count_global = 0;
    warning_count_global = 0;
}

