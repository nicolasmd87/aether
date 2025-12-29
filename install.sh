#!/bin/bash
#
# Aether Programming Language Installer
# Install script for Linux and macOS
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
INSTALL_DIR="${INSTALL_DIR:-$HOME/.aether}"
BIN_DIR="${INSTALL_DIR}/bin"
RUNTIME_DIR="${INSTALL_DIR}/runtime"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Aether Programming Language Installer${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check for GCC
if ! command -v gcc &> /dev/null; then
    echo -e "${RED}Error: GCC not found${NC}"
    echo "Please install GCC before installing Aether:"
    echo ""
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "  macOS: xcode-select --install"
        echo "    or: brew install gcc"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "  Ubuntu/Debian: sudo apt-get install gcc"
        echo "  Fedora/RHEL: sudo dnf install gcc"
        echo "  Arch: sudo pacman -S gcc"
    fi
    exit 1
fi

echo -e "${GREEN}✓${NC} GCC found: $(gcc --version | head -n1)"

# Create directories
echo -e "\n${YELLOW}Creating directories...${NC}"
mkdir -p "$BIN_DIR"
mkdir -p "$RUNTIME_DIR"

# Build compiler
echo -e "\n${YELLOW}Building Aether compiler...${NC}"
if [ -f "Makefile" ]; then
    make clean || true
    make
    
    if [ -f "build/aetherc" ]; then
        cp build/aetherc "$BIN_DIR/"
        chmod +x "$BIN_DIR/aetherc"
        echo -e "${GREEN}✓${NC} Compiler built successfully"
    else
        echo -e "${RED}Error: Build failed${NC}"
        exit 1
    fi
else
    echo -e "${RED}Error: Makefile not found${NC}"
    echo "Please run this script from the Aether project root"
    exit 1
fi

# Copy runtime files
echo -e "\n${YELLOW}Installing runtime library...${NC}"
if [ -d "runtime" ]; then
    cp runtime/*.c runtime/*.h "$RUNTIME_DIR/"
    echo -e "${GREEN}✓${NC} Runtime installed"
else
    echo -e "${RED}Error: Runtime directory not found${NC}"
    exit 1
fi

# Add to PATH
echo -e "\n${YELLOW}Configuring PATH...${NC}"

SHELL_RC=""
if [ -n "$BASH_VERSION" ]; then
    SHELL_RC="$HOME/.bashrc"
elif [ -n "$ZSH_VERSION" ]; then
    SHELL_RC="$HOME/.zshrc"
else
    SHELL_RC="$HOME/.profile"
fi

PATH_LINE="export PATH=\"\$PATH:$BIN_DIR\""
AETHER_RUNTIME_LINE="export AETHER_RUNTIME=\"$RUNTIME_DIR\""

if ! grep -q "AETHER_RUNTIME" "$SHELL_RC" 2>/dev/null; then
    echo "" >> "$SHELL_RC"
    echo "# Aether Programming Language" >> "$SHELL_RC"
    echo "$PATH_LINE" >> "$SHELL_RC"
    echo "$AETHER_RUNTIME_LINE" >> "$SHELL_RC"
    echo -e "${GREEN}✓${NC} Added to $SHELL_RC"
    echo -e "${YELLOW}  Please run: source $SHELL_RC${NC}"
else
    echo -e "${GREEN}✓${NC} PATH already configured"
fi

# Create aether wrapper script
echo -e "\n${YELLOW}Creating aether CLI wrapper...${NC}"
cat > "$BIN_DIR/aether" << 'EOF'
#!/bin/bash
# Aether CLI wrapper

AETHER_RUNTIME="${AETHER_RUNTIME:-$HOME/.aether/runtime}"

case "$1" in
    build)
        shift
        input="${1:?Input file required}"
        output="${2:-${input%.ae}.c}"
        aetherc "$input" "$output"
        ;;
    run)
        shift
        input="${1:?Input file required}"
        temp_c="${input}.c"
        temp_exe="${input}.exe"
        
        aetherc "$input" "$temp_c" || exit 1
        gcc "$temp_c" -I"$AETHER_RUNTIME" "$AETHER_RUNTIME"/*.c -o "$temp_exe" -lpthread || exit 1
        "./$temp_exe"
        rm -f "$temp_c" "$temp_exe"
        ;;
    version|--version|-v)
        aetherc --version
        ;;
    help|--help|-h)
        echo "Aether Programming Language"
        echo ""
        echo "Usage:"
        echo "  aether build <input.ae> [output.c]  Compile Aether to C"
        echo "  aether run <input.ae>                Compile and run"
        echo "  aether version                       Show version"
        echo "  aether help                          Show this help"
        ;;
    *)
        aetherc "$@"
        ;;
esac
EOF

chmod +x "$BIN_DIR/aether"
echo -e "${GREEN}✓${NC} Created aether CLI wrapper"

# Summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Installation complete!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Installed to: $INSTALL_DIR"
echo ""
echo "To use Aether, run:"
echo -e "  ${YELLOW}source $SHELL_RC${NC}  (or restart your terminal)"
echo ""
echo "Then try:"
echo -e "  ${YELLOW}aether version${NC}"
echo -e "  ${YELLOW}aether run examples/basic/hello_world.ae${NC}"
echo ""

