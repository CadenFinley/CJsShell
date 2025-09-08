#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Extract version from cjsh.h
VERSION_FILE="$SCRIPT_DIR/../include/cjsh.h"
if [ -f "$VERSION_FILE" ]; then
    VERSION=$(grep "const std::string c_version" "$VERSION_FILE" | sed 's/.*"\(.*\)".*/\1/')
    if [ -z "$VERSION" ]; then
        echo -e "${YELLOW}Warning: Could not extract version from $VERSION_FILE, using 'unknown'${NC}"
        VERSION="unknown"
    fi
else
    echo -e "${YELLOW}Warning: Version file not found, using 'unknown'${NC}"
    VERSION="unknown"
fi

echo -e "${YELLOW}Installing cjsh_${VERSION}_prerelease...${NC}"

EXECUTABLE_PATH="$SCRIPT_DIR/../build/cjsh"

if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo -e "${YELLOW}Error: Executable not found at $EXECUTABLE_PATH${NC}"
    exit 1
fi

INSTALL_DIR="/usr/local/bin"
TARGET="$INSTALL_DIR/cjsh_${VERSION}_prerelease"

echo -e "${YELLOW}Installing to $TARGET...${NC}"
sudo cp "$EXECUTABLE_PATH" "$TARGET"
sudo chmod +x "$TARGET"

if [ -f "$TARGET" ]; then
    echo -e "${GREEN}cjsh_${VERSION}_prerelease has been successfully installed to $INSTALL_DIR${NC}"
    echo -e "${GREEN}You can now run it by typing 'cjsh_${VERSION}_prerelease' in your terminal${NC}"
else
    echo -e "${YELLOW}Error: Installation failed.${NC}"
    exit 1
fi

exit 0
