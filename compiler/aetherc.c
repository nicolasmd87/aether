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
#include "tokens.h"
#include "ast.h"
#include "parser.h"
#include "typechecker.h"
#include "codegen.h"

// Compiler limits
#define MAX_TOKENS 10000
#define AETHER_VERSION "0.1.0"

// Constants for better maintainability
#define DEFAULT_MAX_ACTORS 1000
#define DEFAULT_WORKER_THREADS 4

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
int file_exists(const char* path) {
    return access(path, F_OK) == 0;
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
    
    printf("Compiling %s...\n", input_path);
    
    // Step 1: Lexical Analysis
    printf("Step 1: Tokenizing...\n");
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
            fprintf(stderr, "Lexical error at line %d, column %d: %s\n", 
                    token->line, token->column, token->value);
            // Cleanup tokens
            for (int i = 0; i < token_count; i++) {
                free_token(tokens[i]);
            }
            free(source);
            return 0;
        }
    }
    
    printf("Generated %d tokens\n", token_count);
    
    // Step 2: Parsing
    printf("Step 2: Parsing...\n");
    Parser* parser = create_parser(tokens, token_count);
    ASTNode* program = parse_program(parser);
    
    if (!program) {
        fprintf(stderr, "Parse error\n");
        // Cleanup
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 0;
    }
    
    printf("Parse successful\n");
    
    // Step 3: Type Checking
    printf("Step 3: Type checking...\n");
    if (!typecheck_program(program)) {
        fprintf(stderr, "Type checking failed\n");
        // Cleanup
        free_ast_node(program);
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 0;
    }
    
    printf("Type checking successful\n");
    
    // Step 4: Code Generation
    printf("Step 4: Generating C code...\n");
    FILE *output = fopen(output_path, "w");
    if (!output) {
        perror("Error opening output file");
        // Cleanup
        free_ast_node(program);
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 0;
    }
    
    CodeGenerator* codegen = create_code_generator(output);
    generate_program(codegen, program);
    fclose(output);
    
    printf("Code generation successful\n");
    
    // Cleanup
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
    if (!file_exists("runtime/actor.c")) {
        if (file_exists("../runtime/actor.c")) {
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

    printf("Executing: %s\n", cmd);
    int result = system(cmd);
    return result == 0;
}

int main(int argc, char *argv[]) {
    // Version flag (check before other args)
    if (argc >= 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        printf("Aether Compiler v%s\n", AETHER_VERSION);
        return 0;
    }
    
    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  Compile to C: %s <input.ae> <output.c>\n", argv[0]);
        fprintf(stderr, "  Run directly: %s run <input.ae>\n", argv[0]);
        fprintf(stderr, "  Version:     %s --version\n", argv[0]);
        return 1;
    }

    // Check for "run" command
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s run <input.ae>\n", argv[0]);
            return 1;
        }
        
        const char* input_path = argv[2];
        
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
    
    // Default mode: Compile to C (original behavior)
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.ae> <output.c>\n", argv[0]);
        return 1;
    }
    
    if (!compile_source(argv[1], argv[2])) {
        return 1;
    }
    
    printf("Output written to %s\n", argv[2]);
    return 0;
}
