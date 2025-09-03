#!/bin/bash

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Create a build directory inside the plugin directory
BUILD_DIR="${SCRIPT_DIR}/build"
mkdir -p "$BUILD_DIR"

# Navigate to the build directory
cd "$BUILD_DIR"

# Run cmake to configure the build
cmake ..

# Build the plugin
make

# Determine the extension based on the platform
if [[ "$(uname)" == "Darwin" ]]; then
    EXT=".dylib"
else
    EXT=".so"
fi

# Copy the compiled plugin to the script directory
cp "jarvis${EXT}" "${SCRIPT_DIR}/"
