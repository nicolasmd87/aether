# Aether Language Server Protocol (LSP)

A Language Server Protocol implementation for Aether, providing IDE support for:
- Autocomplete
- Go-to-definition
- Error checking
- Syntax highlighting
- Hover information

## Features

### Implemented
- Basic LSP server structure
- Text document synchronization
- Diagnostic reporting (errors/warnings)
- Hover information
- Go-to-definition
- Completion (autocomplete)
- Document symbols

### Planned
- Semantic highlighting
- Code actions (quick fixes)
- Rename symbol
- Find references
- Code formatting

## Usage

### Building
```bash
make lsp_server
```

### VS Code Extension
See `editors/vscode/` for the VS Code extension.

### Vim/Neovim
Add to your LSP config:
```lua
require'lspconfig'.aether.setup{
  cmd = {"aether-lsp"},
  filetypes = {"aether"},
}
```

### Other Editors
Any editor supporting LSP can use aether-lsp. Configure it to run `aether-lsp` for `.ae` files.

## Protocol Messages

The LSP server implements the following capabilities:
- `textDocument/completion`
- `textDocument/hover`
- `textDocument/definition`
- `textDocument/documentSymbol`
- `textDocument/publishDiagnostics`

## Architecture

```
aether-lsp
├── main.c          - LSP server entry point
├── lsp_server.c    - Core LSP logic
├── lsp_protocol.c  - JSON-RPC protocol handling
├── lsp_analysis.c  - Code analysis (uses compiler frontend)
└── lsp_utils.c     - Utility functions
```

The LSP server reuses the Aether compiler's lexer, parser, and type checker for accurate analysis.

