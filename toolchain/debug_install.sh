#!/bin/bash

# Debug install script - moves built binary to user's local bin directory

# Check if cjsh is already installed somewhere
EXISTING_CJSH=$(which cjsh 2>/dev/null)

if [ -n "$EXISTING_CJSH" ]; then
    # Check if it's a symlink and resolve to the actual binary
    if [ -L "$EXISTING_CJSH" ]; then
        ACTUAL_BINARY=$(readlink -f "$EXISTING_CJSH")
        INSTALL_PATH=$(dirname "$ACTUAL_BINARY")
        echo "Found existing cjsh symlink at: $EXISTING_CJSH"
        echo "Points to actual binary at: $ACTUAL_BINARY"
        echo "Will overwrite actual binary at: $INSTALL_PATH"
    else
        # Use the existing installation path
        INSTALL_PATH=$(dirname "$EXISTING_CJSH")
        echo "Found existing cjsh at: $EXISTING_CJSH"
        echo "Will overwrite at: $INSTALL_PATH"
    fi
else
    # Install for current user only (no sudo required)
    INSTALL_PATH="$HOME/.local/bin"
    echo "No existing cjsh found, installing to: $INSTALL_PATH"
    
    # Create the directory if it doesn't exist
    mkdir -p "$INSTALL_PATH"
fi


# Move the binary
mv build/cjsh "$INSTALL_PATH/cjsh"
echo "Moved cjsh to $INSTALL_PATH"

# Make sure it's executable
chmod +x "$INSTALL_PATH/cjsh"
echo "Made cjsh executable"