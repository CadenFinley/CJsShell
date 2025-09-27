#!/usr/bin/env bash

set -euo pipefail

REPO_OWNER="CadenFinley"
REPO_NAME="CJsShell"
ASSET_NAME="build.sh"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_PATH="${SCRIPT_DIR}/${ASSET_NAME}"

if ! command -v curl >/dev/null 2>&1; then
	echo "Error: curl is required to install ${ASSET_NAME}" >&2
	exit 1
fi

TEMP_FILE="$(mktemp)"
cleanup() {
	rm -f "${TEMP_FILE}"
}
trap cleanup EXIT

DOWNLOAD_URL="https://github.com/${REPO_OWNER}/${REPO_NAME}/releases/latest/download/${ASSET_NAME}"

echo "Downloading ${ASSET_NAME} from latest release..."
curl -fsSL "${DOWNLOAD_URL}" -o "${TEMP_FILE}"

mkdir -p "${SCRIPT_DIR}"
mv "${TEMP_FILE}" "${TARGET_PATH}"
chmod +x "${TARGET_PATH}"

echo "Running ${ASSET_NAME}..."
"${TARGET_PATH}" "$@"
