#!/bin/bash
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  branch=$(git symbolic-ref --short HEAD 2>/dev/null || git describe --tags --exact-match 2>/dev/null || git rev-parse --short HEAD)
  status=$(git status --porcelain | awk '{print $1}' | sort -u | tr '\n' ' ')
  echo "[$branch $status]"
fi
