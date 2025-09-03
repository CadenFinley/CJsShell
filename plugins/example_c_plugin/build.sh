#!/bin/bash
# Build script for the example_c_plugin for CJSH

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PLUGIN_NAME=$(basename "$SCRIPT_DIR")

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

# Compile the plugin
gcc -Wall -fPIC -shared $EXTRA_FLAGS \
    -I"$SCRIPT_DIR/include" \
    -I"$SCRIPT_DIR/../../include" \
    -o "$SCRIPT_DIR/$PLUGIN_NAME.$FILE_EXT" \
    "$SCRIPT_DIR/src/$PLUGIN_NAME.c"

if [ $? -eq 0 ]; then
    echo "Plugin built successfully: $SCRIPT_DIR/$PLUGIN_NAME.$FILE_EXT"
else
    echo "Error building plugin"
    exit 1
fi
