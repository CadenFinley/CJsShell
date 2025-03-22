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
    DOWNLOAD_URL=$(echo "$RELEASE_DATA" | grep -o '"browser_download_url": "[^"]*' | grep -v '.exe' | head -1 | cut -d'"' -f4)
    LATEST_VERSION=$(echo "$RELEASE_DATA" | grep -o '"tag_name": "[^"]*' | head -1 | cut -d'"' -f4)
    
    if [ -n "$DOWNLOAD_URL" ]; then
        echo "Found latest version: $LATEST_VERSION"
        echo "Downloading from: $DOWNLOAD_URL"
        
        # Download to data directory
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

# Make uninstall script executable (if we have it)
UNINSTALL_SCRIPT="$SCRIPT_DIR/dtt-uninstall.sh"
if [ -f "$UNINSTALL_SCRIPT" ]; then
    chmod +x "$UNINSTALL_SCRIPT"
    cp "$UNINSTALL_SCRIPT" "$DATA_DIR/uninstall-$APP_NAME.sh"
    chmod +x "$DATA_DIR/uninstall-$APP_NAME.sh"
fi

echo "Installation complete!"
echo "To uninstall later, run: !uninstall"

# Determine terminal type and restart appropriately
CURRENT_SHELL=$(basename "$SHELL")

# Detect operating system
OS_TYPE="unknown"
if [[ "$(uname)" == "Darwin" ]]; then
    OS_TYPE="macos"
elif [[ "$(uname)" == "Linux" ]]; then
    OS_TYPE="linux"
fi

echo "Detected operating system: $OS_TYPE"

if [[ "$CURRENT_SHELL" == "zsh" || "$CURRENT_SHELL" == "bash" ]]; then
    echo "Setting up terminal restart..."
    
    if [[ "$OS_TYPE" == "macos" ]]; then
        # macOS approach using AppleScript
        TEMP_RESTART_SCRIPT=$(mktemp)
        echo "#!/bin/bash" > "$TEMP_RESTART_SCRIPT"
        echo "sleep 1" >> "$TEMP_RESTART_SCRIPT"
        
        if [[ "$CURRENT_SHELL" == "zsh" ]]; then
            echo "osascript -e 'tell application \"Terminal\" to do script \"cd \\\"$(pwd)\\\"; exec zsh -l\"'" >> "$TEMP_RESTART_SCRIPT"
            echo "osascript -e 'tell application \"Terminal\" to close first window'" >> "$TEMP_RESTART_SCRIPT"
        else
            echo "osascript -e 'tell application \"Terminal\" to do script \"cd \\\"$(pwd)\\\"; exec bash -l\"'" >> "$TEMP_RESTART_SCRIPT"
            echo "osascript -e 'tell application \"Terminal\" to close first window'" >> "$TEMP_RESTART_SCRIPT"
        fi
        
        chmod +x "$TEMP_RESTART_SCRIPT"
        
        # Execute the restart script in the background
        bash "$TEMP_RESTART_SCRIPT" &
        
        echo "Terminal will restart momentarily..."
        sleep 1
        exit 0
    else
        # Linux approach
        echo "For Linux systems, we recommend manually restarting your terminal"
        echo "Please close this terminal and open a new one to start using DevToolsTerminal."
        
        # Alternative: try using the PROMPT_COMMAND method
        if [[ "$CURRENT_SHELL" == "bash" ]]; then
            echo "Attempting to restart bash session..."
            exec bash -l
        elif [[ "$CURRENT_SHELL" == "zsh" ]]; then
            echo "Attempting to restart zsh session..."
            exec zsh -l
        fi
    fi
else
    echo "Unable to automatically restart your terminal with shell: $CURRENT_SHELL"
    echo "Please restart your terminal manually to start using DevToolsTerminal."
fi

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

