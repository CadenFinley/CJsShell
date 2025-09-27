#!/usr/bin/env bash

# CJ's Shell Installation Script
# Usage: curl -fsSL https://raw.githubusercontent.com/CadenFinley/CJsShell/master/tool_scripts/install.sh | bash

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
REPO_URL="https://github.com/CadenFinley/CJsShell.git"
PROJECT_NAME="cjsh"
INSTALL_DIR="${HOME}/.local/bin"
TEMP_DIR="/tmp/cjsh-install-$$"
VERSION=""
RELEASE_TAG=""

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect OS
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "freebsd"* ]]; then
        echo "freebsd"
    else
        echo "unknown"
    fi
}

# Function to check system requirements
check_requirements() {
    log_info "Checking system requirements..."
    
    local missing_deps=()
    local os=$(detect_os)
    
    # Check for required tools
    if ! command_exists git; then
        missing_deps+="git"
    fi
    
    if ! command_exists make; then
        missing_deps+="make"
    fi
    
    # Check for C++ compiler
    if ! command_exists g++ && ! command_exists clang++; then
        missing_deps+="C++ compiler (g++ or clang++)"
    fi
    
    # Check for C compiler  
    if ! command_exists gcc && ! command_exists clang; then
        missing_deps+="C compiler (gcc or clang)"
    fi
    
    # Check for download tools
    if ! command_exists curl && ! command_exists wget; then
        missing_deps+="curl or wget"
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing required dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo
        log_info "Please install the missing dependencies and try again."
        
        # Provide OS-specific installation instructions
        case $os in
            "macos")
                log_info "On macOS, you can install dependencies using:"
                log_info "  brew install git make"
                log_info "  xcode-select --install  # For compilers"
                ;;
            "linux")
                if command_exists apt; then
                    log_info "On Ubuntu/Debian, you can install dependencies using:"
                    log_info "  sudo apt update && sudo apt install git build-essential curl"
                elif command_exists yum; then
                    log_info "On RHEL/CentOS, you can install dependencies using:"
                    log_info "  sudo yum install git gcc gcc-c++ make curl"
                elif command_exists dnf; then
                    log_info "On Fedora, you can install dependencies using:"
                    log_info "  sudo dnf install git gcc gcc-c++ make curl"
                elif command_exists pacman; then
                    log_info "On Arch Linux, you can install dependencies using:"
                    log_info "  sudo pacman -S git base-devel curl"
                fi
                ;;
        esac
        
        exit 1
    fi
    
    log_success "All required dependencies are available"
}

# Function to create temporary directory
create_temp_dir() {
    log_info "Creating temporary build directory..."
    mkdir -p "$TEMP_DIR"
    cd "$TEMP_DIR"
}

# Function to clone repository
clone_repository() {
    if [[ -z "$RELEASE_TAG" ]]; then
        log_error "Release tag not set before cloning"
        exit 1
    fi

    log_info "Cloning CJ's Shell $RELEASE_TAG..."
    
    if ! git clone --depth 1 --branch "$RELEASE_TAG" "$REPO_URL" cjsh; then
        log_error "Failed to clone repository"
        exit 1
    fi
    
    cd cjsh
    log_success "Repository cloned successfully"
}

# Function to fetch the latest release tag from GitHub
fetch_latest_release() {
    log_info "Determining latest CJ's Shell release..."

    local api_url="https://api.github.com/repos/CadenFinley/CJsShell/releases/latest"
    local release_data=""

    if command_exists curl; then
        release_data=$(curl -fsSL "$api_url" || true)
    fi

    if [[ -z "$release_data" ]] && command_exists wget; then
        release_data=$(wget -qO- "$api_url" || true)
    fi

    if [[ -n "$release_data" ]]; then
        local tag

        tag=$(printf '%s' "$release_data" | grep -o '"tag_name":[[:space:]]*"[^"]*"' | head -n1 | sed 's/.*"tag_name":[[:space:]]*"//; s/"$//')

        if [[ -n "$tag" ]]; then
            RELEASE_TAG="$tag"
            VERSION="$tag"
            log_success "Latest release detected: ${VERSION}"
            return
        fi
    fi

    log_error "Unable to determine the latest release of CJ's Shell."
    log_info "Ensure releases are published on GitHub and try again."
    exit 1
}

