#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Installing cjsh_debug...${NC}"

EXECUTABLE_PATH="$SCRIPT_DIR/../build/cjsh"

if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo -e "${YELLOW}Error: Executable not found at $EXECUTABLE_PATH${NC}"
    exit 1
fi

INSTALL_DIR="/usr/local/bin"
TARGET="$INSTALL_DIR/cjsh_debug"

echo -e "${YELLOW}Installing to $TARGET...${NC}"
sudo cp "$EXECUTABLE_PATH" "$TARGET"
sudo chmod +x "$TARGET"

if [ -f "$TARGET" ]; then
    echo -e "${GREEN}cjsh_debug has been successfully installed to $INSTALL_DIR${NC}"
    echo -e "${GREEN}You can now run it by typing 'cjsh_debug' in your terminal${NC}"
else
    echo -e "${YELLOW}Error: Installation failed.${NC}"
    exit 1
fi

exit 0
