/*
  firstboot_command.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "firstboot_command.h"

#include "builtin_help.h"

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <filesystem>
#include <system_error>

#include "cjsh_filesystem.h"
#include "error_out.h"

int firstboot_command(const std::vector<std::string>& args) {
    constexpr const char* kUsage = "Usage: firstboot";
    if (builtin_handle_help(
            args, {kUsage, "Create the first-boot marker and suppress the welcome banner.",
                   "This command succeeds only while the marker does not exist."})) {
        return 0;
    }

    if (args.size() != 1) {
        print_error({ErrorType::INVALID_ARGUMENT, "firstboot", "unexpected argument", {kUsage}});
        return 2;
    }

    const auto& marker_path = cjsh_filesystem::g_cjsh_first_boot_path();
    std::error_code directory_error;
    (void)std::filesystem::create_directories(marker_path.parent_path(), directory_error);
    if (directory_error) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "firstboot",
                     "failed to prepare first-boot directory: " + directory_error.message(),
                     {}});
        return 1;
    }

    const int fd = ::open(marker_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        if (errno == EEXIST) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "firstboot",
                         "first-boot marker already exists",
                         {"The first-boot banner has already been suppressed."}});
        } else {
            print_error_errno(
                {ErrorType::RUNTIME_ERROR, "firstboot", "failed to create first-boot marker", {}});
        }
        return 1;
    }

    if (::close(fd) != 0) {
        print_error_errno(
            {ErrorType::RUNTIME_ERROR, "firstboot", "failed to finalize first-boot marker", {}});
        return 1;
    }

    return 0;
}
