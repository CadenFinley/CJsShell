# Completion System Updates

## Changes Made

### 1. Directory-Only Completion for `cd` and `ls`
- Added `should_complete_directories_only()` function that checks if the current command should only complete directories
- Supports commands: `cd`, `ls`, `dir`, `rmdir`
- Modified `cjsh_filename_completer()` to filter out regular files when in directory-only mode

### 2. Case-Insensitive Completion with Case Correction
- Added `starts_with_case_insensitive()` helper function for case-insensitive string matching
- Updated directory-only completion to use case-insensitive matching and correct case
- Updated tilde (~) completion to use case-insensitive matching and correct case
- Updated dash (-) completion to use case-insensitive matching and correct case
- **Case Correction**: When you type lowercase letters and tab-complete, the completion will replace your lowercase input with the correctly cased filename/directory name

## Testing

To test the new functionality:

1. **Directory-only completion for cd:**
   ```bash
   cd <TAB>
   # Should only show directories, not files
   ```

2. **Case-insensitive completion with case correction:**
   ```bash
   cd doc<TAB>
   # If directory is "Documents/", it will replace "doc" with "Documents/"
   # The lowercase "doc" gets replaced with properly cased "Documents/"
   ```

3. **Regular commands still complete files and directories:**
   ```bash
   cat <TAB>
   # Should show both files and directories
   ```

## Implementation Details

- The `should_complete_directories_only()` function checks the command part of the input
- Directory filtering is applied in multiple places:
  - Main directory iteration loop (when path ends with '/')
  - Custom directory-only completion block
  - Tilde completion
  - Dash completion
- Case-insensitive matching uses `std::tolower()` for character comparison
- **Case correction**: Uses `ic_add_completion_prim()` with `delete_before` parameter to replace incorrectly cased prefixes with correctly cased completions
- Hidden files are still only shown when explicitly requested (prefix starts with '.')