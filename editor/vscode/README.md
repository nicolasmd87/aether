# VS Code / Cursor Language Support for Aether

This folder contains the syntax highlighting configuration for the Aether programming language with an Erlang-inspired color scheme.

## Installation

### Option 1: Using Installation Scripts (Recommended)

**Windows (PowerShell):**
```powershell
.\editor\vscode\install.ps1
```

**Linux/macOS (Bash):**
```bash
chmod +x editor/vscode/install.sh
./editor/vscode/install.sh
```

**Using Make (Linux/macOS):**
```bash
make install-vscode
```

The scripts automatically detect VS Code or Cursor and install to the correct location.

### Option 2: Manual Installation

1. Copy the contents of this folder to your VS Code/Cursor extensions directory:
   - **Windows**: `%USERPROFILE%\.vscode\extensions\aether-language-0.0.1\` (or `.cursor\extensions\...` for Cursor)
   - **macOS**: `~/.vscode/extensions/aether-language-0.0.1/` (or `~/.cursor/extensions/...` for Cursor)
   - **Linux**: `~/.vscode/extensions/aether-language-0.0.1/` (or `~/.cursor/extensions/...` for Cursor)

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
- `install.ps1` - Windows installation script
- `install.sh` - Linux/macOS installation script

## Color Scheme

The syntax highlighting uses an Erlang-inspired color scheme:
- **Keywords**: Blue (`#569CD6`) - bold
- **Types**: Cyan (`#4EC9B0`)
- **Functions**: Yellow (`#DCDCAA`)
- **Actors**: Cyan (`#4EC9B0`) - bold
- **Strings**: Orange (`#CE9178`)
- **Numbers**: Green (`#B5CEA8`)
- **Comments**: Green (`#6A9955`)
- **Variables**: Light Blue (`#9CDCFE`)

## Why Not in `.vscode/`?

The `.vscode/` folder is git-ignored to avoid affecting users' workspace settings. Language support files are kept in `editor/vscode/` so they can be committed to the repository without interfering with individual developer configurations.

**Note:** If you see `aether.tmLanguage.json` in your local `.vscode/` folder, that's fine - it's for your local workspace use and won't be committed to the repository. The official files are in `editor/vscode/`.

