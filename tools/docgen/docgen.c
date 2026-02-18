/*
 * Aether Documentation Generator
 *
 * Generates searchable HTML documentation from stdlib headers.
 * Similar to Zig's autodoc or Rust's rustdoc.
 *
 * Usage: docgen <std_dir> <output_dir>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_FUNCTIONS 500
#define MAX_LINE 2048
#define MAX_DOC 4096
#define MAX_MODULES 50

typedef struct {
    char name[256];
    char signature[512];       // Original C signature (for reference)
    char aether_sig[512];      // Aether extern declaration
    char return_type[128];     // C return type
    char aether_return[64];    // Aether return type
    char params[512];          // C params
    char aether_params[512];   // Aether params
    char doc[MAX_DOC];
    char module[64];
    int line_number;
} Function;

typedef struct {
    char name[64];
    char description[512];
    Function functions[MAX_FUNCTIONS];
    int function_count;
} Module;

static Module modules[MAX_MODULES];
static int module_count = 0;

// Trim whitespace from string
static char* trim(char* str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Convert C type to Aether type
static const char* c_type_to_aether(const char* c_type) {
    // Trim and normalize
    while (isspace((unsigned char)*c_type)) c_type++;

    // void return type means no return
    if (strcmp(c_type, "void") == 0) return NULL;

    // String types
    if (strstr(c_type, "char*") || strstr(c_type, "char *") ||
        strcmp(c_type, "const char*") == 0 || strcmp(c_type, "const char *") == 0) {
        return "string";
    }

    // Integer types
    if (strcmp(c_type, "int") == 0 || strcmp(c_type, "int32_t") == 0 ||
        strcmp(c_type, "int64_t") == 0 || strcmp(c_type, "size_t") == 0 ||
        strcmp(c_type, "bool") == 0 || strcmp(c_type, "ssize_t") == 0 ||
        strncmp(c_type, "uint", 4) == 0 || strcmp(c_type, "long") == 0) {
        return "int";
    }

    // Float types
    if (strcmp(c_type, "float") == 0 || strcmp(c_type, "double") == 0) {
        return "float";
    }

    // Any pointer type -> ptr
    if (strchr(c_type, '*')) {
        return "ptr";
    }

    // Default to ptr for unknown types (structs, etc.)
    return "ptr";
}

// Parse a single C parameter and convert to Aether
static void parse_c_param_to_aether(const char* c_param, char* name_out, char* type_out) {
    char trimmed[256];
    strncpy(trimmed, c_param, sizeof(trimmed) - 1);
    trimmed[sizeof(trimmed) - 1] = '\0';

    // Trim whitespace
    char* p = trimmed;
    while (isspace((unsigned char)*p)) p++;
    char* end = p + strlen(p) - 1;
    while (end > p && isspace((unsigned char)*end)) *end-- = '\0';

    // Handle "void" - no parameters
    if (strcmp(p, "void") == 0 || strlen(p) == 0) {
        name_out[0] = '\0';
        type_out[0] = '\0';
        return;
    }

    // Find the parameter name (last word that's not a pointer indicator)
    // Work backwards from the end
    char* name_start = NULL;
    char* scan = p + strlen(p) - 1;

    // Skip trailing whitespace and array brackets
    while (scan > p && (isspace((unsigned char)*scan) || *scan == ']')) scan--;
    if (*scan == '[') { // Skip array notation
        while (scan > p && *scan != ' ') scan--;
    }

    // The name is the last identifier
    char* name_end = scan + 1;
    while (scan > p && (isalnum((unsigned char)*scan) || *scan == '_')) scan--;
    name_start = scan + 1;

    // Extract name
    size_t name_len = name_end - name_start;
    if (name_len >= 128) name_len = 127;
    strncpy(name_out, name_start, name_len);
    name_out[name_len] = '\0';

    // Everything before the name is the type
    size_t type_len = name_start - p;
    char c_type[256];
    if (type_len >= sizeof(c_type)) type_len = sizeof(c_type) - 1;
    strncpy(c_type, p, type_len);
    c_type[type_len] = '\0';

    // Trim type
    char* type_trim = c_type;
    while (isspace((unsigned char)*type_trim)) type_trim++;
    end = type_trim + strlen(type_trim) - 1;
    while (end > type_trim && isspace((unsigned char)*end)) *end-- = '\0';

    // Convert to Aether type
    const char* aether_type = c_type_to_aether(type_trim);
    if (aether_type) {
        strncpy(type_out, aether_type, 63);
        type_out[63] = '\0';
    } else {
        type_out[0] = '\0';
    }
}

// Check if line is a function declaration
static int is_function_decl(const char* line) {
    // Skip typedefs, structs, #define, etc.
    if (strncmp(line, "typedef", 7) == 0) return 0;
    if (strncmp(line, "struct", 6) == 0) return 0;
    if (strncmp(line, "#", 1) == 0) return 0;
    if (strncmp(line, "//", 2) == 0) return 0;
    if (strncmp(line, "/*", 2) == 0) return 0;
    if (strncmp(line, "}", 1) == 0) return 0;
    if (strncmp(line, "{", 1) == 0) return 0;
    if (strlen(line) < 5) return 0;

    // Must have parentheses and semicolon
    if (!strchr(line, '(') || !strchr(line, ')')) return 0;
    if (!strchr(line, ';')) return 0;

    // Must start with a type
    if (strncmp(line, "void", 4) == 0 ||
        strncmp(line, "int", 3) == 0 ||
        strncmp(line, "char", 4) == 0 ||
        strncmp(line, "bool", 4) == 0 ||
        strncmp(line, "size_t", 6) == 0 ||
        strncmp(line, "float", 5) == 0 ||
        strncmp(line, "double", 6) == 0 ||
        strncmp(line, "const", 5) == 0 ||
        strncmp(line, "uint", 4) == 0 ||
        strncmp(line, "int64_t", 7) == 0 ||
        strncmp(line, "int32_t", 7) == 0 ||
        strstr(line, "Aether") != NULL ||
        strstr(line, "HashMap") != NULL ||
        strstr(line, "Vector") != NULL ||
        strstr(line, "Set") != NULL ||
        strstr(line, "Json") != NULL ||
        strstr(line, "HTTP") != NULL) {
        return 1;
    }
    return 0;
}

// Parse function declaration
static void parse_function(const char* line, Function* func) {
    strncpy(func->signature, line, sizeof(func->signature) - 1);
    func->signature[sizeof(func->signature) - 1] = '\0';

    // Remove trailing semicolon and newline
    char* semi = strchr(func->signature, ';');
    if (semi) *semi = '\0';

    // Extract return type (everything before function name)
    char* paren = strchr(func->signature, '(');
    if (!paren) return;

    // Find function name (last word before paren)
    char* name_end = paren;
    while (name_end > func->signature && *(name_end - 1) == ' ') name_end--;
    char* name_start = name_end;
    while (name_start > func->signature && *(name_start - 1) != ' ' && *(name_start - 1) != '*') {
        name_start--;
    }

    // Copy function name
    size_t name_len = name_end - name_start;
    if (name_len >= sizeof(func->name)) name_len = sizeof(func->name) - 1;
    strncpy(func->name, name_start, name_len);
    func->name[name_len] = '\0';

    // Copy return type
    size_t ret_len = name_start - func->signature;
    if (ret_len >= sizeof(func->return_type)) ret_len = sizeof(func->return_type) - 1;
    strncpy(func->return_type, func->signature, ret_len);
    func->return_type[ret_len] = '\0';
    trim(func->return_type);

    // Copy parameters
    char* params_start = paren + 1;
    char* params_end = strchr(params_start, ')');
    if (params_end) {
        size_t params_len = params_end - params_start;
        if (params_len >= sizeof(func->params)) params_len = sizeof(func->params) - 1;
        strncpy(func->params, params_start, params_len);
        func->params[params_len] = '\0';
        trim(func->params);
    }

    // === Generate Aether signature ===

    // Convert return type to Aether
    const char* aether_ret = c_type_to_aether(func->return_type);
    if (aether_ret) {
        strncpy(func->aether_return, aether_ret, sizeof(func->aether_return) - 1);
    } else {
        func->aether_return[0] = '\0';
    }

    // Convert parameters to Aether
    func->aether_params[0] = '\0';
    if (strlen(func->params) > 0 && strcmp(func->params, "void") != 0) {
        // Split params by comma
        char params_copy[512];
        strncpy(params_copy, func->params, sizeof(params_copy) - 1);
        params_copy[sizeof(params_copy) - 1] = '\0';

        char* param = strtok(params_copy, ",");
        int first = 1;
        while (param) {
            char param_name[128], param_type[64];
            parse_c_param_to_aether(param, param_name, param_type);

            if (strlen(param_name) > 0 && strlen(param_type) > 0) {
                if (!first) {
                    strncat(func->aether_params, ", ", sizeof(func->aether_params) - strlen(func->aether_params) - 1);
                }
                strncat(func->aether_params, param_name, sizeof(func->aether_params) - strlen(func->aether_params) - 1);
                strncat(func->aether_params, ": ", sizeof(func->aether_params) - strlen(func->aether_params) - 1);
                strncat(func->aether_params, param_type, sizeof(func->aether_params) - strlen(func->aether_params) - 1);
                first = 0;
            }
            param = strtok(NULL, ",");
        }
    }

    // Build full Aether signature
    snprintf(func->aether_sig, sizeof(func->aether_sig), "extern %s(%s)%s%s",
             func->name,
             func->aether_params,
             strlen(func->aether_return) > 0 ? " -> " : "",
             func->aether_return);
}

// Parse a header file
static void parse_header(const char* filepath, const char* module_name) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Warning: Cannot open %s\n", filepath);
        return;
    }

    // Find or create module
    Module* mod = NULL;
    for (int i = 0; i < module_count; i++) {
        if (strcmp(modules[i].name, module_name) == 0) {
            mod = &modules[i];
            break;
        }
    }
    if (!mod && module_count < MAX_MODULES) {
        mod = &modules[module_count++];
        strncpy(mod->name, module_name, sizeof(mod->name) - 1);
        mod->function_count = 0;
    }
    if (!mod) {
        fclose(f);
        return;
    }

    char line[MAX_LINE];
    char pending_doc[MAX_DOC] = "";
    int line_number = 0;

    while (fgets(line, sizeof(line), f)) {
        line_number++;
        char* trimmed = trim(line);

        // Collect comments as documentation
        if (strncmp(trimmed, "//", 2) == 0) {
            // Skip include guard comments
            if (strstr(trimmed, "#ifndef") || strstr(trimmed, "#define") || strstr(trimmed, "#endif")) {
                continue;
            }
            const char* comment = trimmed + 2;
            while (*comment == ' ') comment++;
            if (strlen(pending_doc) + strlen(comment) + 2 < MAX_DOC) {
                if (strlen(pending_doc) > 0) strcat(pending_doc, " ");
                strcat(pending_doc, comment);
            }
            continue;
        }

        // Check for function declaration
        if (is_function_decl(trimmed)) {
            if (mod->function_count < MAX_FUNCTIONS) {
                Function* func = &mod->functions[mod->function_count++];
                parse_function(trimmed, func);
                strncpy(func->doc, pending_doc, sizeof(func->doc) - 1);
                strncpy(func->module, module_name, sizeof(func->module) - 1);
                func->line_number = line_number;
            }
            pending_doc[0] = '\0';
        } else if (strlen(trimmed) > 0 && strncmp(trimmed, "//", 2) != 0) {
            // Non-comment, non-function line clears pending doc
            pending_doc[0] = '\0';
        }
    }

    fclose(f);
}

// Escape HTML special characters
static void html_escape(const char* src, char* dest, size_t dest_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dest_size - 6; i++) {
        switch (src[i]) {
            case '<': strcpy(dest + j, "&lt;"); j += 4; break;
            case '>': strcpy(dest + j, "&gt;"); j += 4; break;
            case '&': strcpy(dest + j, "&amp;"); j += 5; break;
            case '"': strcpy(dest + j, "&quot;"); j += 6; break;
            default: dest[j++] = src[i]; break;
        }
    }
    dest[j] = '\0';
}

// Module descriptions
static const char* get_module_description(const char* name) {
    if (strcmp(name, "string") == 0) return "String manipulation and formatting";
    if (strcmp(name, "collections") == 0) return "Lists, maps, sets, and data structures";
    if (strcmp(name, "net") == 0) return "HTTP client, server, and networking";
    if (strcmp(name, "json") == 0) return "JSON parsing and serialization";
    if (strcmp(name, "fs") == 0) return "File system operations";
    if (strcmp(name, "io") == 0) return "Input/output and console";
    if (strcmp(name, "math") == 0) return "Mathematical functions";
    if (strcmp(name, "log") == 0) return "Logging and diagnostics";
    return "";
}

// Generate the main index.html
static void generate_index(const char* output_dir) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/index.html", output_dir);

    FILE* f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create %s\n", filepath);
        return;
    }

    fprintf(f, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
    fprintf(f, "  <meta charset=\"UTF-8\">\n");
    fprintf(f, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(f, "  <title>Aether Standard Library</title>\n");
    fprintf(f, "  <link rel=\"stylesheet\" href=\"style.css\">\n");
    fprintf(f, "</head>\n<body>\n");

    // Sidebar
    fprintf(f, "<nav class=\"sidebar\">\n");
    fprintf(f, "  <div class=\"sidebar-header\">\n");
    fprintf(f, "    <h1>Aether</h1>\n");
    fprintf(f, "    <p class=\"tagline\">Standard Library</p>\n");
    fprintf(f, "  </div>\n");
    fprintf(f, "  <input type=\"text\" id=\"search\" placeholder=\"Search functions...\" onkeyup=\"search()\">\n");
    fprintf(f, "  <h2>Modules</h2>\n");
    fprintf(f, "  <ul class=\"module-list\">\n");

    for (int i = 0; i < module_count; i++) {
        fprintf(f, "    <li><a href=\"%s.html\">%s</a></li>\n",
                modules[i].name, modules[i].name);
    }

    fprintf(f, "  </ul>\n</nav>\n");

    // Main content
    fprintf(f, "<main class=\"content\">\n");
    fprintf(f, "  <div class=\"hero\">\n");
    fprintf(f, "    <h1>Standard Library</h1>\n");
    fprintf(f, "    <p>Built-in functions for strings, collections, networking, file I/O, and more.</p>\n");
    fprintf(f, "  </div>\n");

    fprintf(f, "  <div class=\"module-grid\">\n");

    for (int i = 0; i < module_count; i++) {
        const char* desc = get_module_description(modules[i].name);
        fprintf(f, "    <a href=\"%s.html\" class=\"module-card\">\n", modules[i].name);
        fprintf(f, "      <h3>%s</h3>\n", modules[i].name);
        fprintf(f, "      <p>%s</p>\n", desc);
        fprintf(f, "    </a>\n");
    }

    fprintf(f, "  </div>\n");

    // Quick reference - all functions (hidden by default, shown on search)
    fprintf(f, "  <div id=\"search-results\" class=\"search-results hidden\">\n");
    fprintf(f, "    <h2>Search Results</h2>\n");
    fprintf(f, "    <ul class=\"function-list\" id=\"all-functions\">\n");

    for (int i = 0; i < module_count; i++) {
        for (int j = 0; j < modules[i].function_count; j++) {
            Function* func = &modules[i].functions[j];
            char escaped_name[512];
            html_escape(func->name, escaped_name, sizeof(escaped_name));
            fprintf(f, "      <li class=\"function-item\" data-name=\"%s\"><a href=\"%s.html#%s\"><span class=\"fn-name\">%s</span><span class=\"fn-module\">%s</span></a></li>\n",
                    escaped_name, modules[i].name, escaped_name, escaped_name, modules[i].name);
        }
    }

    fprintf(f, "    </ul>\n");
    fprintf(f, "  </div>\n");
    fprintf(f, "</main>\n");

    // Search script
    fprintf(f, "<script src=\"search.js\"></script>\n");
    fprintf(f, "</body>\n</html>\n");

    fclose(f);
    printf("Generated: %s\n", filepath);
}

// Generate module page
static void generate_module_page(const char* output_dir, Module* mod) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s.html", output_dir, mod->name);

    FILE* f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create %s\n", filepath);
        return;
    }

    const char* mod_desc = get_module_description(mod->name);

    fprintf(f, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
    fprintf(f, "  <meta charset=\"UTF-8\">\n");
    fprintf(f, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(f, "  <title>%s - Aether</title>\n", mod->name);
    fprintf(f, "  <link rel=\"stylesheet\" href=\"style.css\">\n");
    fprintf(f, "</head>\n<body>\n");

    // Sidebar
    fprintf(f, "<nav class=\"sidebar\">\n");
    fprintf(f, "  <div class=\"sidebar-header\">\n");
    fprintf(f, "    <h1><a href=\"index.html\">Aether</a></h1>\n");
    fprintf(f, "    <p class=\"tagline\">Standard Library</p>\n");
    fprintf(f, "  </div>\n");
    fprintf(f, "  <input type=\"text\" id=\"search\" placeholder=\"Search %s...\" onkeyup=\"searchModule()\">\n", mod->name);
    fprintf(f, "  <h2>Functions</h2>\n");
    fprintf(f, "  <ul class=\"function-nav\">\n");

    for (int i = 0; i < mod->function_count; i++) {
        // Show shorter function names (strip aether_ prefix)
        const char* display_name = mod->functions[i].name;
        if (strncmp(display_name, "aether_", 7) == 0) {
            display_name += 7;
        }
        char escaped_name[512], escaped_display[512];
        html_escape(mod->functions[i].name, escaped_name, sizeof(escaped_name));
        html_escape(display_name, escaped_display, sizeof(escaped_display));
        fprintf(f, "    <li><a href=\"#%s\">%s</a></li>\n", escaped_name, escaped_display);
    }

    fprintf(f, "  </ul>\n");
    fprintf(f, "  <hr>\n");
    fprintf(f, "  <h2>Modules</h2>\n");
    fprintf(f, "  <ul class=\"module-list\">\n");

    for (int i = 0; i < module_count; i++) {
        const char* active = (strcmp(modules[i].name, mod->name) == 0) ? " class=\"active\"" : "";
        fprintf(f, "    <li%s><a href=\"%s.html\">%s</a></li>\n", active, modules[i].name, modules[i].name);
    }

    fprintf(f, "  </ul>\n</nav>\n");

    // Main content
    fprintf(f, "<main class=\"content\">\n");
    fprintf(f, "  <div class=\"module-header\">\n");
    fprintf(f, "    <h1>%s</h1>\n", mod->name);
    fprintf(f, "    <p class=\"module-desc\">%s</p>\n", mod_desc);
    fprintf(f, "  </div>\n");

    fprintf(f, "  <div class=\"functions\" id=\"functions\">\n");

    for (int i = 0; i < mod->function_count; i++) {
        Function* func = &mod->functions[i];

        // Strip aether_ prefix for display
        const char* display_name = func->name;
        if (strncmp(display_name, "aether_", 7) == 0) {
            display_name += 7;
        }

        char escaped_name[512], escaped_display[512], escaped_doc[MAX_DOC * 2];
        html_escape(func->name, escaped_name, sizeof(escaped_name));
        html_escape(display_name, escaped_display, sizeof(escaped_display));
        html_escape(func->doc, escaped_doc, sizeof(escaped_doc));

        fprintf(f, "    <div class=\"function\" id=\"%s\" data-name=\"%s\">\n", escaped_name, escaped_name);
        fprintf(f, "      <h3 class=\"function-name\">%s</h3>\n", escaped_display);

        // Show usage example with clean name (no aether_ prefix)
        fprintf(f, "      <pre class=\"usage\"><code>%s(", display_name);

        // Add parameter placeholders
        if (strlen(func->aether_params) > 0) {
            // Parse params and show just names
            char params_copy[512];
            strncpy(params_copy, func->aether_params, sizeof(params_copy) - 1);
            char* p = params_copy;
            int first = 1;
            while (*p) {
                // Find param name (before the colon)
                char* colon = strchr(p, ':');
                if (colon) {
                    *colon = '\0';
                    if (!first) fprintf(f, ", ");
                    fprintf(f, "%s", p);
                    first = 0;
                    // Skip to next param
                    p = colon + 1;
                    char* comma = strchr(p, ',');
                    if (comma) {
                        p = comma + 1;
                        while (*p == ' ') p++;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
        }
        fprintf(f, ")</code></pre>\n");

        if (strlen(func->doc) > 0) {
            fprintf(f, "      <p class=\"doc\">%s</p>\n", escaped_doc);
        }


        fprintf(f, "    </div>\n");
    }

    fprintf(f, "  </div>\n");
    fprintf(f, "</main>\n");

    // Search script
    fprintf(f, "<script src=\"search.js\"></script>\n");
    fprintf(f, "</body>\n</html>\n");

    fclose(f);
    printf("Generated: %s\n", filepath);
}

// Generate CSS
static void generate_css(const char* output_dir) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/style.css", output_dir);

    FILE* f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create %s\n", filepath);
        return;
    }

    fprintf(f,
"* { margin: 0; padding: 0; box-sizing: border-box; }\n\n"

"body {\n"
"  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Inter', Roboto, sans-serif;\n"
"  background: #0a0a0a;\n"
"  color: #d4d4d4;\n"
"  line-height: 1.6;\n"
"  display: flex;\n"
"  min-height: 100vh;\n"
"}\n\n"

"a { color: inherit; text-decoration: none; }\n\n"

/* Sidebar */
".sidebar {\n"
"  width: 240px;\n"
"  background: #111;\n"
"  padding: 20px 16px;\n"
"  position: fixed;\n"
"  height: 100vh;\n"
"  overflow-y: auto;\n"
"  border-right: 1px solid #222;\n"
"}\n\n"

