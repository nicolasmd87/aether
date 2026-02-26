#include "aether_module.h"
#include "aether_error.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <io.h>
    #define access _access
    #define F_OK 0
#else
    #include <unistd.h>
#endif

ModuleRegistry* global_module_registry = NULL;

// Module management
void module_registry_init() {
    if (!global_module_registry) {
        global_module_registry = (ModuleRegistry*)malloc(sizeof(ModuleRegistry));
        global_module_registry->modules = NULL;
        global_module_registry->module_count = 0;
        global_module_registry->module_capacity = 0;
    }
}

void module_registry_shutdown() {
    if (global_module_registry) {
        for (int i = 0; i < global_module_registry->module_count; i++) {
            module_free(global_module_registry->modules[i]);
        }
        free(global_module_registry->modules);
        free(global_module_registry);
        global_module_registry = NULL;
    }
}

AetherModule* module_create(const char* name, const char* file_path) {
    AetherModule* module = (AetherModule*)malloc(sizeof(AetherModule));
    module->name = strdup(name);
    module->file_path = strdup(file_path);
    module->ast = NULL;
    module->exports = NULL;
    module->export_count = 0;
    module->imports = NULL;
    module->import_count = 0;
    return module;
}

void module_free(AetherModule* module) {
    if (!module) return;
    
    free(module->name);
    free(module->file_path);
    
    if (module->ast) {
        free_ast_node(module->ast);
    }
    
    for (int i = 0; i < module->export_count; i++) {
        free(module->exports[i]);
    }
    free(module->exports);
    
    for (int i = 0; i < module->import_count; i++) {
        free(module->imports[i]);
    }
    free(module->imports);
    
    free(module);
}

// Module registration
void module_register(AetherModule* module) {
    if (!global_module_registry) {
        module_registry_init();
    }
    
    // Check if module already exists
    for (int i = 0; i < global_module_registry->module_count; i++) {
        if (strcmp(global_module_registry->modules[i]->name, module->name) == 0) {
            fprintf(stderr, "Warning: Module '%s' already registered, replacing\n", module->name);
            module_free(global_module_registry->modules[i]);
            global_module_registry->modules[i] = module;
            return;
        }
    }
    
    // Grow array if needed
    if (global_module_registry->module_count >= global_module_registry->module_capacity) {
        int new_capacity = global_module_registry->module_capacity == 0 ? 8 : global_module_registry->module_capacity * 2;
        AetherModule** new_modules = (AetherModule**)realloc(
            global_module_registry->modules,
            new_capacity * sizeof(AetherModule*)
        );
        if (!new_modules) return;
        global_module_registry->modules = new_modules;
        global_module_registry->module_capacity = new_capacity;
    }
    
    global_module_registry->modules[global_module_registry->module_count++] = module;
}

AetherModule* module_find(const char* name) {
    if (!global_module_registry) return NULL;
    
    for (int i = 0; i < global_module_registry->module_count; i++) {
        if (strcmp(global_module_registry->modules[i]->name, name) == 0) {
            return global_module_registry->modules[i];
        }
    }
    
    return NULL;
}

// Import/export handling
void module_add_export(AetherModule* module, const char* symbol) {
    if (!module) return;
    
    // Check if already exported
    for (int i = 0; i < module->export_count; i++) {
        if (strcmp(module->exports[i], symbol) == 0) {
            return;
        }
    }
    
    char** new_exports = (char**)realloc(module->exports, (module->export_count + 1) * sizeof(char*));
    if (!new_exports) return;
    module->exports = new_exports;
    module->exports[module->export_count++] = strdup(symbol);
}

void module_add_import(AetherModule* module, const char* module_name) {
    if (!module) return;
    
    // Check if already imported
    for (int i = 0; i < module->import_count; i++) {
        if (strcmp(module->imports[i], module_name) == 0) {
            return;
        }
    }
    
    char** new_imports = (char**)realloc(module->imports, (module->import_count + 1) * sizeof(char*));
    if (!new_imports) return;
    module->imports = new_imports;
    module->imports[module->import_count++] = strdup(module_name);
}