# Function to build the project
build_project() {
    log_info "Building CJ's Shell..."
    
    # Run the build script
    log_info "Running build script (this may take a few minutes)..."
    if ! ./tool_scripts/build.sh; then
        log_error "Build failed"
        exit 1
    fi
    
    log_success "Build completed successfully"
}

# Function to install the binary
install_binary() {
    log_info "Installing CJ's Shell to $INSTALL_DIR..."
    
    # Create install directory if it doesn't exist
    mkdir -p "$INSTALL_DIR"
    
    # Copy the binary
    if ! cp "build/$PROJECT_NAME" "$INSTALL_DIR/$PROJECT_NAME"; then
        log_error "Failed to copy binary to $INSTALL_DIR"
        exit 1
    fi
    
    # Make it executable
    chmod +x "$INSTALL_DIR/$PROJECT_NAME"
    
    log_success "CJ's Shell installed to $INSTALL_DIR/$PROJECT_NAME"
}

# Function to setup PATH
setup_path() {
    local shell_rc=""
    local current_shell=$(basename "$SHELL")
    
    # Determine which shell config file to update
    case $current_shell in
        "bash")
            shell_rc="$HOME/.bashrc"
            ;;
        "zsh")
            shell_rc="$HOME/.zshrc"
            ;;
        "fish")
            shell_rc="$HOME/.config/fish/config.fish"
            ;;
        *)
            shell_rc="$HOME/.profile"
            ;;
    esac
    
    # Check if ~/.local/bin is already in PATH
    if [[ ":$PATH:" == *":$HOME/.local/bin:"* ]]; then
        log_info "~/.local/bin is already in your PATH"
        return
    fi
    
    log_info "To add ~/.local/bin to your PATH, add the following line to $shell_rc:"

    if [[ $current_shell == "fish" ]]; then
        echo "  set -gx PATH \$HOME/.local/bin \$PATH"
    else
        echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
    fi

    log_warning "After updating $shell_rc, restart your shell or run 'source $shell_rc' to apply the changes"
}

# Function to cleanup
cleanup() {
    log_info "Cleaning up temporary files..."
    cd /
    rm -rf "$TEMP_DIR"
    log_success "Cleanup completed"
}

# Function to verify installation
verify_installation() {
    log_info "Verifying installation..."
    
    if [[ ":$PATH:" == *":$HOME/.local/bin:"* ]] && command_exists "$PROJECT_NAME"; then
        log_success "CJ's Shell is installed and available in PATH"
        log_info "You can now run '$PROJECT_NAME' to start the shell"
    elif [ -x "$INSTALL_DIR/$PROJECT_NAME" ]; then
        log_success "CJ's Shell is installed at $INSTALL_DIR/$PROJECT_NAME"
        log_info "You can run it with: $INSTALL_DIR/$PROJECT_NAME"
        log_warning "Or add ~/.local/bin to your PATH and run '$PROJECT_NAME'"
    else
        log_error "Installation verification failed"
        exit 1
    fi
}

# Function to display completion message
display_completion() {
    echo
    echo "=================================="
    log_success "CJ's Shell Installation Complete!"
    echo "=================================="
    echo
    log_info "Version: $VERSION"
    log_info "Installed to: $INSTALL_DIR/$PROJECT_NAME"
    echo
    log_info "To get started:"
    if [[ ":$PATH:" == *":$HOME/.local/bin:"* ]]; then
        log_info "  $PROJECT_NAME"
    else
        log_info "  $INSTALL_DIR/$PROJECT_NAME"
        log_info "  # OR add ~/.local/bin to your PATH and run: $PROJECT_NAME"
    fi
    echo
    log_info "For help and documentation, visit:"
    log_info "  https://github.com/CadenFinley/CJsShell"
    echo
}

# Main installation function
main() {
    log_info "Starting CJ's Shell installation..."
    echo
    
    # Trap to ensure cleanup happens
    trap cleanup EXIT
    
    check_requirements
    fetch_latest_release
    create_temp_dir
    clone_repository
    build_project
    install_binary
    setup_path
    verify_installation
    display_completion
}

# Run main function
main "$@"
