#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
HOME_DIR="$HOME"
DATA_DIR="$HOME_DIR/.cjsh_data"
APP_NAME="cjsh"
INSTALL_PATH="/usr/local/bin"
APP_PATH="$INSTALL_PATH/$APP_NAME"
GITHUB_API_URL="https://api.github.com/repos/cadenfinley/CJsShell/releases/latest"
UNINSTALL_SCRIPT_URL="https://raw.githubusercontent.com/cadenfinley/CJsShell/master/tool-scripts/cjsh_uninstall.sh"
UPDATE_SCRIPT_URL="https://raw.githubusercontent.com/cadenfinley/CJsShell/master/tool-scripts/cjsh_update.sh"
SET_AS_DEFAULT_SHELL=false
REGISTER_AS_FILE_HANDLER=false
SHELLS_FILE="/etc/shells"

# Argument handling
while [[ $# -gt 0 ]]; do
  case $1 in
    -s|--set-as-shell)
      SET_AS_DEFAULT_SHELL=true
      shift
      ;;
    -f|--register-file-handler)
      REGISTER_AS_FILE_HANDLER=true
      shift
      ;;
    -a|--all)
      SET_AS_DEFAULT_SHELL=true
      REGISTER_AS_FILE_HANDLER=true
      shift
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [-s|--set-as-shell] [-f|--register-file-handler] [-a|--all]"
      exit 1
      ;;
  esac
done

echo "CJ's Shell Installer"
echo "-------------------------"
echo "This will install CJ's Shell to $DATA_DIR."

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
    echo "Error: Unsupported operating system. This installer supports macOS and Linux only."
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

# Download the binary with the correct filename
echo "Downloading CJ's Shell binary ($BINARY_NAME)..."
curl -L "$DOWNLOAD_URL" -o "$DATA_DIR/$BINARY_NAME"
if [ $? -ne 0 ]; then
    echo "Error: Failed to download CJ's Shell binary."
    exit 1
fi

# Make it executable
chmod +x "$DATA_DIR/$BINARY_NAME"

# Install to system path (requires sudo)
echo "Installing CJ's Shell to $APP_PATH (requires sudo)..."
sudo cp "$DATA_DIR/$BINARY_NAME" "$APP_PATH"
if [ $? -ne 0 ]; then
    echo "Error: Failed to install CJ's Shell to $APP_PATH. Please check your permissions."
    exit 1
fi

# Clean up the downloaded binary file
echo "Cleaning up temporary files..."
rm -f "$DATA_DIR/$BINARY_NAME"
if [ $? -ne 0 ]; then
    echo "Warning: Could not remove temporary file $DATA_DIR/$BINARY_NAME"
fi

# Download uninstall script
echo "Downloading uninstall script..."
curl -L "$UNINSTALL_SCRIPT_URL" -o "$DATA_DIR/cjsh_uninstall.sh"
if [ $? -ne 0 ]; then
    echo "Warning: Failed to download uninstall script."
else
    chmod +x "$DATA_DIR/cjsh_uninstall.sh"
fi

# Add to /etc/shells if not already there
if ! grep -q "^$APP_PATH$" "$SHELLS_FILE"; then
    echo "Adding CJ's Shell to $SHELLS_FILE (requires sudo)..."
    echo "$APP_PATH" | sudo tee -a "$SHELLS_FILE" > /dev/null
    if [ $? -ne 0 ]; then
        echo "Warning: Failed to add CJ's Shell to $SHELLS_FILE. You may need to do this manually."
    fi
fi

# Set as default shell if requested
if $SET_AS_DEFAULT_SHELL; then
    # Save the original shell
    echo "Saving your original shell..."
    echo "$SHELL" > "$DATA_DIR/original_shell.txt"
    
    echo "Setting CJ's Shell as your default shell..."
    chsh -s "$APP_PATH"
    if [ $? -ne 0 ]; then
        echo "Warning: Failed to set CJ's Shell as default shell. You may need to run 'chsh -s $APP_PATH' manually."
    else
        echo "CJ's Shell set as your default shell!"
        echo "Your previous shell ($SHELL) was saved to $DATA_DIR/original_shell.txt"
    fi
fi

# Register as file handler if requested or if set as default shell
if $REGISTER_AS_FILE_HANDLER || $SET_AS_DEFAULT_SHELL; then
    echo "Registering CJ's Shell as a file handler..."
    
    # For macOS
    if [ "$(uname)" == "Darwin" ]; then
        # Create Info.plist for the app
        mkdir -p ~/Library/Application\ Support/CJsShell
        cat > ~/Library/Application\ Support/CJsShell/launcher.plist <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>com.cadenfinley.cjshell</string>
    <key>CFBundleName</key>
    <string>CJsShell</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleAllowMixedLocalizations</key>
    <true/>
    <key>CFBundleExecutable</key>
    <string>$APP_PATH</string>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>*</string>
            </array>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
        </dict>
    </array>
</dict>
</plist>
EOF

        echo "CJ's Shell registered as a file handler on macOS"
        echo "To set as default for specific file types, right-click a file, select 'Get Info', change 'Open with' and click 'Change All'"

    # For Linux
    elif [ "$(uname)" == "Linux" ]; then
        # Create .desktop file
        mkdir -p ~/.local/share/applications
        cat > ~/.local/share/applications/cjshell.desktop <<EOF
[Desktop Entry]
Type=Application
Name=CJ's Shell
GenericName=Terminal
Comment=CJ's Shell
Exec=$APP_PATH %f
Icon=utilities-terminal
Terminal=false
Categories=System;TerminalEmulator;Utility;
MimeType=text/plain;text/x-shellscript;application/x-executable;
EOF

        # Update desktop database
        update-desktop-database ~/.local/share/applications &>/dev/null || true
        
        echo "CJ's Shell registered as a file handler on Linux"
        echo "To set as default for specific file types, right-click a file, select 'Open With' and choose CJ's Shell"
    fi
fi

echo "Installation complete! CJ's Shell has been installed to $APP_PATH"
echo "Uninstall script saved to $DATA_DIR/cjsh-uninstall.sh"
echo "Update script saved to $DATA_DIR/cjsh-update.sh"

if ! $SET_AS_DEFAULT_SHELL; then
    echo "To set as your default shell, run: chsh -s $APP_PATH"
fi

if ! $REGISTER_AS_FILE_HANDLER && ! $SET_AS_DEFAULT_SHELL; then
    echo "To register as a file handler, run: $0 --register-file-handler"
fi

echo -e "\nStart a new terminal session to begin using CJ's Shell."
