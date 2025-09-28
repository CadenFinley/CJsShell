#!/bin/bash

# Debug install script - moves built binary to user's local bin directory

# Check if cjsh is already installed somewhere
# EXISTING_CJSH=$(which cjsh 2>/dev/null)

# if [ -n "$EXISTING_CJSH" ]; then
#     # Use the existing installation path
#     INSTALL_PATH=$(dirname "$EXISTING_CJSH")
#     echo "Found existing cjsh at: $EXISTING_CJSH"
#     echo "Will overwrite at: $INSTALL_PATH"
# else
#     # Install for current user only (no sudo required)
#     INSTALL_PATH="$HOME/.local/bin"
#     echo "No existing cjsh found, installing to: $INSTALL_PATH"
    
#     # Create the directory if it doesn't exist
#     mkdir -p "$INSTALL_PATH"
# fi

INSTALL_PATH="$HOME/.local/bin"

# Move the binary
mv build/cjsh "$INSTALL_PATH/cjsh"
echo "Moved cjsh to $INSTALL_PATH"

# Make sure it's executable
chmod +x "$INSTALL_PATH/cjsh"
echo "Made cjsh executable"

# Check if ~/.local/bin is in PATH
case ":$PATH:" in
    *":$INSTALL_PATH:"*)
        ;;
    *)
        echo ""
        echo "Warning: $INSTALL_PATH is not in your PATH"
        echo "Add this line to your shell configuration file (~/.bashrc, ~/.zshrc, etc.):"
        echo "export PATH=\"$INSTALL_PATH:\$PATH\""
        ;;
esac