#!/bin/bash

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
APP_PATH="$APP_DIR/DevToolsTerminal"
ZSHRC_PATH="$HOME/.zshrc"
GITHUB_API_URL="https://api.github.com/repos/cadenfinley/DevToolsTerminal/releases/latest"

echo "DevToolsTerminal Installer"
echo "-------------------------"
echo "This will install DevToolsTerminal and set it to auto-launch with zsh."

# Check for latest version
echo "Checking for latest version..."
if command -v curl &> /dev/null; then
    RELEASE_DATA=$(curl -s "$GITHUB_API_URL")
    DOWNLOAD_URL=$(echo "$RELEASE_DATA" | grep -o '"browser_download_url": "[^"]*' | grep -v '.exe' | head -1 | cut -d'"' -f4)
    LATEST_VERSION=$(echo "$RELEASE_DATA" | grep -o '"tag_name": "[^"]*' | head -1 | cut -d'"' -f4)
    
    if [ -n "$DOWNLOAD_URL" ]; then
        echo "Found latest version: $LATEST_VERSION"
        echo "Downloading from: $DOWNLOAD_URL"
        
        # Download the latest release
        curl -L -o "$APP_PATH" "$DOWNLOAD_URL"
        if [ $? -ne 0 ]; then
            echo "Error: Failed to download DevToolsTerminal."
            exit 1
        fi
    else
        echo "Warning: Couldn't find download URL. Proceeding with local version if available."
    fi
else
    echo "Warning: curl is not installed. Cannot check for updates."
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

echo "Installation complete!"

