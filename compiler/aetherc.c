/*
 * Aether Programming Language Compiler
 * Copyright (c) 2025 Aether Programming Language Contributors
 * 
 * This file is part of Aether.
 * Licensed under the MIT License. See LICENSE file in the project root.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#  if defined(_MSC_VER) && !defined(getpid)
#    define getpid _getpid
#  endif
#else
#include <unistd.h>
#endif
#include "parser/tokens.h"
#include "ast.h"
#include "parser/parser.h"
#include "analysis/typechecker.h"
#include "codegen/optimizer.h"
#include "codegen/codegen.h"
#include "aether_error.h"
#include "aether_module.h"

// Compiler limits
#define MAX_TOKENS 50000

// Version is set by Makefile from VERSION file
#ifndef AETHER_VERSION
#define AETHER_VERSION "0.0.0-dev"
#endif

// Constants for better maintainability
#define DEFAULT_MAX_ACTORS 1000
#define DEFAULT_WORKER_THREADS 4

// Global flags
static bool verbose_mode = false;
static bool dump_ast_mode = false;
static bool emit_c_mode = false;
static bool check_only_mode = false;
static bool preempt_mode = false;
static const char* emit_header_path = NULL;

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define access _access
    #define F_OK 0
    #define PATH_SEP '\\'
#else
    #include <unistd.h>
    #define PATH_SEP '/'
#endif

// Helper to check file existence
int compiler_file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

// Derive header filename from output path (output.c -> output.h)
static char* derive_header_path(const char* output_path) {
    size_t len = strlen(output_path);
    char* header_path = malloc(len + 1);
    strcpy(header_path, output_path);

    // Replace .c with .h, or append .h if no .c extension
    if (len > 2 && header_path[len-2] == '.' && header_path[len-1] == 'c') {
        header_path[len-1] = 'h';
    } else {
        header_path = realloc(header_path, len + 3);
        strcat(header_path, ".h");
    }
    return header_path;
}

// Print a summary line if any errors were recorded
static void report_compilation_failure(void) {
    int n = aether_error_count();
    if (n > 0) {
        fprintf(stderr, "aborting: %d error(s) found\n", n);
    }
}

// Compile aether source to C
int compile_source(const char* input_path, const char* output_path) {
    // Read input file
    FILE *input = fopen(input_path, "r");
    if (!input) {
        perror("Error opening input file");
        return 0;
    }
    
    fseek(input, 0, SEEK_END);
    long file_size = ftell(input);
    fseek(input, 0, SEEK_SET);
    
    char *source = malloc(file_size + 1);
    if (!source) {
        perror("Memory allocation error");
        fclose(input);
        return 0;
    }
    
    size_t bytes_read = fread(source, 1, file_size, input);
    fclose(input);
    if (bytes_read == 0 && file_size > 0) {
        fprintf(stderr, "Error: Failed to read file\n");
        free(source);
        return 0;
    }
    // On Windows text mode, bytes_read may be less than file_size due to line ending conversion
    // Null-terminate at actual bytes read
    source[bytes_read] = '\0';
    
    if (verbose_mode) printf("Compiling %s...\n", input_path);

    aether_error_init(input_path, source);

    // Step 1: Lexical Analysis
    if (verbose_mode) {
        printf("[Phase 1/5] Lexical Analysis...\n");
    }
    
    lexer_init(source);
    
    Token* tokens[MAX_TOKENS];
    int token_count = 0;
    
    while (token_count < MAX_TOKENS - 1) {
        Token* token = next_token();
        tokens[token_count] = token;
        token_count++;
        
        if (token->type == TOKEN_EOF) {
            break;
        }
        
        if (token->type == TOKEN_ERROR) {
            aether_error_with_code(token->value, token->line, token->column,
                                   AETHER_ERR_SYNTAX);
            // Cleanup tokens
            for (int i = 0; i < token_count; i++) {
                free_token(tokens[i]);
            }
            free(source);
            return 0;
        }
    }
    
    // Check for token overflow (file too large)
    if (token_count >= MAX_TOKENS - 1 && tokens[token_count - 1]->type != TOKEN_EOF) {
        fprintf(stderr, "error: source file exceeds maximum token limit (%d tokens)\n", MAX_TOKENS);
        fprintf(stderr, "  help: split into multiple files using imports\n");
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free(source);
        return 0;
    }

    if (verbose_mode) printf("Generated %d tokens\n", token_count);

    // Step 2: Parsing
    if (verbose_mode) printf("Step 2: Parsing...\n");
    Parser* parser = create_parser(tokens, token_count);
    ASTNode* program = parse_program(parser);
    
    if (!program) {
        report_compilation_failure();
        // Cleanup
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 0;
    }

    if (verbose_mode) printf("Parse successful\n");

    // --dump-ast: print the AST and exit (no codegen)
    if (dump_ast_mode) {
        print_ast(program, 0);
        free_ast_node(program);
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 1;  // success
    }

    // Step 2.5: Module Orchestration
    if (verbose_mode) printf("[Phase 2.5/5] Module resolution...\n");
    module_set_source_dir(input_path);
    if (!module_orchestrate(program)) {
        report_compilation_failure();
        free_ast_node(program);
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 0;
    }
    if (verbose_mode) printf("Module resolution successful\n");

    // Step 2.6: Merge pure Aether module functions into program AST
    module_merge_into_program(program);

    // Step 3: Type Checking
    if (verbose_mode) printf("Step 3: Type checking...\n");
    if (!typecheck_program(program)) {
        report_compilation_failure();
        // Cleanup
        module_registry_shutdown();
        free_ast_node(program);
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 0;
    }
    
    if (verbose_mode) printf("Type checking successful\n");

    // --check: stop after typecheck + type inference, no codegen
    if (check_only_mode) {
        int warnings = aether_warning_count();
        if (warnings > 0) {
            fprintf(stderr, "OK: %d warning(s)\n", warnings);
        } else {
            fprintf(stderr, "OK: no errors\n");
        }
        module_registry_shutdown();
        free_ast_node(program);
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 1;  // success
    }

    // Step 3.5: Optimization (AST-level passes: constant folding, dead code, tail calls)
    if (verbose_mode) printf("Step 3.5: Optimizing...\n");
    program = optimize_ast(program);

    // Step 4: Code Generation
    if (verbose_mode) printf("Step 4: Generating C code...\n");
    FILE *output = fopen(output_path, "w");
    if (!output) {
        perror("Error opening output file");
        // Cleanup
        module_registry_shutdown();
        free_ast_node(program);
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 0;
    }
    
    // Open header file if --emit-header was specified
    FILE* header_output = NULL;
    char* header_path = NULL;
    if (emit_header_path) {
        if (strcmp(emit_header_path, "auto") == 0) {
            header_path = derive_header_path(output_path);
        } else {
            header_path = strdup(emit_header_path);
        }
        header_output = fopen(header_path, "w");
        if (!header_output) {
            fprintf(stderr, "Warning: Could not open header file %s\n", header_path);
        }
    }

    CodeGenerator* codegen;
    if (header_output) {
        codegen = create_code_generator_with_header(output, header_output, header_path);
        if (verbose_mode) printf("Also generating header: %s\n", header_path);
    } else {
        codegen = create_code_generator(output);
    }
    if (preempt_mode) codegen->preempt_loops = 1;
    generate_program(codegen, program);
    fclose(output);
    if (header_output) {
        fclose(header_output);
    }
    if (header_path) {
        free(header_path);
    }

    if (verbose_mode) {
        printf("Code generation successful\n");
        // Print all optimization stats here — series/linear loop collapse happens during codegen,
        // so stats must be printed after generate_program(), not before it.
        print_optimization_stats();
    }

    // Cleanup
    module_registry_shutdown();
    free_ast_node(program);
    for (int i = 0; i < token_count; i++) {
        free_token(tokens[i]);
    }
    free_parser(parser);
    free_code_generator(codegen);
    free(source);

    return 1;
}

// Compile C file to executable using system compiler (gcc)
int compile_c_to_exe(const char* c_file, const char* exe_file) {
    char cmd[1024];
    
    // Assume runtime is in "runtime/" relative to current dir, or check specific paths
    // For now, assume user is running from project root or has runtime folder nearby.
    // We try to locate the runtime folder.
    
    const char* runtime_path = "runtime";
    if (!compiler_file_exists("runtime/actor.c")) {
        if (compiler_file_exists("../runtime/actor.c")) {
            runtime_path = "../runtime";
        } else {
            fprintf(stderr, "Error: Could not locate Aether runtime files.\n");
            return 0;
        }
    }

#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "gcc \"%s\" \"%s\\*.c\" -o \"%s\" -I\"%s\" -O2 -lpthread", 
             c_file, runtime_path, exe_file, runtime_path);
#else
    snprintf(cmd, sizeof(cmd), "gcc \"%s\" \"%s\"/*.c -o \"%s\" -I\"%s\" -O2 -lpthread", 
             c_file, runtime_path, exe_file, runtime_path);
#endif

    if (verbose_mode) printf("Executing: %s\n", cmd);
    int result = system(cmd);
    return result == 0;
}

void print_help(const char* program_name) {
    printf("Aether Compiler v%s\n\n", AETHER_VERSION);
    printf("Usage:\n");
    printf("  %s <input.ae> <output.c>         Compile Aether to C\n", program_name);
    printf("  %s run <input.ae>                Compile and run immediately\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  --version, -v                    Show version information\n");
    printf("  --verbose                        Show detailed compilation phases and timing\n");
    printf("  --emit-c                         Print generated C code to stdout\n");
    printf("  --emit-header [path]             Generate C header for embedding (default: auto)\n");
    printf("  --check                          Type-check only (no code generation)\n");
    printf("  --dump-ast                       Print AST and exit (no code generation)\n");
    printf("  --help, -h                       Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s hello.ae hello.c              Compile to C\n", program_name);
    printf("  %s run hello.ae                  Quick run\n", program_name);
    printf("  %s --verbose hello.ae hello.c    Compile with timing info\n", program_name);
    printf("  %s --emit-header hello.ae hello.c  Generate hello.h for C embedding\n", program_name);
}

int main(int argc, char *argv[]) {
    // Parse flags
    int arg_offset = 1;
    while (arg_offset < argc && argv[arg_offset][0] == '-') {
        if (strcmp(argv[arg_offset], "--version") == 0 || strcmp(argv[arg_offset], "-v") == 0) {
            printf("Aether Compiler v%s\n", AETHER_VERSION);
            return 0;
        } else if (strcmp(argv[arg_offset], "--help") == 0 || strcmp(argv[arg_offset], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[arg_offset], "--verbose") == 0) {
            verbose_mode = true;
            arg_offset++;
        } else if (strcmp(argv[arg_offset], "--emit-c") == 0) {
            emit_c_mode = true;
            arg_offset++;
        } else if (strcmp(argv[arg_offset], "--dump-ast") == 0) {
            dump_ast_mode = true;
            arg_offset++;
        } else if (strcmp(argv[arg_offset], "--check") == 0) {
            check_only_mode = true;
            arg_offset++;
        } else if (strcmp(argv[arg_offset], "--preempt") == 0) {
            preempt_mode = true;
            arg_offset++;
        } else if (strcmp(argv[arg_offset], "--emit-header") == 0) {
            // Check for optional explicit path argument (must end in .h)
            if (arg_offset + 1 < argc && argv[arg_offset + 1][0] != '-') {
                const char* next_arg = argv[arg_offset + 1];
                size_t len = strlen(next_arg);
                if (len > 2 && next_arg[len-2] == '.' && next_arg[len-1] == 'h') {
                    // Explicit .h path provided
                    emit_header_path = next_arg;
                    arg_offset += 2;
                } else {
                    // Next arg is probably the input file, not a header path
                    emit_header_path = "auto";
                    arg_offset++;
                }
            } else {
                emit_header_path = "auto";  // Auto-derive from output filename
                arg_offset++;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg_offset]);
            fprintf(stderr, "Use --help for usage information\n");
            return 1;
        }
    }
    
    if (argc - arg_offset < 1) {
        print_help(argv[0]);
        return 1;
    }

    // Check for "run" command
    if (strcmp(argv[arg_offset], "run") == 0) {
        if (argc - arg_offset < 2) {
            fprintf(stderr, "Usage: %s run <input.ae>\n", argv[0]);
            return 1;
        }
        
        const char* input_path = argv[arg_offset + 1];
        
        // Generate temp filenames
        char c_path[256];
        char exe_path[256];
        
        // Simple temp name generation based on input
        // "test.ae" -> "test.ae.c", "test.ae.exe"
        snprintf(c_path, sizeof(c_path), "%s.c", input_path);
        snprintf(exe_path, sizeof(exe_path), "%s.exe", input_path); // .exe works on Linux too usually, or just append nothing
        
        // 1. Compile Aether -> C
        if (!compile_source(input_path, c_path)) {
            return 1;
        }
        
        // 2. Compile C -> Exe
        if (!compile_c_to_exe(c_path, exe_path)) {
            fprintf(stderr, "Build failed.\n");
            // Try to cleanup temp C file at least
            remove(c_path); 
            return 1;
        }
        
        // 3. Run Exe
        printf("Running program...\n----------------\n");
        int result = system(exe_path);
        
        // 4. Cleanup
        // Note: Temporary files are kept for debugging
        
        return result;
    }
    
    // --dump-ast only needs the input file
    if (dump_ast_mode) {
        if (!compile_source(argv[arg_offset], "/dev/null")) {
            return 1;
        }
        return 0;
    }

    // --emit-c: compile and print generated C to stdout
    if (emit_c_mode) {
        // Compile to a temp file, then cat to stdout
        char tmp_path[256];
        snprintf(tmp_path, sizeof(tmp_path), "/tmp/aether_emit_%d.c", (int)getpid());
        if (!compile_source(argv[arg_offset], tmp_path)) {
            return 1;
        }
        FILE* f = fopen(tmp_path, "r");
        if (f) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                fwrite(buf, 1, n, stdout);
            }
            fclose(f);
        }
        remove(tmp_path);
        return 0;
    }

    // --check: type-check only, no output file needed
    if (check_only_mode) {
        if (!compile_source(argv[arg_offset], "/dev/null")) {
            return 1;
        }
        return 0;
    }

    // Default mode: Compile to C (original behavior)
    if (argc - arg_offset < 2) {
        fprintf(stderr, "Usage: %s <input.ae> <output.c>\n", argv[0]);
        fprintf(stderr, "Use --help for more information\n");
        return 1;
    }

    if (!compile_source(argv[arg_offset], argv[arg_offset + 1])) {
        return 1;
    }
    
    if (verbose_mode) printf("Output written to %s\n", argv[arg_offset + 1]);
    return 0;
}
