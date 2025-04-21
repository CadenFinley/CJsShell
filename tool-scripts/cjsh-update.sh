#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.cjsh_data"
APP_NAME="cjsh"
INSTALL_PATH="/usr/local/bin"
APP_PATH="$INSTALL_PATH/$APP_NAME"
GITHUB_API_URL="https://api.github.com/repos/cadenfinley/CJsShell/releases/latest"
TEMP_DIR="$DATA_DIR/temp"

echo "CJ's Shell Updater"
echo "-------------------------"
echo "This will update CJ's Shell to the latest version."
echo "Note: This script requires sudo privileges."

# Request sudo upfront
if sudo -n true 2>/dev/null; then
    echo "Sudo access verified."
else
    echo "Please enter your password to proceed with update:"
    if sudo -v; then
        echo "Sudo access granted."
    else
        echo "Failed to get sudo access. Update will likely fail."
        echo "Please run this script again with sudo privileges."
        exit 1
    fi
fi

# Determine original shell for running commands
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

# This script will run the actual update steps using the user's original shell

SCRIPT_DIR="$1"
HOME_DIR="$2"
DATA_DIR="$3"
APP_NAME="$4"
INSTALL_PATH="$5"
APP_PATH="$6"
GITHUB_API_URL="$7"
TEMP_DIR="$8"

# Create temp directory if it doesn't exist
if [ ! -d "$TEMP_DIR" ]; then
    mkdir -p "$TEMP_DIR"
fi

# Check for curl
if ! command -v curl &> /dev/null; then
    echo "Error: curl is required but not installed. Please install curl and try again."
    exit 1
fi

# Check if CJ's Shell is installed
if [ ! -f "$APP_PATH" ]; then
    echo "Error: CJ's Shell is not installed at $APP_PATH."
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
    PLATFORM_PATTERN="cjsh-macos"
elif [[ "$(uname)" == "Linux" ]]; then
    # Linux
    PLATFORM_PATTERN="cjsh-linux"
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
echo "Downloading latest CJ's Shell binary..."
curl -L "$DOWNLOAD_URL" -o "$TEMP_DIR/$APP_NAME"
if [ $? -ne 0 ]; then
    echo "Error: Failed to download CJ's Shell binary."
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Make it executable
chmod +x "$TEMP_DIR/$APP_NAME"

# Replace the existing binary (requires sudo)
echo "Updating CJ's Shell at $APP_PATH (requires sudo)..."
sudo cp "$TEMP_DIR/$APP_NAME" "$APP_PATH"
if [ $? -ne 0 ]; then
    echo "Error: Failed to update CJ's Shell at $APP_PATH. Please check your permissions."
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Also update the copy in DATA_DIR
echo "Updating local copy at $DATA_DIR/$APP_NAME..."
cp "$TEMP_DIR/$APP_NAME" "$DATA_DIR/$APP_NAME"

# Clean up
rm -rf "$TEMP_DIR"

echo "Update complete! CJ's Shell has been updated to the latest version."
echo "To use CJ's Shell, run: $APP_NAME"
EOF

# Make the temporary script executable
chmod +x "$TMP_SCRIPT"

# Execute the update with the original shell
"$ORIGINAL_SHELL" "$TMP_SCRIPT" "$SCRIPT_DIR" "$HOME_DIR" "$DATA_DIR" "$APP_NAME" "$INSTALL_PATH" "$APP_PATH" "$GITHUB_API_URL" "$TEMP_DIR"

# Clean up the temporary script
rm -f "$TMP_SCRIPT"
