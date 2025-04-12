#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.DTT-Data"
APP_NAME="DevToolsTerminal"
APP_PATH="$DATA_DIR/$APP_NAME"
ZSHRC_PATH="$HOME/.zshrc"
GITHUB_API_URL="https://api.github.com/repos/cadenfinley/DevToolsTerminal/releases/latest"

echo "DevToolsTerminal Installer"
echo "-------------------------"
echo "This will install DevToolsTerminal to $DATA_DIR and set it to auto-launch with zsh."

# Create data directory if it doesn't exist
if [ ! -d "$DATA_DIR" ]; then
    echo "Creating directory $DATA_DIR..."
    mkdir -p "$DATA_DIR"
fi

# Check for sudo permissions - not needed anymore but kept for compatibility
check_permissions() {
    if [ -w "$DATA_DIR" ]; then
        return 0
    else
        return 1
    fi
}

# Check for latest version
echo "Checking for latest version..."
if command -v curl &> /dev/null; then
    RELEASE_DATA=$(curl -s "$GITHUB_API_URL")
    UNAME_STR=$(uname)
    DOWNLOAD_URL=$(echo "$RELEASE_DATA" | grep -o '"browser_download_url": "[^"]*' | grep -v '.exe' | head -1 | cut -d'"' -f4)
    LATEST_VERSION=$(echo "$RELEASE_DATA" | grep -o '"tag_name": "[^"]*' | head -1 | cut -d'"' -f4)
    
    if [ -n "$DOWNLOAD_URL" ]; then
        echo "Found latest version: $LATEST_VERSION"
        echo "Downloading from GitHub: $DOWNLOAD_URL"
    else
        echo "Error: Couldn't find download URL on GitHub."
        exit 1
    fi
    
    # Download to data directory
    curl -L -o "$APP_PATH" "$DOWNLOAD_URL"
    
    if [ $? -ne 0 ]; then
        echo "Error: Failed to download DevToolsTerminal."
        exit 1
    fi
else
    echo "Error: curl is not installed. Cannot download the application."
    exit 1
fi

# Make sure the application is executable
chmod +x "$APP_PATH"

if [ ! -x "$APP_PATH" ]; then
    echo "Error: Could not make DevToolsTerminal executable."
    exit 1
fi

echo "Application path: $APP_PATH"

# Check if .zshrc exists
if [ ! -f "$ZSHRC_PATH" ]; then
    echo "Creating new .zshrc file..."
    touch "$ZSHRC_PATH"
fi

# Check if the auto-start entry already exists
if grep -q "# DevToolsTerminal Auto-Launch" "$ZSHRC_PATH"; then
    # Update the existing path in case it has changed
    sed -i.bak "s|\".*DevToolsTerminal\"|\"$APP_PATH\"|g" "$ZSHRC_PATH" && rm "$ZSHRC_PATH.bak"
    echo "Updated auto-start configuration in .zshrc."
else
    echo "Adding auto-start to .zshrc..."
    echo "" >> "$ZSHRC_PATH"
    echo "# DevToolsTerminal Auto-Launch" >> "$ZSHRC_PATH"
    echo "if [ -x \"$APP_PATH\" ]; then" >> "$ZSHRC_PATH"
    echo "    \"$APP_PATH\"" >> "$ZSHRC_PATH"
    echo "fi" >> "$ZSHRC_PATH"
    
    echo "Auto-start configuration added to .zshrc."
fi

# Make uninstall script executable (if we have it)
UNINSTALL_SCRIPT="$SCRIPT_DIR/dtt-uninstall.sh"
if [ -f "$UNINSTALL_SCRIPT" ]; then
    chmod +x "$UNINSTALL_SCRIPT"
    cp "$UNINSTALL_SCRIPT" "$DATA_DIR/uninstall-$APP_NAME.sh"
    chmod +x "$DATA_DIR/uninstall-$APP_NAME.sh"
fi

echo "Installation complete!"
echo "To uninstall later, run: !uninstall"

# Instead of launching, tell the user to restart their terminal
echo ""
echo "Please restart your terminal to start using DevToolsTerminal."

# Ask about deleting the installer
echo ""
read -p "Would you like to delete this installer script? (y/n): " delete_choice

# Delete installer if requested
if [[ "$delete_choice" =~ ^[Yy]$ ]]; then
    SELF_PATH="$SCRIPT_DIR/$(basename "${BASH_SOURCE[0]}")"
    echo "Removing installer script..."
    rm "$SELF_PATH"
    echo "Installer deleted."
fi

