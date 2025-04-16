#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.DTT-Data"
APP_NAME="DevToolsTerminal"
APP_PATH="$DATA_DIR/$APP_NAME"
ZSHRC_PATH="$HOME/.zshrc"
GITHUB_API_URL="https://api.github.com/repos/cadenfinley/DevToolsTerminal/releases/latest"

echo -e "${GREEN}=======================================${NC}"
echo -e "${GREEN} DevToolsTerminal Installer            ${NC}"
echo -e "${GREEN}=======================================${NC}"
echo -e "${YELLOW}This will install DevToolsTerminal to $DATA_DIR and set it to auto-launch with zsh.${NC}"

if [ ! -d "$DATA_DIR" ]; then
    echo -e "${YELLOW}Creating directory $DATA_DIR...${NC}"
    mkdir -p "$DATA_DIR"
    echo -e "${GREEN}Created data directory${NC}"
fi

check_permissions() {
    if [ -w "$DATA_DIR" ]; then
        return 0
    else
        return 1
    fi
}

echo -e "${YELLOW}Checking for latest version...${NC}"
if command -v curl &> /dev/null; then
    RELEASE_DATA=$(curl -s "$GITHUB_API_URL")
    UNAME_STR=$(uname)
    DOWNLOAD_URL=$(echo "$RELEASE_DATA" | grep -o '"browser_download_url": "[^"]*' | grep -v '.exe' | head -1 | cut -d'"' -f4)
    LATEST_VERSION=$(echo "$RELEASE_DATA" | grep -o '"tag_name": "[^"]*' | head -1 | cut -d'"' -f4)
    
    if [ -n "$DOWNLOAD_URL" ]; then
        echo -e "${GREEN}Found latest version: $LATEST_VERSION${NC}"
        echo -e "${YELLOW}Downloading from GitHub: $DOWNLOAD_URL${NC}"
    else
        echo -e "${RED}Error: Couldn't find download URL on GitHub.${NC}"
        exit 1
    fi
    
    curl -L -o "$APP_PATH" "$DOWNLOAD_URL"
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Failed to download DevToolsTerminal.${NC}"
        exit 1
    fi
else
    echo -e "${RED}Error: curl is not installed. Cannot download the application.${NC}"
    exit 1
fi

chmod +x "$APP_PATH"

if [ ! -x "$APP_PATH" ]; then
    echo -e "${RED}Error: Could not make DevToolsTerminal executable.${NC}"
    exit 1
fi

echo -e "${GREEN}Application path: $APP_PATH${NC}"

if [ ! -f "$ZSHRC_PATH" ]; then
    echo -e "${YELLOW}Creating new .zshrc file...${NC}"
    touch "$ZSHRC_PATH"
fi

if grep -q "# DevToolsTerminal Auto-Launch" "$ZSHRC_PATH"; then
    sed -i.bak "s|\".*DevToolsTerminal\"|\"$APP_PATH\"|g" "$ZSHRC_PATH" && rm "$ZSHRC_PATH.bak"
    echo -e "${GREEN}Updated auto-start configuration in .zshrc.${NC}"
else
    echo -e "${YELLOW}Adding auto-start to .zshrc...${NC}"
    echo "" >> "$ZSHRC_PATH"
    echo "# DevToolsTerminal Auto-Launch" >> "$ZSHRC_PATH"
    echo "if [ -x \"$APP_PATH\" ]; then" >> "$ZSHRC_PATH"
    echo "    \"$APP_PATH\"" >> "$ZSHRC_PATH"
    echo "fi" >> "$ZSHRC_PATH"
    
    echo -e "${GREEN}Auto-start configuration added to .zshrc.${NC}"
fi

UNINSTALL_SCRIPT="$SCRIPT_DIR/dtt-uninstall.sh"
if [ -f "$UNINSTALL_SCRIPT" ]; then
    chmod +x "$UNINSTALL_SCRIPT"
    cp "$UNINSTALL_SCRIPT" "$DATA_DIR/uninstall-$APP_NAME.sh"
    chmod +x "$DATA_DIR/uninstall-$APP_NAME.sh"
fi

echo -e "${GREEN}=======================================${NC}"
echo -e "${GREEN} Installation Complete                 ${NC}"
echo -e "${GREEN}=======================================${NC}"
echo ""
echo -e "${YELLOW}Please restart your terminal to start using DevToolsTerminal.${NC}"

echo ""
read -p "Would you like to delete this installer script? (y/n): " delete_choice

if [[ "$delete_choice" =~ ^[Yy]$ ]]; then
    SELF_PATH="$SCRIPT_DIR/$(basename "${BASH_SOURCE[0]}")"
    echo -e "${YELLOW}Removing installer script...${NC}"
    rm "$SELF_PATH"
    echo -e "${GREEN}Installer deleted.${NC}"
fi

