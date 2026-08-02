/*
  completion_context.cpp

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

#include "completion_context.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace completion_context {
namespace {

enum class QuoteMode : unsigned char {
    None,
    Single,
    Double
};

bool is_assignment(const std::string& word) {
    std::size_t equals = word.find('=');
    if (equals == std::string::npos || equals == 0)
        return false;

    if (!(std::isalpha(static_cast<unsigned char>(word[0])) != 0 || word[0] == '_'))
        return false;
    for (std::size_t index = 1; index < equals; ++index) {
        unsigned char ch = static_cast<unsigned char>(word[index]);
        if (std::isalnum(ch) == 0 && word[index] != '_')
            return false;
    }
    return true;
}

bool is_control_operator(char ch) {
    return ch == '|' || ch == '&' || ch == ';' || ch == '\n' || ch == '\r';
}

void finish_word(CommandLineContext& context, Word& word, bool& word_active, std::size_t end) {
    if (!word_active)
        return;
    word.end = end;
    word.assignment = is_assignment(word.text);
    context.words.push_back(std::move(word));
    word = Word{};
    word_active = false;
}

void tokenize_active_segment(const std::string& input, std::size_t cursor,
                             CommandLineContext& context) {
    Word word;
    bool word_active = false;
    QuoteMode quote = QuoteMode::None;

    auto begin_word = [&](std::size_t index) {
        if (word_active)
            return;
        word_active = true;
        word = Word{};
        word.begin = index;
    };

    for (std::size_t index = 0; index < cursor; ++index) {
        char ch = input[index];

        if (quote == QuoteMode::None && is_control_operator(ch)) {
            finish_word(context, word, word_active, index);

            std::size_t operator_length = 1;
            if (index + 1 < cursor &&
                ((ch == '|' && (input[index + 1] == '|' || input[index + 1] == '&')) ||
                 (ch == '&' && input[index + 1] == '&'))) {
                operator_length = 2;
            }
            index += operator_length - 1;
            context.words.clear();
            context.segment_start = index + 1;
            continue;
        }

        if (quote == QuoteMode::None && std::isspace(static_cast<unsigned char>(ch)) != 0) {
            finish_word(context, word, word_active, index);
            continue;
        }

        begin_word(index);
        word.raw.push_back(ch);

        if (ch == '\\' && quote != QuoteMode::Single) {
            if (index + 1 < cursor) {
                char escaped = input[++index];
                word.raw.push_back(escaped);
                word.text.push_back(escaped);
            } else {
                word.text.push_back(ch);
            }
            continue;
        }

        if (ch == '\'' && quote != QuoteMode::Double) {
            word.quoted = true;
            quote = quote == QuoteMode::Single ? QuoteMode::None : QuoteMode::Single;
            continue;
        }
        if (ch == '"' && quote != QuoteMode::Single) {
            word.quoted = true;
            quote = quote == QuoteMode::Double ? QuoteMode::None : QuoteMode::Double;
            continue;
        }

        word.text.push_back(ch);
    }

    context.at_word_boundary = !word_active;
    finish_word(context, word, word_active, cursor);
    context.segment_prefix = input.substr(context.segment_start, cursor - context.segment_start);

    if (!context.at_word_boundary && !context.words.empty()) {
        context.current_prefix = context.words.back().text;
        context.current_raw_prefix = context.words.back().raw;
    }
}

struct WrapperResult {
    std::size_t next{0};
    bool found_command{false};
    bool waiting_for_value{false};
    bool suppress_unwrap{false};
};

bool option_is(const std::string& option, const char* short_name, const char* long_name) {
    return option == short_name || option == long_name;
}

bool required_option_value(const std::string& token, const std::string& wrapper,
                           bool& inline_value) {
    inline_value = false;
    std::string option = token;
    std::size_t equals = token.find('=');
    if (equals != std::string::npos) {
        option = token.substr(0, equals);
        inline_value = true;
    }

    if (!inline_value && token.size() > 2 && token[0] == '-' && token[1] != '-') {
        static const std::string sudo_value_options = "ugpCTrtDR";
        static const std::string env_value_options = "uCSa";
        const std::string& value_options =
            wrapper == "sudo" ? sudo_value_options : env_value_options;
        for (std::size_t index = 1; index < token.size(); ++index) {
            if (value_options.find(token[index]) == std::string::npos)
                continue;
            option = std::string{"-"} + token[index];
            inline_value = index + 1 < token.size();
            break;
        }
    }

    if (wrapper == "sudo") {
        static const std::unordered_set<std::string> required = {
            "-u",       "--user", "-g",           "--group", "-p",
            "--prompt", "-C",     "--close-from", "-T",      "--command-timeout",
            "-r",       "--role", "-t",           "--type",  "-D",
            "--chdir",  "-R",     "--chroot",     "--host"};
        if (required.find(option) == required.end())
            return false;
        return true;
    }

    if (wrapper == "env") {
        bool required = option_is(option, "-u", "--unset") || option_is(option, "-C", "--chdir") ||
                        option_is(option, "-S", "--split-string") ||
                        option_is(option, "-a", "--argv0");
        if (!required)
            return false;
        return true;
    }

    return false;
}

WrapperResult consume_wrapper(const std::vector<Word>& words, std::size_t start,
                              const std::string& wrapper, bool at_word_boundary) {
    WrapperResult result;
    std::size_t index = start;
    bool options_enabled = true;

    while (index < words.size()) {
        const std::string& token = words[index].text;

        if (wrapper == "env" && is_assignment(token)) {
            ++index;
            continue;
        }

        if (options_enabled && token == "--") {
            options_enabled = false;
            ++index;
            continue;
        }

        if (options_enabled && !token.empty() && token.front() == '-') {
            if (wrapper == "command" &&
                (token == "-v" || token == "-V" || token.find('v') != std::string::npos ||
                 token.find('V') != std::string::npos)) {
                result.suppress_unwrap = true;
                return result;
            }

            bool inline_value = false;
            if (required_option_value(token, wrapper, inline_value) && !inline_value) {
                ++index;
                if (index >= words.size()) {
                    result.waiting_for_value = true;
                    return result;
                }
                if (index + 1 == words.size() && !at_word_boundary) {
                    result.waiting_for_value = true;
                    return result;
                }
                ++index;
                continue;
            }
            ++index;
            continue;
        }

        result.next = index;
        result.found_command = true;
        return result;
    }

    return result;
}

bool is_wrapper(const std::string& word) {
    return word == "sudo" || word == "env" || word == "command";
}

void resolve_effective_command(CommandLineContext& context) {
    std::size_t index = 0;
    while (index < context.words.size() && context.words[index].assignment)
        ++index;

    if (index == context.words.size()) {
        context.cursor_in_command_position = context.words.empty() || context.at_word_boundary;
        return;
    }

    while (index < context.words.size() && is_wrapper(context.words[index].text)) {
        // Until the wrapper token is complete, it is still the command being completed.
        if (index + 1 == context.words.size() && !context.at_word_boundary)
            break;

        WrapperResult wrapper = consume_wrapper(context.words, index + 1, context.words[index].text,
                                                context.at_word_boundary);
        if (wrapper.suppress_unwrap) {
            context.cursor_in_command_position = false;
            return;
        }
        if (wrapper.waiting_for_value) {
            context.cursor_in_command_position = false;
            context.cursor_in_wrapper_option_value = true;
            return;
        }
        if (!wrapper.found_command) {
            context.cursor_in_command_position = context.at_word_boundary;
            return;
        }
        index = wrapper.next;

        while (index < context.words.size() && context.words[index].assignment)
            ++index;
        if (index == context.words.size()) {
            context.cursor_in_command_position = context.at_word_boundary;
            return;
        }
    }

    for (std::size_t token_index = index; token_index < context.words.size(); ++token_index)
        context.effective_tokens.push_back(context.words[token_index].text);

    context.cursor_in_command_position =
        index + 1 == context.words.size() && !context.at_word_boundary;
}

}  // namespace

CommandLineContext parse(const std::string& input, std::size_t cursor) {
    CommandLineContext context;
    context.cursor = cursor == std::string::npos ? input.size() : std::min(cursor, input.size());
    tokenize_active_segment(input, context.cursor, context);
    resolve_effective_command(context);
    return context;
}

}  // namespace completion_context
