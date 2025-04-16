#!/bin/bash

# Installation locations to check
HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.DTT-Data"
APP_NAME="DevToolsTerminal"
APP_PATH="$DATA_DIR/$APP_NAME"
ZSHRC_PATH="$HOME/.zshrc"
UNINSTALL_SCRIPT="$DATA_DIR/uninstall-$APP_NAME.sh"

# Legacy installation locations to check for backward compatibility
SYSTEM_INSTALL_DIR="/usr/local/bin"
USER_INSTALL_DIR="$HOME/.local/bin"
SYSTEM_APP_PATH="$SYSTEM_INSTALL_DIR/$APP_NAME"
USER_APP_PATH="$USER_INSTALL_DIR/$APP_NAME"
LEGACY_UNINSTALL_SCRIPT="$SYSTEM_INSTALL_DIR/uninstall-$APP_NAME.sh"
LEGACY_USER_UNINSTALL_SCRIPT="$USER_INSTALL_DIR/uninstall-$APP_NAME.sh"

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
        
        # Also remove any PATH additions for legacy user installation
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

# First check the new location in .DTT-Data
if [ -f "$APP_PATH" ]; then
    echo "Found DevToolsTerminal at $APP_PATH"
    APP_FOUND=true
    
    echo "Removing executable from $APP_PATH..."
    rm "$APP_PATH"
    
    # Also remove the uninstall script from the data directory if it exists
    if [ -f "$UNINSTALL_SCRIPT" ] && [ "$UNINSTALL_SCRIPT" != "$0" ]; then
        rm "$UNINSTALL_SCRIPT"
    fi
fi

# Check for legacy installations and remove them if found
if [ -f "$SYSTEM_APP_PATH" ]; then
    echo "Found legacy installation at $SYSTEM_APP_PATH"
    APP_FOUND=true
    
    # Check if we have permission to remove it
    if [ -w "$SYSTEM_INSTALL_DIR" ]; then
        echo "Removing legacy installation from $SYSTEM_APP_PATH..."
        rm "$SYSTEM_APP_PATH"
        # Also remove the uninstall script if it exists
        if [ -f "$LEGACY_UNINSTALL_SCRIPT" ]; then
            rm "$LEGACY_UNINSTALL_SCRIPT"
        fi
    elif sudo -n true 2>/dev/null; then
        echo "Removing legacy installation with sudo..."
        sudo rm "$SYSTEM_APP_PATH"
        # Also remove the uninstall script if it exists
        if [ -f "$LEGACY_UNINSTALL_SCRIPT" ]; then
            sudo rm "$LEGACY_UNINSTALL_SCRIPT"
        fi
    else
        echo "Warning: You need root privileges to remove legacy installation at $SYSTEM_APP_PATH"
        echo "Please run: sudo rm $SYSTEM_APP_PATH"
        if [ -f "$LEGACY_UNINSTALL_SCRIPT" ]; then
            echo "And also run: sudo rm $LEGACY_UNINSTALL_SCRIPT"
        fi
    fi
fi

if [ -f "$USER_APP_PATH" ]; then
    echo "Found legacy user installation at $USER_APP_PATH"
    APP_FOUND=true
    
    echo "Removing legacy user installation..."
    rm "$USER_APP_PATH"
    
    # Also remove the user uninstall script if it exists
    if [ -f "$LEGACY_USER_UNINSTALL_SCRIPT" ]; then
        rm "$LEGACY_USER_UNINSTALL_SCRIPT"
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
    echo "  - $APP_PATH (current)"
    echo "  - $SYSTEM_APP_PATH (legacy)"
    echo "  - $USER_APP_PATH (legacy)"
else
    # Remove from .zshrc regardless of which installation was found
    remove_from_zshrc
    
    echo "Uninstallation complete!"
    echo "Note: Your personal data in $DATA_DIR has not been removed."
    read -p "Would you like to remove all data? (y/n): " remove_data
    if [[ "$remove_data" =~ ^[Yy]$ ]]; then
        echo "Removing all data from $DATA_DIR..."
        rm -rf "$DATA_DIR"
        echo "All data removed."
    else
        echo "Data directory preserved. To manually remove it later, run: rm -rf $DATA_DIR"
    fi
fi

# Self-delete if this script was executed directly
SCRIPT_PATH=$(realpath "$0")
if [[ "$SCRIPT_PATH" != "$UNINSTALL_SCRIPT" && "$SCRIPT_PATH" != "$LEGACY_UNINSTALL_SCRIPT" && "$SCRIPT_PATH" != "$LEGACY_USER_UNINSTALL_SCRIPT" ]]; then
    rm "$SCRIPT_PATH"
fi