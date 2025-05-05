#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}CJ's Shell Uninstaller${NC}"
echo "This script will remove cjsh from your system."

# Check if script is run with sudo
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}Error: This script must be run with sudo privileges.${NC}"
  echo "Please run: sudo $0"
  exit 1
fi

# Check if cjsh is installed
if [ ! -f "/usr/local/bin/cjsh" ]; then
  echo -e "${YELLOW}Warning: cjsh binary not found in /usr/local/bin/${NC}"
  echo "It seems that cjsh is not installed or was installed to a different location."
else
  # Remove binary
  echo "Removing cjsh binary from /usr/local/bin/..."
  rm -f /usr/local/bin/cjsh
  echo -e "${GREEN}Binary removed successfully.${NC}"
fi

# Check if man page exists and remove it
if [ -f "/usr/share/man/man1/cjsh.1" ]; then
  echo "Removing man page from /usr/share/man/man1/..."
  rm -f /usr/share/man/man1/cjsh.1
  # Update man database
  mandb &> /dev/null
  echo -e "${GREEN}Man page removed successfully.${NC}"
fi

# Check if cjsh is in /etc/shells and ask to remove it
if grep -q "/usr/local/bin/cjsh" /etc/shells; then
  echo -e "${YELLOW}cjsh is registered in /etc/shells. Do you want to remove it? (y/n)${NC}"
  read -r response
  if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
    grep -v "/usr/local/bin/cjsh" /etc/shells > /tmp/shells.tmp
    mv /tmp/shells.tmp /etc/shells
    echo -e "${GREEN}Removed cjsh from /etc/shells.${NC}"
  fi
fi

# Ask about user config files
echo -e "${YELLOW}Do you want to remove cjsh user configuration files? (y/n)${NC}"
echo "This will delete the ~/.cjsh directory and all its contents, including plugins, themes, and settings."
read -r remove_config

if [[ "$remove_config" =~ ^([yY][eE][sS]|[yY])$ ]]; then
  # Get actual user home directory (since we're running with sudo)
  SUDO_USER_HOME=$(eval echo ~$(logname))
  
  if [ -d "$SUDO_USER_HOME/.cjsh" ]; then
    echo "Removing cjsh configuration directory..."
    rm -rf "$SUDO_USER_HOME/.cjsh"
    echo -e "${GREEN}Configuration directory removed.${NC}"
  else
    echo "No configuration directory found at $SUDO_USER_HOME/.cjsh"
  fi
  
  # Also check for any rc files
  if [ -f "$SUDO_USER_HOME/.cjshrc" ]; then
    rm -f "$SUDO_USER_HOME/.cjshrc"
    echo "Removed .cjshrc file."
  fi
  
  if [ -f "$SUDO_USER_HOME/.cjprofile" ]; then
    rm -f "$SUDO_USER_HOME/.cjprofile"
    echo "Removed .cjprofile file."
  fi
else
  echo "Keeping configuration files. They remain at ~/.cjsh"
fi

# Final confirmation
echo -e "${GREEN}CJ's Shell has been uninstalled from your system.${NC}"

# Check if cjsh was the user's default shell and warn if so
CURRENT_SHELL=$(getent passwd $(logname) | cut -d: -f7)
if [ "$CURRENT_SHELL" = "/usr/local/bin/cjsh" ]; then
  echo -e "${RED}Warning: cjsh was set as the default shell for user $(logname).${NC}"
  echo "You should change your default shell using:"
  echo "  sudo chsh -s /bin/bash $(logname)  # or another available shell"
fi

exit 0
