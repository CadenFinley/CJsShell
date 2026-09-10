#!/usr/bin/env python3

# extract-release-notes.py
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

"""Print a tagged release's Keep a Changelog entry for use as GitHub release notes."""

import argparse
from datetime import date
from pathlib import Path
import re


def extract_release_notes(changelog: str, tag: str) -> str:
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag):
        raise ValueError("release tag must use the stable version format vX.Y.Z")

    version = tag[1:]
    headings = list(re.finditer(r"^## .+$", changelog, re.MULTILINE))
    matches = [
        (index, heading)
        for index, heading in enumerate(headings)
        if heading.group().startswith(f"## [{version}]")
    ]
    if len(matches) != 1:
        raise ValueError(f"expected one changelog entry for {tag}, found {len(matches)}")

    index, heading = matches[0]
    dated_heading = re.fullmatch(
        rf"## \[{re.escape(version)}\] - (\d{{4}}-\d{{2}}-\d{{2}})(?: \[YANKED\])?",
        heading.group(),
    )
    if dated_heading is None:
        raise ValueError(f"changelog entry for {tag} must include a YYYY-MM-DD date")
    date.fromisoformat(dated_heading.group(1))

    end = headings[index + 1].start() if index + 1 < len(headings) else len(changelog)
    definition_pattern = re.compile(r"^\[([^\]]+)\]:[ \t]+(\S+)[ \t]*$", re.MULTILINE)
    definitions = dict(definition_pattern.findall(changelog))
    section = definition_pattern.sub("", changelog[heading.start():end]).strip()
    if not re.search(r"^- \S", section, re.MULTILINE):
        raise ValueError(f"changelog entry for {tag} has no changes")
    if version not in definitions:
        raise ValueError(f"changelog entry for {tag} has no version link")

    used_definitions = [
        f"[{label}]: {url}"
        for label, url in definitions.items()
        if f"[{label}]" in section
    ]
    return (
        f"{section}\n\n**Full Changelog**: {definitions[version]}\n\n"
        + "\n".join(used_definitions)
        + "\n"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag", help="stable release tag (vX.Y.Z)")
    parser.add_argument("--changelog", type=Path, default=Path("CHANGELOG.md"))
    args = parser.parse_args()
    try:
        notes = extract_release_notes(args.changelog.read_text(encoding="utf-8"), args.tag)
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    print(notes, end="")


if __name__ == "__main__":
    main()
