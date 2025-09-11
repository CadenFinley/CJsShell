#!/bin/bash
set -e

# CJsShell Auto-Installer - Downloads and installs the appropriate binary
# Usage: curl -sSL https://raw.githubusercontent.com/CadenFinley/CJsShell/master/tool-scripts/install-from-release.sh | bash

# Colors for output
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly RED='\033[0;31m'
readonly BLUE='\033[0;34m'
readonly CYAN='\033[0;36m'
readonly NC='\033[0m'
readonly BOLD='\033[1m'

# Symbols
readonly CHECK_MARK="✓"
readonly CROSS_MARK="✗"
readonly INFO="ℹ"

print_status() {
    echo -e "  ${GREEN}${CHECK_MARK}${NC} $1"
}

print_info() {
    echo -e "  ${BLUE}${INFO}${NC} $1"
}

print_error() {
    echo -e "  ${RED}${CROSS_MARK}${NC} $1"
}

print_warning() {
    echo -e "  ${YELLOW}⚠${NC} $1"
}

# Detect platform and architecture
detect_platform() {
    local os=""
    local arch=""
    local variant=""
    
    # Detect OS
    case "$(uname -s)" in
        Darwin)
            os="macos"
            ;;
        Linux)
            os="linux"
            # Detect Linux variant for better compatibility
            if command -v lsb_release >/dev/null 2>&1; then
                local distro=$(lsb_release -si 2>/dev/null | tr '[:upper:]' '[:lower:]')
                local version=$(lsb_release -sr 2>/dev/null | cut -d. -f1)
                
                case "$distro" in
                    ubuntu)
                        if [ "$version" -ge 24 ]; then
                            variant="ubuntu24"
                        elif [ "$version" -ge 22 ]; then
                            variant="ubuntu22"
                        fi
                        ;;
                esac
            elif [ -f /etc/alpine-release ]; then
                variant="static"  # Use static build for Alpine
            elif [ -f /etc/centos-release ] || [ -f /etc/redhat-release ]; then
                variant="static"  # Use static build for older RHEL/CentOS
            fi
            ;;
        *)
            print_error "Unsupported operating system: $(uname -s)"
            print_error "CJsShell only supports macOS and Linux"
            exit 1
            ;;
    esac
    
    # Detect architecture
    case "$(uname -m)" in
        x86_64|amd64)
            arch="x86_64"
            ;;
        arm64|aarch64)
            arch="aarch64"
            if [ "$os" = "macos" ]; then
                arch="arm64"  # macOS uses arm64 naming
            fi
            ;;
        armv7l|armv6l)
            print_error "32-bit ARM is not supported"
            exit 1
            ;;
        i386|i686)
            print_error "32-bit x86 is not supported"
            exit 1
            ;;
        *)
            print_error "Unsupported architecture: $(uname -m)"
            exit 1
            ;;
    esac
    
    # Build binary name
    local binary_name="cjsh-${os}-${arch}"
    if [ -n "$variant" ]; then
        binary_name="${binary_name}-${variant}"
    fi
    
    echo "$binary_name"
}

# Get latest release version from GitHub API
get_latest_version() {
    local api_url="https://api.github.com/repos/CadenFinley/CJsShell/releases/latest"
    
    if command -v curl >/dev/null 2>&1; then
        curl -s "$api_url" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'
    elif command -v wget >/dev/null 2>&1; then
        wget -qO- "$api_url" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'
    else
        print_error "Neither curl nor wget is available"
        exit 1
    fi
}

# Download and verify binary
download_binary() {
    local binary_name="$1"
    local version="$2"
    local download_url="https://github.com/CadenFinley/CJsShell/releases/download/${version}/${binary_name}.tar.gz"
    local checksum_url="https://github.com/CadenFinley/CJsShell/releases/download/${version}/${binary_name}.tar.gz.sha256"
    
    print_info "Downloading ${binary_name}.tar.gz..."
    
    # Download binary
    if command -v curl >/dev/null 2>&1; then
        if ! curl -L -o "${binary_name}.tar.gz" "$download_url"; then
            print_error "Failed to download binary"
            return 1
        fi
        # Download checksum
        curl -L -o "${binary_name}.tar.gz.sha256" "$checksum_url" 2>/dev/null || true
    elif command -v wget >/dev/null 2>&1; then
        if ! wget -O "${binary_name}.tar.gz" "$download_url"; then
            print_error "Failed to download binary"
            return 1
        fi
        # Download checksum
        wget -O "${binary_name}.tar.gz.sha256" "$checksum_url" 2>/dev/null || true
    fi
    
    # Verify checksum if available
    if [ -f "${binary_name}.tar.gz.sha256" ]; then
        print_info "Verifying checksum..."
        if command -v shasum >/dev/null 2>&1; then
            if shasum -a 256 -c "${binary_name}.tar.gz.sha256" >/dev/null 2>&1; then
                print_status "Checksum verification passed"
            else
                print_warning "Checksum verification failed, but continuing..."
            fi
        elif command -v sha256sum >/dev/null 2>&1; then
            if sha256sum -c "${binary_name}.tar.gz.sha256" >/dev/null 2>&1; then
                print_status "Checksum verification passed"
            else
                print_warning "Checksum verification failed, but continuing..."
            fi
        fi
    fi
    
    # Extract binary
    print_info "Extracting binary..."
    if ! tar -xzf "${binary_name}.tar.gz"; then
        print_error "Failed to extract binary"
        return 1
    fi
    
    # Clean up
    rm -f "${binary_name}.tar.gz" "${binary_name}.tar.gz.sha256"
    
    return 0
}

