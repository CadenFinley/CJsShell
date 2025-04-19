#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.DTT-Data"
APP_NAME="DevToolsTerminal"
INSTALL_PATH="/usr/local/bin"
APP_PATH="$INSTALL_PATH/$APP_NAME"
GITHUB_API_URL="https://api.github.com/repos/cadenfinley/DevToolsTerminal/releases/latest"
UNINSTALL_SCRIPT_URL="https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/master/tool-scripts/dtt-uninstall.sh"
UPDATE_SCRIPT_URL="https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/master/tool-scripts/dtt-update.sh"
SET_AS_DEFAULT_SHELL=false
SHELLS_FILE="/etc/shells"

# Simple argument handling for set-as-shell option
if [[ "$1" == "-s" || "$1" == "--set-as-shell" ]]; then
    SET_AS_DEFAULT_SHELL=true
fi

echo "DevToolsTerminal Installer"
echo "-------------------------"
echo "This will install DevToolsTerminal to $DATA_DIR."

# Create data directory if it doesn't exist
if [ ! -d "$DATA_DIR" ]; then
    echo "Creating directory $DATA_DIR..."
    mkdir -p "$DATA_DIR"
fi

# Check for curl
if ! command -v curl &> /dev/null; then
    echo "Error: curl is required but not installed. Please install curl and try again."
    exit 1
fi

# Fetch the latest release from GitHub
echo "Fetching latest release information..."
RELEASE_JSON=$(curl -s "$GITHUB_API_URL")
if [ $? -ne 0; then
    echo "Error: Failed to fetch release information from GitHub."
    exit 1
fi

# Extract download URL for the appropriate platform
if [[ "$(uname)" == "Darwin" ]]; then
    # macOS
    DOWNLOAD_URL=$(echo "$RELEASE_JSON" | grep -o "https://github.com/cadenfinley/DevToolsTerminal/releases/download/[^\"]*DevToolsTerminal-macos[^\"]*" | head -n 1)
    BINARY_NAME=$(basename "$DOWNLOAD_URL")
elif [[ "$(uname)" == "Linux" ]]; then
    # Linux
    DOWNLOAD_URL=$(echo "$RELEASE_JSON" | grep -o "https://github.com/cadenfinley/DevToolsTerminal/releases/download/[^\"]*DevToolsTerminal-linux[^\"]*" | head -n 1)
    BINARY_NAME=$(basename "$DOWNLOAD_URL")
else
    echo "Error: Unsupported operating system. This installer supports macOS and Linux only."
    exit 1
fi

if [ -z "$DOWNLOAD_URL" ]; then
    echo "Error: Could not find download URL for your platform in the latest release."
    exit 1
fi

# Download the binary with the correct filename
echo "Downloading DevToolsTerminal binary ($BINARY_NAME)..."
curl -L "$DOWNLOAD_URL" -o "$DATA_DIR/$BINARY_NAME"
if [ $? -ne 0; then
    echo "Error: Failed to download DevToolsTerminal binary."
    exit 1
fi

# Make it executable
chmod +x "$DATA_DIR/$BINARY_NAME"

# Install to system path (requires sudo)
echo "Installing DevToolsTerminal to $APP_PATH (requires sudo)..."
sudo cp "$DATA_DIR/$BINARY_NAME" "$APP_PATH"
if [ $? -ne 0; then
    echo "Error: Failed to install DevToolsTerminal to $APP_PATH. Please check your permissions."
    exit 1
fi

# Download uninstall script
echo "Downloading uninstall script..."
curl -L "$UNINSTALL_SCRIPT_URL" -o "$DATA_DIR/dtt-uninstall.sh"
if [ $? -ne 0; then
    echo "Warning: Failed to download uninstall script."
else
    chmod +x "$DATA_DIR/dtt-uninstall.sh"
fi

# Download update script
echo "Downloading update script..."
curl -L "$UPDATE_SCRIPT_URL" -o "$DATA_DIR/dtt-update.sh"
if [ $? -ne 0; then
    echo "Warning: Failed to download update script."
else
    chmod +x "$DATA_DIR/dtt-update.sh"
fi

# Add to /etc/shells if not already there
if ! grep -q "^$APP_PATH$" "$SHELLS_FILE"; then
    echo "Adding DevToolsTerminal to $SHELLS_FILE (requires sudo)..."
    echo "$APP_PATH" | sudo tee -a "$SHELLS_FILE" > /dev/null
    if [ $? -ne 0; then
        echo "Warning: Failed to add DevToolsTerminal to $SHELLS_FILE. You may need to do this manually."
    fi
fi

# Set as default shell if requested
if $SET_AS_DEFAULT_SHELL; then
    # Save the original shell
    echo "Saving your original shell..."
    echo "$SHELL" > "$DATA_DIR/original_shell.txt"
    
    echo "Setting DevToolsTerminal as your default shell..."
    chsh -s "$APP_PATH"
    if [ $? -ne 0 ]; then
        echo "Warning: Failed to set DevToolsTerminal as default shell. You may need to run 'chsh -s $APP_PATH' manually."
    else
        echo "DevToolsTerminal set as your default shell!"
        echo "Your previous shell ($SHELL) was saved to $DATA_DIR/original_shell.txt"
    fi
fi

echo "Installation complete! DevToolsTerminal has been installed to $APP_PATH"
echo "Uninstall script saved to $DATA_DIR/dtt-uninstall.sh"
echo "Update script saved to $DATA_DIR/dtt-update.sh"
echo "To use DevToolsTerminal, run: $APP_NAME"
echo "To update DevToolsTerminal, run: $DATA_DIR/dtt-update.sh"
echo "To set as your default shell, run: chsh -s $APP_PATH"

