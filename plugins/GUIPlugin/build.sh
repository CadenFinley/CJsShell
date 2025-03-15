#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
make -j4

# Copy the built plugin to the application's plugins directory
mkdir -p ../../.DTT-Data/plugins
cp plugins/GUIPlugin.dylib ../../.DTT-Data/plugins/

echo "GUIPlugin built successfully"
