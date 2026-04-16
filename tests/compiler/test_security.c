// test_security.c — Compiler C-API security tests.
// Tests the symbol table internals (hide/seal, NULL safety,
// scope chains) which can only be exercised from C.
//
// Pipeline tests (parsing, codegen, string escaping, stress)
// are in .ae files under tests/syntax/ and tests/integration/.

#include "../runtime/test_harness.h"
#include "../../compiler/parser/lexer.h"
#include "../../compiler/parser/parser.h"
#include "../../compiler/analysis/typechecker.h"
#include "../../compiler/ast.h"
#include <string.h>
#include <stdlib.h>

// -----------------------------------------------------------
// Helpers
// -----------------------------------------------------------

static void free_table_chain(SymbolTable* t) {
  free_symbol_table(t);
}

// -----------------------------------------------------------
// CATEGORY 1: hide / seal boundary enforcement
// -----------------------------------------------------------

TEST_CATEGORY(hide_blocks_parent_name,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "secret", create_type(TYPE_INT),
             0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  scope_hide_name(child, "secret");

  ASSERT_NULL(lookup_symbol(child, "secret"));
  ASSERT_NOT_NULL(lookup_symbol(parent, "secret"));

  free_table_chain(child);
  free_table_chain(parent);
}

TEST_CATEGORY(hide_does_not_block_local,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "x", create_type(TYPE_INT), 0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  scope_hide_name(child, "x");
  add_symbol(child, "x", create_type(TYPE_STRING), 0, 0, 0);

  Symbol* s = lookup_symbol(child, "x");
  ASSERT_NOT_NULL(s);
  ASSERT_EQ(TYPE_STRING, s->type->kind);

  free_table_chain(child);
  free_table_chain(parent);
}

TEST_CATEGORY(hide_propagates_to_nested,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* top = create_symbol_table(NULL);
  add_symbol(top, "db", create_type(TYPE_PTR), 0, 0, 0);
  SymbolTable* mid = create_symbol_table(top);
  scope_hide_name(mid, "db");
  SymbolTable* inner = create_symbol_table(mid);

  ASSERT_NULL(lookup_symbol(inner, "db"));

  free_table_chain(inner);
  free_table_chain(mid);
  free_table_chain(top);
}

TEST_CATEGORY(seal_blocks_unlisted_name,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "allowed", create_type(TYPE_INT),
             0, 0, 0);
  add_symbol(parent, "blocked", create_type(TYPE_INT),
             0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  child->is_sealed = 1;
  scope_seal_except(child, "allowed");

  ASSERT_NOT_NULL(lookup_symbol(child, "allowed"));
  ASSERT_NULL(lookup_symbol(child, "blocked"));

  free_table_chain(child);
  free_table_chain(parent);
}

TEST_CATEGORY(seal_allows_local_binding,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "outer", create_type(TYPE_INT),
             0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  child->is_sealed = 1;
  add_symbol(child, "local", create_type(TYPE_STRING),
             0, 0, 0);

  ASSERT_NOT_NULL(lookup_symbol(child, "local"));
  ASSERT_NULL(lookup_symbol(child, "outer"));

  free_table_chain(child);
  free_table_chain(parent);
}

TEST_CATEGORY(hide_multiple_names,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "a", create_type(TYPE_INT), 0, 0, 0);
  add_symbol(parent, "b", create_type(TYPE_INT), 0, 0, 0);
  add_symbol(parent, "c", create_type(TYPE_INT), 0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  scope_hide_name(child, "a");
  scope_hide_name(child, "c");

  ASSERT_NULL(lookup_symbol(child, "a"));
  ASSERT_NOT_NULL(lookup_symbol(child, "b"));
  ASSERT_NULL(lookup_symbol(child, "c"));

  free_table_chain(child);
  free_table_chain(parent);
}

TEST_CATEGORY(seal_empty_whitelist_blocks_all,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "x", create_type(TYPE_INT), 0, 0, 0);
  add_symbol(parent, "y", create_type(TYPE_INT), 0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  child->is_sealed = 1;

  ASSERT_NULL(lookup_symbol(child, "x"));
  ASSERT_NULL(lookup_symbol(child, "y"));

  free_table_chain(child);
  free_table_chain(parent);
}

TEST_CATEGORY(hide_duplicate_is_safe,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "x", create_type(TYPE_INT), 0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  scope_hide_name(child, "x");
  scope_hide_name(child, "x");
  scope_hide_name(child, "x");

  ASSERT_NULL(lookup_symbol(child, "x"));

  free_table_chain(child);
  free_table_chain(parent);
}

