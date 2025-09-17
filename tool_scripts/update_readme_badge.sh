#!/usr/bin/env sh

# Update README.md with current test badge
# This script runs the test badge generator and updates the README

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT="$SCRIPT_DIR/.."
README_FILE="$PROJECT_ROOT/README.md"
BADGE_GENERATOR="$SCRIPT_DIR/generate_test_badge.sh"

echo "Updating README test badge..."

# Generate fresh badge data
"$BADGE_GENERATOR"

# Check if badge files were created
if [ ! -f "$PROJECT_ROOT/build/badge_markdown.txt" ]; then
    echo "Error: Badge markdown file not found!"
    exit 1
fi

# Read the new badge markdown
NEW_BADGE=$(cat "$PROJECT_ROOT/build/badge_markdown.txt" | tr -d '\n')

echo "New badge: $NEW_BADGE"

# Create a backup of the original README
cp "$README_FILE" "$README_FILE.backup"

# Use sed to replace the test badge line
# Look for a line that contains [![Tests] and replace it
sed -i.tmp '/\[\!\[Tests\]/c\
'"$NEW_BADGE"'' "$README_FILE"

# Remove the temporary file created by sed
rm -f "$README_FILE.tmp"

echo "README.md updated successfully!"
echo "Backup saved as README.md.backup"

# Show the difference
echo ""
echo "Changes made:"
diff "$README_FILE.backup" "$README_FILE" || true