#!/bin/bash
set -e

# Script directory and setup
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

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

# Function to print a decorative header
print_header() {
    echo
    echo -e "${BRIGHT_BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BRIGHT_BLUE}║                                                            ║${NC}"
    echo -e "${BRIGHT_BLUE}║              ${WHITE}${BOLD}CJ's Shell (cjsh) Installer${NC}${BRIGHT_BLUE}                   ║${NC}"
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

# Function to animate a spinner for operations
spinner() {
    local pid=$1
    local delay=0.1
    local spinstr='|/-\'
    printf "  "
    while [ "$(ps a | awk '{print $1}' | grep $pid)" ]; do
        local temp=${spinstr#?}
        printf "${BLUE}[%c]${NC}" "$spinstr"
        local spinstr=$temp${spinstr%"$temp"}
        sleep $delay
        printf "\b\b\b"
    done
    printf "   \b\b\b"
}

# Main installation function
main() {
    print_header
    
    print_section "Pre-Installation Information"
    print_info "This installer will copy cjsh to ${BRIGHT_BLUE}/usr/local/bin${NC}"
    print_info "You may be prompted for your password (sudo required)"
    echo
    
    print_section "Important Warnings"
    print_warning "cjsh is approximately ${BOLD}90% POSIX compliant${NC}"
    print_warning "Setting as default login shell may have ${BOLD}unintended consequences${NC}"
    echo -e "  ${BRIGHT_RED}${BOLD}⚠  NO WARRANTY PROVIDED ⚠${NC}"
    echo
    
    print_section "Testing Recommendation"
    print_info "Before making cjsh your default shell, test it thoroughly:"
    print_command "./tests/run_shell_tests.sh"
    echo
    
    # Verify executable exists
    local executable_path="$SCRIPT_DIR/../build/cjsh"
    print_section "Verification"
    
    if [ ! -f "$executable_path" ]; then
        print_error "Executable not found at: ${executable_path}"
        echo
        echo -e "${RED}${BOLD}Installation aborted.${NC} Please build cjsh first by running:"
        print_command "cmake -B build && cmake --build build"
        echo
        exit 1
    fi
    
    print_status "Found cjsh executable"
    
    # Check if already installed
    if command -v cjsh >/dev/null 2>&1; then
        print_warning "cjsh is already installed at: $(which cjsh)"
        echo -e "  ${YELLOW}This installation will overwrite the existing version.${NC}"
        echo
    fi
    
    # Installation
    local install_dir="/usr/local/bin"
    local target="$install_dir/cjsh"
    
    print_section "Installation Process"
    echo -e "  ${BLUE}${ARROW}${NC} Installing to: ${BRIGHT_BLUE}${target}${NC}"
    
    # Copy executable with progress indication
    if sudo cp "$executable_path" "$target" 2>/dev/null; then
        print_status "Binary copied successfully"
    else
        print_error "Failed to copy binary"
        exit 1
    fi
    
    # Set permissions
    if sudo chmod +x "$target" 2>/dev/null; then
        print_status "Executable permissions set"
    else
        print_error "Failed to set permissions"
        exit 1
    fi
    
    # Verify installation
    if [ -f "$target" ] && [ -x "$target" ]; then
        echo
        print_section "Installation Complete"
        print_status "cjsh successfully installed to ${BRIGHT_GREEN}${install_dir}${NC}"
        print_status "You can now run: ${CYAN}cjsh${NC}"
        
        # Version check if possible
        if "$target" --version >/dev/null 2>&1; then
            local version=$("$target" --version 2>/dev/null | head -n1)
            print_info "Installed version: ${version}"
        fi
        
        echo
        print_section "Setting as Default Shell (Optional)"
        print_info "To make cjsh your default login shell:"
        echo
        echo -e "  ${BOLD}Step 1:${NC} Add cjsh to system shells list"
        print_command "sudo sh -c 'echo /usr/local/bin/cjsh >> /etc/shells'"
        echo
        echo -e "  ${BOLD}Step 2:${NC} Change your default shell"
        print_command "sudo chsh -s /usr/local/bin/cjsh"
        echo
        print_warning "You may need to log out and back in for changes to take effect"
        echo
        
        echo -e "${BRIGHT_GREEN}${BOLD}${CHECK_MARK} Installation completed successfully!${NC}"
        echo
    else
        print_error "Installation verification failed"
        exit 1
    fi
}

# Run main function
main

exit 0
