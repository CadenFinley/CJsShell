/*
  completion_context.h

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

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace completion_context {

struct Word {
    std::string text;
    std::string raw;
    std::size_t begin{0};
    std::size_t end{0};
    bool quoted{false};
    bool assignment{false};
};

struct CommandLineContext {
    std::size_t cursor{0};
    std::size_t segment_start{0};
    std::string segment_prefix;
    std::vector<Word> words;

    // Tokens beginning with the command that completion should describe. Leading
    // assignments and transparent wrappers (sudo, env, command) are omitted.
    std::vector<std::string> effective_tokens;

    std::string current_prefix;
    std::string current_raw_prefix;
    bool at_word_boundary{true};
    bool cursor_in_command_position{true};
    bool cursor_in_wrapper_option_value{false};
    bool cursor_in_assignment_lhs{false};
    bool cursor_before_existing_word{false};
};

CommandLineContext parse(const std::string& input, std::size_t cursor = std::string::npos);

}  // namespace completion_context
