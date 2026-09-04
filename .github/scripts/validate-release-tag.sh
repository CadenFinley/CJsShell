#!/usr/bin/env sh

# validate-release-tag.sh
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 <vX.Y.Z> [--allow-untagged]" >&2
    exit 2
fi

release_tag=$1
allow_untagged=false
if [ "$#" -eq 2 ]; then
    if [ "$2" != "--allow-untagged" ]; then
        echo "unknown option: $2" >&2
        exit 2
    fi
    allow_untagged=true
fi

if ! printf '%s\n' "$release_tag" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "release tag must use the stable semantic version format vX.Y.Z: $release_tag" >&2
    exit 1
fi

project_version=$(sed -nE \
    's/^[[:space:]]*project\(cjsh VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES.*$/\1/p' \
    CMakeLists.txt)

if [ -z "$project_version" ]; then
    echo "could not determine the cjsh project version from CMakeLists.txt" >&2
    exit 1
fi

if [ "v$project_version" != "$release_tag" ]; then
    echo "release tag $release_tag does not match CMake project version v$project_version" >&2
    exit 1
fi

if [ "$allow_untagged" = false ] && command -v git >/dev/null 2>&1 &&
    git rev-parse --git-dir >/dev/null 2>&1; then
    if ! git tag --points-at HEAD | grep -Fqx "$release_tag"; then
        echo "release tag $release_tag does not point at the checked-out commit" >&2
        exit 1
    fi
fi

printf '%s\n' "$project_version"
