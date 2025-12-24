#!/bin/bash
# Install Aether Language Support for VS Code/Cursor (Linux/macOS)
# Run this script from the project root: ./editor/vscode/install.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_NAME="aether-language-0.0.1"

install_extension() {
    local target_path="$1"
    local editor_name="$2"
    
    echo "Installing Aether language support for $editor_name..."
    echo "Target: $target_path"
    
    # Create extension directory
    mkdir -p "$target_path"
    
    # Copy extension files
    cp "$SCRIPT_DIR/package.json" "$target_path/"
    cp "$SCRIPT_DIR/aether.tmLanguage.json" "$target_path/"
    cp "$SCRIPT_DIR/language-configuration.json" "$target_path/"
    
    echo "✓ Extension installed successfully!"
    echo "Please restart VS Code/Cursor for changes to take effect."
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

