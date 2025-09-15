#include "parser.h"

#include <fcntl.h>
#include <glob.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "cjsh.h"
#include "command_preprocessor.h"
#include "job_control.h"
#include "readonly_command.h"

std::vector<std::string> Parser::parse_into_lines(const std::string& script) {
  auto strip_inline_comment = [](std::string_view s) -> std::string {
    bool in_quotes = false;
    char quote = '\0';
    const char* data = s.data();
    size_t size = s.size();

    for (size_t i = 0; i < size; ++i) {
      char c = data[i];

      if (c == '"' || c == '\'') {
        size_t backslash_count = 0;
        for (size_t j = i; j > 0; --j) {
          if (data[j - 1] == '\\') {
            backslash_count++;
          } else {
            break;
          }
        }

        bool is_escaped = (backslash_count % 2) == 1;

        if (!is_escaped) {
          if (!in_quotes) {
            in_quotes = true;
            quote = c;
          } else if (quote == c) {
            in_quotes = false;
            quote = '\0';
          }
        }
      } else if (!in_quotes && c == '#') {
        return std::string(s.substr(0, i));
      }
    }
    return std::string(s);
  };

  std::vector<std::string> lines;
  lines.reserve(32);
  size_t start = 0;
  bool in_quotes = false;
  char quote_char = '\0';
  bool in_here_doc = false;
  bool strip_tabs = false;
  std::string here_doc_delimiter;
  here_doc_delimiter.reserve(64);
  std::string here_doc_content;
  here_doc_content.reserve(1024);
  std::string current_here_doc_line;
  current_here_doc_line.reserve(256);

  for (size_t i = 0; i < script.size(); ++i) {
    char c = script[i];

    if (in_here_doc) {
      if (c == '\n') {
        std::string trimmed_line = current_here_doc_line;

        trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
        trimmed_line.erase(trimmed_line.find_last_not_of(" \t\r") + 1);

        if (trimmed_line == here_doc_delimiter) {
          std::string_view segment_view{script.data() + start, i - start};

          size_t here_pos = std::string::npos;

          here_pos = segment_view.find("<<- " + here_doc_delimiter);
          if (here_pos == std::string::npos) {
            here_pos = segment_view.find("<<-" + here_doc_delimiter);
            if (here_pos == std::string::npos) {
              here_pos = segment_view.find("<< " + here_doc_delimiter);
              if (here_pos == std::string::npos) {
                here_pos = segment_view.find("<<" + here_doc_delimiter);
              }
            }
          }

          if (here_pos != std::string::npos) {
            std::string before_here{segment_view.substr(0, here_pos)};

            std::string placeholder =
                "HEREDOC_PLACEHOLDER_" + std::to_string(lines.size());

            std::string segment = before_here + "< " + placeholder;

            current_here_docs[placeholder] = here_doc_content;

            if (!segment.empty() && segment.back() == '\r') {
              segment.pop_back();
            }
            lines.push_back(std::move(segment));
          } else {
            std::string segment{segment_view};
            if (!segment.empty() && segment.back() == '\r') {
              segment.pop_back();
            }
            lines.push_back(std::move(segment));
          }

          in_here_doc = false;
          strip_tabs = false;
          here_doc_delimiter.clear();
          here_doc_content.clear();
          start = i + 1;
        } else {
          if (!here_doc_content.empty()) {
            here_doc_content += "\n";
          }

          std::string line_to_add = current_here_doc_line;
          if (strip_tabs) {
            size_t first_non_tab = line_to_add.find_first_not_of('\t');
            if (first_non_tab != std::string::npos) {
              line_to_add = line_to_add.substr(first_non_tab);
            } else {
              line_to_add.clear();
            }
          }
          here_doc_content += line_to_add;
        }
        current_here_doc_line.clear();
      } else {
        current_here_doc_line += c;
      }
      continue;
    }

    if (!in_quotes && (c == '"' || c == '\'')) {
      in_quotes = true;
      quote_char = c;
    } else if (in_quotes && c == quote_char) {
      in_quotes = false;
    } else if (!in_quotes && c == '\n') {
      std::string_view segment_view{script.data() + start, i - start};
      if (!in_quotes && segment_view.find("<<") != std::string_view::npos) {
        std::string segment_no_comment = strip_inline_comment(segment_view);

        size_t here_pos = segment_no_comment.find("<<-");
        bool is_strip_tabs = false;
        size_t operator_len = 2;

        if (here_pos != std::string::npos) {
          is_strip_tabs = true;
          operator_len = 3;
        } else {
          here_pos = segment_no_comment.find("<<");
          if (here_pos == std::string::npos) {
            goto normal_line_processing;
          }
        }

        size_t delim_start = here_pos + operator_len;
        while (delim_start < segment_no_comment.size() &&
               std::isspace(segment_no_comment[delim_start])) {
          delim_start++;
        }
        size_t delim_end = delim_start;
        while (delim_end < segment_no_comment.size() &&
               !std::isspace(segment_no_comment[delim_end])) {
          delim_end++;
        }
        if (delim_start < delim_end) {
          here_doc_delimiter =
              segment_no_comment.substr(delim_start, delim_end - delim_start);
          in_here_doc = true;
          strip_tabs = is_strip_tabs;
          here_doc_content.clear();
          current_here_doc_line.clear();
          continue;
        }
      }

    normal_line_processing:

      std::string segment{script.data() + start, i - start};
      if (!segment.empty() && segment.back() == '\r') {
        segment.pop_back();
      }
      lines.push_back(std::move(segment));
      start = i + 1;
    }
  }

  if (start <= script.size()) {
    std::string_view tail_view{script.data() + start, script.size() - start};
    if (!tail_view.empty() && !in_quotes && !in_here_doc) {
      std::string tail{tail_view};
      if (!tail.empty() && tail.back() == '\r') {
        tail.pop_back();
      }
      lines.push_back(std::move(tail));
    } else if (!tail_view.empty()) {
      std::string tail{tail_view};
      if (!tail.empty() && tail.back() == '\r') {
        tail.pop_back();
      }
      lines.push_back(std::move(tail));
    }
  }

  return lines;
}

