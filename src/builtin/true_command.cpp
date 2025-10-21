// true - do nothing, successfully
// Copyright (C) 1999-2025 Free Software Foundation, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "true_command.h"
#include <iostream>

int true_command(const std::vector<std::string>& args) {
    if (args.size() == 2 && args[1] == "--help") {
        std::cout << "Usage: true [ignored command line arguments]\n";
        std::cout << "Exit with a status code indicating success.\n";
        std::cout << "\n";
        std::cout << "  --help     display this help and exit\n";
        std::cout << "\n";
        std::cout
            << "NOTE: your shell may have its own version of true, which usually supersedes\n";
        std::cout << "the version described here.  Please refer to your shell's documentation\n";
        std::cout << "for details about the options it supports.\n";
        return 0;
    }

    if (args.size() == 2 && args[1] == "--version") {
        std::cout << "true (CJsShell coreutils)\n";
        return 0;
    }

    // Always return success, ignore all arguments
    return 0;
}
