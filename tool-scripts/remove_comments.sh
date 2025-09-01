#!/bin/bash

# Script to remove comments from C++ files, except for whitelisted files
# Usage: ./remove_comments.sh [--dry-run] [--verbose]

set -e

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DRY_RUN=0
VERBOSE=0

# Process command line options
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --dry-run) DRY_RUN=1; echo "Running in dry-run mode. No files will be modified."; shift ;;
        --verbose) VERBOSE=1; echo "Verbose mode enabled."; shift ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
done

# Whitelist of files that should keep their comments
# Add or remove entries as needed
WHITELISTED_FILES=(
    # Core files that need documentation
    "src/main.cpp"
    "src/cjsh_filesystem.cpp"
    "include/cjsh_filesystem.h"
    "include/main.h"
    "include/pluginapi.h"
    "src/utils/prompt_info.cpp"
    "include/colors.h"
    "include/parser.h"

    # Add more files as needed
)

# Whitelisted directories (all files within these dirs will keep comments)
WHITELISTED_DIRS=(
    "plugins/"
    "tests/"
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
    
    # Use a more robust approach to remove C++ comments
    # First handle multi-line comments, then single-line comments
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS (BSD) version
        # First pass: remove multi-line comments
        perl -0777 -pe 's|/\*[\s\S]*?\*/||g' "$file" > "$temp_file"
        # Second pass: remove single-line comments
        perl -pe 's|//.*$||g' "$temp_file" > "${temp_file}.2"
        mv "${temp_file}.2" "$temp_file"
    else
        # Linux (GNU) version
        # Remove both types of comments in one pass
        perl -0777 -pe 's|/\*[\s\S]*?\*/||g; s|//.*$||gm' "$file" > "$temp_file"
    fi
    
    # Compare file sizes to check if changes were made
    original_size=$(wc -c < "$file")
    new_size=$(wc -c < "$temp_file")
    
    # If not dry run, replace the original file
    if [[ $DRY_RUN -eq 0 ]]; then
        mv "$temp_file" "$file"
        if [[ $VERBOSE -eq 1 ]]; then
            echo "Processed $file (Original: $original_size bytes, New: $new_size bytes)"
        else
            echo "Removed comments from $file"
        fi
    else
        if [[ $VERBOSE -eq 1 ]]; then
            echo "Would process $file (Original: $original_size bytes, New: $new_size bytes)"
        else
            echo "Would remove comments from $file"
        fi
        rm "$temp_file"
    fi
}

# Find all C++ files in the project
echo "Starting comment removal process..."
find "$PROJECT_ROOT" -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cc" \) | while read -r file; do
    # Skip files in vendor directory and build directory
    if [[ "$file" == *"/vendor/"* || "$file" == *"/build/"* ]]; then
        echo "Skipping vendor/build file: $file"
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
