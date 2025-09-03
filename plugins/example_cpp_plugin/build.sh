#!/bin/bash
# Build script for all_features_plugin

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$SCRIPT_DIR"

# Clean and create build directory
rm -rf "$PLUGIN_DIR/build"
mkdir -p "$PLUGIN_DIR/build"
cd "$PLUGIN_DIR/build"

# Configure with CMake
echo "Configuring plugin with CMake..."
cmake "$PLUGIN_DIR"

# Build
echo "Building plugin..."
make

# Get plugin file
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLUGIN_FILE="all_features_plugin.dylib"
else
    PLUGIN_FILE="all_features_plugin.so"
fi

# Check if build was successful
if [ -f "$PLUGIN_FILE" ]; then
    echo "Build successful!"
    echo "Plugin binary built at: $PLUGIN_DIR/build/$PLUGIN_FILE"
    
    # Copy plugin to script directory (same directory as this script)
    echo "Copying plugin to script directory..."
    cp "$PLUGIN_FILE" "$PLUGIN_DIR/"
    echo "Plugin placed at: $PLUGIN_DIR/$PLUGIN_FILE"
else
    echo "Build failed!"
    exit 1
fi
