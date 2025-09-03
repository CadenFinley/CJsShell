#!/bin/bash

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Create a build directory inside the plugin directory
BUILD_DIR="${SCRIPT_DIR}/build"
mkdir -p "$BUILD_DIR"

# Create an include directory to store header files
INCLUDE_DIR="${SCRIPT_DIR}/include"
mkdir -p "$INCLUDE_DIR"

# Download required header files from GitHub if they don't exist
echo "Checking for required header files..."
if [ ! -f "${INCLUDE_DIR}/cjsh.h" ] || [ ! -f "${INCLUDE_DIR}/pluginapi.h" ]; then
    echo "Downloading header files from GitHub..."
    # Create a temporary directory for the download
    TMP_DIR=$(mktemp -d)
    
    # Clone only the necessary files (sparse checkout)
    git clone --depth 1 --filter=blob:none --sparse https://github.com/CadenFinley/CJsShell.git "$TMP_DIR"
    cd "$TMP_DIR"
    git sparse-checkout set include
    
    # Copy the header files to the include directory
    echo "Copying header files to ${INCLUDE_DIR}..."
    cp -r include/* "${INCLUDE_DIR}/"
    
    # Clean up
    cd "$SCRIPT_DIR"
    rm -rf "$TMP_DIR"
    echo "Header files downloaded successfully."
else
    echo "Header files already exist. Skipping download."
fi

# Navigate to the build directory
cd "$BUILD_DIR"

# Update CMakeLists.txt to use local include directory
sed -i.bak "s|include_directories(\${CMAKE_CURRENT_SOURCE_DIR}/../../include)|include_directories(\${CMAKE_CURRENT_SOURCE_DIR}/../include)|g" ../CMakeLists.txt 2>/dev/null || \
sed "s|include_directories(\${CMAKE_CURRENT_SOURCE_DIR}/../../include)|include_directories(\${CMAKE_CURRENT_SOURCE_DIR}/../include)|g" ../CMakeLists.txt > ../CMakeLists.txt.new && \
mv ../CMakeLists.txt.new ../CMakeLists.txt

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
