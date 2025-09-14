#!/usr/bin/env python3
"""
Python script to find all system includes (#include <...>) in the CJsShell codebase.
This script analyzes C/C++ source files and headers to extract system library dependencies.
"""

import argparse
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple
from collections import defaultdict


def get_project_root() -> Path:
    """Get the project root directory."""
    script_dir = Path(__file__).parent.absolute()
    return script_dir.parent


def should_include_file(file_path: Path, project_root: Path, 
                       excluded_dirs: Set[str], included_extensions: Set[str]) -> bool:
    """Check if a file should be included in the analysis."""
    relative_path = file_path.relative_to(project_root)
    
    # Exclude certain directories
    if any(part in excluded_dirs for part in relative_path.parts):
        return False
    
    # Include specific file extensions
    return file_path.suffix in included_extensions


def extract_system_includes(file_path: Path) -> List[Tuple[str, int]]:
    """Extract all system includes (#include <...>) from a file."""
    includes = []
    
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except (IOError, OSError) as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
        return includes
    
    # Regular expression to match #include <...>
    include_pattern = re.compile(r'^\s*#\s*include\s*<([^>]+)>')
    
    for line_num, line in enumerate(lines, 1):
        # Remove comments from the line
        line = re.sub(r'//.*$', '', line)  # Remove single-line comments
        
        match = include_pattern.match(line)
        if match:
            header_name = match.group(1).strip()
            includes.append((header_name, line_num))
    
    return includes


def analyze_codebase(project_root: Path, excluded_dirs: Set[str], 
                    included_extensions: Set[str]) -> Tuple[Dict[str, List[Tuple[str, int]]], Dict[str, Set[str]]]:
    """Analyze the entire codebase for system includes."""
    file_includes = {}  # file_path -> [(header, line_num), ...]
    header_usage = defaultdict(set)  # header -> {file_paths}
    
    for root, dirs, files in os.walk(project_root):
        # Modify dirs in-place to skip excluded directories
        dirs[:] = [d for d in dirs if d not in excluded_dirs]
        
        for file in files:
            file_path = Path(root) / file
            
            if should_include_file(file_path, project_root, excluded_dirs, included_extensions):
                includes = extract_system_includes(file_path)
                if includes:
                    relative_path = str(file_path.relative_to(project_root))
                    file_includes[relative_path] = includes
                    
                    # Track which files use each header
                    for header, _ in includes:
                        header_usage[header].add(relative_path)
    
    return file_includes, dict(header_usage)


def print_detailed_report(file_includes: Dict[str, List[Tuple[str, int]]], 
                         header_usage: Dict[str, Set[str]], args):
    """Print a detailed report of system includes."""
    
    print("=" * 80)
    print("SYSTEM INCLUDES ANALYSIS FOR CJSSHELL CODEBASE")
    print("=" * 80)
    print()
    
    # Summary statistics
    total_files = len(file_includes)
    total_includes = sum(len(includes) for includes in file_includes.values())
    unique_headers = len(header_usage)
    
    print(f"📊 SUMMARY:")
    print(f"   Files analyzed: {total_files}")
    print(f"   Total system includes: {total_includes}")
    print(f"   Unique system headers: {unique_headers}")
    print()
    
    if args.by_file:
        print("📁 INCLUDES BY FILE:")
        print("-" * 50)
        for file_path in sorted(file_includes.keys()):
            includes = file_includes[file_path]
            print(f"\n{file_path} ({len(includes)} includes):")
            for header, line_num in sorted(includes):
                print(f"   Line {line_num:3d}: #include <{header}>")
        print()
    
    if args.by_header:
        print("📋 INCLUDES BY HEADER:")
        print("-" * 50)
        for header in sorted(header_usage.keys()):
            files = header_usage[header]
            print(f"\n<{header}> (used in {len(files)} files):")
            for file_path in sorted(files):
                # Find line numbers for this header in this file
                line_nums = [str(line_num) for h, line_num in file_includes[file_path] if h == header]
                line_info = f" (lines: {', '.join(line_nums)})" if line_nums else ""
                print(f"   {file_path}{line_info}")
        print()
    
    if args.frequency:
        print("📈 HEADER USAGE FREQUENCY:")
        print("-" * 50)
        # Sort headers by frequency (most used first)
        sorted_headers = sorted(header_usage.items(), key=lambda x: len(x[1]), reverse=True)
        for i, (header, files) in enumerate(sorted_headers, 1):
            print(f"{i:3d}. <{header}> - {len(files)} files")
        print()
    
    if args.categories:
        print("📚 HEADERS BY CATEGORY:")
        print("-" * 50)
        
        # Categorize headers
        categories = {
            "C Standard Library": [
                "stdio.h", "stdlib.h", "string.h", "math.h", "time.h", "assert.h",
                "errno.h", "limits.h", "stdarg.h", "stddef.h", "stdint.h", "stdbool.h",
                "ctype.h", "signal.h", "setjmp.h", "locale.h", "wchar.h", "wctype.h"
            ],
            "C++ Standard Library": [
                "iostream", "vector", "string", "map", "set", "algorithm", "memory",
                "functional", "utility", "iterator", "exception", "stdexcept", "new",
                "typeinfo", "numeric", "list", "deque", "queue", "stack", "fstream",
                "sstream", "iomanip", "regex", "thread", "mutex", "atomic", "chrono",
                "random", "array", "unordered_map", "unordered_set", "tuple", "initializer_list"
            ],
            "POSIX/Unix": [
                "unistd.h", "sys/types.h", "sys/stat.h", "sys/wait.h", "fcntl.h",
                "dirent.h", "pwd.h", "grp.h", "termios.h", "sys/ioctl.h", "sys/select.h",
                "sys/socket.h", "netinet/in.h", "arpa/inet.h", "netdb.h", "pthread.h",
                "semaphore.h", "sys/mman.h", "sys/shm.h", "sys/msg.h", "sys/sem.h"
            ],
            "Platform Specific": [
                "windows.h", "winbase.h", "winsock2.h", "direct.h", "io.h",
                "sys/syscall.h", "mach/mach.h", "CoreFoundation/CoreFoundation.h"
            ]
        }
        
        categorized = {cat: [] for cat in categories}
        uncategorized = []
        
        for header in sorted(header_usage.keys()):
            found_category = False
            for category, headers in categories.items():
                if header in headers:
                    categorized[category].append(header)
                    found_category = True
                    break
            if not found_category:
                uncategorized.append(header)
        
        for category, headers in categorized.items():
            if headers:
                print(f"\n{category}:")
                for header in headers:
                    count = len(header_usage[header])
                    print(f"   <{header}> ({count} files)")
        
        if uncategorized:
            print(f"\nOther/Unknown:")
            for header in uncategorized:
                count = len(header_usage[header])
                print(f"   <{header}> ({count} files)")
        print()


