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

// Module resolution
AetherModule* module_resolve(const char* import_path) {
    // Simple resolution: just look up by name
    // In a full implementation, this would:
    // 1. Check local modules
    // 2. Check installed packages
    // 3. Download from registry if needed
    
    return module_find(import_path);
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

