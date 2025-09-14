#!/bin/bash
set -e

# CJ's Shell (cjsh) Auto-Installer
# Download and run with: curl -fsSL https://raw.githubusercontent.com/CadenFinley/CJsShell/master/install.sh | bash

# Enhanced color palette
readonly GREEN='\033[0;32m'
readonly BRIGHT_GREEN='\033[1;32m'
readonly YELLOW='\033[1;33m'
readonly RED='\033[0;31m'
readonly BRIGHT_RED='\033[1;31m'
readonly BLUE='\033[0;34m'
readonly BRIGHT_BLUE='\033[1;34m'
readonly PURPLE='\033[0;35m'
readonly CYAN='\033[0;36m'
readonly WHITE='\033[1;37m'
readonly GRAY='\033[0;37m'
readonly NC='\033[0m'
readonly BOLD='\033[1m'

# Unicode symbols for better visual appeal
readonly CHECK_MARK="✓"
readonly CROSS_MARK="✗"
readonly WARNING="⚠"
readonly INFO="ℹ"
readonly ARROW="→"

# Installation configuration
readonly REPO_URL="https://github.com/CadenFinley/CJsShell"
readonly GITHUB_API_URL="https://api.github.com/repos/CadenFinley/CJsShell"
readonly INSTALL_DIR="/usr/local/bin"
readonly TEMP_DIR=$(mktemp -d)

# Cleanup function
cleanup() {
    if [ -n "$TEMP_DIR" ] && [ -d "$TEMP_DIR" ]; then
        rm -rf "$TEMP_DIR"
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Function to print a decorative header
print_header() {
    echo
    echo -e "${BRIGHT_BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BRIGHT_BLUE}║                                                            ║${NC}"
    echo -e "${BRIGHT_BLUE}║          ${WHITE}${BOLD}CJ's Shell (cjsh) Auto-Installer${NC}${BRIGHT_BLUE}                 ║${NC}"
    echo -e "${BRIGHT_BLUE}║                                                            ║${NC}"
    echo -e "${BRIGHT_BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
    echo
}

# Function to print section headers
print_section() {
    echo -e "${CYAN}${BOLD}$1${NC}"
    echo -e "${GRAY}$(printf '%.60s' "────────────────────────────────────────────────────────────")${NC}"
}

# Function to print status messages
print_status() {
    echo -e "  ${BRIGHT_GREEN}${CHECK_MARK}${NC} $1"
}

print_info() {
    echo -e "  ${BLUE}${INFO}${NC} $1"
}

print_warning() {
    echo -e "  ${YELLOW}${WARNING}${NC} $1"
}

print_error() {
    echo -e "  ${BRIGHT_RED}${CROSS_MARK}${NC} $1"
}

print_command() {
    echo -e "    ${GRAY}\$${NC} ${CYAN}$1${NC}"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Function to install dependencies based on OS
install_dependencies() {
    local os=$(detect_os)
    
    print_section "Checking Dependencies"
    
    local missing_deps=()
    
    # Check for required tools
    if ! command_exists cmake; then
        missing_deps+=("cmake")
    else
        print_status "cmake found"
    fi
    
    if ! command_exists make; then
        missing_deps+=("make")
    else
        print_status "make found"
    fi
    
    if ! command_exists git; then
        missing_deps+=("git")
    else
        print_status "git found"
    fi
    
    # Check for C++ compiler
    if command_exists g++; then
        print_status "g++ compiler found"
    elif command_exists clang++; then
        print_status "clang++ compiler found"
    else
        missing_deps+=("g++/clang++")
    fi
    
    if [ ${#missing_deps[@]} -eq 0 ]; then
        print_status "All dependencies satisfied"
        return 0
    fi
    
    print_warning "Missing dependencies: ${missing_deps[*]}"
    echo
    
    case "$os" in
        "linux")
            if command_exists apt-get; then
                print_info "Installing dependencies with apt-get..."
                print_command "sudo apt-get update && sudo apt-get install -y cmake make git build-essential"
                sudo apt-get update && sudo apt-get install -y cmake make git build-essential
            elif command_exists yum; then
                print_info "Installing dependencies with yum..."
                print_command "sudo yum install -y cmake make git gcc-c++"
                sudo yum install -y cmake make git gcc-c++
            elif command_exists dnf; then
                print_info "Installing dependencies with dnf..."
                print_command "sudo dnf install -y cmake make git gcc-c++"
                sudo dnf install -y cmake make git gcc-c++
            elif command_exists pacman; then
                print_info "Installing dependencies with pacman..."
                print_command "sudo pacman -S --noconfirm cmake make git gcc"
                sudo pacman -S --noconfirm cmake make git gcc
            else
                print_error "Unable to detect package manager. Please install: ${missing_deps[*]}"
                exit 1
            fi
            ;;
        "macos")
            if command_exists brew; then
                print_info "Installing dependencies with Homebrew..."
                print_command "brew install cmake git"
                brew install cmake git
                # Xcode command line tools should provide make and clang++
                if ! command_exists make || ! command_exists clang++; then
                    print_info "Installing Xcode command line tools..."
                    xcode-select --install
                fi
            else
                print_error "Homebrew not found. Please install Homebrew or manually install: ${missing_deps[*]}"
                print_info "Install Homebrew: /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                exit 1
            fi
            ;;
        "windows")
            print_error "Windows installation requires manual dependency setup."
            print_info "Please install the following and run this script in a Unix-like environment (WSL, Git Bash, etc.):"
            print_info "- CMake: https://cmake.org/download/"
            print_info "- Git: https://git-scm.com/download/win"
            print_info "- MinGW-w64 or Visual Studio Build Tools"
            exit 1
            ;;
        *)
            print_error "Unsupported operating system. Please manually install: ${missing_deps[*]}"
            exit 1
            ;;
    esac
    
    print_status "Dependencies installed successfully"
}

