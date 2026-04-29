# Aether LSP Server

`build/aether-lsp` is the Aether language server. It speaks JSON-RPC
2.0 over stdio (the LSP standard) and is configured for the VS Code
extension at [editor/vscode/](../editor/vscode/).

Run via `make lsp`. The binary is the same on every platform — start
it from your editor as a stdio child process.

## What's wired today

The server advertises these capabilities at `initialize`:

| Method | Status | Notes |
|---|---|---|
| `textDocument/didOpen` / `didChange` / `didClose` | Working | Documents stored in an in-memory index keyed by URI. |
| `textDocument/publishDiagnostics` | Working | Lexer-level errors only — `TOKEN_ERROR` from `compiler/parser/lexer.c` is converted to LSP diagnostics. Republished on every change. |
| `textDocument/completion` | **Hard-coded list** | Static keyword + stdlib catalogue (`lsp_handle_completion` in `aether_lsp.c`). Covers control flow, storage keywords, primitive types, and the most-used `std.*` surfaces (string, collections, intarr, bytes, fs, json, http, os, math, log, cryptography). Updated by hand when the stdlib changes. |
| `textDocument/hover` | **Stub** | Returns the literal string `"**Aether Actor**\n\nLightweight concurrent actor"` regardless of cursor position. |
| `textDocument/definition` | **Stub** | Returns `null`. |
| `textDocument/documentSymbol` | **Stub** | Returns `[]`. |

The diagnostics pipeline is the only real-knowledge feature today.
Everything else is hard-coded text.

## What's missing — concrete next-step list

### 1. Symbol-aware completion

Today's list is static. Real completion should:

- Re-parse the document (or fetch the cached AST from a recent
  parse) and locate the cursor's enclosing scope using the same
  `SymbolTable` chain `compiler/analysis/typechecker.c` builds.
- Emit completion items for:
  - In-scope locals (function parameters, `let` / `var` bindings).
  - Top-level declarations in the current file (`func`, `actor`,
    `struct`, `message`, `extern`, `const`).
  - Names exported by `import` modules (resolve `import std.X`
    against the actual `std/X/module.ae` `exports (...)` list,
    not a hard-coded list).
  - Receiver-method completion: when the cursor is right after `.`,
    inspect the LHS's type and surface its members. For `string.`,
    `intarr.`, `bytes.`, etc., that means the namespace's exported
    names.
- Mark each item with the right `CompletionItemKind` so the
  editor's icons line up (`Function`, `Variable`, `Struct`,
  `Field`, `Keyword`, `Constant`).

The hand-maintained list in `lsp_handle_completion` is the floor
this should replace, not extend.

### 2. Hover with type info

`textDocument/hover` should:

- Convert `(line, character)` to a byte offset against the cached
  document text.
- Walk the parsed AST to find the smallest expression / identifier
  enclosing that offset.
- Read the node's `node_type` (set by the typechecker on every
  expression / variable declaration) and format it as Markdown:
  - Identifiers → `name : T` with `T` formatted via
    `compiler/analysis/typechecker.c`'s `type_name`.
  - Function calls → the callee's signature (parameter list +
    return type).
  - Pattern bindings inside a `match` arm → the bound type the
    typechecker assigned (`string` for cons-cell head, `*StringSeq`
    for tail, etc.).
- Fall back to a short doc string from the stdlib's `module.ae`
  comments where one is reachable.

### 3. Go-to-definition

`textDocument/definition` should:

- Resolve the identifier at the cursor (same byte-offset walk
  used by hover).
- For locals, find the enclosing `AST_VARIABLE_DECLARATION` /
  `AST_FUNCTION_DEFINITION` parameter and return its source location.
- For top-level names, find the matching `AST_FUNCTION_DEFINITION` /
  `AST_STRUCT_DEFINITION` / `AST_ACTOR_DEFINITION` /
  `AST_MESSAGE_DEFINITION` / `AST_EXTERN_FUNCTION` in the same file
  (use the AST's `line` / `column` fields).
- For names imported from another module, follow the `import std.X`
  reference, parse `std/X/module.ae`, and return the matching
  declaration's location there.

### 4. Document symbol enumeration

`textDocument/documentSymbol` should walk the parsed AST and emit
one `DocumentSymbol` per top-level declaration:

- `AST_FUNCTION_DEFINITION` → `Function`, with parameter signatures
  in the `detail` field.
- `AST_ACTOR_DEFINITION` → `Class`, with `state` declarations and
  `receive` arms as nested children.
- `AST_STRUCT_DEFINITION` → `Struct`, with each `AST_STRUCT_FIELD`
  as a `Field` child.
- `AST_MESSAGE_DEFINITION` → `Class`, with each
  `AST_MESSAGE_FIELD` as a `Field` child.
- `AST_EXTERN_FUNCTION` → `Function`, marked with `Interface` kind
  to differentiate from native definitions.
- `AST_CONST_DECLARATION` → `Constant`.

### 5. Typechecker-level diagnostics

Today's `lsp_publish_diagnostics` only forwards lexer errors. It
should also:

- Run the typechecker on each `didChange` and capture every
  `type_error` / `aether_warning_report` call as an LSP diagnostic.
- Emit the existing diagnostic codes (`E0100`, `E0200`, `E0300`,
  `W1001`, etc.) so the editor's problem panel groups them.
- Honour quick-fix hints — many existing errors include a
  suggestion string already; surface it via LSP `code action`.

### 6. Incremental updates

Documents are re-stored entirely on each `didChange`. The protocol
also supports incremental edits (`Range`-bounded text changes).
For large files this matters; not urgent until a real user complains.

### 7. References / rename

`textDocument/references` and `textDocument/rename` are unimplemented.
Both want a global symbol index — the same cross-file resolution
work that go-to-definition needs.

## Layout

| File | Role |
|---|---|
| `main.c` | Entry point; spins up the server loop. |
| `aether_lsp.c` | All protocol handlers + the in-memory document store. |
| `aether_lsp.h` | Public types + function prototypes. |

The implementation links against `compiler/` and `runtime/` so the
server can re-use the existing parser, AST, and typechecker rather
than carry a second one. See the `lsp:` target in the top-level
`Makefile` for the link line.

## Testing

Today the LSP isn't part of `make ci` — only `make lsp` builds it.
A future iteration should add a small Aether file as a fixture and
run a recorded JSON-RPC session against it (open + completion +
hover + diagnostics) to lock the surface down. The fixture/runner
pair would slot under `tests/integration/lsp_*/`.
