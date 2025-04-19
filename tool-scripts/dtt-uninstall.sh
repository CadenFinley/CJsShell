#!/bin/bash

HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.DTT-Data"
APP_NAME="DevToolsTerminal"
INSTALL_PATH="/usr/local/bin"
APP_PATH="$INSTALL_PATH/$APP_NAME"
SHELLS_FILE="/etc/shells"
REMOVE_DATA=false

# Simple argument handling
if [[ "$1" == "-a" || "$1" == "--all" ]]; then
    REMOVE_DATA=true
fi

echo "DevToolsTerminal Uninstaller"
echo "----------------------------"
echo "This will uninstall DevToolsTerminal and remove all associated configurations."

# Check if DevToolsTerminal is the current shell, if so, revert to original
if [[ "$SHELL" == "$APP_PATH" ]]; then
    if [ -f "$DATA_DIR/original_shell.txt" ]; then
        ORIGINAL_SHELL=$(cat "$DATA_DIR/original_shell.txt")
        echo "Reverting to your original shell ($ORIGINAL_SHELL)..."
        chsh -s "$ORIGINAL_SHELL"
        if [ $? -ne 0 ]; then
            echo "Warning: Failed to revert to original shell. Please run 'chsh -s $ORIGINAL_SHELL' manually."
        else
            echo "Successfully reverted to original shell."
        fi
    else
        echo "Warning: Cannot find original shell information. You may need to set your shell manually."
    fi
fi

# Remove from /etc/shells
if grep -q "^$APP_PATH$" "$SHELLS_FILE"; then
    echo "Removing DevToolsTerminal from $SHELLS_FILE (requires sudo)..."
    sudo sed -i.bak "\|^$APP_PATH$|d" "$SHELLS_FILE"
    if [ $? -ne 0 ]; then
        echo "Warning: Failed to remove DevToolsTerminal from $SHELLS_FILE. You may need to do this manually."
    fi
fi

# Remove binary from install path
if [ -f "$APP_PATH" ]; then
    echo "Removing DevToolsTerminal binary from $APP_PATH (requires sudo)..."
    sudo rm "$APP_PATH"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to remove binary. Please check your permissions."
        exit 1
    fi
fi

# Remove data directory if requested
if $REMOVE_DATA; then
    echo "Removing data directory at $DATA_DIR..."
    rm -rf "$DATA_DIR"
    if [ $? -ne 0 ]; then
        echo "Warning: Failed to remove data directory. You may need to remove it manually."
    fi
else
    echo "Data directory at $DATA_DIR has been preserved."
    echo "To remove it manually, run: rm -rf $DATA_DIR"
fi

echo "DevToolsTerminal has been uninstalled successfully."

