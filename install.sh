#!/bin/sh
# Aether Language Installer
# Usage: ./install.sh              (installs to ~/.aether)
#        ./install.sh /usr/local   (installs to /usr/local, needs sudo)
#        ./install.sh --editor-only (installs only editor extension)
set -e

# Handle --editor-only flag
if [ "$1" = "--editor-only" ]; then
    EDITOR_ONLY=1
    INSTALL_DIR="$HOME/.aether"
else
    EDITOR_ONLY=0
    INSTALL_DIR="${1:-$HOME/.aether}"
fi
BIN_DIR="$INSTALL_DIR/bin"
LIB_DIR="$INSTALL_DIR/lib"
INCLUDE_DIR="$INSTALL_DIR/include/aether"
SRC_DIR="$INSTALL_DIR/share/aether"

# Colors (if terminal supports it)
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    GREEN='' YELLOW='' RED='' BOLD='' NC=''
fi

info()  { printf "${BOLD}%s${NC}\n" "$1"; }
ok()    { printf "${GREEN}%s${NC}\n" "$1"; }
warn()  { printf "${YELLOW}%s${NC}\n" "$1"; }
error() { printf "${RED}%s${NC}\n" "$1"; }

if [ "$EDITOR_ONLY" -eq 0 ]; then
    # Check prerequisites
    info "Checking prerequisites..."

    if ! command -v gcc >/dev/null 2>&1 && ! command -v cc >/dev/null 2>&1; then
        error "Error: C compiler (gcc or cc) not found."
        echo ""
        case "$(uname -s)" in
            Darwin) echo "  Install: xcode-select --install" ;;
            Linux)  echo "  Install: sudo apt-get install build-essential" ;;
            *)      echo "  Install GCC or Clang for your platform." ;;
        esac
        exit 1
    fi

    if ! command -v make >/dev/null 2>&1; then
        error "Error: make not found."
        case "$(uname -s)" in
            Darwin) echo "  Install: xcode-select --install" ;;
            Linux)  echo "  Install: sudo apt-get install build-essential" ;;
            *)      echo "  Install GNU Make for your platform." ;;
        esac
        exit 1
    fi

    ok "  Prerequisites OK"
    echo ""

    # Build
    info "Building Aether..."
    make compiler 2>&1 | tail -1
    make ae 2>&1 | tail -1

    # Build precompiled stdlib
    info "Building standard library..."
    make stdlib 2>&1 | tail -1

    # Build REPL (optional — needs readline)
    info "Building REPL..."
    make repl 2>&1 | tail -1 || warn "  REPL build skipped (install libreadline-dev to enable)"
    echo ""

    # Install
    info "Installing to $INSTALL_DIR..."

    mkdir -p "$BIN_DIR" "$LIB_DIR" "$INCLUDE_DIR" "$SRC_DIR"

    # Binaries
    cp build/ae "$BIN_DIR/ae"
    cp build/aetherc "$BIN_DIR/aetherc"
    if [ -f build/aether_repl ]; then
        cp build/aether_repl "$BIN_DIR/aether_repl"
        chmod 755 "$BIN_DIR/aether_repl"
    fi
    chmod 755 "$BIN_DIR/ae" "$BIN_DIR/aetherc"

    # Precompiled library
    if [ -f build/libaether.a ]; then
        cp build/libaether.a "$LIB_DIR/libaether.a"
    fi

    # Headers (preserve directory structure for relative includes)
    for dir in runtime runtime/actors runtime/scheduler runtime/utils \
               runtime/memory runtime/config std/string std/math std/net \
               std/collections std/json std/fs std/log; do
        if [ -d "$dir" ]; then
            mkdir -p "$INCLUDE_DIR/$dir"
            for h in "$dir"/*.h; do
                [ -f "$h" ] && cp "$h" "$INCLUDE_DIR/$dir/" 2>/dev/null || true
            done
        fi
    done

    # Runtime source (fallback for linking)
    mkdir -p "$SRC_DIR/runtime" "$SRC_DIR/std"
    cp -r runtime/*.c runtime/*.h "$SRC_DIR/runtime/" 2>/dev/null || true
    for subdir in actors scheduler memory config utils; do
        if [ -d "runtime/$subdir" ]; then
            mkdir -p "$SRC_DIR/runtime/$subdir"
            cp runtime/$subdir/*.c runtime/$subdir/*.h "$SRC_DIR/runtime/$subdir/" 2>/dev/null || true
        fi
    done
    for subdir in string math net collections json fs log io; do
        if [ -d "std/$subdir" ]; then
            mkdir -p "$SRC_DIR/std/$subdir"
            cp std/$subdir/*.c std/$subdir/*.h "$SRC_DIR/std/$subdir/" 2>/dev/null || true
        fi
    done

    ok "  Installed successfully"
    echo ""

    # PATH setup
    SHELL_NAME="$(basename "$SHELL")"
    EXPORT_LINE="export PATH=\"$BIN_DIR:\$PATH\""
    AETHER_HOME_LINE="export AETHER_HOME=\"$INSTALL_DIR\""

    IN_PATH=0
    case ":$PATH:" in
        *":$BIN_DIR:"*) IN_PATH=1 ;;
    esac

    if [ "$IN_PATH" -eq 0 ]; then
        info "Setting up PATH..."

        # Detect shell config file
        SHELL_RC=""
        case "$SHELL_NAME" in
            zsh)  SHELL_RC="$HOME/.zshrc" ;;
            bash) SHELL_RC="$HOME/.bash_profile" ;;
            fish) SHELL_RC="$HOME/.config/fish/config.fish" ;;
        esac

        if [ -n "$SHELL_RC" ]; then
            # Check if already in rc file
            if ! grep -q "AETHER_HOME" "$SHELL_RC" 2>/dev/null; then
                echo "" >> "$SHELL_RC"
                echo "# Aether Language" >> "$SHELL_RC"
                echo "$AETHER_HOME_LINE" >> "$SHELL_RC"
                echo "$EXPORT_LINE" >> "$SHELL_RC"
                ok "  Added to $SHELL_RC"
            else
                ok "  Already configured in $SHELL_RC"
            fi
        fi
        echo ""
    fi

    # Verify
    info "Verifying installation..."
    if "$BIN_DIR/ae" version >/dev/null 2>&1; then
        VERSION=$("$BIN_DIR/ae" version 2>&1)
        ok "  $VERSION"
    else
        error "  Verification failed"
        exit 1
    fi

    echo ""
    echo "========================================="
    ok "  Aether installed successfully!"
    echo "========================================="
    echo ""

    if [ "$IN_PATH" -eq 0 ]; then
        warn "Restart your terminal or run:"
        echo "  source $SHELL_RC"
        echo ""
    fi

    echo "Get started:"
    echo "  ae init myproject"
    echo "  cd myproject"
    echo "  ae run"
    echo ""
    echo "Or run a file directly:"
    echo "  ae run hello.ae"
    echo ""
fi

# IDE Extension Installation (optional)
install_editor_extension() {
    local editor_cmd="$1"
    local editor_name="$2"
    local ext_dir="$3"
    local ext_name="aether-language-0.4.1"
    local ext_path="$ext_dir/$ext_name"
    local src_dir="$(dirname "$0")/editor/vscode"

    if [ ! -d "$src_dir" ]; then
        warn "  Extension source not found at $src_dir"
        return 1
    fi

    info "Installing Aether extension for $editor_name..."
    mkdir -p "$ext_path"
    cp "$src_dir/package.json" "$ext_path/"
    cp "$src_dir/aether.tmLanguage.json" "$ext_path/"
    cp "$src_dir/language-configuration.json" "$ext_path/"
    [ -f "$src_dir/icon.png" ] && cp "$src_dir/icon.png" "$ext_path/"
    [ -f "$src_dir/icon-module.svg" ] && cp "$src_dir/icon-module.svg" "$ext_path/"
    ok "  Extension installed to $ext_path"
    warn "  Restart $editor_name for changes to take effect"
    return 0
}

# Detect all supported editors
EDITORS_FOUND=0

prompt_install_extension() {
    local editor_cmd="$1"
    local editor_name="$2"
    local ext_dir="$3"

    if [ "$EDITOR_ONLY" -eq 1 ]; then
        # Direct install when using --editor-only flag
        install_editor_extension "$editor_cmd" "$editor_name" "$ext_dir"
    else
        # Interactive prompt during normal install
        printf "Install Aether syntax highlighting for $editor_name? [y/N] "
        read -r response
        case "$response" in
            [yY]|[yY][eE][sS])
                install_editor_extension "$editor_cmd" "$editor_name" "$ext_dir"
                ;;
            *)
                echo "  Skipped"
                ;;
        esac
    fi
}

# Check for Cursor (command in PATH, or macOS app bundle with extensions dir)
if command -v cursor >/dev/null 2>&1 && [ -d "$HOME/.cursor" ]; then
    [ "$EDITORS_FOUND" -eq 0 ] && echo ""
    info "Detected Cursor"
    prompt_install_extension "cursor" "Cursor" "$HOME/.cursor/extensions"
    EDITORS_FOUND=$((EDITORS_FOUND + 1))
elif [ -d "/Applications/Cursor.app" ] && [ -d "$HOME/.cursor/extensions" ]; then
    [ "$EDITORS_FOUND" -eq 0 ] && echo ""
    info "Detected Cursor (macOS app)"
    prompt_install_extension "cursor" "Cursor" "$HOME/.cursor/extensions"
    EDITORS_FOUND=$((EDITORS_FOUND + 1))
fi

# Check for VS Code (command in PATH, or macOS app bundle with extensions dir)
if command -v code >/dev/null 2>&1 && [ -d "$HOME/.vscode" ]; then
    [ "$EDITORS_FOUND" -eq 0 ] && echo ""
    info "Detected VS Code"
    prompt_install_extension "code" "VS Code" "$HOME/.vscode/extensions"
    EDITORS_FOUND=$((EDITORS_FOUND + 1))
elif [ -d "/Applications/Visual Studio Code.app" ] && [ -d "$HOME/.vscode/extensions" ]; then
    [ "$EDITORS_FOUND" -eq 0 ] && echo ""
    info "Detected VS Code (macOS app)"
    prompt_install_extension "code" "VS Code" "$HOME/.vscode/extensions"
    EDITORS_FOUND=$((EDITORS_FOUND + 1))
fi

# Check for VSCodium (command in PATH, or macOS app bundle with extensions dir)
if command -v codium >/dev/null 2>&1 && [ -d "$HOME/.vscode-oss" ]; then
    [ "$EDITORS_FOUND" -eq 0 ] && echo ""
    info "Detected VSCodium"
    prompt_install_extension "codium" "VSCodium" "$HOME/.vscode-oss/extensions"
    EDITORS_FOUND=$((EDITORS_FOUND + 1))
elif [ -d "/Applications/VSCodium.app" ] && [ -d "$HOME/.vscode-oss/extensions" ]; then
    [ "$EDITORS_FOUND" -eq 0 ] && echo ""
    info "Detected VSCodium (macOS app)"
    prompt_install_extension "codium" "VSCodium" "$HOME/.vscode-oss/extensions"
    EDITORS_FOUND=$((EDITORS_FOUND + 1))
fi

# Show skip message only once at the end (for normal install)
if [ "$EDITORS_FOUND" -gt 0 ] && [ "$EDITOR_ONLY" -eq 0 ]; then
    echo ""
    echo "You can reinstall editor extensions later with:"
    echo "  $0 --editor-only"
fi

# Error if --editor-only but no editors found
if [ "$EDITORS_FOUND" -eq 0 ] && [ "$EDITOR_ONLY" -eq 1 ]; then
    error "No supported editor detected."
    echo ""
    echo "Supported editors: VS Code, Cursor, VSCodium"
    echo "Make sure the editor is installed and its CLI command is in PATH:"
    echo "  - VS Code: 'code' command (install from Command Palette)"
    echo "  - Cursor: 'cursor' command"
    echo "  - VSCodium: 'codium' command"
    exit 1
fi
