#!/bin/bash

HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.cjsh_data"
APP_NAME="cjsh"
INSTALL_PATH="/usr/local/bin"
APP_PATH="$INSTALL_PATH/$APP_NAME"
SHELLS_FILE="/etc/shells"
REMOVE_DATA=false
SUDO_OPTS="-n" # Default to non-interactive mode
INITIAL_SUDO=false

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

# Request sudo upfront
echo "CJ's Shell Uninstaller"
echo "----------------------------"
echo "This will uninstall CJ's Shell and remove all associated configurations."
echo "Note: This script requires sudo privileges for some operations."

if sudo -n true 2>/dev/null; then
    INITIAL_SUDO=true
    echo "Sudo access verified."
else
    echo "Please enter your password to proceed with uninstallation:"
    if sudo -v; then
        INITIAL_SUDO=true
        echo "Sudo access granted."
    else
        echo "Failed to get sudo access. Some operations may fail."
    fi
fi

# Get current user info for shell restoration
CURRENT_USER=$(whoami)
USER_ENTRY=$(dscl . -read /Users/$CURRENT_USER UserShell 2>/dev/null)
CURRENT_SHELL=$(echo "$USER_ENTRY" | sed 's/UserShell: //')

# Determine the original shell to use for running commands
if [ -f "$DATA_DIR/original_shell.txt" ]; then
    ORIGINAL_SHELL=$(cat "$DATA_DIR/original_shell.txt")
    if [ ! -x "$ORIGINAL_SHELL" ]; then
        echo "Warning: Original shell ($ORIGINAL_SHELL) is not executable."
        ORIGINAL_SHELL="/bin/bash"  # Fallback to bash
    fi
else
    echo "No record of original shell found, using /bin/bash as fallback."
    ORIGINAL_SHELL="/bin/bash"
fi

# Create a temporary script to execute with the original shell
TMP_SCRIPT=$(mktemp)
cat > "$TMP_SCRIPT" << 'EOF'
#!/bin/bash

# This script will run the actual uninstallation steps using the user's original shell

HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.cjsh_data"
APP_NAME="$1"
INSTALL_PATH="$2"
APP_PATH="$3"
SHELLS_FILE="$4"
REMOVE_DATA="$5"
CURRENT_USER="$6"
ORIGINAL_SHELL="$7"
CURRENT_SHELL="$8"

echo "Executing uninstallation with $ORIGINAL_SHELL"

# Restore original shell if current shell is CJsShell
if [[ "$CURRENT_SHELL" == "$APP_PATH" || "$SHELL" == "$APP_PATH" ]]; then
    echo "Restoring your original shell ($ORIGINAL_SHELL)..."
    if ! chsh -s "$ORIGINAL_SHELL" 2>/dev/null; then
        echo "Attempting with sudo..."
        if sudo chsh -s "$ORIGINAL_SHELL" "$CURRENT_USER" 2>/dev/null; then
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
    echo "Current shell is not CJ's Shell. No need to restore."
fi

# Remove from /etc/shells
if grep -q "^$APP_PATH$" "$SHELLS_FILE"; then
    echo "Removing CJ's Shell from $SHELLS_FILE (requires sudo)..."
    if sudo sed -i.bak "\|^$APP_PATH$|d" "$SHELLS_FILE" 2>/dev/null; then
        echo "Successfully removed CJ's Shell from $SHELLS_FILE."
    else
        echo "Manual action required: Please run the following command to remove CJ's Shell from $SHELLS_FILE:"
        echo "    sudo sed -i.bak '\|^$APP_PATH$|d' $SHELLS_FILE"
    fi
fi

# Remove binary from install path
if [ -f "$APP_PATH" ]; then
    echo "Removing CJ's Shell binary from $APP_PATH (requires sudo)..."
    if sudo rm "$APP_PATH" 2>/dev/null; then
        echo "Successfully removed CJ's Shell binary."
    else
        echo "Manual action required: Please run the following command to remove the binary:"
        echo "    sudo rm $APP_PATH"
        echo "Continuing with uninstallation..."
    fi
fi

# Remove data directory if requested
if [ "$REMOVE_DATA" = "true" ]; then
    echo "Removing data directory at $DATA_DIR..."
    rm -rf "$DATA_DIR"
    if [ $? -ne 0 ]; then
        echo "Warning: Failed to remove data directory. You may need to remove it manually."
    fi
else
    echo "Data directory at $DATA_DIR has been preserved."
    echo "To remove it manually, run: rm -rf $DATA_DIR"
fi

echo "CJ's Shell has been uninstalled successfully."
EOF

# Make the temporary script executable
chmod +x "$TMP_SCRIPT"

# Execute the uninstallation with the original shell
"$ORIGINAL_SHELL" "$TMP_SCRIPT" "$APP_NAME" "$INSTALL_PATH" "$APP_PATH" "$SHELLS_FILE" "$REMOVE_DATA" "$CURRENT_USER" "$ORIGINAL_SHELL" "$CURRENT_SHELL"

# Clean up the temporary script
rm -f "$TMP_SCRIPT"
