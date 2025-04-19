#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.DTT-Data"
APP_NAME="DevToolsTerminal"
INSTALL_PATH="/usr/local/bin"
APP_PATH="$INSTALL_PATH/$APP_NAME"
GITHUB_API_URL="https://api.github.com/repos/cadenfinley/DevToolsTerminal/releases/latest"
TEMP_DIR="$DATA_DIR/temp"

echo "DevToolsTerminal Updater"
echo "-------------------------"
echo "This will update DevToolsTerminal to the latest version."

# Create temp directory if it doesn't exist
if [ ! -d "$TEMP_DIR" ]; then
    mkdir -p "$TEMP_DIR"
fi

# Check for curl
if ! command -v curl &> /dev/null; then
    echo "Error: curl is required but not installed. Please install curl and try again."
    exit 1
fi

# Check if DevToolsTerminal is installed
if [ ! -f "$APP_PATH" ]; then
    echo "Error: DevToolsTerminal is not installed at $APP_PATH."
    echo "Please run the installation script first."
    exit 1
fi

# Fetch the latest release from GitHub
echo "Fetching latest release information..."
RELEASE_JSON=$(curl -s "$GITHUB_API_URL")
if [ $? -ne 0 ]; then
    echo "Error: Failed to fetch release information from GitHub."
    exit 1
fi

# Extract download URL for the appropriate platform
if [[ "$(uname)" == "Darwin" ]]; then
    # macOS
    PLATFORM_PATTERN="macos"
elif [[ "$(uname)" == "Linux" ]]; then
    # Linux
    PLATFORM_PATTERN="linux"
else
    echo "Error: Unsupported operating system. This updater supports macOS and Linux only."
    exit 1
fi

# Try to use jq if available, otherwise fall back to grep/sed
if command -v jq &> /dev/null; then
    echo "Using jq to parse release information..."
    DOWNLOAD_URL=$(echo "$RELEASE_JSON" | jq -r ".assets[] | select(.name | contains(\"$PLATFORM_PATTERN\")) | .browser_download_url" | head -n 1)
else
    echo "Using grep to parse release information..."
    # More robust pattern matching
    DOWNLOAD_URL=$(echo "$RELEASE_JSON" | grep -o "\"browser_download_url\":\"[^\"]*$PLATFORM_PATTERN[^\"]*\"" | sed -E 's/"browser_download_url":"([^"]+)"/\1/' | head -n 1)
fi

if [ -z "$DOWNLOAD_URL" ]; then
    echo "Error: Could not find download URL for your platform ($PLATFORM_PATTERN) in the latest release."
    exit 1
fi

BINARY_NAME=$(basename "$DOWNLOAD_URL")
echo "Found download URL: $DOWNLOAD_URL"

# Download the binary to temp location
echo "Downloading latest DevToolsTerminal binary..."
curl -L "$DOWNLOAD_URL" -o "$TEMP_DIR/$APP_NAME"
if [ $? -ne 0 ]; then
    echo "Error: Failed to download DevToolsTerminal binary."
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Make it executable
chmod +x "$TEMP_DIR/$APP_NAME"

# Replace the existing binary (requires sudo)
echo "Updating DevToolsTerminal at $APP_PATH (requires sudo)..."
sudo cp "$TEMP_DIR/$APP_NAME" "$APP_PATH"
if [ $? -ne 0 ]; then
    echo "Error: Failed to update DevToolsTerminal at $APP_PATH. Please check your permissions."
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Also update the copy in DATA_DIR
echo "Updating local copy at $DATA_DIR/$APP_NAME..."
cp "$TEMP_DIR/$APP_NAME" "$DATA_DIR/$APP_NAME"

# Clean up
rm -rf "$TEMP_DIR"

echo "Update complete! DevToolsTerminal has been updated to the latest version."
echo "To use DevToolsTerminal, run: $APP_NAME"
