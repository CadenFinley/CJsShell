#!/usr/bin/env python3
"""
Python script to count lines of code in the CJsShell project.
This is the Python equivalent of count_loc.sh.
"""

import os
import sys
from pathlib import Path
from typing import List, Tuple


def get_project_root() -> Path:
    """Get the project root directory."""
    script_dir = Path(__file__).parent.absolute()
    return script_dir.parent


def should_include_file(file_path: Path, project_root: Path) -> bool:
    """Check if a file should be included in the line count."""
    relative_path = file_path.relative_to(project_root)
    
    # Exclude certain directories
    excluded_dirs = {'vendor', 'plugins', 'themes', 'cmake', 'build', 'tool-scripts', 'tests'}
    if any(part in excluded_dirs for part in relative_path.parts):
        return False
    
    # Include specific file extensions
    included_extensions = {'.c', '.h', '.cpp', '.hpp', '.cc', '.sh'}
    return file_path.suffix in included_extensions


def count_lines_in_file(file_path: Path) -> int:
    """Count the number of lines in a file."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            return sum(1 for _ in f)
    except (IOError, OSError) as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
        return 0


def find_source_files(project_root: Path) -> List[Path]:
    """Find all source files that should be included in the count."""
    source_files = []
    
    for file_path in project_root.rglob('*'):
        if file_path.is_file() and should_include_file(file_path, project_root):
            source_files.append(file_path)
    
    return source_files


def main():
    """Main function to count lines of code."""
    project_root = get_project_root()
    os.chdir(project_root)
    
    # Find all source files
    source_files = find_source_files(project_root)
    
    # Count lines in each file and collect results
    file_info: List[Tuple[int, Path]] = []
    total_lines = 0
    
    for file_path in source_files:
        lines = count_lines_in_file(file_path)
        file_info.append((lines, file_path))
        total_lines += lines
    
    # Sort by line count (descending)
    file_info.sort(key=lambda x: x[0], reverse=True)
    
    # Print results
    for lines, file_path in file_info:
        relative_path = file_path.relative_to(project_root)
        print(f"{lines:6d} lines: ./{relative_path}")
    
    # Print summary
    print(f"Total files included in count: {len(source_files)}")
    print(f"{total_lines}")


if __name__ == "__main__":
    main()
