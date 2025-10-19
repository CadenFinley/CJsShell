#!/usr/bin/env bash

cd "$(dirname "$0")/nob"

if [ ! -f "./nob" ]; then
    echo "Building nob..."
    cc -o nob nob.c
    if [ $? -ne 0 ]; then
        echo "Failed to compile nob"
        exit 1
    fi
fi

./nob "$@"