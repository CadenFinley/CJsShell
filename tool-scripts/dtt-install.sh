#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
INSTALL_DIR="/usr/local/bin"
APP_NAME="DevToolsTerminal"
APP_PATH="$INSTALL_DIR/$APP_NAME"
ZSHRC_PATH="$HOME/.zshrc"
GITHUB_API_URL="https://api.github.com/repos/cadenfinley/DevToolsTerminal/releases/latest"

echo "DevToolsTerminal Installer"
echo "-------------------------"
echo "This will install DevToolsTerminal to $INSTALL_DIR and set it to auto-launch with zsh."

# Check for sudo permissions
check_permissions() {
    if [ -w "$INSTALL_DIR" ]; then
        return 0
    elif sudo -n true 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

# Check for latest version
echo "Checking for latest version..."
if command -v curl &> /dev/null; then
    RELEASE_DATA=$(curl -s "$GITHUB_API_URL")
    DOWNLOAD_URL=$(echo "$RELEASE_DATA" | grep -o '"browser_download_url": "[^"]*' | grep -v '.exe' | head -1 | cut -d'"' -f4)
    LATEST_VERSION=$(echo "$RELEASE_DATA" | grep -o '"tag_name": "[^"]*' | head -1 | cut -d'"' -f4)
    
    if [ -n "$DOWNLOAD_URL" ]; then
        echo "Found latest version: $LATEST_VERSION"
        echo "Downloading from: $DOWNLOAD_URL"
        
        # Check if we need sudo
        if check_permissions; then
            # Download with sudo if needed
            if [ -w "$INSTALL_DIR" ]; then
                curl -L -o "$APP_PATH" "$DOWNLOAD_URL"
            else
                sudo curl -L -o "$APP_PATH" "$DOWNLOAD_URL"
            fi
            
            if [ $? -ne 0 ]; then
                echo "Error: Failed to download DevToolsTerminal."
                exit 1
            fi
        else
            echo "No permission to write to $INSTALL_DIR"
            echo "Falling back to user's home directory..."
            INSTALL_DIR="$HOME/.local/bin"
            APP_PATH="$INSTALL_DIR/$APP_NAME"
            
            # Create directory if it doesn't exist
            mkdir -p "$INSTALL_DIR"
            
            # Download to user's local bin
            curl -L -o "$APP_PATH" "$DOWNLOAD_URL"
            if [ $? -ne 0 ]; then
                echo "Error: Failed to download DevToolsTerminal."
                exit 1
            fi
            
            # Add to PATH if not already there
            if ! grep -q "$INSTALL_DIR" "$ZSHRC_PATH"; then
                echo "Adding $INSTALL_DIR to your PATH..."
                echo 'export PATH="$PATH:'"$INSTALL_DIR"'"' >> "$ZSHRC_PATH"
            fi
        fi
    else
        echo "Warning: Couldn't find download URL. Proceeding with local version if available."
    fi
else
    echo "Warning: curl is not installed. Cannot check for updates."
fi

# Make sure the application is executable
if [ -w "$(dirname "$APP_PATH")" ]; then
    chmod +x "$APP_PATH"
else
    sudo chmod +x "$APP_PATH"
fi

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
    if [ -w "$UNINSTALL_SCRIPT" ]; then
        chmod +x "$UNINSTALL_SCRIPT"
    else
        sudo chmod +x "$UNINSTALL_SCRIPT"
    fi
    
    if [ -w "$INSTALL_DIR" ]; then
        cp "$UNINSTALL_SCRIPT" "$INSTALL_DIR/uninstall-$APP_NAME.sh"
        chmod +x "$INSTALL_DIR/uninstall-$APP_NAME.sh"
    elif sudo -n true 2>/dev/null; then
        sudo cp "$UNINSTALL_SCRIPT" "$INSTALL_DIR/uninstall-$APP_NAME.sh"
        sudo chmod +x "$INSTALL_DIR/uninstall-$APP_NAME.sh"
    else
        echo "Note: Could not install uninstall script to $INSTALL_DIR."
    fi
fi

echo "Installation complete!"
echo "To uninstall later, run: $INSTALL_DIR/uninstall-$APP_NAME.sh"

