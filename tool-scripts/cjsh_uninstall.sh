#!/bin/bash

# Set up variables to match the install script
HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.cjsh_data"
APP_NAME="cjsh"
INSTALL_PATH="/usr/local/bin"
APP_PATH="$INSTALL_PATH/$APP_NAME"
SHELLS_FILE="/etc/shells"

# Check for --all flag
REMOVE_USER_DATA=false
for arg in "$@"; do
    if [ "$arg" = "--all" ]; then
        REMOVE_USER_DATA=true
    fi
done

echo "CJ's Shell Uninstaller"
echo "-------------------------"
echo "This will completely remove CJ's Shell from your system."
if [ "$REMOVE_USER_DATA" = true ]; then
    echo "WARNING: All user data in $DATA_DIR will be removed!"
else
    echo "Note: Your user data in $DATA_DIR will be preserved."
    echo "To remove all data, run with the --all flag."
fi
echo "Press ENTER to continue or CTRL+C to cancel..."
read

# Check if the binary exists
if [ -f "$APP_PATH" ]; then
    echo "Removing CJ's Shell binary from $APP_PATH (requires sudo)..."
    sudo rm -f "$APP_PATH"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to remove CJ's Shell binary. Please check your permissions."
    else
        echo "Binary removed successfully."
    fi
else
    echo "CJ's Shell binary not found at $APP_PATH. Skipping removal."
fi

# Remove from /etc/shells
if grep -q "^$APP_PATH$" "$SHELLS_FILE"; then
    echo "Removing CJ's Shell from $SHELLS_FILE (requires sudo)..."
    sudo sed -i.bak "\|^$APP_PATH$|d" "$SHELLS_FILE"
    if [ $? -ne 0 ]; then
        echo "Warning: Failed to remove CJ's Shell from $SHELLS_FILE."
    else
        echo "Entry removed from $SHELLS_FILE successfully."
        # Remove backup file created by sed on macOS
        if [ -f "${SHELLS_FILE}.bak" ]; then
            sudo rm -f "${SHELLS_FILE}.bak"
        fi
    fi
fi

# Restore original shell if we changed it
if [ -f "$DATA_DIR/original_shell.txt" ]; then
    ORIGINAL_SHELL=$(cat "$DATA_DIR/original_shell.txt")
    if [ -n "$ORIGINAL_SHELL" ] && [ -x "$ORIGINAL_SHELL" ]; then
        echo "Restoring your original shell ($ORIGINAL_SHELL)..."
        chsh -s "$ORIGINAL_SHELL"
        if [ $? -ne 0 ]; then
            echo "Warning: Failed to restore original shell. You may need to run 'chsh -s $ORIGINAL_SHELL' manually."
        else
            echo "Original shell restored successfully."
        fi
    else
        echo "Original shell information found but appears invalid. You may need to set your shell manually."
    fi
fi

# Clean up file handler registrations
if [ "$(uname)" == "Darwin" ]; then
    # macOS cleanup
    if [ -d ~/Library/Application\ Support/CJsShell ]; then
        echo "Removing macOS application support files..."
        rm -rf ~/Library/Application\ Support/CJsShell
        if [ $? -ne 0 ]; then
            echo "Warning: Failed to remove application support files."
        else
            echo "macOS application support files removed successfully."
        fi
    fi
elif [ "$(uname)" == "Linux" ]; then
    # Linux cleanup
    if [ -f ~/.local/share/applications/cjshell.desktop ]; then
        echo "Removing Linux desktop integration..."
        rm -f ~/.local/share/applications/cjshell.desktop
        if [ $? -ne 0 ]; then
            echo "Warning: Failed to remove desktop integration."
        else
            echo "Linux desktop integration removed successfully."
            # Update desktop database
            update-desktop-database ~/.local/share/applications &>/dev/null || true
        fi
    fi
fi

# Remove data directory
if [ "$REMOVE_USER_DATA" = true ]; then
    if [ -d "$DATA_DIR" ]; then
        echo "Removing data directory at $DATA_DIR..."
        rm -rf "$DATA_DIR"
        if [ $? -ne 0 ]; then
            echo "Warning: Failed to remove data directory. You may need to remove it manually."
        else
            echo "Data directory removed successfully."
        fi
    fi
else
    echo "Preserving user data in $DATA_DIR."
    echo "If you want to remove it later, you can manually delete this directory."
fi

echo "Uninstallation complete! CJ's Shell has been removed from your system."
echo "You may need to restart your terminal for all changes to take effect."
