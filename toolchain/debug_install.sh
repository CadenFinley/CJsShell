#!/bin/bash

EXISTING_CJSH=$(which cjsh 2>/dev/null)

if [ -n "$EXISTING_CJSH" ]; then
    if [ -L "$EXISTING_CJSH" ]; then
        ACTUAL_BINARY=$(readlink "$EXISTING_CJSH")
        echo "Found existing cjsh symlink at: $EXISTING_CJSH"
        echo "Points to: $ACTUAL_BINARY"
        cp build/cjsh "$ACTUAL_BINARY"
        echo "Copied new cjsh binary to $ACTUAL_BINARY"
    else
        echo "Found existing cjsh at: $EXISTING_CJSH"
        cp build/cjsh "$EXISTING_CJSH"
        echo "Copied new cjsh binary over existing installation"
    fi
else
    INSTALL_PATH="$HOME/.local/bin"
    echo "No existing cjsh found, installing to: $INSTALL_PATH"
    
    mkdir -p "$INSTALL_PATH"
    cp build/cjsh "$INSTALL_PATH/cjsh"
    chmod +x "$INSTALL_PATH/cjsh"
    echo "Copied cjsh to $INSTALL_PATH"
fi