# Function to get the latest release version
get_latest_version() {
    if command_exists curl; then
        curl -s "$GITHUB_API_URL/releases/latest" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'
    elif command_exists wget; then
        wget -qO- "$GITHUB_API_URL/releases/latest" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'
    else
        echo "master" # fallback to master branch
    fi
}

# Function to download and extract source code
download_source() {
    local version="$1"
    local download_url
    
    print_section "Downloading Source Code"
    
    cd "$TEMP_DIR"
    
    if [ "$version" = "master" ]; then
        download_url="$REPO_URL/archive/refs/heads/master.tar.gz"
        print_info "Downloading latest development version..."
    else
        download_url="$REPO_URL/archive/refs/tags/$version.tar.gz"
        print_info "Downloading version $version..."
    fi
    
    if command_exists curl; then
        curl -fsSL "$download_url" -o cjsh-source.tar.gz
    elif command_exists wget; then
        wget -q "$download_url" -O cjsh-source.tar.gz
    else
        print_error "Neither curl nor wget found. Cannot download source code."
        exit 1
    fi
    
    print_status "Source code downloaded"
    
    # Extract the tarball
    print_info "Extracting source code..."
    tar -xzf cjsh-source.tar.gz
    
    # Find the extracted directory (it might be CJsShell-master or CJsShell-version)
    local extracted_dir=$(find . -maxdepth 1 -type d -name "CJsShell-*" | head -n1)
    if [ -z "$extracted_dir" ]; then
        print_error "Failed to find extracted directory"
        exit 1
    fi
    
    # Move into the source directory
    cd "$extracted_dir"
    print_status "Source code extracted to $PWD"
}

# Function to build the project
build_project() {
    print_section "Building CJ's Shell"
    
    print_info "Configuring build with CMake..."
    mkdir -p build
    cd build
    
    if ! cmake .. -DCMAKE_BUILD_TYPE=Release; then
        print_error "CMake configuration failed"
        exit 1
    fi
    
    print_status "Build configured successfully"
    
    print_info "Compiling project..."
    local cpu_cores=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")
    
    if ! make -j"$cpu_cores"; then
        print_error "Build failed"
        exit 1
    fi
    
    print_status "Build completed successfully"
    
    # Verify the executable was created
    if [ ! -f "cjsh" ]; then
        print_error "Executable 'cjsh' not found after build"
        exit 1
    fi
    
    print_status "Executable 'cjsh' created successfully"
}