".sidebar-header h1 {\n"
"  font-size: 1.1rem;\n"
"  font-weight: 600;\n"
"  color: #fff;\n"
"}\n\n"

".sidebar-header .tagline {\n"
"  font-size: 0.75rem;\n"
"  color: #666;\n"
"  margin-top: 2px;\n"
"}\n\n"

"#search {\n"
"  width: 100%%;\n"
"  padding: 8px 12px;\n"
"  border: 1px solid #333;\n"
"  border-radius: 6px;\n"
"  background: #0a0a0a;\n"
"  color: #d4d4d4;\n"
"  font-size: 0.85rem;\n"
"  margin: 16px 0;\n"
"}\n\n"

"#search:focus { outline: none; border-color: #4a9eff; }\n\n"

".sidebar h2 {\n"
"  font-size: 0.7rem;\n"
"  font-weight: 500;\n"
"  color: #555;\n"
"  text-transform: uppercase;\n"
"  letter-spacing: 0.08em;\n"
"  margin: 16px 0 8px;\n"
"}\n\n"

".module-list, .function-nav { list-style: none; }\n\n"

".module-list li a, .function-nav li a {\n"
"  display: block;\n"
"  padding: 6px 10px;\n"
"  border-radius: 4px;\n"
"  font-size: 0.85rem;\n"
"  color: #888;\n"
"  transition: all 0.1s;\n"
"}\n\n"