namespace {

static const char QUOTE_PREFIX = '\x1F';
static const char QUOTE_SINGLE = 'S';
static const char QUOTE_DOUBLE = 'D';

inline bool is_single_quoted_token(const std::string& s) {
  return s.size() >= 2 && s[0] == QUOTE_PREFIX && s[1] == QUOTE_SINGLE;
}

inline bool is_double_quoted_token(const std::string& s) {
  return s.size() >= 2 && s[0] == QUOTE_PREFIX && s[1] == QUOTE_DOUBLE;
}

inline std::string strip_quote_tag(const std::string& s) {
  if (s.size() >= 2 && s[0] == QUOTE_PREFIX &&
      (s[1] == QUOTE_SINGLE || s[1] == QUOTE_DOUBLE)) {
    return s.substr(2);
  }
  return s;
}

static inline std::pair<std::string, bool> strip_noenv_sentinels(
    const std::string& s) {
  const std::string start = "\x1E__NOENV_START__\x1E";
  const std::string end = "\x1E__NOENV_END__\x1E";
  size_t a = s.find(start);
  size_t b = s.rfind(end);
  if (a != std::string::npos && b != std::string::npos &&
      b >= a + start.size()) {
    std::string mid = s.substr(a + start.size(), b - (a + start.size()));
    std::string out = s.substr(0, a) + mid + s.substr(b + end.size());
    return {out, true};
  }
  return {s, false};
}
}  // namespace

std::vector<std::string> tokenize_command(const std::string& cmdline) {
  std::vector<std::string> tokens;
  tokens.reserve(16);
  std::string current_token;
  current_token.reserve(128);
  bool in_quotes = false;
  char quote_char = '\0';
  bool escaped = false;

  bool token_saw_single = false;
  bool token_saw_double = false;

  for (size_t i = 0; i < cmdline.length(); ++i) {
    char c = cmdline[i];

    if (escaped) {
      if (in_quotes && quote_char == '"') {
        if (c == '$' || c == '`' || c == '"' || c == '\\' || c == '\n') {
          current_token += c;
        } else {
          current_token += '\\';
          current_token += c;
        }
      } else {
        current_token += c;
      }
      escaped = false;
    } else if (c == '\\' && (!in_quotes || quote_char != '\'')) {
      escaped = true;
    } else if ((c == '"' || c == '\'') && !in_quotes) {
      in_quotes = true;
      quote_char = c;
      if (c == '\'')
        token_saw_single = true;
      else
        token_saw_double = true;
    } else if (c == quote_char && in_quotes) {
      in_quotes = false;
      quote_char = '\0';
    } else if ((c == '(' || c == ')' || c == '<' || c == '>' || c == '&' ||
                c == '|') &&
               !in_quotes) {
      if (!current_token.empty()) {
        if (token_saw_single && !token_saw_double) {
          tokens.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_SINGLE +
                           current_token);
        } else if (token_saw_double && !token_saw_single) {
          tokens.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE +
                           current_token);
        } else {
          tokens.push_back(current_token);
        }
        current_token.clear();
        token_saw_single = token_saw_double = false;
      }
      tokens.push_back(std::string(1, c));
    } else if (std::isspace(c) && !in_quotes) {
      if (!current_token.empty()) {
        if (token_saw_single && !token_saw_double) {
          tokens.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_SINGLE +
                           current_token);
        } else if (token_saw_double && !token_saw_single) {
          tokens.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE +
                           current_token);
        } else {
          tokens.push_back(current_token);
        }
        current_token.clear();
        token_saw_single = token_saw_double = false;
      }
    } else {
      current_token += c;
    }
  }

  if (!current_token.empty()) {
    if (token_saw_single && !token_saw_double) {
      tokens.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_SINGLE +
                       current_token);
    } else if (token_saw_double && !token_saw_single) {
      tokens.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE +
                       current_token);
    } else {
      tokens.push_back(current_token);
    }
  }

  if (in_quotes) {
    throw std::runtime_error("Unclosed quote: missing closing " +
                             std::string(1, quote_char));
  }

  return tokens;
}

