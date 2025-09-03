#!/bin/bash

# Exit on any error
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PLUGIN_NAME="fast_prompt_tags"
BUILD_DIR="${SCRIPT_DIR}/build"
MAIN_PROJECT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Determine platform-specific extension
if [[ "$(uname)" == "Darwin" ]]; then
    EXTENSION=".dylib"
else
    EXTENSION=".so"
fi

# Create build directory if it doesn't exist
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake ..

cmake --build . --config Release

# The built plugin should be in BUILD_DIR/plugins/
if [ ! -f "${BUILD_DIR}/plugins/${PLUGIN_NAME}${EXTENSION}" ]; then
    echo "Error: Plugin was not built successfully."
    exit 1
fi

cp "${BUILD_DIR}/plugins/${PLUGIN_NAME}${EXTENSION}" "${SCRIPT_DIR}/"

