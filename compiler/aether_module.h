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

// Module resolution
AetherModule* module_resolve(const char* import_path);
char* module_resolve_symbol(const char* module_name, const char* symbol);

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

#endif // AETHER_MODULE_H

