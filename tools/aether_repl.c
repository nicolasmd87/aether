// Aether REPL (Read-Eval-Print Loop)
// Interactive interpreter for Aether programming language

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"

typedef struct {
    char** lines;
    int count;
    int capacity;
} InputBuffer;

typedef struct {
    bool multiline_mode;
    int line_count;
    char* temp_file;
    char* output_file;
} REPLState;

void init_input_buffer(InputBuffer* buf) {
    buf->capacity = 10;
    buf->count = 0;
    buf->lines = (char**)malloc(sizeof(char*) * buf->capacity);
}

void add_line(InputBuffer* buf, const char* line) {
    if (buf->count >= buf->capacity) {
        buf->capacity *= 2;
        buf->lines = (char**)realloc(buf->lines, sizeof(char*) * buf->capacity);
    }
    buf->lines[buf->count] = strdup(line);
    buf->count++;
}

void clear_buffer(InputBuffer* buf) {
    for (int i = 0; i < buf->count; i++) {
        free(buf->lines[i]);
    }
    buf->count = 0;
}

void free_input_buffer(InputBuffer* buf) {
    clear_buffer(buf);
    free(buf->lines);
}

void print_welcome() {
    printf("%s", COLOR_CYAN);
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                              ║\n");
    printf("║   █████╗ ███████╗████████╗██╗  ██╗███████╗██████╗           ║\n");
    printf("║  ██╔══██╗██╔════╝╚══██╔══╝██║  ██║██╔════╝██╔══██╗          ║\n");
    printf("║  ███████║█████╗     ██║   ███████║█████╗  ██████╔╝          ║\n");
    printf("║  ██╔══██║██╔══╝     ██║   ██╔══██║██╔══╝  ██╔══██╗          ║\n");
    printf("║  ██║  ██║███████╗   ██║   ██║  ██║███████╗██║  ██║          ║\n");
    printf("║  ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝          ║\n");
    printf("║                                                              ║\n");
    printf("║          Interactive Aether REPL  v0.4.0                    ║\n");
    printf("║          Type :help for commands                            ║\n");
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("%s\n", COLOR_RESET);
}

void print_help() {
    printf("%s", COLOR_GREEN);
    printf("\nREPL Commands:\n");
    printf("  :help, :h        - Show this help message\n");
    printf("  :quit, :q, :exit - Exit the REPL\n");
    printf("  :clear, :c       - Clear the screen\n");
    printf("  :multi, :m       - Enter multiline mode (end with empty line)\n");
    printf("  :reset, :r       - Reset the session (clear all definitions)\n");
    printf("  :show, :s        - Show current session code\n");
    printf("\nUsage:\n");
    printf("  - Enter expressions to evaluate them immediately\n");
    printf("  - Define functions, actors, and structs\n");
    printf("  - Use multiline mode for complex definitions\n");
    printf("\nExamples:\n");
    printf("  >>> 2 + 3\n");
    printf("  5\n");
    printf("  >>> int x = 42\n");
    printf("  >>> x * 2\n");
    printf("  84\n");
    printf("%s\n", COLOR_RESET);
}

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

bool is_complete_statement(const char* line) {
    // Check if the line looks like a complete statement
    int len = strlen(line);
    if (len == 0) return true;
    
    // Trim trailing whitespace
    while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t' || line[len-1] == '\n')) {
        len--;
    }
    
    if (len == 0) return true;
    
    // Check for block starters that need continuation
    const char* block_starters[] = {"actor", "func", "struct", "if", "while", "for", "match", NULL};
    for (int i = 0; block_starters[i] != NULL; i++) {
        if (strstr(line, block_starters[i]) != NULL) {
            // Check if it has an opening brace without closing
            int open_braces = 0;
            for (int j = 0; j < len; j++) {
                if (line[j] == '{') open_braces++;
                if (line[j] == '}') open_braces--;
            }
            if (open_braces > 0) return false;
        }
    }
    
    // Check for unclosed braces
    int braces = 0;
    for (int i = 0; i < len; i++) {
        if (line[i] == '{') braces++;
        if (line[i] == '}') braces--;
    }
    
    return braces == 0;
}

