#include "aether_module.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
        global_module_registry->modules = (AetherModule**)realloc(
            global_module_registry->modules,
            new_capacity * sizeof(AetherModule*)
        );
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
    
    module->exports = (char**)realloc(module->exports, (module->export_count + 1) * sizeof(char*));
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
    
    module->imports = (char**)realloc(module->imports, (module->import_count + 1) * sizeof(char*));
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

// Resolve module file path
// Converts import path like "std.collections.HashMap" to "std/collections/HashMap.ae"
// or "game.player" to "game/player.ae"
char* module_resolve_file_path(const char* import_path, const char* base_dir) {
    if (!import_path) return NULL;
    
    // Convert dots to slashes
    char path_buffer[1024];
    snprintf(path_buffer, sizeof(path_buffer), "%s", import_path);
    for (int i = 0; path_buffer[i]; i++) {
        if (path_buffer[i] == '.') {
            path_buffer[i] = '/';
        }
    }
    
    // Try various paths
    char* attempts[4];
    int attempt_count = 0;
    
    // 1. Base directory (current file's directory)
    if (base_dir) {
        attempts[attempt_count] = malloc(2048);
        snprintf(attempts[attempt_count], 2048, "%s/%s.ae", base_dir, path_buffer);
        attempt_count++;
    }
    
    // 2. Current working directory
    attempts[attempt_count] = malloc(2048);
    snprintf(attempts[attempt_count], 2048, "%s.ae", path_buffer);
    attempt_count++;
    
    // 3. std/ directory (standard library)
    attempts[attempt_count] = malloc(2048);
    snprintf(attempts[attempt_count], 2048, "std/%s.ae", path_buffer);
    attempt_count++;
    
    // 4. Direct path if already has .ae extension
    if (strstr(import_path, ".ae")) {
        attempts[attempt_count] = malloc(1024);
        snprintf(attempts[attempt_count], 1024, "%s", import_path);
        attempt_count++;
    }
    
    // Try each path
    for (int i = 0; i < attempt_count; i++) {
        FILE* f = fopen(attempts[i], "r");
        if (f) {
            fclose(f);
            // Return this path, free others
            char* result = attempts[i];
            for (int j = 0; j < attempt_count; j++) {
                if (j != i) free(attempts[j]);
            }
            return result;
        }
    }
    
    // Not found, free all
    for (int i = 0; i < attempt_count; i++) {
        free(attempts[i]);
    }
    
    return NULL;
}

// Load module from file
AetherModule* module_load_from_file(const char* import_path, const char* base_dir) {
    // Check if already loaded
    AetherModule* existing = module_find(import_path);
    if (existing) {
        return existing;
    }
    
    // Resolve file path
    char* file_path = module_resolve_file_path(import_path, base_dir);
    if (!file_path) {
        fprintf(stderr, "Error: Could not resolve module '%s'\n", import_path);
        return NULL;
    }
    
    // Read file content
    FILE* f = fopen(file_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open module file '%s'\n", file_path);
        free(file_path);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    size_t bytes_read = fread(content, 1, size, f);
    content[bytes_read] = '\0';
    fclose(f);

    if (bytes_read != (size_t)size) {
        free(content);
        free(file_path);
        return NULL;
    }
    
    // Create module entry
    AetherModule* module = module_create(import_path, file_path);
    
    // Note: Full AST parsing happens later in the compilation pipeline
    // The module system tracks dependencies and load order, actual parsing
    // is done by the main compiler when it processes each module
    
    free(content);
    free(file_path);
    
    // Register module
    module_register(module);
    
    return module;
}

// Module resolution
AetherModule* module_resolve(const char* import_path) {
    // First check if already loaded
    AetherModule* existing = module_find(import_path);
    if (existing) {
        return existing;
    }
    
    // Try to load from file
    return module_load_from_file(import_path, NULL);
}

char* module_resolve_symbol(const char* module_name, const char* symbol) {
    AetherModule* module = module_find(module_name);
    if (!module) return NULL;
    
    if (!module_is_exported(module, symbol)) {
        fprintf(stderr, "Error: Symbol '%s' is not exported from module '%s'\n",
                symbol, module_name);
        return NULL;
    }
    
    // Return fully qualified name
    char* qualified = (char*)malloc(strlen(module_name) + strlen(symbol) + 2);
    sprintf(qualified, "%s.%s", module_name, symbol);
    return qualified;
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
    graph->nodes = realloc(graph->nodes, (graph->node_count + 1) * sizeof(DependencyNode*));
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
    from_node->dependencies = realloc(from_node->dependencies,
                                     (from_node->dependency_count + 1) * sizeof(DependencyNode*));
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
                fprintf(stderr, "Error: Circular dependency detected involving module '%s'\n",
                       graph->nodes[i]->module_name);
                return 1;
            }
        }
    }
    
    return 0;
}