int module_is_exported(AetherModule* module, const char* symbol) {
    if (!module) return 0;
    
    for (int i = 0; i < module->export_count; i++) {
        if (strcmp(module->exports[i], symbol) == 0) {
            return 1;
        }
    }
    
    return 0;
}

// Package manifest (aether.toml)
PackageManifest* package_manifest_load(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open package manifest: %s\n", path);
        return NULL;
    }
    
    PackageManifest* manifest = (PackageManifest*)malloc(sizeof(PackageManifest));
    manifest->package_name = NULL;
    manifest->version = NULL;
    manifest->author = NULL;
    manifest->dependencies = NULL;
    manifest->dependency_count = 0;
    
    // Simple TOML parser (basic implementation)
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') continue;
        
        // Parse key = "value"
        char* equals = strchr(line, '=');
        if (equals) {
            *equals = '\0';
            char* key = line;
            char* value = equals + 1;
            
            // Trim whitespace
            while (*key == ' ') key++;
            while (*value == ' ') value++;
            
            // Remove quotes
            if (*value == '"') {
                value++;
                char* end = strchr(value, '"');
                if (end) *end = '\0';
            }
            
            if (strcmp(key, "name") == 0) {
                manifest->package_name = strdup(value);
            } else if (strcmp(key, "version") == 0) {
                manifest->version = strdup(value);
            } else if (strcmp(key, "author") == 0) {
                manifest->author = strdup(value);
            }
        }
    }
    
    fclose(file);
    return manifest;
}

void package_manifest_free(PackageManifest* manifest) {
    if (!manifest) return;
    
    free(manifest->package_name);
    free(manifest->version);
    free(manifest->author);
    
    for (int i = 0; i < manifest->dependency_count; i++) {
        free(manifest->dependencies[i]);
    }
    free(manifest->dependencies);
    
    free(manifest);
}

// Dependency Graph Implementation

DependencyGraph* dependency_graph_create() {
    DependencyGraph* graph = malloc(sizeof(DependencyGraph));
    graph->nodes = NULL;
    graph->node_count = 0;
    return graph;
}

void dependency_graph_free(DependencyGraph* graph) {
    if (!graph) return;
    
    for (int i = 0; i < graph->node_count; i++) {
        DependencyNode* node = graph->nodes[i];
        free(node->module_name);
        free(node->dependencies);
        free(node);
    }
    free(graph->nodes);
    free(graph);
}

DependencyNode* dependency_graph_find_node(DependencyGraph* graph, const char* module_name) {
    if (!graph) return NULL;
    
    for (int i = 0; i < graph->node_count; i++) {
        if (strcmp(graph->nodes[i]->module_name, module_name) == 0) {
            return graph->nodes[i];
        }
    }
    return NULL;
}

DependencyNode* dependency_graph_add_node(DependencyGraph* graph, const char* module_name) {
    if (!graph) return NULL;
    
    // Check if node already exists
    DependencyNode* existing = dependency_graph_find_node(graph, module_name);
    if (existing) return existing;
    
    // Create new node
    DependencyNode* node = malloc(sizeof(DependencyNode));
    node->module_name = strdup(module_name);
    node->dependencies = NULL;
    node->dependency_count = 0;
    node->visited = 0;
    node->in_stack = 0;
    
    // Add to graph
    DependencyNode** new_nodes = realloc(graph->nodes, (graph->node_count + 1) * sizeof(DependencyNode*));
    if (!new_nodes) { free(node); return NULL; }
    graph->nodes = new_nodes;
    graph->nodes[graph->node_count++] = node;
    
    return node;
}

