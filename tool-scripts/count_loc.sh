#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(dirname "$script_dir")"
cd "$project_root"

  #echo "Files included in count:"
  files=$(find . -type f \
    -not -path "./vendor/*" \
    -not -path "./plugins/*" \
    -not -path "./themes/*" \
    -not -path "./cmake/*" \
    -not -path "./build/*" \
    -not -path "./tool-scripts/*" \
    \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' -o -name '*.cc' -o -name '*.sh' \))
  
  # Create an array to store file info
  # declare -a file_info
  
  # Get line count for each file
  # for file in $files; do
  #   lines=$(wc -l < "$file")
  #   file_info+=("$lines $file")
  # done
  
  # Sort files by line count (numerically) and print
  # printf '%s\n' "${file_info[@]}" | sort -nr | while read -r line_info; do
  #   count=$(echo "$line_info" | awk '{print $1}')
  #   file_path=$(echo "$line_info" | cut -d' ' -f2-)
  #   printf "%6d lines: %s\n" "$count" "$file_path"
  # done
  
  # Count total lines
  total=$(echo "$files" | xargs cat 2>/dev/null | wc -l)
  echo -e "$total"