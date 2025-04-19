#!/bin/bash

# Script to add DevToolsTerminal to /etc/shells and configure PAM
DATA_DIR="$HOME/.DTT-Data"
APP_PATH="$DATA_DIR/DevToolsTerminal"

if [ ! -f "$APP_PATH" ]; then
    echo "Error: DevToolsTerminal not found at $APP_PATH"
    exit 1
fi

# Check if already in /etc/shells
if grep -q "$APP_PATH" /etc/shells; then
    echo "DevToolsTerminal is already registered in /etc/shells"
else
    # Add to /etc/shells (requires sudo)
    echo "Adding DevToolsTerminal to /etc/shells..."
    echo "This requires administrative privileges."

    # Backup the existing file
    sudo cp /etc/shells /etc/shells.bak
    echo "Backup created at /etc/shells.bak"

    # Add our shell
    echo "$APP_PATH" | sudo tee -a /etc/shells > /dev/null

    if [ $? -eq 0 ]; then
        echo "Successfully added DevToolsTerminal to /etc/shells"
    else
        echo "Failed to add DevToolsTerminal to /etc/shells"
        exit 1
    fi
fi

# Check if we need to create a PAM configuration
PAM_CONFIG="/etc/pam.d/DevToolsTerminal"
if [ ! -f "$PAM_CONFIG" ]; then
    echo "Creating PAM configuration for DevToolsTerminal..."
    echo "This requires administrative privileges."
    
    # Create a standard PAM configuration for a login shell
    cat << EOF | sudo tee "$PAM_CONFIG" > /dev/null
# PAM configuration for DevToolsTerminal

# Authentication modules
auth       required     pam_unix.so
auth       include      system-auth

# Account management
account    required     pam_unix.so
account    include      system-account

# Password management
password   include      system-password

# Session management
session    required     pam_unix.so
session    include      system-session
session    optional     pam_keyinit.so force revoke
session    optional     pam_motd.so
session    optional     pam_mail.so dir=/var/mail standard
EOF

    if [ $? -eq 0 ]; then
        echo "Successfully created PAM configuration at $PAM_CONFIG"
    else
        echo "Failed to create PAM configuration"
        echo "You may need to manually configure PAM for full login shell functionality"
    fi
fi

echo "You can now set DevToolsTerminal as your login shell with: chsh -s $APP_PATH"
exit 0
