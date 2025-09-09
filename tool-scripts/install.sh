#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${YELLOW}Installing cjsh...${NC}"
echo -e "${YELLOW}Make sure you have built the project before running this script.${NC}"
echo -e "${YELLOW}WARNING: cjsh is only ~90% POSIX compliant. Setting it as your default login shell can have unintended consequences. AND THERE IS NO WARRANTY.${NC}"

EXECUTABLE_PATH="$SCRIPT_DIR/../build/cjsh"

if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo -e "${RED}Error: Executable not found at $EXECUTABLE_PATH${NC}"
    exit 1
fi

INSTALL_DIR="/usr/local/bin"
TARGET="$INSTALL_DIR/cjsh"

echo -e "${YELLOW}Installing to $TARGET...${NC}"
sudo cp "$EXECUTABLE_PATH" "$TARGET"
sudo chmod +x "$TARGET"

if [ -f "$TARGET" ]; then
    echo -e "${GREEN}cjsh has been successfully installed to $INSTALL_DIR${NC}"
    echo -e "${GREEN}You can now run it by typing 'cjsh' in your terminal${NC}"
else
    echo -e "${RED}Error: Installation failed.${NC}"
    exit 1
fi

exit 0
