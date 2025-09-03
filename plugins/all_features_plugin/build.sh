#!/bin/bash
# Build script for all_features_plugin

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$SCRIPT_DIR"

# Create build directory
mkdir -p "$PLUGIN_DIR/build"
cd "$PLUGIN_DIR/build"

# Configure with CMake
echo "Configuring plugin with CMake..."
cmake ..

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
    echo "Plugin binary: $PLUGIN_DIR/build/$PLUGIN_FILE"
    
    # Offer to install
    read -p "Do you want to install the plugin to ~/.config/cjsh/plugins? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        mkdir -p ~/.config/cjsh/plugins
        cp "$PLUGIN_FILE" ~/.config/cjsh/plugins/
        echo "Plugin installed to ~/.config/cjsh/plugins/$PLUGIN_FILE"
    fi
else
    echo "Build failed!"
    exit 1
fi