std::vector<std::string> merge_redirection_tokens(
    const std::vector<std::string>& tokens) {
  std::vector<std::string> result;
  result.reserve(tokens.size());

  if (g_debug_mode) {
    std::cerr << "DEBUG: merge_redirection_tokens input: ";
    for (const auto& token : tokens) {
      std::cerr << "'" << token << "' ";
    }
    std::cerr << std::endl;
  }

  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& token = tokens[i];

    if (token == "2" && i + 1 < tokens.size()) {
      if (tokens[i + 1] == ">&1") {
        result.push_back("2>&1");
        i++;
      } else if (i + 3 < tokens.size() && tokens[i + 1] == ">" &&
                 tokens[i + 2] == "&" && tokens[i + 3] == "1") {
        result.push_back("2>&1");
        i += 3;
      } else if (i + 2 < tokens.size() && tokens[i + 1] == ">" &&
                 tokens[i + 2] == ">") {
        result.push_back("2>>");
        i += 2;
      } else if (i + 1 < tokens.size() && tokens[i + 1] == ">") {
        result.push_back("2>");
        i++;
      } else {
        result.push_back(token);
      }
    }

    else if (token == "&" && i + 1 < tokens.size() && tokens[i + 1] == ">") {
      result.push_back("&>");
      i++;
    }

    else if (token == ">" && i + 1 < tokens.size() && tokens[i + 1] == "|") {
      result.push_back(">|");
      i++;
    }

    else if (token == "<" && i + 2 < tokens.size() && tokens[i + 1] == "<" &&
             tokens[i + 2] == "<") {
      result.push_back("<<<");
      i += 2;
    }

    else if (token == "<" && i + 2 < tokens.size() && tokens[i + 1] == "<" &&
             tokens[i + 2] == "-") {
      result.push_back("<<-");
      i += 2;
    }

    else if (token == "<" && i + 1 < tokens.size() && tokens[i + 1] == "<") {
      result.push_back("<<");
      i++;
    }

    else if (token == "<<" && i + 1 < tokens.size() && tokens[i + 1] == "<") {
      result.push_back("<<<");
      i++;
    }

    else if (token == "<<" && i + 1 < tokens.size() && tokens[i + 1] == "-") {
      result.push_back("<<-");
      i++;
    }

    else if (token == ">" && i + 1 < tokens.size() && tokens[i + 1] == ">") {
      result.push_back(">>");
      i++;
    }

    else if ((token == ">>" || token == ">") && i + 1 < tokens.size() &&
             (tokens[i + 1] == "&1" || tokens[i + 1] == "&2")) {
      result.push_back(token + tokens[i + 1]);
      i++;
    }

    else if (token == ">" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
             tokens[i + 2] == "2") {
      result.push_back(">&2");
      i += 2;
    }

    else if (token == ">" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
             tokens[i + 2] == "1") {
      result.push_back(">&1");
      i += 2;
    }

    else if (token == "2" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
             tokens[i + 2] == "1") {
      result.push_back("2>&1");
      i += 2;
    }

    else if (token == "2" && i + 1 < tokens.size() &&
             (tokens[i + 1] == ">" || tokens[i + 1] == ">>")) {
      result.push_back("2" + tokens[i + 1]);
      i++;
    }

    else if (std::isdigit(token[0]) && token.length() > 1) {
      size_t first_non_digit = 0;
      while (first_non_digit < token.length() &&
             std::isdigit(token[first_non_digit])) {
        first_non_digit++;
      }
      if (first_non_digit > 0 && first_non_digit < token.length()) {
        std::string rest = token.substr(first_non_digit);
        if (rest == "<" || rest == ">" || rest.find(">&") == 0) {
          result.push_back(token);
        } else {
          result.push_back(token);
        }
      } else {
        result.push_back(token);
      }
    } else {
      result.push_back(token);
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: merge_redirection_tokens output: ";
    for (const auto& token : result) {
      std::cerr << "'" << token << "' ";
    }
    std::cerr << std::endl;
  }

  return result;
}

std::vector<std::string> Parser::parse_command(const std::string& cmdline) {
  std::vector<std::string> args;
  args.reserve(8);

  try {
    std::vector<std::string> raw_args = tokenize_command(cmdline);
    args = merge_redirection_tokens(raw_args);
  } catch (const std::exception& e) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: tokenize_command failed: " << e.what() << std::endl;
    }
    return args;
  }

  if (!args.empty()) {
    auto alias_it = aliases.find(args[0]);
    if (alias_it != aliases.end()) {
      std::vector<std::string> alias_args;

      try {
        std::vector<std::string> raw_alias_args =
            tokenize_command(alias_it->second);
        alias_args = merge_redirection_tokens(raw_alias_args);

        if (!alias_args.empty()) {
          std::vector<std::string> new_args;
          new_args.insert(new_args.end(), alias_args.begin(), alias_args.end());
          if (args.size() > 1) {
            new_args.insert(new_args.end(), args.begin() + 1, args.end());
          }

          args = new_args;
        }
      } catch (const std::exception& e) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: tokenize_command for alias failed: " << e.what()
                    << std::endl;
        }
      }
    }
  }

  std::vector<std::string> expanded_args;
  expanded_args.reserve(args.size() * 2);
  for (const auto& raw_arg : args) {
    const bool is_single = is_single_quoted_token(raw_arg);
    const bool is_double = is_double_quoted_token(raw_arg);
    const std::string arg = strip_quote_tag(raw_arg);

    if (!is_single && !is_double && arg.find('{') != std::string::npos &&
        arg.find('}') != std::string::npos) {
      std::vector<std::string> brace_expansions = expand_braces(arg);
      brace_expansions.reserve(8);
      expanded_args.insert(expanded_args.end(),
                           std::make_move_iterator(brace_expansions.begin()),
                           std::make_move_iterator(brace_expansions.end()));
    } else {
      if (is_single) {
        expanded_args.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_SINGLE +
                                arg);
      } else if (is_double) {
        expanded_args.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE +
                                arg);
      } else {
        expanded_args.push_back(arg);
      }
    }
  }
  args = std::move(expanded_args);

  std::vector<std::string> pre_expanded_args;
  for (const auto& raw_arg : args) {
    if (is_double_quoted_token(raw_arg)) {
      std::string tmp = strip_quote_tag(raw_arg);

      if (tmp == "$@" && shell) {
        auto params = shell->get_positional_parameters();
        for (const auto& param : params) {
          pre_expanded_args.push_back(std::string(1, QUOTE_PREFIX) +
                                      QUOTE_DOUBLE + param);
        }
        continue;
      }
    }
    pre_expanded_args.push_back(raw_arg);
  }
  args = pre_expanded_args;

  for (auto& raw_arg : args) {
    if (!is_single_quoted_token(raw_arg)) {
      std::string tmp = strip_quote_tag(raw_arg);
      auto [noenv_stripped, had_noenv] = strip_noenv_sentinels(tmp);
      if (!had_noenv) {
        expand_env_vars(noenv_stripped);
      }

      if (is_double_quoted_token(raw_arg)) {
        raw_arg = std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE + noenv_stripped;
      } else {
        raw_arg = noenv_stripped;
      }
    }
  }

  std::vector<std::string> ifs_expanded_args;
  ifs_expanded_args.reserve(args.size() * 2);
  for (const auto& raw_arg : args) {
    if (!is_single_quoted_token(raw_arg) && !is_double_quoted_token(raw_arg)) {
      std::vector<std::string> split_words = split_by_ifs(raw_arg);
      ifs_expanded_args.insert(ifs_expanded_args.end(),
                               std::make_move_iterator(split_words.begin()),
                               std::make_move_iterator(split_words.end()));
    } else {
      ifs_expanded_args.push_back(raw_arg);
    }
  }
  args = std::move(ifs_expanded_args);
  std::vector<std::string> tilde_expanded_args;
  for (auto& raw_arg : args) {
    const bool is_single = is_single_quoted_token(raw_arg);
    const bool is_double = is_double_quoted_token(raw_arg);
    std::string arg = strip_quote_tag(raw_arg);
    bool has_tilde = false;
    if (!arg.empty()) {
      if (arg[0] == '~') {
        has_tilde = true;
      } else {
        for (size_t i = 1; i < arg.length(); ++i) {
          if (arg[i] == '~') {
            has_tilde = true;
            break;
          }
        }
      }
    }

    if (has_tilde && !is_single && !is_double) {
      char* home_dir = std::getenv("HOME");
      if (home_dir) {
        std::string home_str(home_dir);

        std::string expanded_arg;
        if (arg.front() == '~') {
          expanded_arg = home_str + arg.substr(1);
        } else {
          expanded_arg = arg;
        }
        tilde_expanded_args.push_back(std::move(expanded_arg));
      } else {
        tilde_expanded_args.push_back(arg);
      }
    } else {
      if (is_single) {
        tilde_expanded_args.push_back(std::string(1, QUOTE_PREFIX) +
                                      QUOTE_SINGLE + arg);
      } else if (is_double) {
        tilde_expanded_args.push_back(std::string(1, QUOTE_PREFIX) +
                                      QUOTE_DOUBLE + arg);
      } else {
        tilde_expanded_args.push_back(arg);
      }
    }
  }

  std::vector<std::string> final_args;
  final_args.reserve(tilde_expanded_args.size() * 2);
  for (const auto& raw_arg : tilde_expanded_args) {
    const bool is_single = is_single_quoted_token(raw_arg);
    const bool is_double = is_double_quoted_token(raw_arg);
    std::string arg = strip_quote_tag(raw_arg);
    if (!is_single && !is_double) {
      auto gw = expand_wildcards(arg);
      final_args.insert(final_args.end(), gw.begin(), gw.end());
    } else {
      final_args.push_back(arg);
    }
  }
  return final_args;
}