void dependency_graph_add_edge(DependencyGraph* graph, const char* from, const char* to) {
    if (!graph) return;
    
    DependencyNode* from_node = dependency_graph_add_node(graph, from);
    DependencyNode* to_node = dependency_graph_add_node(graph, to);
    
    // Check if edge already exists
    for (int i = 0; i < from_node->dependency_count; i++) {
        if (from_node->dependencies[i] == to_node) {
            return;
        }
    }
    
    // Add edge
    DependencyNode** new_deps = realloc(from_node->dependencies,
                                     (from_node->dependency_count + 1) * sizeof(DependencyNode*));
    if (!new_deps) return;
    from_node->dependencies = new_deps;
    from_node->dependencies[from_node->dependency_count++] = to_node;
}

// DFS helper for cycle detection
static int dfs_has_cycle(DependencyNode* node) {
    if (node->in_stack) {
        // Found a back edge (cycle)
        return 1;
    }
    
    if (node->visited) {
        // Already checked this node
        return 0;
    }
    
    node->visited = 1;
    node->in_stack = 1;
    
    // Visit all dependencies
    for (int i = 0; i < node->dependency_count; i++) {
        if (dfs_has_cycle(node->dependencies[i])) {
            return 1;
        }
    }
    
    node->in_stack = 0;
    return 0;
}

int dependency_graph_has_cycle(DependencyGraph* graph) {
    if (!graph) return 0;
    
    // Reset visited flags
    for (int i = 0; i < graph->node_count; i++) {
        graph->nodes[i]->visited = 0;
        graph->nodes[i]->in_stack = 0;
    }
    
    // Run DFS from each unvisited node
    for (int i = 0; i < graph->node_count; i++) {
        if (!graph->nodes[i]->visited) {
            if (dfs_has_cycle(graph->nodes[i])) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "circular import dependency involving module '%s'",
                    graph->nodes[i]->module_name);
                aether_error_simple(msg, 0, 0);
                return 1;
            }
        }
    }

    return 0;
}

// --- Module Orchestration ---

// Resolve a stdlib module name (e.g., "fs") to a file path.
char* module_resolve_stdlib_path(const char* module_name) {
    char path[512];

    // Try 1: Local development path (relative to CWD)
    snprintf(path, sizeof(path), "std/%s/module.ae", module_name);
    if (access(path, F_OK) == 0) return strdup(path);

    // Try 2: Installed path via AETHER_HOME
    const char* aether_home = getenv("AETHER_HOME");
    if (aether_home) {
        snprintf(path, sizeof(path), "%s/share/aether/std/%s/module.ae", aether_home, module_name);
        if (access(path, F_OK) == 0) return strdup(path);
    }

    // Try 3: Common install locations
    snprintf(path, sizeof(path), "/usr/local/share/aether/std/%s/module.ae", module_name);
    if (access(path, F_OK) == 0) return strdup(path);

    snprintf(path, sizeof(path), "%s/.aether/share/aether/std/%s/module.ae",
             getenv("HOME") ? getenv("HOME") : "", module_name);
    if (access(path, F_OK) == 0) return strdup(path);

    return NULL;
}

// Resolve a local module path (e.g., "mypackage.utils") to a file path.
char* module_resolve_local_path(const char* module_path) {
    char converted[512];
    char path[sizeof(converted) + 16];

    // Convert dots to slashes
    strncpy(converted, module_path, sizeof(converted) - 1);
    converted[sizeof(converted) - 1] = '\0';
    for (char* p = converted; *p; p++) {
        if (*p == '.') *p = '/';
    }

    // Try 1: lib/module_path/module.ae
    snprintf(path, sizeof(path), "lib/%s/module.ae", converted);
    if (access(path, F_OK) == 0) return strdup(path);

    // Try 2: lib/module_path.ae
    snprintf(path, sizeof(path), "lib/%s.ae", converted);
    if (access(path, F_OK) == 0) return strdup(path);

    // Try 3: src/module_path/module.ae
    snprintf(path, sizeof(path), "src/%s/module.ae", converted);
    if (access(path, F_OK) == 0) return strdup(path);

    // Try 4: src/module_path.ae
    snprintf(path, sizeof(path), "src/%s.ae", converted);
    if (access(path, F_OK) == 0) return strdup(path);

    // Try 5: module_path/module.ae (project root)
    snprintf(path, sizeof(path), "%s/module.ae", converted);
    if (access(path, F_OK) == 0) return strdup(path);

    // Try 6: module_path.ae (single file in root)
    snprintf(path, sizeof(path), "%s.ae", converted);
    if (access(path, F_OK) == 0) return strdup(path);

    return NULL;
}