bool compile_and_run(REPLState* state, const char* code, bool is_expression) {
    // Write code to temporary file
    FILE* temp_file = fopen(state->temp_file, "w");
    if (!temp_file) {
        fprintf(stderr, "%sError: Cannot create temp file%s\n", COLOR_RED, COLOR_RESET);
        return false;
    }
    
    // Wrap in main if it's just an expression
    if (is_expression) {
        fprintf(temp_file, "func main() {\n");
        fprintf(temp_file, "    result = %s\n", code);
        fprintf(temp_file, "    print(result)\n");
        fprintf(temp_file, "}\n");
    } else {
        // Check if it has main, if not wrap it
        if (strstr(code, "main") == NULL) {
            fprintf(temp_file, "%s\n", code);
            fprintf(temp_file, "\nfunc main() {\n");
            fprintf(temp_file, "    print(\"OK\")\n");
            fprintf(temp_file, "}\n");
        } else {
            fprintf(temp_file, "%s\n", code);
        }
    }
    
    fclose(temp_file);
    
    // Compile
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd), 
             "aetherc %s -o %s 2>&1", state->temp_file, state->output_file);
    
    FILE* compile_proc = popen(compile_cmd, "r");
    if (!compile_proc) {
        fprintf(stderr, "%sError: Cannot run compiler%s\n", COLOR_RED, COLOR_RESET);
        return false;
    }
    
    char compile_output[4096] = {0};
    fread(compile_output, 1, sizeof(compile_output) - 1, compile_proc);
    int compile_status = pclose(compile_proc);
    
    if (compile_status != 0) {
        // Compilation error
        printf("%s%s%s", COLOR_RED, compile_output, COLOR_RESET);
        return false;
    }
    
    // Run the compiled program
    char run_cmd[512];
#ifdef _WIN32
    snprintf(run_cmd, sizeof(run_cmd), "%s.exe 2>&1", state->output_file);
#else
    snprintf(run_cmd, sizeof(run_cmd), "./%s 2>&1", state->output_file);
#endif
    
    FILE* run_proc = popen(run_cmd, "r");
    if (!run_proc) {
        fprintf(stderr, "%sError: Cannot run program%s\n", COLOR_RED, COLOR_RESET);
        return false;
    }
    
    char run_output[4096] = {0};
    fread(run_output, 1, sizeof(run_output) - 1, run_proc);
    pclose(run_proc);
    
    // Print output
    if (strlen(run_output) > 0) {
        printf("%s%s%s", COLOR_YELLOW, run_output, COLOR_RESET);
    }
    
    return true;
}

bool handle_command(const char* input, REPLState* state, InputBuffer* session_buf) {
    if (strcmp(input, ":help") == 0 || strcmp(input, ":h") == 0) {
        print_help();
        return true;
    }
    
    if (strcmp(input, ":quit") == 0 || strcmp(input, ":q") == 0 || strcmp(input, ":exit") == 0) {
        printf("%sGoodbye!%s\n", COLOR_CYAN, COLOR_RESET);
        return false;
    }
    
    if (strcmp(input, ":clear") == 0 || strcmp(input, ":c") == 0) {
        clear_screen();
        print_welcome();
        return true;
    }
    
    if (strcmp(input, ":multi") == 0 || strcmp(input, ":m") == 0) {
        state->multiline_mode = true;
        printf("%sEntering multiline mode. End with empty line.%s\n", COLOR_GREEN, COLOR_RESET);
        return true;
    }
    
    if (strcmp(input, ":reset") == 0 || strcmp(input, ":r") == 0) {
        clear_buffer(session_buf);
        printf("%sSession reset.%s\n", COLOR_GREEN, COLOR_RESET);
        return true;
    }
    
    if (strcmp(input, ":show") == 0 || strcmp(input, ":s") == 0) {
        if (session_buf->count == 0) {
            printf("%sNo code in session.%s\n", COLOR_YELLOW, COLOR_RESET);
        } else {
            printf("%s--- Session Code ---%s\n", COLOR_BLUE, COLOR_RESET);
            for (int i = 0; i < session_buf->count; i++) {
                printf("%s\n", session_buf->lines[i]);
            }
            printf("%s-------------------%s\n", COLOR_BLUE, COLOR_RESET);
        }
        return true;
    }
    
    printf("%sUnknown command: %s%s\n", COLOR_RED, input, COLOR_RESET);
    printf("Type :help for available commands\n");
    return true;
}