std::vector<std::string> Parser::expand_braces(const std::string& pattern) {
  std::vector<std::string> result;
  result.reserve(8);

  size_t open_pos = pattern.find('{');
  if (open_pos == std::string::npos) {
    result.push_back(pattern);
    return result;
  }

  int depth = 1;
  size_t close_pos = open_pos + 1;

  while (close_pos < pattern.size() && depth > 0) {
    if (pattern[close_pos] == '{') {
      depth++;
    } else if (pattern[close_pos] == '}') {
      depth--;
    }

    if (depth > 0) {
      close_pos++;
    }
  }

  if (depth != 0) {
    result.push_back(pattern);
    return result;
  }

  std::string prefix = pattern.substr(0, open_pos);
  std::string content = pattern.substr(open_pos + 1, close_pos - open_pos - 1);
  std::string suffix = pattern.substr(close_pos + 1);

  size_t range_pos = content.find("..");
  if (range_pos != std::string::npos) {
    std::string start_str = content.substr(0, range_pos);
    std::string end_str = content.substr(range_pos + 2);

    try {
      int start = std::stoi(start_str);
      int end = std::stoi(end_str);

      if (start <= end) {
        for (int i = start; i <= end; ++i) {
          std::vector<std::string> expanded_results =
              expand_braces(prefix + std::to_string(i) + suffix);
          result.insert(result.end(),
                        std::make_move_iterator(expanded_results.begin()),
                        std::make_move_iterator(expanded_results.end()));
        }
      } else {
        for (int i = start; i >= end; --i) {
          std::vector<std::string> expanded_results =
              expand_braces(prefix + std::to_string(i) + suffix);
          result.insert(result.end(),
                        std::make_move_iterator(expanded_results.begin()),
                        std::make_move_iterator(expanded_results.end()));
        }
      }
      return result;
    } catch (const std::exception&) {
      if (start_str.length() == 1 && end_str.length() == 1 &&
          std::isalpha(start_str[0]) && std::isalpha(end_str[0])) {
        char start_char = start_str[0];
        char end_char = end_str[0];

        if (start_char <= end_char) {
          for (char c = start_char; c <= end_char; ++c) {
            std::vector<std::string> expanded_results =
                expand_braces(prefix + std::string(1, c) + suffix);
            result.insert(result.end(), expanded_results.begin(),
                          expanded_results.end());
          }
        } else {
          for (char c = start_char; c >= end_char; --c) {
            std::vector<std::string> expanded_results =
                expand_braces(prefix + std::string(1, c) + suffix);
            result.insert(result.end(),
                          std::make_move_iterator(expanded_results.begin()),
                          std::make_move_iterator(expanded_results.end()));
          }
        }
        return result;
      }
    }
  }

  std::vector<std::string> options;
  options.reserve(8);
  size_t start = 0;
  depth = 0;

  for (size_t i = 0; i <= content.size(); ++i) {
    if (i == content.size() || (content[i] == ',' && depth == 0)) {
      options.emplace_back(content.substr(start, i - start));
      start = i + 1;
    } else if (content[i] == '{') {
      depth++;
    } else if (content[i] == '}') {
      depth--;
    }
  }

  for (const auto& option : options) {
    std::vector<std::string> expanded_results =
        expand_braces(prefix + option + suffix);
    result.insert(result.end(),
                  std::make_move_iterator(expanded_results.begin()),
                  std::make_move_iterator(expanded_results.end()));
  }

  return result;
}