// Parse a module file into an AST. Saves/restores lexer state.
ASTNode* module_parse_file(const char* file_path) {
    FILE* f = fopen(file_path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(source, 1, size, f);
    fclose(f);
    source[bytes_read] = '\0';

    // Save lexer state (lexer is global)
    LexerState saved;
    lexer_save(&saved);

    // Tokenize
    lexer_init(source);
    Token* tokens[MAX_MODULE_TOKENS];
    int token_count = 0;

    while (token_count < MAX_MODULE_TOKENS - 1) {
        Token* token = next_token();
        tokens[token_count++] = token;
        if (token->type == TOKEN_EOF || token->type == TOKEN_ERROR) break;
    }

    // Parse
    Parser* parser = create_parser(tokens, token_count);
    ASTNode* ast = parse_program(parser);

    // Cleanup
    for (int i = 0; i < token_count; i++) {
        free_token(tokens[i]);
    }
    free_parser(parser);
    free(source);

    // Restore lexer state
    lexer_restore(&saved);

    return ast;
}

// Recursive helper: load a single module and its transitive imports
static int orchestrate_module(const char* module_name, const char* file_path,
                              DependencyGraph* graph) {
    // Already loaded? Skip.
    if (module_find(module_name)) return 1;

    // Parse the file
    ASTNode* ast = module_parse_file(file_path);
    if (!ast) return 1;  // Graceful: file may exist but be empty/invalid

    // Create and register module
    AetherModule* mod = module_create(module_name, file_path);
    mod->ast = ast;
    module_register(mod);

    // Add node to dependency graph
    dependency_graph_add_node(graph, module_name);

    // Recursively process this module's imports
    for (int i = 0; i < ast->child_count; i++) {
        ASTNode* child = ast->children[i];
        if (child->type != AST_IMPORT_STATEMENT || !child->value) continue;

        const char* sub_path = child->value;
        char* sub_file = NULL;

        if (strncmp(sub_path, "std.", 4) == 0) {
            sub_file = module_resolve_stdlib_path(sub_path + 4);
        } else {
            sub_file = module_resolve_local_path(sub_path);
        }

        dependency_graph_add_edge(graph, module_name, sub_path);
        module_add_import(mod, sub_path);

        if (sub_file) {
            if (!orchestrate_module(sub_path, sub_file, graph)) {
                free(sub_file);
                return 0;
            }
            free(sub_file);
        }
    }

    return 1;
}

// Top-level orchestration: scan program AST, resolve all imports,
// parse modules, build dependency graph, detect cycles.
int module_orchestrate(ASTNode* program) {
    module_registry_init();

    DependencyGraph* graph = dependency_graph_create();
    dependency_graph_add_node(graph, "__main__");

    // Scan top-level AST for imports
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type != AST_IMPORT_STATEMENT || !child->value) continue;

        const char* module_path = child->value;
        char* file_path = NULL;

        if (strncmp(module_path, "std.", 4) == 0) {
            file_path = module_resolve_stdlib_path(module_path + 4);
        } else {
            file_path = module_resolve_local_path(module_path);
        }

        dependency_graph_add_edge(graph, "__main__", module_path);

        if (file_path) {
            if (!orchestrate_module(module_path, file_path, graph)) {
                free(file_path);
                dependency_graph_free(graph);
                return 0;
            }
            free(file_path);
        }
        // If file not found: silently continue (backwards compat)
    }

    // Check for cycles
    if (dependency_graph_has_cycle(graph)) {
        dependency_graph_free(graph);
        return 0;
    }

    dependency_graph_free(graph);
    return 1;
}