".function-nav li a {\n"
"  font-family: 'SF Mono', 'Fira Code', monospace;\n"
"  font-size: 0.8rem;\n"
"  padding: 4px 10px;\n"
"}\n\n"

".module-list li a:hover, .function-nav li a:hover {\n"
"  background: #1a1a1a;\n"
"  color: #fff;\n"
"}\n\n"

".module-list .active a {\n"
"  background: #1a2a3a;\n"
"  color: #4a9eff;\n"
"}\n\n"

"hr { border: none; border-top: 1px solid #222; margin: 16px 0; }\n\n"

/* Main content */
".content {\n"
"  margin-left: 240px;\n"
"  padding: 32px 40px;\n"
"  max-width: 900px;\n"
"  width: 100%%;\n"
"}\n\n"

".hero { margin-bottom: 40px; }\n"
".hero h1 { font-size: 1.5rem; font-weight: 600; color: #fff; margin-bottom: 8px; }\n"
".hero p { color: #666; font-size: 0.95rem; }\n\n"

".module-header { margin-bottom: 32px; }\n"
".module-header h1 { font-size: 1.4rem; font-weight: 600; color: #fff; margin-bottom: 4px; }\n"
".module-desc { color: #666; font-size: 0.9rem; }\n\n"

/* Module grid */
".module-grid {\n"
"  display: grid;\n"
"  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));\n"
"  gap: 12px;\n"
"}\n\n"