char* read_line(const char* prompt) {
#ifndef _WIN32
    // Use readline on Unix systems for better line editing
    char* line = readline(prompt);
    if (line && *line) {
        add_history(line);
    }
    return line;
#else
    // Simple line reading on Windows
    printf("%s", prompt);
    fflush(stdout);
    
    char* line = (char*)malloc(1024);
    if (!fgets(line, 1024, stdin)) {
        free(line);
        return NULL;
    }
    
    // Remove newline
    size_t len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
        line[len-1] = '\0';
    }
    
    return line;
#endif
}

void trim_whitespace(char* str) {
    char* end;
    
    // Trim leading space
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    
    if (*str == 0) return;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) end--;
    
    end[1] = '\0';
}

int main(int argc, char** argv) {
    // Initialize REPL state
    REPLState state = {
        .multiline_mode = false,
        .line_count = 0,
        .temp_file = ".aether_repl_temp.ae",
        .output_file = ".aether_repl_out"
    };
    
    InputBuffer session_buffer;
    init_input_buffer(&session_buffer);
    
    InputBuffer multiline_buffer;
    init_input_buffer(&multiline_buffer);
    
    // Print welcome
    clear_screen();
    print_welcome();
    
    // Main REPL loop
    bool running = true;
    while (running) {
        const char* prompt = state.multiline_mode ? 
            COLOR_MAGENTA "... " COLOR_RESET : 
            COLOR_GREEN ">>> " COLOR_RESET;
        
        char* line = read_line(prompt);
        if (!line) break;
        
        // Trim whitespace
        trim_whitespace(line);
        
        // Handle empty line in multiline mode
        if (state.multiline_mode && strlen(line) == 0) {
            if (multiline_buffer.count > 0) {
                // Combine all lines
                int total_len = 0;
                for (int i = 0; i < multiline_buffer.count; i++) {
                    total_len += strlen(multiline_buffer.lines[i]) + 1;
                }
                
                char* code = (char*)malloc(total_len + 1);
                code[0] = '\0';
                for (int i = 0; i < multiline_buffer.count; i++) {
                    strcat(code, multiline_buffer.lines[i]);
                    strcat(code, "\n");
                }
                
                // Add to session
                add_line(&session_buffer, code);
                
                // Compile and run
                compile_and_run(&state, code, false);
                
                free(code);
                clear_buffer(&multiline_buffer);
            }
            state.multiline_mode = false;
            free(line);
            continue;
        }
        
        // Handle commands
        if (line[0] == ':') {
            running = handle_command(line, &state, &session_buffer);
            free(line);
            continue;
        }
        
        // Handle multiline mode
        if (state.multiline_mode) {
            add_line(&multiline_buffer, line);
            free(line);
            continue;
        }
        
        // Check if it's an expression or statement
        bool is_expr = true;
        const char* stmt_keywords[] = {"func", "actor", "struct", "if", "while", "for", "match", "int", "float", "bool", "string", NULL};
        for (int i = 0; stmt_keywords[i] != NULL; i++) {
            if (strstr(line, stmt_keywords[i]) == line) {
                is_expr = false;
                break;
            }
        }
        
        if (is_expr && !is_complete_statement(line)) {
            // Enter multiline mode automatically
            state.multiline_mode = true;
            add_line(&multiline_buffer, line);
            free(line);
            continue;
        }
        
        // Execute single line
        if (is_expr) {
            // It's an expression
            compile_and_run(&state, line, true);
        } else {
            // It's a statement/definition
            add_line(&session_buffer, line);
            compile_and_run(&state, line, false);
        }
        
        free(line);
    }
    
    // Cleanup
    free_input_buffer(&session_buffer);
    free_input_buffer(&multiline_buffer);
    
    // Remove temp files
    remove(state.temp_file);
    remove(state.output_file);
#ifdef _WIN32
    char temp_exe[256];
    snprintf(temp_exe, sizeof(temp_exe), "%s.exe", state.output_file);
    remove(temp_exe);
#endif
    
    return 0;
}
