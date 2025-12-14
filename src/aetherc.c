#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"
#include "ast.h"
#include "parser.h"
#include "typechecker.h"
#include "codegen.h"

#define MAX_TOKENS 10000

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.ae output.c\n", argv[0]);
        return 1;
    }
    
    // Read input file
    FILE *input = fopen(argv[1], "r");
    if (!input) {
        perror("Error opening input file");
        return 1;
    }
    
    fseek(input, 0, SEEK_END);
    long file_size = ftell(input);
    fseek(input, 0, SEEK_SET);
    
    char *source = malloc(file_size + 1);
    if (!source) {
        perror("Memory allocation error");
        fclose(input);
        return 1;
    }
    
    fread(source, 1, file_size, input);
    fclose(input);
    source[file_size] = '\0';
    
    printf("Compiling %s...\n", argv[1]);
    
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
            return 1;
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
        return 1;
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
        return 1;
    }
    
    printf("Type checking successful\n");
    
    // Step 4: Code Generation
    printf("Step 4: Generating C code...\n");
    FILE *output = fopen(argv[2], "w");
    if (!output) {
        perror("Error opening output file");
        // Cleanup
        free_ast_node(program);
        for (int i = 0; i < token_count; i++) {
            free_token(tokens[i]);
        }
        free_parser(parser);
        free(source);
        return 1;
    }
    
    printf("Creating code generator...\n");
    CodeGenerator* codegen = create_code_generator(output);
    printf("Generating program...\n");
    generate_program(codegen, program);
    printf("Closing output file...\n");
    fclose(output);
    
    printf("Code generation successful\n");
    printf("Output written to %s\n", argv[2]);
    
    // Cleanup
    free_ast_node(program);
    for (int i = 0; i < token_count; i++) {
        free_token(tokens[i]);
    }
    free_parser(parser);
    free_code_generator(codegen);
    free(source);
    
    return 0;
}
