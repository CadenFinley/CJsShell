#!/bin/bash

# Script to remove comments from C++ files, except for whitelisted files
# Usage: ./remove_comments.sh [--dry-run]

set -e

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DRY_RUN=0

# Check for dry run option
if [[ "$1" == "--dry-run" ]]; then
    DRY_RUN=1
    echo "Running in dry-run mode. No files will be modified."
fi

# Whitelist of files that should keep their comments
# Add or remove entries as needed
WHITELISTED_FILES=(
    # Core files that need documentation
    "src/main.cpp"
    "src/cjsh_filesystem.cpp"
    "include/cjsh_filesystem.h"
    "include/main.h"
    "include/pluginapi.h"

    # Add more files as needed
)

# Whitelisted directories (all files within these dirs will keep comments)
WHITELISTED_DIRS=(
    # Example of whitelisted directories
    # "src/assistant"
    # "plugins/jarvis"
)

# Function to check if a file is in the whitelist
is_whitelisted() {
    local file="$1"
    local rel_path="${file#$PROJECT_ROOT/}"
    
    # Check if file is directly in whitelist
    for whitelisted_file in "${WHITELISTED_FILES[@]}"; do
        if [[ "$rel_path" == "$whitelisted_file" ]]; then
            return 0 # True, file is whitelisted
        fi
    done
    
    # Check if file is in a whitelisted directory
    for whitelisted_dir in "${WHITELISTED_DIRS[@]}"; do
        if [[ "$rel_path" == "$whitelisted_dir"* ]]; then
            return 0 # True, file is in a whitelisted directory
        fi
    done
    
    return 1 # False, file is not whitelisted
}

# Function to remove comments from a file
remove_comments() {
    local file="$1"
    
    # Temporary file
    local temp_file=$(mktemp)
    
    # Use sed to remove C++ comments
    # This handles both // line comments and /* */ block comments
    sed -E '
        # Handle block comments
        /\/\*/!b
        :a
        /\*\//!{
            N
            ba
        }
        s/\/\*([^*]|[\r\n]|(\*+([^*\/]|[\r\n])))*\*+\///g
        
        # Handle line comments
        s/\/\/.*$//
    ' "$file" > "$temp_file"
    
    # If not dry run, replace the original file
    if [[ $DRY_RUN -eq 0 ]]; then
        mv "$temp_file" "$file"
        echo "Removed comments from $file"
    else
        echo "Would remove comments from $file"
        rm "$temp_file"
    fi
}

# Find all C++ files in the project
find "$PROJECT_ROOT" -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cc" \) | while read -r file; do
    # Skip files in vendor directory and build directory
    if [[ "$file" == *"/vendor/"* || "$file" == *"/build/"* ]]; then
        continue
    fi
    
    # Check if file is whitelisted
    if is_whitelisted "$file"; then
        echo "Skipping whitelisted file: $file"
    else
        remove_comments "$file"
    fi
done

echo "Comment removal complete!"
