#!/usr/bin/env bash
# Aether Language Installer
# Usage: ./install.sh              (installs to ~/.aether)
#        ./install.sh /usr/local   (installs to /usr/local, needs sudo)
#        ./install.sh --editor-only (installs only editor extension)
set -eo pipefail

# Always run from the repository root (where this script lives)
cd "$(dirname "$0")"

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
    # Detect Linux distribution
    detect_linux_distro() {
        if [ -f /etc/os-release ]; then
            . /etc/os-release
            echo "$ID"
        elif [ -f /etc/debian_version ]; then
            echo "debian"
        elif [ -f /etc/redhat-release ]; then
            echo "rhel"
        elif [ -f /etc/arch-release ]; then
            echo "arch"
        else
            echo "unknown"
        fi
    }

    # Get install command for the current OS
    get_install_hint() {
        case "$(uname -s)" in
            Darwin)
                echo "  macOS: xcode-select --install"
                echo "     Or: brew install gcc make"
                ;;
            Linux)
                distro=$(detect_linux_distro)
                case "$distro" in
                    ubuntu|debian|pop|mint|elementary)
                        echo "  Debian/Ubuntu: sudo apt-get install build-essential"
                        ;;
                    fedora)
                        echo "  Fedora: sudo dnf install gcc make"
                        ;;
                    rhel|centos|rocky|almalinux)
                        echo "  RHEL/CentOS: sudo yum install gcc make"
                        ;;
                    arch|manjaro|endeavouros)
                        echo "  Arch Linux: sudo pacman -S base-devel"
                        ;;
                    opensuse*)
                        echo "  openSUSE: sudo zypper install gcc make"
                        ;;
                    alpine)
                        echo "  Alpine: apk add build-base"
                        ;;
                    void)
                        echo "  Void Linux: sudo xbps-install -S base-devel"
                        ;;
                    gentoo)
                        echo "  Gentoo: emerge sys-devel/gcc sys-devel/make"
                        ;;
                    *)
                        echo "  Linux: Install gcc and make using your package manager"
                        echo "         Common packages: build-essential, base-devel, or gcc + make"
                        ;;
                esac
                ;;
            MINGW*|MSYS*|CYGWIN*)
                echo "  Windows: Install MinGW-w64 from https://www.mingw-w64.org/"
                echo "           Add MinGW bin directory to PATH"
                echo "           Use: mingw32-make ae"
                ;;
            *)
                echo "  Install GCC (or Clang) and GNU Make for your platform"
                ;;
        esac
    }

    # Check prerequisites
    info "Checking prerequisites..."

    MISSING_DEPS=""

    if ! command -v gcc >/dev/null 2>&1 && ! command -v cc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then
        MISSING_DEPS="C compiler (gcc, clang, or cc)"
    fi

    if ! command -v make >/dev/null 2>&1; then
        if [ -n "$MISSING_DEPS" ]; then
            MISSING_DEPS="$MISSING_DEPS, make"
        else
            MISSING_DEPS="make"
        fi
    fi

    if [ -n "$MISSING_DEPS" ]; then
        error "Error: Missing prerequisites: $MISSING_DEPS"
        echo ""
        echo "Install the required tools:"
        get_install_hint
        echo ""
        echo "After installing, run this script again:"
        echo "  $0"
        exit 1
    fi

    # Show what compiler we found
    if command -v gcc >/dev/null 2>&1; then
        CC_VERSION=$(gcc --version 2>&1 | head -1)
    elif command -v clang >/dev/null 2>&1; then
        CC_VERSION=$(clang --version 2>&1 | head -1)
    else
        CC_VERSION=$(cc --version 2>&1 | head -1)
    fi
    ok "  C compiler: $CC_VERSION"
    ok "  make: $(make --version 2>&1 | head -1)"
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
    _cc=$(command -v gcc 2>/dev/null || command -v cc 2>/dev/null || command -v clang 2>/dev/null || echo gcc)
    if [ "$(uname -s)" = "Linux" ]; then
        if ! echo 'int main(){return 0;}' | "$_cc" -x c - -lreadline -o /dev/null 2>/dev/null; then
            distro=$(detect_linux_distro)
            case "$distro" in
                ubuntu|debian|pop|mint|elementary)
                    warn "  readline not found. Install with: sudo apt-get install libreadline-dev"
                    ;;
                fedora)
                    warn "  readline not found. Install with: sudo dnf install readline-devel"
                    ;;
                arch|manjaro|endeavouros)
                    warn "  readline not found. Install with: sudo pacman -S readline"
                    ;;
            esac
        fi
    fi
    make repl 2>&1 | tail -1 || warn "  REPL build skipped (install readline: apt-get install libreadline-dev / brew install readline)"
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
               runtime/memory runtime/config std std/string std/io std/math \
               std/net std/collections std/json std/fs std/log; do
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
    for subdir in string math net collections json fs log io file dir path tcp http list map; do
        if [ -d "std/$subdir" ]; then
            mkdir -p "$SRC_DIR/std/$subdir"
            cp std/$subdir/*.c std/$subdir/*.h "$SRC_DIR/std/$subdir/" 2>/dev/null || true
            # Copy module.ae files for import system
            cp std/$subdir/*.ae "$SRC_DIR/std/$subdir/" 2>/dev/null || true
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
        IS_FISH=0
        case "$SHELL_NAME" in
            zsh)  SHELL_RC="$HOME/.zshrc" ;;
            bash) SHELL_RC="$HOME/.bash_profile" ;;
            fish) SHELL_RC="$HOME/.config/fish/config.fish"; IS_FISH=1 ;;
        esac

        if [ -n "$SHELL_RC" ]; then
            # Ensure parent directory exists (fish config may not exist yet)
            mkdir -p "$(dirname "$SHELL_RC")"

            if grep -q "AETHER_HOME" "$SHELL_RC" 2>/dev/null; then
                # Update existing AETHER_HOME and PATH to point to the new install dir
                if [ "$IS_FISH" -eq 1 ]; then
                    sed -i.bak "s|set -gx AETHER_HOME .*|set -gx AETHER_HOME \"$INSTALL_DIR\"|" "$SHELL_RC"
                    sed -i.bak "s|fish_add_path .*aether.*|fish_add_path \"$BIN_DIR\"|" "$SHELL_RC"
                else
                    sed -i.bak "s|export AETHER_HOME=.*|$AETHER_HOME_LINE|" "$SHELL_RC"
                    # Match only PATH lines that start with the aether bin dir (not lines
                    # that happen to contain "aether" alongside other unrelated entries)
                    sed -i.bak "s|export PATH=\".*aether.*/bin:\\\$PATH\"|$EXPORT_LINE|" "$SHELL_RC"
                fi
                rm -f "$SHELL_RC.bak"
                ok "  Updated AETHER_HOME in $SHELL_RC"
            else
                # Fresh install -- append new block
                if [ -f "$SHELL_RC" ] && [ -s "$SHELL_RC" ]; then
                    if [ "$(tail -c 1 "$SHELL_RC" | wc -l)" -eq 0 ]; then
                        printf '\n' >> "$SHELL_RC"
                    fi
                fi
                echo "" >> "$SHELL_RC"
                echo "# Aether Language" >> "$SHELL_RC"
                if [ "$IS_FISH" -eq 1 ]; then
                    echo "set -gx AETHER_HOME \"$INSTALL_DIR\"" >> "$SHELL_RC"
                    echo "fish_add_path \"$BIN_DIR\"" >> "$SHELL_RC"
                else
                    echo "$AETHER_HOME_LINE" >> "$SHELL_RC"
                    echo "$EXPORT_LINE" >> "$SHELL_RC"
                fi
                ok "  Added to $SHELL_RC"
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

    if [ "$IN_PATH" -eq 0 ] && [ -n "$SHELL_RC" ]; then
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
    local version=$(cat "$(dirname "$0")/VERSION" 2>/dev/null | tr -d '[:space:]' || echo "0.0.0")
    local ext_name="aether-language-$version"
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
    elif [ ! -t 0 ]; then
        # Non-interactive (piped/CI) — skip prompt, auto-install
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
