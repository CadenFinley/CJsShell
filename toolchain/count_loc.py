#!/usr/bin/env python3
"""
Enhanced Python script to count lines of code in the CJsShell project.
This is an enhanced version of count_loc.sh with additional features.
"""

import argparse
import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Set


def get_project_root() -> Path:
    """Get the project root directory."""
    script_dir = Path(__file__).parent.absolute()
    return script_dir.parent


def should_include_file(file_path: Path, project_root: Path, 
                       excluded_dirs: Set[str], included_extensions: Set[str]) -> bool:
    """Check if a file should be included in the line count."""
    relative_path = file_path.relative_to(project_root)
    
    # Exclude certain directories
    if any(part in excluded_dirs for part in relative_path.parts):
        return False
    
    # Exclude specific files
    if file_path.name == 'nob.h':
        return False
    
    # Include specific file extensions
    return file_path.suffix in included_extensions


def count_lines_in_file(file_path: Path, count_blank: bool = True, count_comments: bool = True) -> Dict[str, int]:
    """Count different types of lines in a file."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except (IOError, OSError) as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
        return {'total': 0, 'blank': 0, 'comment': 0, 'code': 0}
    
    total_lines = len(lines)
    blank_lines = 0
    comment_lines = 0
    
    for line in lines:
        stripped = line.strip()
        if not stripped:
            blank_lines += 1
        elif (stripped.startswith('//') or stripped.startswith('#') or 
              stripped.startswith('/*') or stripped.startswith('*')):
            comment_lines += 1
    
    code_lines = total_lines - blank_lines - comment_lines
    
    return {
        'total': total_lines,
        'blank': blank_lines,
        'comment': comment_lines,
        'code': code_lines
    }


def find_source_files(project_root: Path, excluded_dirs: Set[str], 
                     included_extensions: Set[str]) -> List[Path]:
    """Find all source files that should be included in the count."""
    source_files = []
    
    for file_path in project_root.rglob('*'):
        if file_path.is_file() and should_include_file(file_path, project_root, 
                                                      excluded_dirs, included_extensions):
            source_files.append(file_path)
    
    return source_files


def format_number(num: int) -> str:
    """Format number with thousands separators."""
    return f"{num:,}"


def main():
    """Main function to count lines of code."""
    parser = argparse.ArgumentParser(description='Count lines of code in CJsShell project')
    parser.add_argument('--detailed', '-d', action='store_true',
                       help='Show detailed breakdown of line types')
    parser.add_argument('--by-extension', '-e', action='store_true',
                       help='Group results by file extension')
    parser.add_argument('--include-dirs', nargs='*', 
                       help='Additional directories to include (default excludes: vendor, plugins, themes, build, tool-scripts, tests)')
    parser.add_argument('--exclude-dirs', nargs='*',
                       help='Additional directories to exclude')
    parser.add_argument('--extensions', nargs='*', default=['.c', '.h', '.cpp', '.hpp', '.cc', '.sh'],
                       help='File extensions to include (default: .c .h .cpp .hpp .cc .sh)')
    
    args = parser.parse_args()
    
    project_root = get_project_root()
    os.chdir(project_root)
    
    # Set up excluded directories
    excluded_dirs = {'vendor', 'plugins', 'themes', 'build'}
    if args.exclude_dirs:
        excluded_dirs.update(args.exclude_dirs)
    if args.include_dirs:
        excluded_dirs -= set(args.include_dirs)
    
    # Set up included extensions
    included_extensions = set(args.extensions)
    
    # Find all source files
    source_files = find_source_files(project_root, excluded_dirs, included_extensions)
    
    # Count lines in each file and collect results
    file_info: List[Tuple[Dict[str, int], Path]] = []
    totals = {'total': 0, 'blank': 0, 'comment': 0, 'code': 0}
    extension_stats: Dict[str, Dict[str, int]] = {}
    
    for file_path in source_files:
        line_counts = count_lines_in_file(file_path, count_blank=True, count_comments=True)
        file_info.append((line_counts, file_path))
        
        # Add to totals
        for key in totals:
            totals[key] += line_counts[key]
        
        # Add to extension stats
        ext = file_path.suffix
        if ext not in extension_stats:
            extension_stats[ext] = {'total': 0, 'blank': 0, 'comment': 0, 'code': 0, 'files': 0}
        for key in ['total', 'blank', 'comment', 'code']:
            extension_stats[ext][key] += line_counts[key]
        extension_stats[ext]['files'] += 1
    
    # Sort by total line count (descending)
    file_info.sort(key=lambda x: x[0]['total'], reverse=True)
    
    # Print results
    if args.detailed:
        print(f"{'Lines':>6} {'Blank':>6} {'Comment':>8} {'Code':>6} File")
        print("-" * 60)
        for line_counts, file_path in file_info:
            relative_path = file_path.relative_to(project_root)
            print(f"{line_counts['total']:6d} {line_counts['blank']:6d} "
                  f"{line_counts['comment']:8d} {line_counts['code']:6d} ./{relative_path}")
    else:
        for line_counts, file_path in file_info:
            relative_path = file_path.relative_to(project_root)
            print(f"{line_counts['total']:6d} lines: ./{relative_path}")
    
    print()
    
    # Print extension breakdown if requested
    if args.by_extension:
        print("By Extension:")
        print(f"{'Ext':>6} {'Files':>6} {'Total':>8} {'Blank':>6} {'Comment':>8} {'Code':>6}")
        print("-" * 50)
        for ext in sorted(extension_stats.keys()):
            stats = extension_stats[ext]
            print(f"{ext:>6} {stats['files']:6d} {stats['total']:8d} "
                  f"{stats['blank']:6d} {stats['comment']:8d} {stats['code']:6d}")
        print()
    
    # Print summary
    print(f"Total files included in count: {len(source_files)}")
    if args.detailed:
        print(f"Total lines: {format_number(totals['total'])}")
        print(f"  Code lines: {format_number(totals['code'])} ({totals['code']/totals['total']*100:.1f}%)")
        print(f"  Comment lines: {format_number(totals['comment'])} ({totals['comment']/totals['total']*100:.1f}%)")
        print(f"  Blank lines: {format_number(totals['blank'])} ({totals['blank']/totals['total']*100:.1f}%)")
    else:
        print(f"{totals['total']}")


if __name__ == "__main__":
    main()
