#!/bin/bash


EXISTING_CJSH=$(which cjsh 2>/dev/null)

if [ -n "$EXISTING_CJSH" ]; then
    if [ -L "$EXISTING_CJSH" ]; then
        ACTUAL_BINARY=$(readlink -f "$EXISTING_CJSH")
        INSTALL_PATH=$(dirname "$ACTUAL_BINARY")
        echo "Found existing cjsh symlink at: $EXISTING_CJSH"
        echo "Points to actual binary at: $ACTUAL_BINARY"
        echo "Will overwrite actual binary at: $INSTALL_PATH"
    else
        INSTALL_PATH=$(dirname "$EXISTING_CJSH")
        echo "Found existing cjsh at: $EXISTING_CJSH"
        echo "Will overwrite at: $INSTALL_PATH"
    fi
else
    INSTALL_PATH="$HOME/.local/bin"
    echo "No existing cjsh found, installing to: $INSTALL_PATH"
    
    mkdir -p "$INSTALL_PATH"
fi


mv build/cjsh "$INSTALL_PATH/cjsh"
echo "Moved cjsh to $INSTALL_PATH"

chmod +x "$INSTALL_PATH/cjsh"
echo "Made cjsh executable"