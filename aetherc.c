#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUFFER_SIZE 65536
#define MAX_LINE 1024
#define MAIN_SIG "func main()"

static int spawn_counter = 0;

/* process_spawn_block_line:
   Takes a complete spawn block (as a contiguous string) and generates a unique pthread
   function containing the block’s content. Expects syntax:
   spawn(func () { ... });
*/
void process_spawn_block_line(const char *block_line, FILE *out) {
    const char *p = block_line;
    while (isspace(*p)) p++;
    p += 5; // Skip "spawn"
    while (*p && (isspace(*p) || *p == '(')) p++;
    if (strncmp(p, "func", 4) != 0) {
        fprintf(stderr, "Error: Expected 'func' after spawn\n");
        exit(1);
    }
    p += 4;
    while (*p && isspace(*p)) p++;
    if (*p != '(') {
        fprintf(stderr, "Error: Expected '(' after func\n");
        exit(1);
    }
    p++; // Skip '('
    while (*p && isspace(*p)) p++;
    if (*p != ')') {
        fprintf(stderr, "Error: Expected ')' in func signature\n");
        exit(1);
    }
    p++; // Skip ')'
    while (*p && isspace(*p)) p++;
    if (*p != '{') {
        fprintf(stderr, "Error: Expected '{' after func() in spawn block\n");
        exit(1);
    }
    p++; // Skip '{'
    char content[BUFFER_SIZE];
    int i = 0;
    int brace_count = 1;
    while (*p && brace_count > 0) {
        if (*p == '{') { brace_count++; }
        else if (*p == '}') {
            brace_count--;
            if (brace_count == 0) { p++; break; }
        }
        content[i++] = *p;
        p++;
        if (i >= BUFFER_SIZE - 1) {
            fprintf(stderr, "Error: Spawn block too large\n");
            exit(1);
        }
    }
    if (brace_count != 0) {
        fprintf(stderr, "Error: Unmatched '{' in spawn block\n");
        exit(1);
    }
    content[i] = '\0';
    // Skip any trailing characters until semicolon
    while (*p && *p != ';') p++;
    if (*p == ';') p++;
    spawn_counter++;
    char func_name[64];
    sprintf(func_name, "spawn_func_%d", spawn_counter);
    fprintf(out, "\nvoid* %s(void* arg) {\n%s\n    return NULL;\n}\n", func_name, content);
    fprintf(out, "{ pthread_t thread; pthread_create(&thread, NULL, %s, NULL); pthread_join(thread, NULL); }\n", func_name);
}

/* process_if_statement:
   Copies an if statement (including its block and any trailing else clause) verbatim.
   Assumes the if statement is complete within one or more lines.
*/
void process_if_statement(const char **p, FILE *out) {
    int brace_count = 0;
    const char *start = *p;
    while (**p && **p != '{') (*p)++;
    if (**p == '{') {
        brace_count = 1;
        (*p)++; 
        while (brace_count > 0 && **p) {
            if (**p == '{') brace_count++;
            else if (**p == '}') brace_count--;
            (*p)++;
        }
    }
    // Copy any trailing else clause if present.
    while (**p && isspace(**p)) (*p)++;
    if (strncmp(*p, "else", 4) == 0) {
        while (**p && **p != '\n') (*p)++;
    }
    size_t len = *p - start;
    fwrite(start, 1, len, out);
}

/* process_main_body:
   Splits the main body into lines and processes each line.
   If a line starts with "spawn", it accumulates lines until a semicolon is found,
   then calls process_spawn_block_line. It also replaces "print" with "printf"
   and handles "if" statements. Other lines are output verbatim.
*/
void process_main_body(const char *body, FILE *out) {
    char *copy = strdup(body);
    char *line = strtok(copy, "\n");
    char spawn_accum[BUFFER_SIZE] = "";
    int in_spawn = 0;
    while (line != NULL) {
        // Trim leading whitespace
        char *trim = line;
        while (*trim && isspace(*trim)) trim++;
        if (strlen(trim) == 0) {
            line = strtok(NULL, "\n");
            continue;
        }
        if (strncmp(trim, "import", 6) == 0 || strncmp(trim, "module", 6) == 0) {
            line = strtok(NULL, "\n");
            continue;
        }
        if (strncmp(trim, "print", 5) == 0) {
            fprintf(out, "printf%s\n", trim + 5);
            line = strtok(NULL, "\n");
            continue;
        }
        if (strncmp(trim, "spawn", 5) == 0) {
            in_spawn = 1;
            strcpy(spawn_accum, trim);
            if (strchr(trim, ';') != NULL) {
                process_spawn_block_line(spawn_accum, out);
                in_spawn = 0;
                spawn_accum[0] = '\0';
            }
            line = strtok(NULL, "\n");
            continue;
        }
        if (in_spawn) {
            strcat(spawn_accum, "\n");
            strcat(spawn_accum, trim);
            if (strchr(spawn_accum, ';') != NULL) {
                process_spawn_block_line(spawn_accum, out);
                in_spawn = 0;
                spawn_accum[0] = '\0';
            }
            line = strtok(NULL, "\n");
            continue;
        }
        if (strncmp(trim, "if", 2) == 0) {
            process_if_statement((const char **)&trim, out);
            fprintf(out, "\n");
            line = strtok(NULL, "\n");
            continue;
        }
        fprintf(out, "%s\n", trim);
        line = strtok(NULL, "\n");
    }
    free(copy);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.ae output.c\n", argv[0]);
        return 1;
    }
    FILE *fin = fopen(argv[1], "r");
    if (!fin) {
        perror("Error opening input file");
        return 1;
    }
    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    char *source = malloc(fsize + 1);
    if (!source) {
        perror("Memory allocation error");
        fclose(fin);
        return 1;
    }
    fread(source, 1, fsize, fin);
    fclose(fin);
    source[fsize] = '\0';

    // Extract the main function body from the source.
    char *main_sig = strstr(source, MAIN_SIG);
    if (!main_sig) {
        fprintf(stderr, "Error: 'func main()' not found\n");
        free(source);
        return 1;
    }
    char *open_brace = strchr(main_sig, '{');
    if (!open_brace) {
        fprintf(stderr, "Error: '{' not found after func main()\n");
        free(source);
        return 1;
    }
    open_brace++; // Skip '{'
    int brace_count = 1;
    char *p_main = open_brace;
    while (*p_main && brace_count > 0) {
        if (*p_main == '{') brace_count++;
        else if (*p_main == '}') brace_count--;
        p_main++;
    }
    if (brace_count != 0) {
        fprintf(stderr, "Error: Unmatched braces in main function\n");
        free(source);
        return 1;
    }
    size_t main_body_len = (size_t)(p_main - open_brace - 1);
    char *main_body = malloc(main_body_len + 1);
    if (!main_body) {
        perror("Memory allocation error");
        free(source);
        return 1;
    }
    strncpy(main_body, open_brace, main_body_len);
    main_body[main_body_len] = '\0';

    FILE *fout = fopen(argv[2], "w");
    if (!fout) {
        perror("Error opening output file");
        free(source);
        free(main_body);
        return 1;
    }
    fprintf(fout, "#include <stdio.h>\n#include <stdlib.h>\n#include <pthread.h>\n\n");
    fprintf(fout, "int main() {\n");
    process_main_body(main_body, fout);
    fprintf(fout, "\nreturn 0;\n}\n");
    fclose(fout);
    free(source);
    free(main_body);
    return 0;
}
