#!/bin/bash
# Install Aether Language Support for VS Code/Cursor (Linux/macOS)
# Run this script from the project root: ./editor/vscode/install.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_NAME="aether-language-0.1.0"

install_extension() {
    local target_path="$1"
    local editor_name="$2"
    
    echo "Installing Aether language support for $editor_name..."
    echo "Target: $target_path"
    
    cd "$SCRIPT_DIR"
    
    if ! command -v npm &> /dev/null; then
        echo "Warning: npm not found. Installing syntax highlighting only."
        mkdir -p "$target_path"
        cp "$SCRIPT_DIR/package.json" "$target_path/"
        cp "$SCRIPT_DIR/aether.tmLanguage.json" "$target_path/"
        cp "$SCRIPT_DIR/language-configuration.json" "$target_path/"
    else
        echo "Building extension with LSP support..."
        npm install
        npm run compile
        
        mkdir -p "$target_path"
        cp "$SCRIPT_DIR/package.json" "$target_path/"
        cp "$SCRIPT_DIR/aether.tmLanguage.json" "$target_path/"
        cp "$SCRIPT_DIR/language-configuration.json" "$target_path/"
        cp -r "$SCRIPT_DIR/out" "$target_path/"
        if [ -d "$SCRIPT_DIR/node_modules/vscode-languageclient" ]; then
            mkdir -p "$target_path/node_modules"
            cp -r "$SCRIPT_DIR/node_modules/vscode-languageclient" "$target_path/node_modules/"
        fi
    fi
    
    echo "✓ Extension installed successfully!"
    echo "Please restart VS Code/Cursor for changes to take effect."
    echo "Note: Make sure aether-lsp is in your PATH for LSP features."
}

# Detect VS Code or Cursor
if [ -d "$HOME/.cursor" ]; then
    echo "Detected Cursor installation"
    install_extension "$HOME/.cursor/extensions/$EXT_NAME" "Cursor"
elif [ -d "$HOME/.vscode" ]; then
    echo "Detected VS Code installation"
    install_extension "$HOME/.vscode/extensions/$EXT_NAME" "VS Code"
else
    echo "Error: Neither VS Code nor Cursor installation found."
    echo "Please install VS Code or Cursor first."
    exit 1
fi

