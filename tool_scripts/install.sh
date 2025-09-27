#!/usr/bin/env bash

# this can be edited and pushed with a version forward or new release

set -euo pipefail

REPO_OWNER="CadenFinley"
REPO_NAME="CJsShell"

# Check required commands
if ! command -v git >/dev/null 2>&1; then
	echo "Error: git is required to install CJsShell" >&2
	exit 1
fi

# Check for HTTP client (curl or wget)
if command -v curl >/dev/null 2>&1; then
	HTTP_CLIENT="curl"
elif command -v wget >/dev/null 2>&1; then
	HTTP_CLIENT="wget"
else
	echo "Error: Either curl or wget is required to install CJsShell" >&2
	exit 1
fi

# Create temporary directory
TEMP_DIR="$(mktemp -d)"
cleanup() {
	rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT

echo "Getting latest release information..."
if [ "$HTTP_CLIENT" = "curl" ]; then
	LATEST_TAG=$(curl -fsSL "https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases/latest" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
else
	LATEST_TAG=$(wget -qO- "https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases/latest" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
fi

if [ -z "$LATEST_TAG" ]; then
	echo "Error: Could not determine latest release tag" >&2
	exit 1
fi

echo "Latest release: $LATEST_TAG"
echo "Cloning CJsShell..."

# Clone the specific release tag
cd "$TEMP_DIR"
git clone --depth 1 --branch "$LATEST_TAG" "https://github.com/${REPO_OWNER}/${REPO_NAME}.git" cjsshell

cd cjsshell

echo "Building CJsShell..."
./tool_scripts/build.sh "$@"

# Install for current user only (no sudo)
INSTALL_DIR="$HOME/.local/bin"
BIN_PATH="build/cjsh"

echo "Installing CJsShell to $INSTALL_DIR..."
mkdir -p "$INSTALL_DIR"
cp "$BIN_PATH" "$INSTALL_DIR/cjsh"
chmod +x "$INSTALL_DIR/cjsh"

# Check if ~/.local/bin is in PATH
case ":$PATH:" in
    *":$INSTALL_DIR:"*)
        ;;
    *)
        echo "\nWarning: $INSTALL_DIR is not in your PATH. Add the following line to your shell profile (e.g., ~/.bashrc, ~/.zshrc):"
        echo "export PATH=\"$INSTALL_DIR:\$PATH\""
        ;;
esac

echo "Thank you for installing CJsShell!"
echo "You can run it by typing 'cjsh' in your terminal."
echo "For more information, visit: https://github.com/CadenFinley/CJsShell"
