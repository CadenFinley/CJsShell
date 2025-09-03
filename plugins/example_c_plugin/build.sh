#!/bin/bash
# Build script for the example_c_plugin for CJSH

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PLUGIN_NAME="example_c_plugin"

# Detect OS for correct file extension
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    FILE_EXT="dylib"
    EXTRA_FLAGS="-undefined dynamic_lookup"
else
    # Linux
    FILE_EXT="so"
    EXTRA_FLAGS=""
fi

echo "Building $PLUGIN_NAME for CJSH..."

# Check if source file exists
SRC_FILE="$SCRIPT_DIR/src/$PLUGIN_NAME.c"
if [ ! -f "$SRC_FILE" ]; then
    echo "Error: Source file not found: $SRC_FILE"
    echo "Searching for alternative source files..."
    # List all .c files in the src directory
    find "$SCRIPT_DIR/src" -name "*.c" -type f | while read -r file; do
        echo "Found source file: $file"
        SRC_FILE="$file"
        break
    done
    
    if [ ! -f "$SRC_FILE" ]; then
        echo "No source files found in $SCRIPT_DIR/src"
        exit 1
    fi
    echo "Using source file: $SRC_FILE"
fi

# Compile the plugin
gcc -Wall -fPIC -shared $EXTRA_FLAGS \
    -I"$SCRIPT_DIR/include" \
    -I"$SCRIPT_DIR/../../include" \
    -o "$SCRIPT_DIR/$PLUGIN_NAME.$FILE_EXT" \
    "$SRC_FILE"

if [ $? -eq 0 ]; then
    echo "Plugin built successfully: $SCRIPT_DIR/$PLUGIN_NAME.$FILE_EXT"
else
    echo "Error building plugin"
    exit 1
fi
