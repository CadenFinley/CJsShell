#!/bin/bash

# Exit on any error
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PLUGIN_NAME="fast_prompt_tags"
BUILD_DIR="${SCRIPT_DIR}/build"
MAIN_PROJECT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Parse command line arguments
FORCE_32BIT="false"
CMAKE_ARGS=""
CLEAN_BUILD="false"

while [[ $# -gt 0 ]]; do
    case $1 in
        --force-32bit)
            FORCE_32BIT="true"
            shift
            ;;
        --cmake-arg=*)
            CMAKE_ARGS="${CMAKE_ARGS} ${1#*=}"
            shift
            ;;
        --clean)
            CLEAN_BUILD="true"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --force-32bit       Force 32-bit build"
            echo "  --cmake-arg=ARG     Pass additional argument to CMake"
            echo "  --clean             Clean build directory before building"
            echo "  -h, --help          Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Detect architecture using the main project's detection script
ARCH_DETECT_SCRIPT="${MAIN_PROJECT_DIR}/cmake/detect_arch.sh"
if [ -f "${ARCH_DETECT_SCRIPT}" ]; then
    ARCH=$("${ARCH_DETECT_SCRIPT}" "${FORCE_32BIT}")
    echo "Detected architecture: ${ARCH}"
else
    echo "Warning: Architecture detection script not found, falling back to uname"
    if [ "${FORCE_32BIT}" = "true" ]; then
        ARCH="x86"
    else
        ARCH=$(uname -m)
    fi
fi

# Determine platform-specific extension
if [[ "$(uname)" == "Darwin" ]]; then
    EXTENSION=".dylib"
else
    EXTENSION=".so"
fi

# Create build directory if it doesn't exist
if [ "${CLEAN_BUILD}" = "true" ] && [ -d "${BUILD_DIR}" ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure CMake with architecture information
echo "Configuring build for architecture: ${ARCH}"
CMAKE_COMMAND="cmake -DARCH=\"${ARCH}\""

if [ "${FORCE_32BIT}" = "true" ]; then
    CMAKE_COMMAND="${CMAKE_COMMAND} -DFORCE_32BIT=ON"
fi

if [ -n "${CMAKE_ARGS}" ]; then
    CMAKE_COMMAND="${CMAKE_COMMAND} ${CMAKE_ARGS}"
fi

CMAKE_COMMAND="${CMAKE_COMMAND} .."

echo "Running: ${CMAKE_COMMAND}"
eval "${CMAKE_COMMAND}"

cmake --build . --config Release

# The built plugin should be in BUILD_DIR/plugins/ without architecture suffix
PLUGIN_FILE="${PLUGIN_NAME}${EXTENSION}"
if [ ! -f "${BUILD_DIR}/plugins/${PLUGIN_FILE}" ]; then
    echo "Error: Plugin was not built successfully."
    echo "Expected file: ${BUILD_DIR}/plugins/${PLUGIN_FILE}"
    echo "Available files in plugins directory:"
    ls -la "${BUILD_DIR}/plugins/" 2>/dev/null || echo "No plugins directory found"
    exit 1
fi

echo "Successfully built plugin: ${PLUGIN_FILE}"
cp "${BUILD_DIR}/plugins/${PLUGIN_FILE}" "${SCRIPT_DIR}/"

