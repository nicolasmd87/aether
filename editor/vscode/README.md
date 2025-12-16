# VS Code / Cursor Language Support for Aether

This folder contains the syntax highlighting configuration for the Aether programming language with an Erlang-inspired color scheme.

## Installation

### Option 1: Install as Extension (Recommended)

1. Copy the contents of this folder to your VS Code/Cursor extensions directory:
   - **Windows**: `%USERPROFILE%\.vscode\extensions\aether-language-0.0.1\`
   - **macOS**: `~/.vscode/extensions/aether-language-0.0.1/`
   - **Linux**: `~/.vscode/extensions/aether-language-0.0.1/`

2. Restart VS Code/Cursor

### Option 2: Workspace Settings (Development)

For development, you can create a `.vscode/settings.json` in your workspace (this file is git-ignored):

```json
{
  "files.associations": {
    "*.ae": "aether"
  },
  "editor.tokenColorCustomizations": {
    "textMateRules": [
      {
        "scope": ["keyword.control.aether", "keyword.other.aether"],
        "settings": { "foreground": "#569CD6", "fontStyle": "bold" }
      },
      {
        "scope": ["storage.type.aether"],
        "settings": { "foreground": "#4EC9B0" }
      },
      {
        "scope": ["entity.name.function.aether"],
        "settings": { "foreground": "#DCDCAA" }
      },
      {
        "scope": ["string.quoted.double.aether"],
        "settings": { "foreground": "#CE9178" }
      },
      {
        "scope": ["comment.line.double-slash.aether", "comment.block.aether"],
        "settings": { "foreground": "#6A9955" }
      }
    ]
  }
}
```

**Note:** This requires the extension to be installed for the grammar to work.

## Files

- `aether.tmLanguage.json` - TextMate grammar for syntax highlighting
- `language-configuration.json` - Editor features (comments, brackets, etc.)
- `package.json` - Extension manifest

## Why Not in `.vscode/`?

The `.vscode/` folder is git-ignored to avoid affecting users' workspace settings. Language support files are kept in `editor/vscode/` so they can be committed to the repository without interfering with individual developer configurations.