void Parser::expand_env_vars(std::string& arg) {
  std::string result;
  result.reserve(arg.length() * 1.5);
  bool in_var = false;
  std::string var_name;
  var_name.reserve(64);

  for (size_t i = 0; i < arg.length(); ++i) {
    if (!in_var && arg[i] == '$' && i > 0 && arg[i - 1] == '\\') {
      if (!result.empty() && result.back() == '\\') {
        result.pop_back();
      }
      result += '$';
      continue;
    }
    if (in_var) {
      if (isalnum(arg[i]) || arg[i] == '_' ||
          (var_name.empty() && isdigit(arg[i])) ||
          (var_name.empty() &&
           (arg[i] == '?' || arg[i] == '$' || arg[i] == '#' || arg[i] == '*' ||
            arg[i] == '@' || arg[i] == '!'))) {
        var_name += arg[i];
      } else {
        in_var = false;
        std::string value;

        if (var_name == "?") {
          const char* status_env = getenv("STATUS");
          value = status_env ? status_env : "0";
        } else if (var_name == "$") {
          value = std::to_string(getpid());
        } else if (var_name == "#") {
          if (shell) {
            value = std::to_string(shell->get_positional_parameter_count());
          } else {
            value = "0";
          }
        } else if (var_name == "*") {
          if (shell) {
            auto params = shell->get_positional_parameters();
            std::string result;
            for (size_t i = 0; i < params.size(); ++i) {
              if (i > 0)
                result += " ";
              result += params[i];
            }
            value = result;
          }
        } else if (var_name == "@") {
          if (shell) {
            auto params = shell->get_positional_parameters();
            std::string result;
            for (size_t i = 0; i < params.size(); ++i) {
              if (i > 0)
                result += " ";
              result += params[i];
            }
            value = result;
          }
        } else if (var_name == "!") {
          value = "";
        } else if (isdigit(var_name[0]) && var_name.length() == 1) {
          const char* env_val = getenv(var_name.c_str());
          if (env_val) {
            value = env_val;
          } else {
            int param_num = var_name[0] - '0';
            if (shell && param_num > 0) {
              auto params = shell->get_positional_parameters();
              if (static_cast<size_t>(param_num - 1) < params.size()) {
                value = params[param_num - 1];
              }
            }
          }
        } else {
          auto it = env_vars.find(var_name);
          if (it != env_vars.end()) {
            value = it->second;
          } else {
            const char* env_val = getenv(var_name.c_str());
            if (env_val) {
              value = env_val;
            }
          }
        }
        result += value;

        if (arg[i] != '$') {
          result += arg[i];
        } else {
          i--;
        }
      }
    } else if (arg[i] == '$' && (i + 1 < arg.length()) &&
               (isalpha(arg[i + 1]) || arg[i + 1] == '_' ||
                isdigit(arg[i + 1]) || arg[i + 1] == '?' || arg[i + 1] == '$' ||
                arg[i + 1] == '#' || arg[i + 1] == '*' || arg[i + 1] == '@' ||
                arg[i + 1] == '!')) {
      in_var = true;
      var_name.clear();
      continue;
    } else {
      result += arg[i];
    }
  }

  if (in_var) {
    std::string value;

    if (var_name == "?") {
      const char* status_env = getenv("STATUS");
      value = status_env ? status_env : "0";
    } else if (var_name == "$") {
      value = std::to_string(getpid());
    } else if (var_name == "#") {
      if (shell) {
        value = std::to_string(shell->get_positional_parameter_count());
      } else {
        value = "0";
      }
    } else if (var_name == "*") {
      if (shell) {
        auto params = shell->get_positional_parameters();
        std::string result;
        for (size_t i = 0; i < params.size(); ++i) {
          if (i > 0)
            result += " ";
          result += params[i];
        }
        value = result;
      }
    } else if (var_name == "@") {
      if (shell) {
        auto params = shell->get_positional_parameters();
        std::string result;
        for (size_t i = 0; i < params.size(); ++i) {
          if (i > 0)
            result += " ";
          result += params[i];
        }
        value = result;
      }
    } else if (var_name == "!") {
      const char* last_bg_pid = getenv("!");
      if (last_bg_pid) {
        value = last_bg_pid;
      } else {
        pid_t last_pid = JobManager::instance().get_last_background_pid();
        if (last_pid > 0) {
          value = std::to_string(last_pid);
        } else {
          value = "";
        }
      }
    } else if (isdigit(var_name[0]) && var_name.length() == 1) {
      const char* env_val = getenv(var_name.c_str());
      if (env_val) {
        value = env_val;
      } else {
        int param_num = var_name[0] - '0';
        if (shell && param_num > 0) {
          auto params = shell->get_positional_parameters();
          if (static_cast<size_t>(param_num - 1) < params.size()) {
            value = params[param_num - 1];
          }
        }
      }
    } else {
      auto it = env_vars.find(var_name);
      if (it != env_vars.end()) {
        value = it->second;
      } else {
        const char* env_val = getenv(var_name.c_str());
        if (env_val) {
          value = env_val;
        }
      }
    }
    result += value;
  }

  arg = result;
}

