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
    
    # Exclude isocline directories specifically
    # if 'isocline' in relative_path.parts:
    #     return False
    
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
        return {'total': 0, 'blank': 0, 'comment': 0, 'code': 0, 'size': 0}
    
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
    
    # Get file size
    try:
        file_size = file_path.stat().st_size
    except OSError:
        file_size = 0
    
    # If stripping comments and blanks, only count code lines
    if not count_blank and not count_comments:
        total_lines = code_lines
        blank_lines = 0
        comment_lines = 0
    
    return {
        'total': total_lines,
        'blank': blank_lines,
        'comment': comment_lines,
        'code': code_lines,
        'size': file_size
    }


def categorize_file(file_path: Path, project_root: Path) -> str:
    """Categorize a file by its directory."""
    relative_path = file_path.relative_to(project_root)
    parts = relative_path.parts
    
    if not parts:
        return 'other'
    
    first_dir = parts[0]
    
    # Check for isocline subdirectories first
    if 'isocline' in parts:
        return 'isocline'
    
    # Categorize based on top-level directory
    if first_dir in ['src', 'include']:
        return 'source_code'
    elif first_dir == 'tests':
        return 'tests'
    elif first_dir == 'toolchain':
        return 'toolchain'
    else:
        return 'other'


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


def format_file_size(size_bytes: int) -> str:
    """Format file size in human-readable format."""
    if size_bytes < 1024:
        return f"{size_bytes}B"
    elif size_bytes < 1024 * 1024:
        return f"{size_bytes / 1024:.1f}KB"
    elif size_bytes < 1024 * 1024 * 1024:
        return f"{size_bytes / (1024 * 1024):.1f}MB"
    else:
        return f"{size_bytes / (1024 * 1024 * 1024):.1f}GB"


def main():
    """Main function to count lines of code."""
    parser = argparse.ArgumentParser(description='Count lines of code in CJsShell project')
    parser.add_argument('--detailed', '-d', action='store_true',
                       help='Show detailed breakdown of line types')
    parser.add_argument('--by-extension', '-e', action='store_true',
                       help='Group results by file extension')
    parser.add_argument('--strip', '-s', action='store_true',
                       help='Strip comments and blank lines from count (only count code lines)')
    parser.add_argument('--include-dirs', nargs='*', 
                       help='Additional directories to include (default excludes: vendor, plugins, themes, build, toolchain, tests)')
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
    file_info: List[Tuple[Dict[str, int], Path, str]] = []
    totals = {'total': 0, 'blank': 0, 'comment': 0, 'code': 0, 'size': 0}
    category_stats: Dict[str, Dict[str, int]] = {}
    extension_stats: Dict[str, Dict[str, int]] = {}
    
    for file_path in source_files:
        line_counts = count_lines_in_file(file_path, count_blank=not args.strip, count_comments=not args.strip)
        category = categorize_file(file_path, project_root)
        file_info.append((line_counts, file_path, category))
        
        # Add to totals
        for key in totals:
            totals[key] += line_counts[key]
        
        # Add to category stats
        if category not in category_stats:
            category_stats[category] = {'total': 0, 'blank': 0, 'comment': 0, 'code': 0, 'size': 0, 'files': 0}
        for key in ['total', 'blank', 'comment', 'code', 'size']:
            category_stats[category][key] += line_counts[key]
        category_stats[category]['files'] += 1
        
        # Add to extension stats
        ext = file_path.suffix
        if ext not in extension_stats:
            extension_stats[ext] = {'total': 0, 'blank': 0, 'comment': 0, 'code': 0, 'size': 0, 'files': 0}
        for key in ['total', 'blank', 'comment', 'code', 'size']:
            extension_stats[ext][key] += line_counts[key]
        extension_stats[ext]['files'] += 1
    
    # Sort by total line count (descending)
    file_info.sort(key=lambda x: x[0]['total'], reverse=True)
    
    # Define category order and display names
    category_order = ['source_code', 'isocline', 'tests', 'toolchain', 'other']
    category_names = {
        'source_code': 'Source Code (src/, include/)',
        'isocline': 'Isocline (src/isocline/, include/isocline/)',
        'tests': 'Tests',
        'toolchain': 'Toolchain',
        'other': 'Other'
    }
    
    # Print results organized by category
    for category in category_order:
        if category not in category_stats:
            continue
        
        print(f"\n{'='*70}")
        print(f"{category_names[category]}")
        print(f"{'='*70}")
        
        # Filter files for this category
        category_files = [(lc, fp) for lc, fp, cat in file_info if cat == category]
        
        if args.detailed:
            print(f"{'Lines':>6} {'Blank':>6} {'Comment':>8} {'Code':>6} {'Size':>8} File")
            print("-" * 80)
            for line_counts, file_path in category_files:
                relative_path = file_path.relative_to(project_root)
                size_str = format_file_size(line_counts['size'])
                print(f"{line_counts['total']:6d} {line_counts['blank']:6d} "
                      f"{line_counts['comment']:8d} {line_counts['code']:6d} {size_str:>8} ./{relative_path}")
        else:
            for line_counts, file_path in category_files:
                relative_path = file_path.relative_to(project_root)
                size_str = format_file_size(line_counts['size'])
                print(f"{line_counts['total']:6d} lines ({size_str:>8}): ./{relative_path}")
        
        # Print category summary
        stats = category_stats[category]
        print(f"\n{category_names[category]} Summary:")
        print(f"  Files: {stats['files']}")
        print(f"  Total size: {format_file_size(stats['size'])}")
        if args.detailed:
            print(f"  Total lines: {format_number(stats['total'])}")
            print(f"  Code lines: {format_number(stats['code'])} ({stats['code']/stats['total']*100:.1f}%)")
            print(f"  Comment lines: {format_number(stats['comment'])} ({stats['comment']/stats['total']*100:.1f}%)")
            print(f"  Blank lines: {format_number(stats['blank'])} ({stats['blank']/stats['total']*100:.1f}%)")
        else:
            print(f"  Total lines: {format_number(stats['total'])}")
    
    print(f"\n{'='*70}")
    
    # Print extension breakdown if requested
    if args.by_extension:
        print("\nBy Extension:")
        print(f"{'Ext':>6} {'Files':>6} {'Total':>8} {'Blank':>6} {'Comment':>8} {'Code':>6} {'Size':>8}")
        print("-" * 60)
        for ext in sorted(extension_stats.keys()):
            stats = extension_stats[ext]
            size_str = format_file_size(stats['size'])
            print(f"{ext:>6} {stats['files']:6d} {stats['total']:8d} "
                  f"{stats['blank']:6d} {stats['comment']:8d} {stats['code']:6d} {size_str:>8}")
        print()
    
    # Print overall summary
    project_size_categories = {'source_code', 'isocline'}
    project_size_bytes = sum(
        category_stats[cat]['size']
        for cat in project_size_categories
        if cat in category_stats
    )

    print("\nOverall Project Summary:")
    print(f"Total files included in count: {len(source_files)}")
    print(f"Total project size: {format_file_size(project_size_bytes)}")
    if args.detailed:
        print(f"Total lines: {format_number(totals['total'])}")
        print(f"  Code lines: {format_number(totals['code'])} ({totals['code']/totals['total']*100:.1f}%)")
        print(f"  Comment lines: {format_number(totals['comment'])} ({totals['comment']/totals['total']*100:.1f}%)")
        print(f"  Blank lines: {format_number(totals['blank'])} ({totals['blank']/totals['total']*100:.1f}%)")
    else:
        print(f"Total lines: {format_number(totals['total'])}")


if __name__ == "__main__":
    main()