".module-card {\n"
"  display: block;\n"
"  background: #141414;\n"
"  padding: 16px 18px;\n"
"  border-radius: 8px;\n"
"  border: 1px solid #222;\n"
"  transition: all 0.15s;\n"
"}\n\n"

".module-card:hover { border-color: #333; background: #181818; }\n"
".module-card h3 { font-size: 0.95rem; color: #fff; margin-bottom: 4px; }\n"
".module-card p { font-size: 0.8rem; color: #666; }\n\n"

/* Search results */
".search-results { margin-top: 32px; }\n"
".search-results h2 { font-size: 0.85rem; color: #666; margin-bottom: 16px; }\n\n"

".function-list { list-style: none; }\n"
".function-item a {\n"
"  display: flex;\n"
"  justify-content: space-between;\n"
"  padding: 8px 12px;\n"
"  border-radius: 6px;\n"
"  margin-bottom: 2px;\n"
"  transition: background 0.1s;\n"
"}\n"
".function-item a:hover { background: #181818; }\n"
".fn-name { font-family: 'SF Mono', monospace; font-size: 0.85rem; color: #d4d4d4; }\n"
".fn-module { font-size: 0.75rem; color: #555; }\n\n"

/* Function cards */
".function {\n"
"  background: #141414;\n"
"  padding: 20px;\n"
"  border-radius: 8px;\n"
"  margin-bottom: 12px;\n"
"  border: 1px solid #222;\n"
"}\n\n"