# Install binary to system
install_binary() {
    local binary_path="$1"
    local install_dir="/usr/local/bin"
    local target="$install_dir/cjsh"
    
    # Check if we need sudo
    if [ ! -w "$install_dir" ]; then
        print_info "Installing to $target (requires sudo)..."
        if ! sudo install -m 755 "$binary_path" "$target"; then
            print_error "Failed to install binary"
            return 1
        fi
    else
        print_info "Installing to $target..."
        if ! install -m 755 "$binary_path" "$target"; then
            print_error "Failed to install binary"
            return 1
        fi
    fi
    
    print_status "Binary installed successfully"
    return 0
}

# Main installation function
main() {
    echo
    echo -e "${CYAN}${BOLD}CJsShell Auto-Installer${NC}"
    echo -e "${CYAN}========================${NC}"
    echo
    
    # Check dependencies
    if ! command -v tar >/dev/null 2>&1; then
        print_error "tar is required but not installed"
        exit 1
    fi
    
    if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
        print_error "Either curl or wget is required"
        exit 1
    fi
    
    # Detect platform
    print_info "Detecting platform..."
    local binary_name=$(detect_platform)
    print_status "Detected platform: $binary_name"
    
    # Get latest version
    print_info "Fetching latest version..."
    local version=$(get_latest_version)
    if [ -z "$version" ]; then
        print_error "Failed to get latest version"
        exit 1
    fi
    print_status "Latest version: $version"
    
    # Create temporary directory
    local temp_dir=$(mktemp -d)
    cd "$temp_dir"
    
    # Download binary
    if ! download_binary "$binary_name" "$version"; then
        # Fallback strategies
        print_warning "Primary download failed, trying fallbacks..."
        
        case "$binary_name" in
            cjsh-linux-*-ubuntu24)
                binary_name="cjsh-linux-x86_64-ubuntu22"
                print_info "Trying Ubuntu 22.04 build..."
                ;;
            cjsh-linux-*-ubuntu22)
                binary_name="cjsh-linux-x86_64"
                print_info "Trying Ubuntu 20.04 build..."
                ;;
            cjsh-linux-x86_64)
                binary_name="cjsh-linux-x86_64-static"
                print_info "Trying static build..."
                ;;
            cjsh-macos-arm64)
                binary_name="cjsh-macos-universal"
                print_info "Trying universal macOS build..."
                ;;
        esac
        
        if ! download_binary "$binary_name" "$version"; then
            print_error "All download attempts failed"
            cd / && rm -rf "$temp_dir"
            exit 1
        fi
    fi
    
    # Find the extracted binary
    local binary_file
    if [ -f "cjsh" ]; then
        binary_file="cjsh"
    elif [ -f "cjsh-universal" ]; then
        binary_file="cjsh-universal"
    else
        # Look for any executable file
        binary_file=$(find . -type f -executable | head -n1)
        if [ -z "$binary_file" ]; then
            print_error "No executable binary found"
            cd / && rm -rf "$temp_dir"
            exit 1
        fi
    fi
    
    # Test the binary
    print_info "Testing binary..."
    if ! ./"$binary_file" --version >/dev/null 2>&1; then
        print_warning "Binary test failed, but continuing with installation..."
    else
        print_status "Binary test passed"
    fi
    
    # Install binary
    if ! install_binary "$binary_file"; then
        cd / && rm -rf "$temp_dir"
        exit 1
    fi
    
    # Clean up
    cd / && rm -rf "$temp_dir"
    
    # Verify installation
    if command -v cjsh >/dev/null 2>&1; then
        local installed_version=$(cjsh --version 2>/dev/null | head -n1 || echo "unknown")
        print_status "Installation completed successfully!"
        print_info "Installed version: $installed_version"
        print_info "You can now run: ${CYAN}cjsh${NC}"
        
        echo
        echo -e "${BLUE}Next steps:${NC}"
        echo "1. Run 'cjsh' to start using your new shell"
        echo "2. To make cjsh your default shell:"
        echo "   sudo sh -c 'echo /usr/local/bin/cjsh >> /etc/shells'"
        echo "   chsh -s /usr/local/bin/cjsh"
        echo
    else
        print_error "Installation verification failed"
        exit 1
    fi
}

# Run main function
main "$@"