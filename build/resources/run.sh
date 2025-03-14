#!/bin/bash

# DevToolsTerminal Runner Script
EXECUTABLE="./DevToolsTerminal"

# Check if script is running in a terminal
if [ -t 0 ]; then
    # Running in a terminal, proceed normally
    "$EXECUTABLE" "$@"
else
    # Not running in a terminal, open a new terminal window
    # The approach differs based on the desktop environment
    if [ "$(uname)" == "Darwin" ]; then
        # macOS
        osascript -e "tell application \"Terminal\" to do script \"cd '$(pwd)' && $EXECUTABLE $*\""
    elif [ -n "$DISPLAY" ]; then
        # Linux with GUI
        if command -v gnome-terminal &> /dev/null; then
            gnome-terminal -- bash -c "cd '$(pwd)' && $EXECUTABLE $*; exec bash"
        elif command -v xterm &> /dev/null; then
            xterm -e "cd '$(pwd)' && $EXECUTABLE $*; bash"
        elif command -v konsole &> /dev/null; then
            konsole -e "cd '$(pwd)' && $EXECUTABLE $*; bash"
        else
            echo "No supported terminal emulator found. Please run in a terminal."
            exit 1
        fi
    else
        echo "No graphical environment detected. Please run in a terminal."
        exit 1
    fi
fi