".function-name {\n"
"  font-size: 1rem;\n"
"  font-weight: 500;\n"
"  color: #fff;\n"
"  margin-bottom: 8px;\n"
"}\n\n"

".usage {\n"
"  background: #0a0a0a;\n"
"  padding: 10px 14px;\n"
"  border-radius: 6px;\n"
"  margin-bottom: 10px;\n"
"  overflow-x: auto;\n"
"}\n\n"

".usage code {\n"
"  font-family: 'SF Mono', 'Fira Code', monospace;\n"
"  font-size: 0.85rem;\n"
"  color: #7dd3fc;\n"
"}\n\n"

".doc { color: #888; font-size: 0.9rem; }\n\n"

".hidden { display: none !important; }\n\n"

"@media (max-width: 768px) {\n"
"  .sidebar { display: none; }\n"
"  .content { margin-left: 0; padding: 20px; }\n"
"}\n"
    );

    fclose(f);
    printf("Generated: %s\n", filepath);
}

// Generate search.js
static void generate_search_js(const char* output_dir) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/search.js", output_dir);

    FILE* f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create %s\n", filepath);
        return;
    }

    fprintf(f,
"// Index page search - shows results, hides module grid\n"
"function search() {\n"
"  const query = document.getElementById('search').value.toLowerCase().trim();\n"
"  const results = document.getElementById('search-results');\n"
"  const grid = document.querySelector('.module-grid');\n"
"  const items = document.querySelectorAll('.function-item');\n"
"  \n"
"  if (query.length === 0) {\n"
"    results.classList.add('hidden');\n"
"    grid.classList.remove('hidden');\n"
"    return;\n"
"  }\n"
"  \n"
"  results.classList.remove('hidden');\n"
"  grid.classList.add('hidden');\n"
"  \n"
"  items.forEach(item => {\n"
"    const name = item.dataset.name.toLowerCase();\n"
"    item.classList.toggle('hidden', !name.includes(query));\n"
"  });\n"
"}\n\n"

"// Module page search - filters functions\n"
"function searchModule() {\n"
"  const query = document.getElementById('search').value.toLowerCase().trim();\n"
"  const functions = document.querySelectorAll('.function');\n"
"  const navItems = document.querySelectorAll('.function-nav li');\n"
"  \n"
"  functions.forEach(func => {\n"
"    const name = func.dataset.name.toLowerCase();\n"
"    func.classList.toggle('hidden', query.length > 0 && !name.includes(query));\n"
"  });\n"
"  \n"
"  navItems.forEach(item => {\n"
"    const link = item.querySelector('a');\n"
"    if (link) {\n"
"      const name = link.textContent.toLowerCase();\n"
"      item.classList.toggle('hidden', query.length > 0 && !name.includes(query));\n"
"    }\n"
"  });\n"
"}\n\n"

"// Highlight active function on scroll\n"
"let ticking = false;\n"
"document.addEventListener('scroll', () => {\n"
"  if (!ticking) {\n"
"    requestAnimationFrame(() => {\n"
"      const functions = document.querySelectorAll('.function');\n"
"      const navLinks = document.querySelectorAll('.function-nav a');\n"
"      let current = '';\n"
"      functions.forEach(func => {\n"
"        const rect = func.getBoundingClientRect();\n"
"        if (rect.top <= 80) current = func.id;\n"
"      });\n"
"      navLinks.forEach(link => {\n"
"        link.parentElement.classList.remove('active');\n"
"        if (link.getAttribute('href') === '#' + current) {\n"
"          link.parentElement.classList.add('active');\n"
"        }\n"
"      });\n"
"      ticking = false;\n"
"    });\n"
"    ticking = true;\n"
"  }\n"
"});\n"
    );

    fclose(f);
    printf("Generated: %s\n", filepath);
}

