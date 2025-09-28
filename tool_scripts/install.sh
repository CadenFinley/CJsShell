#!/usr/bin/env bash

# this can be edited and pushed with a version forward or new release

set -euo pipefail

REPO_OWNER="CadenFinley"
REPO_NAME="CJsShell"

# Styling helpers (no-op when not a TTY)
if [ -t 1 ]; then
	bold=$'\033[1m'
	reset=$'\033[0m'
	accent=$'\033[38;5;214m'
	accent_alt=$'\033[38;5;45m'
	muted=$'\033[38;5;244m'
else
	bold=""
	reset=""
	accent=""
	accent_alt=""
	muted=""
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

# Check for tar
if ! command -v tar >/dev/null 2>&1; then
	echo "Error: tar is required to extract the release archive" >&2
	exit 1
fi

printf "\n${accent}%s${reset}\n" "┌────────────────────────────────────┐"
printf "${accent}%s${reset}\n" "│         ${bold}CJsShell Installer${reset}${accent}         │"
printf "${accent}%s${reset}\n" "├────────────────────────────────────┤"
printf "${accent}%s${reset}\n" "│  Caden Finley - cadenfinley.com    │"
printf "${accent}%s${reset}\n\n" "└────────────────────────────────────┘"
printf "${muted}Hang tight cjsh might take a second to build...${reset}\n\n"

# Create temporary directory
TEMP_DIR="$(mktemp -d)"
cleanup() {
	rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT

get_latest_release() {
	# Use GitHub API to get the latest release info
	local api_url="https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases/latest"
	local response
	
	if [ "$HTTP_CLIENT" = "curl" ]; then
		response=$(curl -fsSL "$api_url" 2>/dev/null)
	else
		response=$(wget -qO- "$api_url" 2>/dev/null)
	fi
	
	if [ $? -eq 0 ] && [ -n "$response" ]; then
		# Extract tag name and tarball URL
		local tag_name=$(echo "$response" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
		local tarball_url=$(echo "$response" | grep '"tarball_url":' | sed -E 's/.*"([^"]+)".*/\1/')
		
		if [ -n "$tag_name" ] && [ -n "$tarball_url" ]; then
			printf '%s\n%s' "$tag_name" "$tarball_url"
			return 0
		fi
	fi
	
	# Fallback: construct tarball URL from latest tag
	echo "Warning: Could not get release info from API, trying fallback..." >&2
	printf 'latest\nhttps://github.com/%s/%s/archive/refs/heads/main.tar.gz' "$REPO_OWNER" "$REPO_NAME"
}

echo "Getting latest release information..."
RELEASE_INFO=$(get_latest_release)
LATEST_TAG=$(echo "$RELEASE_INFO" | head -n1)
TARBALL_URL=$(echo "$RELEASE_INFO" | tail -n1)

echo "Latest release: $LATEST_TAG"
echo "Downloading CJsShell release..."

# Download and extract the release
cd "$TEMP_DIR"
ARCHIVE_NAME="cjsshell.tar.gz"

if [ "$HTTP_CLIENT" = "curl" ]; then
	curl -fsSL -o "$ARCHIVE_NAME" "$TARBALL_URL"
else
	wget -q -O "$ARCHIVE_NAME" "$TARBALL_URL"
fi

echo "Extracting release..."
tar -xzf "$ARCHIVE_NAME"

# Find the extracted directory (GitHub tarballs create a directory with repo name and commit hash)
EXTRACTED_DIR=$(find . -maxdepth 1 -type d -name "${REPO_OWNER}-${REPO_NAME}-*" | head -n1)
if [ -z "$EXTRACTED_DIR" ]; then
	echo "Error: Could not find extracted directory" >&2
	exit 1
fi

cd "$EXTRACTED_DIR"

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

echo
echo "Thank you for installing CJsShell!"
echo "You can run it by typing 'cjsh' in your terminal."
echo "For more information, visit: https://github.com/CadenFinley/CJsShell"
