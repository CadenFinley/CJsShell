#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}CJ's Shell Linux Build Helper${NC}"

# Check architecture
ARCH=$(uname -m)
echo "Detected architecture: $ARCH"

# Check if running on Linux
if [[ "$(uname -s)" != "Linux" ]]; then
  echo -e "${RED}This script is intended for Linux systems only.${NC}"
  exit 1
fi

# Function to check and install 32-bit build dependencies
install_32bit_deps() {
  echo -e "${YELLOW}Installing 32-bit build dependencies...${NC}"
  
  # Check for sudo access
  if ! command -v sudo &> /dev/null; then
    echo -e "${RED}Error: sudo command not found. You need sudo privileges to install packages.${NC}"
    exit 1
  fi
  
  # Update package lists
  sudo apt-get update
  
  # Install basic build tools
  sudo apt-get install -y build-essential cmake
  
  # Install 32-bit support packages
  sudo apt-get install -y gcc-multilib g++-multilib
  
  # Install 32-bit version of required libraries
  sudo apt-get install -y lib32z1-dev lib32gcc-9-dev lib32stdc++6
  
  # Install additional 32-bit development libraries that might be needed
  sudo apt-get install -y libc6-dev-i386
  
  echo -e "${GREEN}32-bit build dependencies installed successfully.${NC}"
}

# Function to check for common build issues
check_build_issues() {
  echo -e "${YELLOW}Checking for common build issues...${NC}"
  
  # Check if gcc-multilib is installed
  if ! dpkg -l | grep -q "gcc-multilib"; then
    echo -e "${RED}gcc-multilib is not installed. This is needed for 32-bit builds.${NC}"
    return 1
  fi
  
  # Check if 32-bit libgcc is available
  if [ ! -f /usr/lib32/libgcc_s.so.1 ] && [ ! -f /lib32/libgcc_s.so.1 ]; then
    echo -e "${RED}32-bit libgcc is not found. This is needed for linking 32-bit executables.${NC}"
    return 1
  fi
  
  echo -e "${GREEN}No common build issues detected.${NC}"
  return 0
}

# Parse command line arguments
if [ "$1" == "install-32bit" ]; then
  install_32bit_deps
  exit $?
elif [ "$1" == "check" ]; then
  check_build_issues
  exit $?
elif [ "$1" == "help" ] || [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
  echo "Usage: $0 [command]"
  echo "Commands:"
  echo "  install-32bit   Install all necessary 32-bit build dependencies"
  echo "  check           Check for common build issues"
  echo "  help            Display this help message"
  exit 0
else
  echo -e "${YELLOW}Running complete diagnostics and setup...${NC}"
  check_build_issues || install_32bit_deps
fi

echo -e "${GREEN}Build helper completed successfully.${NC}"
echo "To build CJ's Shell, run:"
echo "  mkdir -p build && cd build"
echo "  cmake .."
echo "  make"

exit 0
