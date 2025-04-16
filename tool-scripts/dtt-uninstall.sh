#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.DTT-Data"
APP_NAME="DevToolsTerminal"
APP_PATH="$DATA_DIR/$APP_NAME"
ZSHRC_PATH="$HOME/.zshrc"
UNINSTALL_SCRIPT="$DATA_DIR/uninstall-$APP_NAME.sh"

SYSTEM_INSTALL_DIR="/usr/local/bin"
USER_INSTALL_DIR="$HOME/.local/bin"
SYSTEM_APP_PATH="$SYSTEM_INSTALL_DIR/$APP_NAME"
USER_APP_PATH="$USER_INSTALL_DIR/$APP_NAME"
LEGACY_UNINSTALL_SCRIPT="$SYSTEM_INSTALL_DIR/uninstall-$APP_NAME.sh"
LEGACY_USER_UNINSTALL_SCRIPT="$USER_INSTALL_DIR/uninstall-$APP_NAME.sh"

echo -e "${GREEN}=======================================${NC}"
echo -e "${GREEN} DevToolsTerminal Uninstaller          ${NC}"
echo -e "${GREEN}=======================================${NC}"
echo -e "${YELLOW}This will uninstall DevToolsTerminal and remove auto-launch from zsh.${NC}"

remove_from_zshrc() {
    if [ -f "$ZSHRC_PATH" ]; then
        echo -e "${YELLOW}Removing auto-launch configuration from .zshrc...${NC}"
        TEMP_FILE=$(mktemp)
        
        sed '/# DevToolsTerminal Auto-Launch/,+3d' "$ZSHRC_PATH" > "$TEMP_FILE"
        
        if [ -d "$USER_INSTALL_DIR" ]; then
            sed "/export PATH=\"\$PATH:$USER_INSTALL_DIR\"/d" "$TEMP_FILE" > "${TEMP_FILE}.2"
            mv "${TEMP_FILE}.2" "$TEMP_FILE"
        fi
        
        cat "$TEMP_FILE" > "$ZSHRC_PATH"
        rm "$TEMP_FILE"
        
        echo -e "${GREEN}Auto-launch configuration removed from .zshrc.${NC}"
    else
        echo -e "${YELLOW}No .zshrc file found.${NC}"
    fi
}

APP_FOUND=false

if [ -f "$APP_PATH" ]; then
    echo -e "${GREEN}Found DevToolsTerminal at $APP_PATH${NC}"
    APP_FOUND=true
    
    echo -e "${YELLOW}Removing executable from $APP_PATH...${NC}"
    rm "$APP_PATH"
    
    if [ -f "$UNINSTALL_SCRIPT" ] && [ "$UNINSTALL_SCRIPT" != "$0" ]; then
        rm "$UNINSTALL_SCRIPT"
    fi
fi

if [ -f "$SYSTEM_APP_PATH" ]; then
    echo -e "${YELLOW}Found legacy installation at $SYSTEM_APP_PATH${NC}"
    APP_FOUND=true
    
    if [ -w "$SYSTEM_INSTALL_DIR" ]; then
        echo -e "${YELLOW}Removing legacy installation from $SYSTEM_APP_PATH...${NC}"
        rm "$SYSTEM_APP_PATH"
        if [ -f "$LEGACY_UNINSTALL_SCRIPT" ]; then
            rm "$LEGACY_UNINSTALL_SCRIPT"
        fi
    elif sudo -n true 2>/dev/null; then
        echo -e "${YELLOW}Removing legacy installation with sudo...${NC}"
        sudo rm "$SYSTEM_APP_PATH"
        if [ -f "$LEGACY_UNINSTALL_SCRIPT" ]; then
            sudo rm "$LEGACY_UNINSTALL_SCRIPT"
        fi
    else
        echo -e "${RED}Warning: You need root privileges to remove legacy installation at $SYSTEM_APP_PATH${NC}"
        echo -e "${YELLOW}Please run: sudo rm $SYSTEM_APP_PATH${NC}"
        if [ -f "$LEGACY_UNINSTALL_SCRIPT" ]; then
            echo -e "${YELLOW}And also run: sudo rm $LEGACY_UNINSTALL_SCRIPT${NC}"
        fi
    fi
fi

if [ -f "$USER_APP_PATH" ]; then
    echo -e "${YELLOW}Found legacy user installation at $USER_APP_PATH${NC}"
    APP_FOUND=true
    
    echo -e "${YELLOW}Removing legacy user installation...${NC}"
    rm "$USER_APP_PATH"
    
    if [ -f "$LEGACY_USER_UNINSTALL_SCRIPT" ]; then
        rm "$LEGACY_USER_UNINSTALL_SCRIPT"
    fi
    
    if [ -d "$USER_INSTALL_DIR" ] && [ -z "$(ls -A "$USER_INSTALL_DIR")" ]; then
        echo -e "${YELLOW}Removing empty directory $USER_INSTALL_DIR...${NC}"
        rmdir "$USER_INSTALL_DIR"
    fi
fi

if [ "$APP_FOUND" = false ]; then
    echo -e "${RED}Error: DevToolsTerminal installation not found.${NC}"
    echo -e "${YELLOW}Checked locations:${NC}"
    echo -e "${YELLOW}  - $APP_PATH (current)${NC}"
    echo -e "${YELLOW}  - $SYSTEM_APP_PATH (legacy)${NC}"
    echo -e "${YELLOW}  - $USER_APP_PATH (legacy)${NC}"
else
    remove_from_zshrc
    
    echo -e "${GREEN}=======================================${NC}"
    echo -e "${GREEN} Uninstallation Complete               ${NC}"
    echo -e "${GREEN}=======================================${NC}"
    
    echo -e "${YELLOW}Note: Your personal data in $DATA_DIR has not been removed.${NC}"
    read -p "Would you like to remove all data? (y/n): " remove_data
    if [[ "$remove_data" =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Removing all data from $DATA_DIR...${NC}"
        rm -rf "$DATA_DIR"
        echo -e "${GREEN}All data removed.${NC}"
    else
        echo -e "${YELLOW}Data directory preserved. To manually remove it later, run: rm -rf $DATA_DIR${NC}"
    fi
fi

SCRIPT_PATH=$(realpath "$0")
if [[ "$SCRIPT_PATH" != "$UNINSTALL_SCRIPT" && "$SCRIPT_PATH" != "$LEGACY_UNINSTALL_SCRIPT" && "$SCRIPT_PATH" != "$LEGACY_USER_UNINSTALL_SCRIPT" ]]; then
    rm "$SCRIPT_PATH"
fi