// Scan directory for header files
static void scan_directory(const char* dir_path, const char* module_name) {
    DIR* dir = opendir(dir_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        // Check if it's a .h file
        const char* ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".h") == 0) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);
            parse_header(filepath, module_name);
        }
    }

    closedir(dir);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <std_dir> <output_dir>\n", argv[0]);
        fprintf(stderr, "Example: %s std docs/api\n", argv[0]);
        return 1;
    }

    const char* std_dir = argv[1];
    const char* output_dir = argv[2];

    // Create output directory
    mkdir(output_dir, 0755);

    // Scan std/ subdirectories
    DIR* dir = opendir(std_dir);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open %s\n", std_dir);
        return 1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char subdir_path[512];
        snprintf(subdir_path, sizeof(subdir_path), "%s/%s", std_dir, entry->d_name);

        struct stat st;
        if (stat(subdir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            printf("Scanning module: %s\n", entry->d_name);
            scan_directory(subdir_path, entry->d_name);
        }
    }
    closedir(dir);

    printf("\nParsed %d modules:\n", module_count);
    for (int i = 0; i < module_count; i++) {
        printf("  - %s: %d functions\n", modules[i].name, modules[i].function_count);
    }

    // Generate output
    printf("\nGenerating documentation...\n");
    generate_css(output_dir);
    generate_search_js(output_dir);
    generate_index(output_dir);

    for (int i = 0; i < module_count; i++) {
        generate_module_page(output_dir, &modules[i]);
    }

    printf("\nDone! Open %s/index.html in a browser.\n", output_dir);
    return 0;
}
