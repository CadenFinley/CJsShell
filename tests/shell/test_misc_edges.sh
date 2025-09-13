#!/usr/bin/env sh
# Test misc edges: PATH resolution, comments/whitespace, pipeline status
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: misc edges..."

# Comments and extra whitespace should be ignored
OUT=$("$CJSH_PATH" -c "   echo   hi   # this is a comment")
if [ "$OUT" != "hi" ]; then
  echo "FAIL: comment/whitespace parsing"
  exit 1
fi

# PATH resolution: ensure a temp script runs when on PATH
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT INT TERM
cat > "$TMPDIR/hello.sh" <<'EOF'
#!/usr/bin/env sh
echo hi
EOF
chmod +x "$TMPDIR/hello.sh"
OUT2=$(PATH="$TMPDIR:$PATH" "$CJSH_PATH" -c "hello.sh")
if [ "$OUT2" != "hi" ]; then
  echo "FAIL: PATH resolution (got '$OUT2')"
  exit 1
fi

# Pipeline exit status: probe behavior; fail if pipeline exit codes not supported
"$CJSH_PATH" -c "false | true"
EC1=$?
"$CJSH_PATH" -c "true | false"
EC2=$?
if [ $EC1 -eq 0 ] && [ $EC2 -ne 0 ]; then
  : # behavior matches POSIX shells
else
  echo "FAIL: pipeline exit status semantics differ or unsupported"
  exit 1
fi

echo "PASS"
exit 0
