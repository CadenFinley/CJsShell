#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}CJ's Shell Linux Installer${NC}"
echo "This script will install cjsh to your system."

# Check if script is run with sudo
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}Error: This script must be run with sudo privileges.${NC}"
  echo "Please run: sudo $0"
  exit 1
fi

# Locate the build directory and binary
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
BINARY="$BUILD_DIR/cjsh"

# Check if the binary exists
if [ ! -f "$BINARY" ]; then
  echo -e "${RED}Error: cjsh binary not found at $BINARY${NC}"
  echo "Please build the project first by running:"
  echo "  mkdir -p build && cd build && cmake .. && make"
  exit 1
fi

# Create install directories if they don't exist
mkdir -p /usr/local/bin
mkdir -p /usr/share/man/man1

# Install the binary
echo "Installing cjsh binary to /usr/local/bin/..."
cp "$BINARY" /usr/local/bin/
chmod 755 /usr/local/bin/cjsh

# Check if man page exists
MAN_PAGE="$PROJECT_ROOT/doc/cjsh.1"
if [ -f "$MAN_PAGE" ]; then
  echo "Installing man page to /usr/share/man/man1/..."
  cp "$MAN_PAGE" /usr/share/man/man1/
  chmod 644 /usr/share/man/man1/cjsh.1
  # Update man database
  mandb &> /dev/null
  echo -e "${GREEN}Man page installed successfully.${NC}"
else
  echo -e "${YELLOW}Warning: Man page not found at $MAN_PAGE. Skipping man page installation.${NC}"
fi

# Create .cjsh directory structure in user's home if it doesn't exist
mkdir -p "$HOME/.cjsh"

# Set proper ownership for the .cjsh directory
SUDO_USER_HOME=$(eval echo ~$(logname))
chown -R $(logname):$(logname) "$SUDO_USER_HOME/.cjsh"

# Final success message
echo -e "${GREEN}CJ's Shell has been installed successfully!${NC}"
echo "You can now run 'cjsh' to start the shell."
echo "For more information, run 'man cjsh' (if man page was installed)."
echo -e "${YELLOW}To set cjsh as your default shell:${NC}"
echo "1. Add it to /etc/shells: echo \"/usr/local/bin/cjsh\" >> /etc/shells"
echo "2. Change your shell: chsh -s /usr/local/bin/cjsh"
echo -e "${YELLOW}To uninstall in the future, run:${NC} ~/.cjsh/linux_uninstall.sh"

exit 0