// -----------------------------------------------------------
// CATEGORY 2: Qualified name hide/seal enforcement
// -----------------------------------------------------------

TEST_CATEGORY(hide_blocks_qualified_prefix,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "http", create_type(TYPE_PTR),
             0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  scope_hide_name(child, "http");

  ASSERT_NULL(lookup_qualified_symbol(child, "http.get"));

  free_table_chain(child);
  free_table_chain(parent);
}

TEST_CATEGORY(seal_blocks_qualified_prefix,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  add_symbol(parent, "http", create_type(TYPE_PTR),
             0, 0, 0);
  add_symbol(parent, "math", create_type(TYPE_PTR),
             0, 0, 0);
  SymbolTable* child = create_symbol_table(parent);
  child->is_sealed = 1;
  scope_seal_except(child, "math");

  ASSERT_NULL(lookup_qualified_symbol(child, "http.get"));

  free_table_chain(child);
  free_table_chain(parent);
}

// -----------------------------------------------------------
// CATEGORY 3: NULL safety
// -----------------------------------------------------------

TEST_CATEGORY(lookup_null_table,
              TEST_CATEGORY_COMPILER) {
  ASSERT_NULL(lookup_symbol(NULL, "x"));
}

TEST_CATEGORY(lookup_null_name,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* t = create_symbol_table(NULL);
  ASSERT_NULL(lookup_symbol(t, NULL));
  free_table_chain(t);
}

TEST_CATEGORY(hide_null_table,
              TEST_CATEGORY_COMPILER) {
  scope_hide_name(NULL, "x");
}

TEST_CATEGORY(hide_null_name,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* t = create_symbol_table(NULL);
  scope_hide_name(t, NULL);
  free_table_chain(t);
}

TEST_CATEGORY(seal_null_table,
              TEST_CATEGORY_COMPILER) {
  scope_seal_except(NULL, "x");
}

TEST_CATEGORY(scope_name_is_hidden_null_safety,
              TEST_CATEGORY_COMPILER) {
  ASSERT_EQ(0, scope_name_is_hidden(NULL, "x"));
  SymbolTable* t = create_symbol_table(NULL);
  ASSERT_EQ(0, scope_name_is_hidden(t, NULL));
  free_table_chain(t);
}

TEST_CATEGORY(qualified_lookup_null_safety,
              TEST_CATEGORY_COMPILER) {
  ASSERT_NULL(lookup_qualified_symbol(NULL, "a.b"));
  SymbolTable* t = create_symbol_table(NULL);
  ASSERT_NULL(lookup_qualified_symbol(t, NULL));
  free_table_chain(t);
}

// -----------------------------------------------------------
// CATEGORY 4: Symbol table stress
// -----------------------------------------------------------

TEST_CATEGORY(symbol_table_many_symbols,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* t = create_symbol_table(NULL);
  char name[32];
  for (int i = 0; i < 200; i++) {
    snprintf(name, sizeof(name), "sym_%d", i);
    add_symbol(t, name, create_type(TYPE_INT), 0, 0, 0);
  }
  ASSERT_NOT_NULL(lookup_symbol(t, "sym_199"));
  ASSERT_NULL(lookup_symbol(t, "sym_999"));
  free_table_chain(t);
}

TEST_CATEGORY(symbol_table_deep_scope_chain,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* tables[50];
  tables[0] = create_symbol_table(NULL);
  add_symbol(tables[0], "root_var", create_type(TYPE_INT),
             0, 0, 0);
  for (int i = 1; i < 50; i++)
    tables[i] = create_symbol_table(tables[i - 1]);

  ASSERT_NOT_NULL(lookup_symbol(tables[49], "root_var"));

  for (int i = 49; i >= 0; i--)
    free_table_chain(tables[i]);
}

TEST_CATEGORY(hide_many_names_in_one_scope,
              TEST_CATEGORY_COMPILER) {
  SymbolTable* parent = create_symbol_table(NULL);
  char name[32];
  for (int i = 0; i < 100; i++) {
    snprintf(name, sizeof(name), "v_%d", i);
    add_symbol(parent, name, create_type(TYPE_INT),
               0, 0, 0);
  }
  SymbolTable* child = create_symbol_table(parent);
  for (int i = 0; i < 100; i++) {
    snprintf(name, sizeof(name), "v_%d", i);
    scope_hide_name(child, name);
  }

  ASSERT_NULL(lookup_symbol(child, "v_0"));
  ASSERT_NULL(lookup_symbol(child, "v_50"));
  ASSERT_NULL(lookup_symbol(child, "v_99"));

  free_table_chain(child);
  free_table_chain(parent);
}
