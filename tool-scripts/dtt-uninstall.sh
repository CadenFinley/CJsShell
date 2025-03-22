#!/bin/bash

# Default installation locations to check
SYSTEM_INSTALL_DIR="/usr/local/bin"
USER_INSTALL_DIR="$HOME/.local/bin"
APP_NAME="DevToolsTerminal"
SYSTEM_APP_PATH="$SYSTEM_INSTALL_DIR/$APP_NAME"
USER_APP_PATH="$USER_INSTALL_DIR/$APP_NAME"
ZSHRC_PATH="$HOME/.zshrc"
UNINSTALL_SCRIPT="$SYSTEM_INSTALL_DIR/uninstall-$APP_NAME.sh"
USER_UNINSTALL_SCRIPT="$USER_INSTALL_DIR/uninstall-$APP_NAME.sh"

echo "DevToolsTerminal Uninstaller"
echo "---------------------------"
echo "This will uninstall DevToolsTerminal and remove auto-launch from zsh."

# Function to remove auto-launch entries from .zshrc
remove_from_zshrc() {
    if [ -f "$ZSHRC_PATH" ]; then
        echo "Removing auto-launch configuration from .zshrc..."
        # Create a temporary file
        TEMP_FILE=$(mktemp)
        
        # Filter out the DevToolsTerminal auto-launch block
        sed '/# DevToolsTerminal Auto-Launch/,+3d' "$ZSHRC_PATH" > "$TEMP_FILE"
        
        # Also remove any PATH additions for the user installation
        if [ -d "$USER_INSTALL_DIR" ]; then
            sed "/export PATH=\"\$PATH:$USER_INSTALL_DIR\"/d" "$TEMP_FILE" > "${TEMP_FILE}.2"
            mv "${TEMP_FILE}.2" "$TEMP_FILE"
        fi
        
        # Replace the original file
        cat "$TEMP_FILE" > "$ZSHRC_PATH"
        rm "$TEMP_FILE"
        
        echo "Auto-launch configuration removed from .zshrc."
    else
        echo "No .zshrc file found."
    fi
}

# Try to find the installed application
APP_FOUND=false

if [ -f "$SYSTEM_APP_PATH" ]; then
    echo "Found DevToolsTerminal at $SYSTEM_APP_PATH"
    APP_PATH="$SYSTEM_APP_PATH"
    APP_FOUND=true
    
    # Check if we have permission to remove it
    if [ -w "$SYSTEM_INSTALL_DIR" ]; then
        echo "Removing $APP_PATH..."
        rm "$APP_PATH"
        # Also remove the uninstall script if it exists
        if [ -f "$UNINSTALL_SCRIPT" ]; then
            rm "$UNINSTALL_SCRIPT"
        fi
    elif sudo -n true 2>/dev/null; then
        echo "Removing $APP_PATH with sudo..."
        sudo rm "$APP_PATH"
        # Also remove the uninstall script if it exists
        if [ -f "$UNINSTALL_SCRIPT" ]; then
            sudo rm "$UNINSTALL_SCRIPT"
        fi
    else
        echo "Error: You need root privileges to remove $APP_PATH"
        echo "Please run: sudo rm $APP_PATH"
        if [ -f "$UNINSTALL_SCRIPT" ]; then
            echo "And also run: sudo rm $UNINSTALL_SCRIPT"
        fi
    fi
fi

if [ -f "$USER_APP_PATH" ]; then
    echo "Found DevToolsTerminal at $USER_APP_PATH"
    APP_PATH="$USER_APP_PATH"
    APP_FOUND=true
    
    echo "Removing $APP_PATH..."
    rm "$USER_APP_PATH"
    
    # Also remove the user uninstall script if it exists
    if [ -f "$USER_UNINSTALL_SCRIPT" ]; then
        rm "$USER_UNINSTALL_SCRIPT"
    fi
    
    # Clean up the directory if it's empty
    if [ -d "$USER_INSTALL_DIR" ] && [ -z "$(ls -A "$USER_INSTALL_DIR")" ]; then
        echo "Removing empty directory $USER_INSTALL_DIR..."
        rmdir "$USER_INSTALL_DIR"
    fi
fi

if [ "$APP_FOUND" = false ]; then
    echo "Error: DevToolsTerminal installation not found."
    echo "Checked locations:"
    echo "  - $SYSTEM_APP_PATH"
    echo "  - $USER_APP_PATH"
else
    # Remove from .zshrc regardless of which installation was found
    remove_from_zshrc
    
    echo "Uninstallation complete!"
    echo "Note: Your personal data in ~/.DTT-Data has not been removed."
    echo "To completely remove all data, run: rm -rf ~/.DTT-Data"
fi

# Self-delete if this script was executed directly and isn't in a system path
SCRIPT_PATH=$(realpath "$0")
if [[ "$SCRIPT_PATH" != "$UNINSTALL_SCRIPT" && "$SCRIPT_PATH" != "$USER_UNINSTALL_SCRIPT" ]]; then
    rm "$SCRIPT_PATH"
fi