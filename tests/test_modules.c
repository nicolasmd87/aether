#include "test_harness.h"
#include "../compiler/lexer.h"
#include "../compiler/parser.h"
#include "../compiler/aether_module.h"
#include <string.h>

static ASTNode* parse_code(const char* code) {
    lexer_init(code);
    Token** tokens = malloc(1000 * sizeof(Token*));
    int count = 0;
    Token* tok;
    int safety = 0;
    const int MAX_TOKENS = 999;
    while (safety++ < MAX_TOKENS && (tok = next_token())->type != TOKEN_EOF && count < MAX_TOKENS) {
        tokens[count++] = tok;
    }
    if (tok && tok->type == TOKEN_EOF) {
        tokens[count++] = tok;
    }
    Parser* parser = create_parser(tokens, count);
    ASTNode* ast = parse_program(parser);
    free_parser(parser);
    free(tokens);
    return ast;
}

TEST(module_create_and_register) {
    module_registry_init();
    
    AetherModule* mod = module_create("test.module", "test.ae");
    ASSERT_NOT_NULL(mod);
    ASSERT_STREQ("test.module", mod->name);
    ASSERT_STREQ("test.ae", mod->file_path);
    
    module_register(mod);
    
    AetherModule* found = module_find("test.module");
    ASSERT_NOT_NULL(found);
    ASSERT_STREQ("test.module", found->name);
    
    module_registry_shutdown();
}

TEST(module_exports) {
    AetherModule* mod = module_create("math", "math.ae");
    
    module_add_export(mod, "add");
    module_add_export(mod, "subtract");
    module_add_export(mod, "multiply");
    
    ASSERT_EQ(3, mod->export_count);
    ASSERT_TRUE(module_is_exported(mod, "add"));
    ASSERT_TRUE(module_is_exported(mod, "multiply"));
    ASSERT_TRUE(!module_is_exported(mod, "private_helper"));
    
    module_free(mod);
}

TEST(module_imports) {
    AetherModule* mod = module_create("main", "main.ae");
    
    module_add_import(mod, "std.io");
    module_add_import(mod, "std.math");
    module_add_import(mod, "mylib.utils");
    
    ASSERT_EQ(3, mod->import_count);
    
    module_free(mod);
}

TEST(module_duplicate_exports) {
    AetherModule* mod = module_create("test", "test.ae");
    
    module_add_export(mod, "func1");
    module_add_export(mod, "func1");  // Duplicate
    module_add_export(mod, "func1");  // Duplicate
    
    // Should only have one
    ASSERT_EQ(1, mod->export_count);
    
    module_free(mod);
}

TEST(module_duplicate_imports) {
    AetherModule* mod = module_create("test", "test.ae");
    
    module_add_import(mod, "std.io");
    module_add_import(mod, "std.io");  // Duplicate
    
    // Should only have one
    ASSERT_EQ(1, mod->import_count);
    
    module_free(mod);
}

TEST(module_registry_multiple_modules) {
    module_registry_init();
    
    AetherModule* mod1 = module_create("module1", "mod1.ae");
    AetherModule* mod2 = module_create("module2", "mod2.ae");
    AetherModule* mod3 = module_create("module3", "mod3.ae");
    
    module_register(mod1);
    module_register(mod2);
    module_register(mod3);
    
    ASSERT_NOT_NULL(module_find("module1"));
    ASSERT_NOT_NULL(module_find("module2"));
    ASSERT_NOT_NULL(module_find("module3"));
    ASSERT_TRUE(module_find("nonexistent") == NULL);
    
    module_registry_shutdown();
}

TEST(module_registry_replace) {
    module_registry_init();
    
    AetherModule* mod1 = module_create("test", "test1.ae");
    module_add_export(mod1, "old_func");
    module_register(mod1);
    
    AetherModule* mod2 = module_create("test", "test2.ae");
    module_add_export(mod2, "new_func");
    module_register(mod2);  // Should replace mod1
    
    AetherModule* found = module_find("test");
    ASSERT_NOT_NULL(found);
    ASSERT_TRUE(module_is_exported(found, "new_func"));
    ASSERT_TRUE(!module_is_exported(found, "old_func"));
    
    module_registry_shutdown();
}

TEST(module_symbol_resolution) {
    module_registry_init();
    
    AetherModule* math_mod = module_create("math", "math.ae");
    module_add_export(math_mod, "sqrt");
    module_add_export(math_mod, "pow");
    module_register(math_mod);
    
    char* qualified = module_resolve_symbol("math", "sqrt");
    ASSERT_NOT_NULL(qualified);
    ASSERT_STREQ("math.sqrt", qualified);
    free(qualified);
    
    // Non-exported symbol should fail
    char* invalid = module_resolve_symbol("math", "private_helper");
    ASSERT_TRUE(invalid == NULL);
    
    module_registry_shutdown();
}

TEST(package_manifest_basic) {
    // Create a test manifest file
    FILE* f = fopen("test_manifest.toml", "w");
    fprintf(f, "name = \"test_package\"\n");
    fprintf(f, "version = \"1.0.0\"\n");
    fprintf(f, "author = \"Test Author\"\n");
    fclose(f);
    
    PackageManifest* manifest = package_manifest_load("test_manifest.toml");
    ASSERT_NOT_NULL(manifest);
    ASSERT_STREQ("test_package", manifest->package_name);
    ASSERT_STREQ("1.0.0", manifest->version);
    ASSERT_STREQ("Test Author", manifest->author);
    
    package_manifest_free(manifest);
    remove("test_manifest.toml");
}

TEST(package_manifest_missing_file) {
    PackageManifest* manifest = package_manifest_load("nonexistent.toml");
    ASSERT_TRUE(manifest == NULL);
}

TEST(lexer_module_keywords) {
    lexer_init("module import export");
    
    Token* tok1 = next_token();
    ASSERT_EQ(TOKEN_MODULE, tok1->type);
    
    Token* tok2 = next_token();
    ASSERT_EQ(TOKEN_IMPORT, tok2->type);
    
    Token* tok3 = next_token();
    ASSERT_EQ(TOKEN_EXPORT, tok3->type);
    
    free_token(tok1);
    free_token(tok2);
    free_token(tok3);
}

TEST(module_empty_exports) {
    AetherModule* mod = module_create("empty", "empty.ae");
    ASSERT_EQ(0, mod->export_count);
    ASSERT_TRUE(!module_is_exported(mod, "anything"));
    module_free(mod);
}

TEST(module_empty_imports) {
    AetherModule* mod = module_create("empty", "empty.ae");
    ASSERT_EQ(0, mod->import_count);
    module_free(mod);
}