std::vector<std::string> Parser::split_by_ifs(const std::string& input) {
  std::vector<std::string> result;

  const char* ifs_env = getenv("IFS");
  std::string ifs = ifs_env ? ifs_env : " \t\n";

  if (input.empty()) {
    return result;
  }

  if (ifs.empty()) {
    result.push_back(input);
    return result;
  }

  std::string current_word;
  bool in_word = false;

  for (char c : input) {
    if (ifs.find(c) != std::string::npos) {
      if (in_word) {
        result.push_back(current_word);
        current_word.clear();
        in_word = false;
      }

    } else {
      current_word += c;
      in_word = true;
    }
  }

  if (in_word) {
    result.push_back(current_word);
  }

  return result;
}

std::vector<Command> Parser::parse_pipeline(const std::string& command) {
  std::vector<Command> commands;
  commands.reserve(4);
  std::vector<std::string> command_parts;
  command_parts.reserve(4);

  std::string current;
  current.reserve(command.length() / 4);
  bool in_quotes = false;
  char quote_char = '\0';
  int paren_depth = 0;

  for (size_t i = 0; i < command.length(); ++i) {
    if (command[i] == '"' || command[i] == '\'') {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = command[i];
      } else if (quote_char == command[i]) {
        in_quotes = false;
      }
      current += command[i];
    } else if (!in_quotes && command[i] == '(') {
      paren_depth++;
      current += command[i];
    } else if (!in_quotes && command[i] == ')') {
      paren_depth--;
      current += command[i];
    } else if (command[i] == '|' && !in_quotes && paren_depth == 0) {
      if (!current.empty()) {
        command_parts.push_back(std::move(current));
        current.clear();
        current.reserve(command.length() / 4);
      }
    } else {
      current += command[i];
    }
  }

  if (!current.empty()) {
    command_parts.push_back(std::move(current));
  }

  for (const auto& cmd_str : command_parts) {
    Command cmd;
    std::string cmd_part = cmd_str;

    bool is_background = false;
    std::string trimmed = cmd_part;

    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (!trimmed.empty() && trimmed.back() == '&') {
      size_t amp_pos = cmd_part.rfind('&');
      if (amp_pos != std::string::npos && amp_pos + 1 < cmd_part.length()) {
        size_t next_non_ws = amp_pos + 1;
        while (next_non_ws < cmd_part.length() &&
               std::isspace(cmd_part[next_non_ws])) {
          next_non_ws++;
        }

        if (next_non_ws < cmd_part.length() && cmd_part[next_non_ws] == '>') {
          is_background = false;
        } else {
          is_background = true;
        }
      } else {
        is_background = true;
      }
    }

    if (is_background) {
      cmd.background = true;
      cmd_part = trimmed.substr(0, trimmed.length() - 1);
      cmd_part.erase(cmd_part.find_last_not_of(" \t\n\r") + 1);
    }

    if (!cmd_part.empty()) {
      size_t lead = cmd_part.find_first_not_of(" \t\r\n");
      if (lead != std::string::npos && cmd_part[lead] == '(') {
        size_t close_paren = cmd_part.find(')', lead + 1);
        if (close_paren != std::string::npos) {
          std::string subshell_content =
              cmd_part.substr(lead + 1, close_paren - (lead + 1));
          std::string remaining = cmd_part.substr(close_paren + 1);

          cmd.args.push_back("sh");
          cmd.args.push_back("-c");
          cmd.args.push_back(subshell_content);

          if (!remaining.empty()) {
            std::vector<std::string> redir_tokens = tokenize_command(remaining);
            std::vector<std::string> merged_redir =
                merge_redirection_tokens(redir_tokens);

            for (size_t i = 0; i < merged_redir.size(); ++i) {
              const std::string tok = strip_quote_tag(merged_redir[i]);
              if (tok == "2>&1") {
                cmd.stderr_to_stdout = true;
              } else if (tok == ">&2") {
                cmd.stdout_to_stderr = true;
              } else if ((tok == "2>" || tok == "2>>") &&
                         i + 1 < merged_redir.size()) {
                cmd.stderr_file = strip_quote_tag(merged_redir[++i]);
                cmd.stderr_append = (tok == "2>>");
              }
            }
          }

          commands.push_back(cmd);
          continue;
        }
      }
    }

    std::vector<std::string> raw_tokens = tokenize_command(cmd_part);
    std::vector<std::string> tokens = merge_redirection_tokens(raw_tokens);
    std::vector<std::string> filtered_args;

    for (size_t i = 0; i < tokens.size(); ++i) {
      const std::string tok = strip_quote_tag(tokens[i]);
      if (tok == "<" && i + 1 < tokens.size()) {
        cmd.input_file = strip_quote_tag(tokens[++i]);
      } else if (tok == ">" && i + 1 < tokens.size()) {
        cmd.output_file = strip_quote_tag(tokens[++i]);
      } else if (tok == ">>" && i + 1 < tokens.size()) {
        cmd.append_file = strip_quote_tag(tokens[++i]);
      } else if (tok == ">|" && i + 1 < tokens.size()) {
        cmd.output_file = strip_quote_tag(tokens[++i]);
        cmd.force_overwrite = true;
      } else if (tok == "&>" && i + 1 < tokens.size()) {
        cmd.both_output_file = strip_quote_tag(tokens[++i]);
        cmd.both_output = true;
      } else if (tok == "<<" && i + 1 < tokens.size()) {
        std::string delimiter = strip_quote_tag(tokens[++i]);

        if (delimiter.find("__HEREDOC_CONTENT__") == 0) {
          std::string line_num = delimiter.substr(19);
          cmd.here_doc = delimiter;
        } else {
          cmd.here_doc = delimiter;
        }
      } else if (tok == "<<-" && i + 1 < tokens.size()) {
        std::string delimiter = strip_quote_tag(tokens[++i]);

        if (delimiter.find("__HEREDOC_CONTENT__") == 0) {
          std::string line_num = delimiter.substr(19);
          cmd.here_doc = delimiter;
        } else {
          cmd.here_doc = delimiter;
        }
      } else if (tok == "<<<" && i + 1 < tokens.size()) {
        cmd.here_string = strip_quote_tag(tokens[++i]);
      } else if ((tok == "2>" || tok == "2>>") && i + 1 < tokens.size()) {
        cmd.stderr_file = strip_quote_tag(tokens[++i]);
        cmd.stderr_append = (tok == "2>>");
      } else if (tok == "2>&1") {
        cmd.stderr_to_stdout = true;
      } else if (tok == ">&2") {
        cmd.stdout_to_stderr = true;
      } else if (tok.find(">&") == 0 && tok.length() > 2) {
        try {
          int src_fd = std::stoi(tok.substr(0, tok.find(">&")));
          int dst_fd = std::stoi(tok.substr(tok.find(">&") + 2));
          cmd.fd_duplications[src_fd] = dst_fd;
        } catch (const std::exception&) {
          filtered_args.push_back(tokens[i]);
        }
      } else if (tok.find("<") == tok.length() - 1 && tok.length() > 1 &&
                 std::isdigit(tok[0]) && i + 1 < tokens.size()) {
        try {
          int fd = std::stoi(tok.substr(0, tok.length() - 1));
          std::string file = strip_quote_tag(tokens[++i]);
          cmd.fd_redirections[fd] = "input:" + file;
        } catch (const std::exception&) {
          filtered_args.push_back(tokens[i]);
        }
      } else if (tok.find(">") == tok.length() - 1 && tok.length() > 1 &&
                 std::isdigit(tok[0]) && i + 1 < tokens.size()) {
        try {
          int fd = std::stoi(tok.substr(0, tok.length() - 1));
          std::string file = strip_quote_tag(tokens[++i]);
          cmd.fd_redirections[fd] = "output:" + file;
        } catch (const std::exception&) {
          filtered_args.push_back(tokens[i]);
        }
      } else {
        if ((tok.find("<(") == 0 && tok.back() == ')') ||
            (tok.find(">(") == 0 && tok.back() == ')')) {
          cmd.process_substitutions.push_back(tok);
        } else {
          filtered_args.push_back(tokens[i]);
        }
      }
    }

    std::vector<std::string> final_args_local;
    for (const auto& raw : filtered_args) {
      bool is_single = is_single_quoted_token(raw);
      bool is_double = is_double_quoted_token(raw);
      std::string val = strip_quote_tag(raw);
      if (!is_single && !is_double &&
          val.find_first_of("*?[]") != std::string::npos) {
        auto expanded = expand_wildcards(val);
        final_args_local.insert(final_args_local.end(), expanded.begin(),
                                expanded.end());
      } else {
        final_args_local.push_back(val);
      }
    }

    cmd.args = final_args_local;

    const char* home = getenv("HOME");
    if (home) {
      std::string h(home);
      if (!cmd.input_file.empty() && cmd.input_file[0] == '~') {
        cmd.input_file = h + cmd.input_file.substr(1);
      }
      if (!cmd.output_file.empty() && cmd.output_file[0] == '~') {
        cmd.output_file = h + cmd.output_file.substr(1);
      }
      if (!cmd.append_file.empty() && cmd.append_file[0] == '~') {
        cmd.append_file = h + cmd.append_file.substr(1);
      }
      if (!cmd.stderr_file.empty() && cmd.stderr_file[0] == '~') {
        cmd.stderr_file = h + cmd.stderr_file.substr(1);
      }
      if (!cmd.both_output_file.empty() && cmd.both_output_file[0] == '~') {
        cmd.both_output_file = h + cmd.both_output_file.substr(1);
      }

      for (auto& fd_redir : cmd.fd_redirections) {
        if (!fd_redir.second.empty() && fd_redir.second[0] == '~') {
          fd_redir.second = h + fd_redir.second.substr(1);
        }
      }
    }

    commands.push_back(cmd);
  }

  return commands;
}