# Function to install the binary
install_binary() {
    print_section "Installing CJ's Shell"
    
    local target="$INSTALL_DIR/cjsh"
    
    print_info "Installing to: $target"
    
    # Check if already installed
    if command -v cjsh >/dev/null 2>&1; then
        print_warning "cjsh is already installed at: $(which cjsh)"
        print_info "This installation will overwrite the existing version."
    fi
    
    # Install the binary
    if sudo install -m 755 cjsh "$target" 2>/dev/null; then
        print_status "Binary installed successfully"
    else
        print_error "Failed to install binary to $target"
        print_info "Trying alternative installation method..."
        
        # Alternative method: copy and set permissions
        if sudo cp cjsh "$target" && sudo chmod 755 "$target"; then
            print_status "Binary installed successfully (alternative method)"
        else
            print_error "Installation failed. You may need to run with sudo or check permissions."
            exit 1
        fi
    fi
    
    # Verify installation
    if [ -f "$target" ] && [ -x "$target" ]; then
        print_status "Installation verification successful"
        
        # Test the binary
        if "$target" --version >/dev/null 2>&1; then
            local version=$("$target" --version 2>/dev/null | head -n1)
            print_info "Installed version: $version"
        fi
    else
        print_error "Installation verification failed"
        exit 1
    fi
}

# Function to show post-installation information
show_completion_info() {
    print_section "Installation Complete"
    print_status "CJ's Shell successfully installed!"
    print_status "You can now run: ${CYAN}cjsh${NC}"
    echo
    
    print_section "Next Steps"
    print_info "Test the installation:"
    print_command "cjsh --version"
    echo
    
    print_info "Run the test suite (recommended):"
    print_command "cjsh -c 'echo \"Hello from CJ'\''s Shell!\"'"
    echo
    
    print_section "Setting as Default Shell (Optional)"
    print_warning "cjsh is approximately ${BOLD}90% POSIX compliant${NC}"
    print_warning "Setting as default login shell may have ${BOLD}unintended consequences${NC}"
    echo -e "  ${BRIGHT_RED}${BOLD}⚠  NO WARRANTY PROVIDED ⚠${NC}"
    echo
    
    print_info "To make cjsh your default login shell:"
    echo
    echo -e "  ${BOLD}Step 1:${NC} Add cjsh to system shells list"
    print_command "sudo sh -c 'echo $INSTALL_DIR/cjsh >> /etc/shells'"
    echo
    echo -e "  ${BOLD}Step 2:${NC} Change your default shell"
    print_command "sudo chsh -s $INSTALL_DIR/cjsh \$USER"
    echo
    print_warning "You may need to log out and back in for changes to take effect"
    echo
    
    echo -e "${BRIGHT_GREEN}${BOLD}${CHECK_MARK} Thank you for trying CJ's Shell!${NC}"
    echo -e "  ${BLUE}${INFO}${NC} For more information: ${CYAN}https://github.com/CadenFinley/CJsShell${NC}"
    echo
}

# Main installation function
main() {
    print_header
    
    print_section "Pre-Installation Information"
    print_info "This installer will:"
    print_info "• Check and install build dependencies"
    print_info "• Download the latest CJ's Shell source code"
    print_info "• Build the project from source"
    print_info "• Install cjsh to ${BRIGHT_BLUE}$INSTALL_DIR${NC}"
    print_warning "You may be prompted for your password (sudo required)"
    echo
    
    # Check for basic requirements
    if [ "$EUID" -eq 0 ]; then
        print_warning "Running as root. This is not recommended."
        print_info "Consider running as a regular user (sudo will be used when needed)."
        echo
    fi
    
    # Install dependencies
    install_dependencies
    echo
    
    # Get latest version
    print_section "Fetching Latest Version"
    local version=$(get_latest_version)
    if [ -n "$version" ] && [ "$version" != "master" ]; then
        print_info "Latest version: $version"
    else
        print_info "Using latest development version"
        version="master"
    fi
    echo
    
    # Download source
    download_source "$version"
    echo
    
    # Build project
    build_project
    echo
    
    # Install binary
    install_binary
    echo
    
    # Show completion information
    show_completion_info
}

# Check if script is being piped from curl
if [ -t 0 ]; then
    # Running normally
    main "$@"
else
    # Being piped from curl, run main function
    main "$@"
fi

exit 0