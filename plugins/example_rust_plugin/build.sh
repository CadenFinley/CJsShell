#!/bin/bash

# Script to build the Rust plugin

set -e

PLUGIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PLUGIN_DIR"

# Check if cargo is available
if command -v cargo &> /dev/null; then
    echo "Building with Cargo..."
    
    # Add environment variables to help with building on macOS
    if [[ "$(uname)" == "Darwin" ]]; then
        export MACOSX_DEPLOYMENT_TARGET=10.14
    fi
    
    # Try to build with Cargo
    cargo build --release
    
    # Determine the platform-specific extension
    if [[ "$(uname)" == "Darwin" ]]; then
        EXT="dylib"
    else
        EXT="so"
    fi
    
    if [ -f "target/release/liball_features_rust_plugin.$EXT" ]; then
        # Copy the plugin to the script directory
        cp "target/release/liball_features_rust_plugin.$EXT" "$PLUGIN_DIR/"
        # Also create a copy with the expected name for plugin installation
        cp "target/release/liball_features_rust_plugin.$EXT" "$PLUGIN_DIR/example_rust_plugin.$EXT"
        echo "Plugin copied to $PLUGIN_DIR/"
    else
        echo "Cargo build failed to produce output file. Trying Makefile..."
        make install
    fi
else
    echo "Cargo not found, building with Makefile..."
    make
    if [ $? -eq 0 ]; then
        # Copy the compiled library to the script directory
        if [[ -f "liball_features_rust_plugin.$EXT" ]]; then
            cp "liball_features_rust_plugin.$EXT" "$PLUGIN_DIR/"
            # Also create a copy with the expected name for plugin installation
            cp "liball_features_rust_plugin.$EXT" "$PLUGIN_DIR/example_rust_plugin.$EXT"
            echo "Plugin copied to $PLUGIN_DIR/"
        else
            echo "Could not find the compiled library after Make build"
            exit 1
        fi
    else
        echo "Make build failed"
        exit 1
    fi
fi

echo "Build complete!"
