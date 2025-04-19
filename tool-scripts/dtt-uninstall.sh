#!/bin/bash

HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.DTT-Data"
APP_NAME="DevToolsTerminal"
INSTALL_PATH="/usr/local/bin"
APP_PATH="$INSTALL_PATH/$APP_NAME"
SHELLS_FILE="/etc/shells"
REMOVE_DATA=false
SUDO_OPTS="-n" # Default to non-interactive mode

# Enhanced argument handling
for arg in "$@"; do
    case $arg in
        "-a"|"--all")
            REMOVE_DATA=true
            ;;
        "-S")
            SUDO_OPTS="-S" # Use -S to read password from stdin
            ;;
    esac
done

# Get current user info for shell restoration
CURRENT_USER=$(whoami)
USER_ENTRY=$(dscl . -read /Users/$CURRENT_USER UserShell 2>/dev/null)
CURRENT_SHELL=$(echo "$USER_ENTRY" | sed 's/UserShell: //')

# Restore original shell - improved to be more robust
if [ -f "$DATA_DIR/original_shell.txt" ]; then
    ORIGINAL_SHELL=$(cat "$DATA_DIR/original_shell.txt")
    if [ -x "$ORIGINAL_SHELL" ]; then
        echo "Restoring your original shell ($ORIGINAL_SHELL)..."
        # If current shell is DTT or we're explicitly trying to restore
        if [[ "$CURRENT_SHELL" == "$APP_PATH" || "$SHELL" == "$APP_PATH" ]]; then
            if ! chsh -s "$ORIGINAL_SHELL" 2>/dev/null; then
                echo "Attempting with sudo..."
                if sudo $SUDO_OPTS chsh -s "$ORIGINAL_SHELL" "$CURRENT_USER" 2>/dev/null; then
                    echo "Successfully restored original shell."
                else
                    echo "Warning: Failed to restore original shell."
                    echo "Please run this command manually to restore your shell:"
                    echo "    sudo chsh -s $ORIGINAL_SHELL $CURRENT_USER"
                fi
            else
                echo "Successfully restored original shell."
            fi
        else
            echo "Current shell is not DevToolsTerminal. No need to restore."
        fi
    else
        echo "Warning: Original shell ($ORIGINAL_SHELL) no longer exists or is not executable."
        echo "Attempting to set a default shell..."
        for default_shell in /bin/bash /bin/zsh /bin/sh; do
            if [ -x "$default_shell" ]; then
                if sudo $SUDO_OPTS chsh -s "$default_shell" "$CURRENT_USER" 2>/dev/null; then
                    echo "Set shell to $default_shell as a fallback."
                    break
                fi
            fi
        done
    fi
else
    echo "No record of original shell found."
    if [[ "$CURRENT_SHELL" == "$APP_PATH" || "$SHELL" == "$APP_PATH" ]]; then
        echo "Your current shell appears to be DevToolsTerminal."
        echo "Attempting to set a default shell..."
        for default_shell in /bin/bash /bin/zsh /bin/sh; do
            if [ -x "$default_shell" ]; then
                if sudo $SUDO_OPTS chsh -s "$default_shell" "$CURRENT_USER" 2>/dev/null || chsh -s "$default_shell" 2>/dev/null; then
                    echo "Set shell to $default_shell as a fallback."
                    break
                fi
            fi
        done
    fi
fi

echo "DevToolsTerminal Uninstaller"
echo "----------------------------"
echo "This will uninstall DevToolsTerminal and remove all associated configurations."
echo "Note: This script requires sudo privileges for some operations."
echo "If you're not running with sudo, you may need to enter your password."

# Check sudo access
if ! sudo -n true 2>/dev/null; then
    echo "Sudo access will be required during uninstallation."
    echo "You may be prompted for your password later."
fi

# Remove from /etc/shells
if grep -q "^$APP_PATH$" "$SHELLS_FILE"; then
    echo "Removing DevToolsTerminal from $SHELLS_FILE (requires sudo)..."
    if sudo $SUDO_OPTS sed -i.bak "\|^$APP_PATH$|d" "$SHELLS_FILE" 2>/dev/null; then
        echo "Successfully removed DevToolsTerminal from $SHELLS_FILE."
    else
        echo "Manual action required: Please run the following command to remove DevToolsTerminal from $SHELLS_FILE:"
        echo "    sudo sed -i.bak '\|^$APP_PATH$|d' $SHELLS_FILE"
    fi
fi

# Remove binary from install path
if [ -f "$APP_PATH" ]; then
    echo "Removing DevToolsTerminal binary from $APP_PATH (requires sudo)..."
    if sudo $SUDO_OPTS rm "$APP_PATH" 2>/dev/null; then
        echo "Successfully removed DevToolsTerminal binary."
    else
        echo "Manual action required: Please run the following command to remove the binary:"
        echo "    sudo rm $APP_PATH"
        echo "Continuing with uninstallation..."
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

