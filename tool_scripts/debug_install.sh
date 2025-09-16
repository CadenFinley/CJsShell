#!/bin/bash

# Debug install script - moves built binary to appropriate location based on OS

# Detect the operating system
OS=$(uname -s)

if [[ "$OS" == "Darwin" ]]; then
    # macOS - check if we're on Apple Silicon or Intel
    ARCH=$(uname -m)
    if [[ "$ARCH" == "arm64" ]]; then
        # Apple Silicon Mac - use Homebrew path
        INSTALL_PATH="/opt/homebrew/bin"
    else
        # Intel Mac - use traditional path
        INSTALL_PATH="/usr/local/bin"
    fi
elif [[ "$OS" == "Linux" ]]; then
    # Linux
    INSTALL_PATH="/usr/local/bin"
else
    echo "Unsupported operating system: $OS"
    exit 1
fi

# Create the directory if it doesn't exist
sudo mkdir -p "$INSTALL_PATH"

# Move the binary
sudo mv build/cjsh "$INSTALL_PATH/cjsh"
echo "Moved cjsh to $INSTALL_PATH"

# Make sure it's executable
sudo chmod +x "$INSTALL_PATH/cjsh"
echo "Made cjsh executable"

# Check if the install path is in PATH
if [[ ":$PATH:" != *":$INSTALL_PATH:"* ]]; then
    echo ""
    echo "Warning: $INSTALL_PATH is not in your PATH"
    echo "Add this line to your shell configuration file (~/.bashrc, ~/.zshrc, etc.):"
    echo "export PATH=\"$INSTALL_PATH:\$PATH\""
fi