def main():
    """Main function."""
    parser = argparse.ArgumentParser(
        description="Find all system includes (#include <...>) in the CJsShell codebase",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                           # Basic analysis
  %(prog)s --by-file                 # Show includes grouped by file
  %(prog)s --by-header               # Show includes grouped by header
  %(prog)s --frequency               # Show headers sorted by usage frequency
  %(prog)s --categories              # Categorize headers by type
  %(prog)s --all                     # Show all reports
  %(prog)s --exclude-vendor          # Exclude vendor directory
        """
    )
    
    parser.add_argument('--by-file', action='store_true',
                       help='Show includes grouped by file')
    parser.add_argument('--by-header', action='store_true',
                       help='Show includes grouped by header')
    parser.add_argument('--frequency', action='store_true',
                       help='Show headers sorted by usage frequency')
    parser.add_argument('--categories', action='store_true',
                       help='Categorize headers by type (C stdlib, C++, POSIX, etc.)')
    parser.add_argument('--all', action='store_true',
                       help='Show all available reports')
    parser.add_argument('--exclude-vendor', action='store_true',
                       help='Exclude vendor directory from analysis')
    parser.add_argument('--exclude-tests', action='store_true',
                       help='Exclude tests directory from analysis')
    
    args = parser.parse_args()
    
    # If --all is specified, enable all report types
    if args.all:
        args.by_file = True
        args.by_header = True
        args.frequency = True
        args.categories = True
    
    # If no specific report type is requested, show basic summary + frequency
    if not any([args.by_file, args.by_header, args.frequency, args.categories]):
        args.frequency = True
        args.categories = True
    
    # Define excluded directories
    excluded_dirs = {'build', '.git', '__pycache__', '.vscode'}
    if args.exclude_vendor:
        excluded_dirs.add('vendor')
    if args.exclude_tests:
        excluded_dirs.add('tests')
    
    # Define included file extensions
    included_extensions = {'.cpp', '.c', '.h', '.hpp', '.cc', '.cxx', '.hxx'}
    
    # Get project root
    project_root = get_project_root()
    
    print(f"Analyzing system includes in: {project_root}")
    print(f"Excluded directories: {', '.join(sorted(excluded_dirs))}")
    print(f"File extensions: {', '.join(sorted(included_extensions))}")
    print()
    
    # Analyze the codebase
    file_includes, header_usage = analyze_codebase(project_root, excluded_dirs, included_extensions)
    
    if not file_includes:
        print("No system includes found in the codebase.")
        return
    
    # Print the report
    print_detailed_report(file_includes, header_usage, args)


if __name__ == "__main__":
    main()
