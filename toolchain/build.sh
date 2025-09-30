#!/usr/bin/env bash
# Wrapper script for nob build system
# This allows running build.sh from tool_scripts directory

# Change to build_tools directory
cd "$(dirname "$0")/nob"

# Check if nob binary exists, if not compile it
if [ ! -f "./nob" ]; then
    echo "Building nob..."
    cc -02 -o nob nob.c
    if [ $? -ne 0 ]; then
        echo "Failed to compile nob"
        exit 1
    fi
fi

# Run nob with all arguments
./nob "$@"