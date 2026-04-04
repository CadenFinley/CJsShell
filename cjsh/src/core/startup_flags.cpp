/*
  startup_flags.cpp

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

#include "startup_flags.h"

#include <algorithm>

namespace startup_flags {

const std::vector<Descriptor>& descriptors() {
    static const std::vector<Descriptor> kDescriptors = {
        {"--login", "Set login mode"},
        {"--interactive", "Force interactive mode"},
        {"--posix", "Enable POSIX mode"},
        {"--no-exec", "Read commands without executing"},
        {"--no-colors", "Disable colors"},
        {"--no-titleline", "Disable title line"},
        {"--show-startup-time", "Display shell startup time"},
        {"--no-source", "Skip sourcing configuration files"},
        {"--no-completions", "Disable tab completions"},
        {"--no-completion-learning", "Skip on-demand completion scraping"},
        {"--no-smart-cd", "Disable smart cd auto-jumps"},
        {"--no-script-extension-interpreter", "Disable extension-based script runners"},
        {"--no-syntax-highlighting", "Disable syntax highlighting"},
        {"--no-error-suggestions", "Disable error suggestions"},
        {"--no-prompt-vars", "Ignore PS1/PS2 prompt variables"},
        {"--no-history", "Disable history recording"},
        {"--no-history-expansion", "Disable history expansion"},
        {"--no-sh-warning", "Suppress the sh invocation warning"},
        {"--minimal", "Disable cjsh extras"},
        {"--secure", "Enable secure mode"},
        {"--startup-test", "Enable startup test mode"},
    };
    return kDescriptors;
}

bool is_supported(const std::string& flag) {
    const auto& all_flags = descriptors();
    return std::any_of(all_flags.begin(), all_flags.end(),
                       [&](const Descriptor& descriptor) { return flag == descriptor.name; });
}

}  // namespace startup_flags