std::vector<Command> Parser::parse_pipeline_with_preprocessing(
    const std::string& command) {
  auto preprocessed = CommandPreprocessor::preprocess(command);

  for (const auto& pair : preprocessed.here_documents) {
    current_here_docs[pair.first] = pair.second;
  }

  {
    const std::string& pt = preprocessed.processed_text;
    size_t lead = pt.find_first_not_of(" \t\r\n");
    if (lead != std::string::npos && pt.find("SUBSHELL{", lead) == lead) {
      size_t start = preprocessed.processed_text.find('{') + 1;
      size_t end = preprocessed.processed_text.find('}', start);
      if (end != std::string::npos) {
        std::string subshell_content =
            preprocessed.processed_text.substr(start, end - start);
        std::string remaining = preprocessed.processed_text.substr(end + 1);

        auto escape_double_quotes = [](const std::string& s) {
          std::string out;
          out.reserve(s.size() + 16);
          for (char c : s) {
            if (c == '"' || c == '\\') {
              out += '\\';
            }
            out += c;
          }
          return out;
        };

        std::string rebuilt = "__INTERNAL_SUBSHELL__ \"" +
                              escape_double_quotes(subshell_content) + "\"" +
                              remaining;

        std::string prefix = preprocessed.processed_text.substr(0, lead);
        preprocessed.processed_text = prefix + rebuilt;
      }
    }
  }

  std::vector<Command> commands = parse_pipeline(preprocessed.processed_text);

  for (auto& cmd : commands) {
    if (!cmd.input_file.empty() &&
        cmd.input_file.find("HEREDOC_PLACEHOLDER_") == 0) {
      auto it = current_here_docs.find(cmd.input_file);
      if (it != current_here_docs.end()) {
        std::string content = it->second;

        if (content.length() >= 10 && content.substr(0, 10) == "__EXPAND__") {
          content = content.substr(10);

          expand_env_vars(content);
        }

        cmd.here_doc = content;
        cmd.input_file.clear();
      }
    }

    if (!cmd.here_doc.empty() && current_here_docs.count(cmd.here_doc)) {
      std::string content = current_here_docs[cmd.here_doc];

      if (content.length() >= 10 && content.substr(0, 10) == "__EXPAND__") {
        content = content.substr(10);

        expand_env_vars(content);
      }

      cmd.here_doc = content;
    }
  }

  return commands;
}

