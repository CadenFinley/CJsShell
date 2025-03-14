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
        # macOS - Launch terminal and bring it to the foreground
        osascript -e "tell application \"Terminal\"" \
                  -e "do script \"cd '$(pwd)' && $EXECUTABLE $*\"" \
                  -e "activate" \
                  -e "end tell"
    elif [ -n "$DISPLAY" ]; then
        # Linux with GUI
        if command -v gnome-terminal &> /dev/null; then
            # gnome-terminal should grab focus by default, but we'll use --wait option
            gnome-terminal --wait -- bash -c "cd '$(pwd)' && $EXECUTABLE $*; exec bash"
        elif command -v xterm &> /dev/null; then
            # Start xterm with raised window
            xterm -raise -e "cd '$(pwd)' && $EXECUTABLE $*; bash"
        elif command -v konsole &> /dev/null; then
            # Konsole with window activation
            konsole --new-tab -e "cd '$(pwd)' && $EXECUTABLE $*; bash" &
            sleep 0.5 # Small delay to allow window to open
            if command -v wmctrl &> /dev/null; then
                wmctrl -a "konsole" # Try to focus konsole window if wmctrl is available
            fi
        else
            echo "No supported terminal emulator found. Please run in a terminal."
            exit 1
        fi
    else
        echo "No graphical environment detected. Please run in a terminal."
        exit 1
    fi
fi
