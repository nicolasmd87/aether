#ifndef AETHER_MODULE_H
#define AETHER_MODULE_H

#include "ast.h"

// Module system for Aether
// Supports: import/export, package management

// Module structure
typedef struct {
    char* name;           // Module name (e.g., "game.player")
    char* file_path;      // Path to source file
    ASTNode* ast;         // Parsed AST
    char** exports;       // Exported symbols
    int export_count;
    char** imports;       // Imported modules
    int import_count;
} AetherModule;

// Module registry
typedef struct {
    AetherModule** modules;
    int module_count;
    int module_capacity;
} ModuleRegistry;

// Global module registry
extern ModuleRegistry* global_module_registry;

// Module management
void module_registry_init();
void module_registry_shutdown();

AetherModule* module_create(const char* name, const char* file_path);
void module_free(AetherModule* module);

// Module registration
void module_register(AetherModule* module);
AetherModule* module_find(const char* name);

// Import/export handling
void module_add_export(AetherModule* module, const char* symbol);
void module_add_import(AetherModule* module, const char* module_name);
int module_is_exported(AetherModule* module, const char* symbol);

// Dependency graph
typedef struct DependencyNode {
    char* module_name;
    struct DependencyNode** dependencies;
    int dependency_count;
    int visited;  // For circular detection
    int in_stack; // For circular detection
} DependencyNode;

typedef struct {
    DependencyNode** nodes;
    int node_count;
} DependencyGraph;

DependencyGraph* dependency_graph_create();
void dependency_graph_free(DependencyGraph* graph);
DependencyNode* dependency_graph_add_node(DependencyGraph* graph, const char* module_name);
void dependency_graph_add_edge(DependencyGraph* graph, const char* from, const char* to);
int dependency_graph_has_cycle(DependencyGraph* graph);
DependencyNode* dependency_graph_find_node(DependencyGraph* graph, const char* module_name);

// Package manifest (aether.toml)
typedef struct {
    char* package_name;
    char* version;
    char* author;
    char** dependencies;
    int dependency_count;
} PackageManifest;

PackageManifest* package_manifest_load(const char* path);
void package_manifest_free(PackageManifest* manifest);

// Module orchestration — call between parsing and type checking
#define MAX_MODULE_TOKENS 20000

// Set the source file directory so module resolution can search lib/ relative to it.
void module_set_source_dir(const char* source_path);

// Orchestrate all module loading: scan imports, resolve, parse, cache, detect cycles.
// Returns 1 on success, 0 on circular dependency error.
int module_orchestrate(ASTNode* program);

// Parse a single module file into an AST. Saves/restores lexer state.
ASTNode* module_parse_file(const char* file_path);

// Resolve module name to file path. Returns malloc'd path or NULL. Caller frees.
char* module_resolve_stdlib_path(const char* module_name);  // "fs" -> path
char* module_resolve_local_path(const char* module_path);   // "mypackage.utils" -> path

// Merge pure Aether module functions into the main program AST.
// Call after module_orchestrate() and before typecheck_program().
void module_merge_into_program(ASTNode* program);

#endif // AETHER_MODULE_H