bool Parser::is_env_assignment(const std::string& command,
                               std::string& var_name, std::string& var_value) {
  std::regex env_regex("^\\s*([A-Za-z_][A-Za-z0-9_]*)=(.*)$");
  std::smatch match;

  if (std::regex_match(command, match, env_regex)) {
    var_name = match[1].str();
    var_value = match[2].str();

    if (ReadonlyManager::instance().is_readonly(var_name)) {
      std::cerr << "cjsh: " << var_name << ": readonly variable" << std::endl;
      return false;
    }

    if (var_value.size() >= 2) {
      if ((var_value.front() == '"' && var_value.back() == '"') ||
          (var_value.front() == '\'' && var_value.back() == '\'')) {
        var_value = var_value.substr(1, var_value.length() - 2);
      }
    }
    return true;
  }
  return false;
}

std::vector<LogicalCommand> Parser::parse_logical_commands(
    const std::string& command) {
  std::vector<LogicalCommand> logical_commands;
  std::string current;
  bool in_quotes = false;
  char quote_char = '\0';
  int paren_depth = 0;

  for (size_t i = 0; i < command.length(); ++i) {
    if (command[i] == '"' || command[i] == '\'') {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = command[i];
      } else if (quote_char == command[i]) {
        in_quotes = false;
      }
      current += command[i];
    } else if (!in_quotes && command[i] == '(') {
      paren_depth++;
      current += command[i];
    } else if (!in_quotes && command[i] == ')') {
      paren_depth--;
      current += command[i];
    } else if (!in_quotes && paren_depth == 0 && i < command.length() - 1) {
      if (command[i] == '&' && command[i + 1] == '&') {
        if (!current.empty()) {
          logical_commands.push_back({current, "&&"});
          current.clear();
        }
        i++;
      } else if (command[i] == '|' && command[i + 1] == '|') {
        if (!current.empty()) {
          logical_commands.push_back({current, "||"});
          current.clear();
        }
        i++;
      } else {
        current += command[i];
      }
    } else {
      current += command[i];
    }
  }

  if (!current.empty()) {
    logical_commands.push_back({current, ""});
  }

  return logical_commands;
}

std::vector<std::string> Parser::parse_semicolon_commands(
    const std::string& command) {
  std::vector<std::string> commands;
  std::string current;
  bool in_quotes = false;
  char quote_char = '\0';
  int paren_depth = 0;
  int control_depth = 0;

  std::vector<bool> is_semicolon_split_point(command.length(), false);

  for (size_t i = 0; i < command.length(); ++i) {
    if (command[i] == '"' || command[i] == '\'') {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = command[i];
      } else if (quote_char == command[i]) {
        in_quotes = false;
      }
    } else if (!in_quotes && command[i] == '(') {
      paren_depth++;
    } else if (!in_quotes && command[i] == ')') {
      paren_depth--;
    } else if (!in_quotes && paren_depth == 0) {
      if (command[i] == ' ' || command[i] == '\t' || i == 0) {
        size_t word_start = i;
        if (command[i] == ' ' || command[i] == '\t')
          word_start = i + 1;

        std::string word;
        size_t j = word_start;
        while (j < command.length() && std::isalpha(command[j])) {
          word += command[j];
          j++;
        }

        if (word == "if" || word == "for" || word == "while" ||
            word == "until" || word == "case") {
          control_depth++;
        } else if ((word == "fi" || word == "done" || word == "esac") &&
                   control_depth > 0) {
          control_depth--;
        }
      }

      if (command[i] == ';' && control_depth == 0) {
        is_semicolon_split_point[i] = true;
      }
    }
  }

  in_quotes = false;
  quote_char = '\0';
  current.clear();

  for (size_t i = 0; i < command.length(); ++i) {
    if (command[i] == '"' || command[i] == '\'') {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = command[i];
      } else if (quote_char == command[i]) {
        in_quotes = false;
      }
      current += command[i];
    } else if (command[i] == ';' && is_semicolon_split_point[i]) {
      if (!current.empty()) {
        current.erase(0, current.find_first_not_of(" \t\n\r"));
        current.erase(current.find_last_not_of(" \t\n\r") + 1);
        if (!current.empty()) {
          commands.push_back(current);
        }
        current.clear();
      }
    } else {
      current += command[i];
    }
  }

  if (!current.empty()) {
    current.erase(0, current.find_first_not_of(" \t\n\r"));
    current.erase(current.find_last_not_of(" \t\n\r") + 1);
    if (!current.empty()) {
      commands.push_back(current);
    }
  }

  return commands;
}

std::vector<std::string> Parser::expand_wildcards(const std::string& pattern) {
  std::vector<std::string> result;

  if (pattern.find_first_of("*?[]") == std::string::npos) {
    result.push_back(pattern);
    return result;
  }

  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_result));

  int return_value =
      glob(pattern.c_str(), GLOB_TILDE | GLOB_MARK, NULL, &glob_result);
  if (return_value == 0) {
    for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
      result.push_back(std::string(glob_result.gl_pathv[i]));
    }
    globfree(&glob_result);
  } else if (return_value == GLOB_NOMATCH) {
    result.push_back(pattern);
  }

  return result;
}
