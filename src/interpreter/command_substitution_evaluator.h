/*
  command_substitution_evaluator.h

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

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "function_ref.h"

class CommandSubstitutionEvaluator {
   public:
    struct ExpansionResult {
        std::string text;
        std::vector<std::string> outputs;
        std::vector<int> exit_codes;
    };
    using CommandExecutor = std::function<std::pair<std::string, int>(const std::string&)>;
    explicit CommandSubstitutionEvaluator(CommandExecutor executor);
    ExpansionResult expand_substitutions(const std::string& input);
    std::pair<std::string, int> capture_command_output(const std::string& command);

    static std::optional<size_t> find_matching_paren(const std::string& text, size_t start_index);

    static CommandExecutor create_command_executor(
        cjsh::FunctionRef<int(const std::string&)> executor);

   private:
    bool find_matching_delimiter(const std::string& text, size_t start, char open_char,
                                 char close_char, size_t& end_out);

    bool try_handle_arithmetic_expansion(const std::string& input, size_t& i,
                                         std::string& output_text);

    bool try_handle_command_substitution(const std::string& input, size_t& i,
                                         ExpansionResult& result, bool in_double_quotes);

    bool try_handle_backtick_substitution(const std::string& input, size_t& i,
                                          ExpansionResult& result, bool in_double_quotes);

    bool try_handle_parameter_expansion(const std::string& input, size_t& i,
                                        std::string& output_text);

    size_t find_closing_backtick(const std::string& input, size_t start);

    void append_substitution_result(const std::string& content, bool in_double_quotes,
                                    std::string& output);

    bool handle_escape_sequence(char c, bool& escaped, std::string& output);

    bool handle_quote_toggle(char c, bool in_single_quotes, bool& in_quotes, char& quote_char,
                             std::string& output);

    CommandExecutor command_executor_;
    static constexpr const char* SUBST_LITERAL_START = "\x1E__SUBST_LITERAL_START__\x1E";
    static constexpr const char* SUBST_LITERAL_END = "\x1E__SUBST_LITERAL_END__\x1E";
    static constexpr const char* NOENV_START = "\x1E__NOENV_START__\x1E";
    static constexpr const char* NOENV_END = "\x1E__NOENV_END__\x1E";
};
