#!/usr/bin/env bash
set -e
# find all .cpp/.h files under src/ and include/
find src include -type f \( -name '*.cpp' -o -name '*.h' \) | while read -r f; do
  # strip all /*…*/ and //… comments
  perl -0777 -pe 's{/\*.*?\*/}{}gs; s{//[^\n]*}{}g' "$f" > "$f.tmp"
  mv "$f.tmp" "$f"
done
echo "All comments removed."
