/*
  redirection_utils.cpp

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

#include "redirection_utils.h"

#include <vector>

namespace redirection_utils {

const std::vector<std::string_view>& canonical_operator_spellings() {
    static const std::vector<std::string_view> kOperators = {"2>&1", "2>>", "<<-", "<<<", ">&2",
                                                             ">>",   "<<",  ">|",  "&>",  "<>",
                                                             "<&",   ">&",  "2>",  "<",   ">"};
    return kOperators;
}

std::optional<RedirectionOperator> parse_operator_token(std::string_view token) {
    if (token == "2>&1") {
        return RedirectionOperator::StderrToStdout;
    }
    if (token == "2>>") {
        return RedirectionOperator::StderrAppend;
    }
    if (token == "<<-") {
        return RedirectionOperator::HereDocStrip;
    }
    if (token == "<<<") {
        return RedirectionOperator::HereString;
    }
    if (token == ">&2") {
        return RedirectionOperator::StdoutToStderr;
    }
    if (token == ">>") {
        return RedirectionOperator::Append;
    }
    if (token == "<<") {
        return RedirectionOperator::HereDoc;
    }
    if (token == ">|") {
        return RedirectionOperator::ForceOutput;
    }
    if (token == "&>") {
        return RedirectionOperator::BothOutput;
    }
    if (token == "<>") {
        return RedirectionOperator::ReadWrite;
    }
    if (token == "<&") {
        return RedirectionOperator::DupInput;
    }
    if (token == ">&") {
        return RedirectionOperator::DupOutput;
    }
    if (token == "2>") {
        return RedirectionOperator::StderrOutput;
    }
    if (token == "<") {
        return RedirectionOperator::Input;
    }
    if (token == ">") {
        return RedirectionOperator::Output;
    }
    return std::nullopt;
}

std::optional<ParsedRedirectionOperator> parse_operator_at(std::string_view text, size_t start) {
    if (start >= text.size()) {
        return std::nullopt;
    }

    for (std::string_view op : canonical_operator_spellings()) {
        if (start + op.size() <= text.size() && text.compare(start, op.size(), op) == 0) {
            auto parsed = parse_operator_token(op);
            if (parsed.has_value()) {
                return ParsedRedirectionOperator{*parsed, op.size()};
            }
        }
    }

    return std::nullopt;
}

bool requires_operand(RedirectionOperator op) {
    switch (op) {
        case RedirectionOperator::Input:
        case RedirectionOperator::Output:
        case RedirectionOperator::Append:
        case RedirectionOperator::ForceOutput:
        case RedirectionOperator::BothOutput:
        case RedirectionOperator::HereDoc:
        case RedirectionOperator::HereDocStrip:
        case RedirectionOperator::HereString:
        case RedirectionOperator::ReadWrite:
        case RedirectionOperator::DupInput:
        case RedirectionOperator::DupOutput:
        case RedirectionOperator::StderrOutput:
        case RedirectionOperator::StderrAppend:
            return true;
        case RedirectionOperator::StderrToStdout:
        case RedirectionOperator::StdoutToStderr:
            return false;
    }
    return false;
}

const char* operator_spelling(RedirectionOperator op) {
    switch (op) {
        case RedirectionOperator::Input:
            return "<";
        case RedirectionOperator::Output:
            return ">";
        case RedirectionOperator::Append:
            return ">>";
        case RedirectionOperator::ForceOutput:
            return ">|";
        case RedirectionOperator::BothOutput:
            return "&>";
        case RedirectionOperator::HereDoc:
            return "<<";
        case RedirectionOperator::HereDocStrip:
            return "<<-";
        case RedirectionOperator::HereString:
            return "<<<";
        case RedirectionOperator::ReadWrite:
            return "<>";
        case RedirectionOperator::DupInput:
            return "<&";
        case RedirectionOperator::DupOutput:
            return ">&";
        case RedirectionOperator::StderrOutput:
            return "2>";
        case RedirectionOperator::StderrAppend:
            return "2>>";
        case RedirectionOperator::StderrToStdout:
            return "2>&1";
        case RedirectionOperator::StdoutToStderr:
            return ">&2";
    }
    return "";
}

}  // namespace redirection_utils
