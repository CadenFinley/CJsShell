#include "parser.h"

#include <fcntl.h>
#include <glob.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>

#include "cjsh.h"
#include "command_preprocessor.h"
#include "job_control.h"
#include "readonly_command.h"

std::vector<std::string> Parser::parse_into_lines(const std::string& script) {
  // Split script content on unquoted newlines into logical lines.
  // Handle here documents specially by collecting content until delimiter.

  // Helper function to strip inline comments - use string_view for efficiency
  auto strip_inline_comment = [](const std::string& s) -> std::string {
    bool in_quotes = false;
    char quote = '\0';
    const char* data = s.c_str();
    size_t size = s.size();

    for (size_t i = 0; i < size; ++i) {
      char c = data[i];

      // Count consecutive backslashes before this quote
      if (c == '"' || c == '\'') {
        size_t backslash_count = 0;
        for (size_t j = i; j > 0; --j) {
          if (data[j - 1] == '\\') {
            backslash_count++;
          } else {
            break;
          }
        }

        // Quote is escaped if there's an odd number of backslashes before it
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
        return s.substr(0, i);
      }
    }
    return s;
  };

  std::vector<std::string> lines;
  lines.reserve(32);  // Reserve space to reduce reallocations
  size_t start = 0;
  bool in_quotes = false;
  char quote_char = '\0';
  bool in_here_doc = false;
  std::string here_doc_delimiter;
  here_doc_delimiter.reserve(64);  // Reserve space for delimiter
  std::string here_doc_content;
  here_doc_content.reserve(1024);  // Reserve space for here document content
  std::string current_here_doc_line;
  current_here_doc_line.reserve(256);  // Reserve space for current line

  for (size_t i = 0; i < script.size(); ++i) {
    char c = script[i];

    if (in_here_doc) {
      if (c == '\n') {
        // Check if this line is the delimiter
        std::string trimmed_line = current_here_doc_line;
        // Trim whitespace
        trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
        trimmed_line.erase(trimmed_line.find_last_not_of(" \t\r") + 1);

        if (trimmed_line == here_doc_delimiter) {
          // End of here document - reconstruct the command with content
          std::string segment = script.substr(start, i - start);
          // Find << delimiter and replace with placeholder
          size_t here_pos = segment.find("<< " + here_doc_delimiter);
          if (here_pos == std::string::npos) {
            here_pos = segment.find("<<" + here_doc_delimiter);
          }
          if (here_pos != std::string::npos) {
            std::string before_here = segment.substr(0, here_pos + 2);
            // Create a placeholder that matches CommandPreprocessor format
            std::string placeholder =
                "HEREDOC_PLACEHOLDER_" + std::to_string(lines.size());
            segment = before_here + " " + placeholder;

            // Store the here document content in the current_here_docs map
            // This makes it accessible to the pipeline parser
            current_here_docs[placeholder] = here_doc_content;
          }

          if (!segment.empty() && segment.back() == '\r') {
            segment.pop_back();
          }
          lines.push_back(segment);

          in_here_doc = false;
          here_doc_delimiter.clear();
          here_doc_content.clear();
          start = i + 1;
        } else {
          // Add this line to here document content
          if (!here_doc_content.empty()) {
            here_doc_content += "\n";
          }
          here_doc_content += current_here_doc_line;
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
      // Check if this line contains a here document start
      std::string segment = script.substr(start, i - start);
      if (!in_quotes && segment.find("<<") != std::string::npos) {
        // Strip comments before parsing here document delimiter
        std::string segment_no_comment = strip_inline_comment(segment);

        // Look for << followed by delimiter
        size_t here_pos = segment_no_comment.find("<<");
        if (here_pos != std::string::npos) {
          // Extract delimiter
          size_t delim_start = here_pos + 2;
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
            here_doc_content.clear();
            current_here_doc_line.clear();
            continue;  // Don't process this as a regular line yet
          }
        }
      }

      // Extract line [start, i) - regular line processing
      if (!segment.empty() && segment.back() == '\r') {
        segment.pop_back();
      }
      lines.push_back(segment);
      start = i + 1;
    }
  }

  if (start <= script.size()) {
    std::string tail = script.substr(start);
    if (!tail.empty() && !in_quotes && !in_here_doc) {
      if (!tail.empty() && tail.back() == '\r') {
        tail.pop_back();
      }
      lines.push_back(tail);
    } else if (!tail.empty()) {
      // Even if still in quotes or here doc (unterminated), push remainder as a
      // line
      if (!tail.empty() && tail.back() == '\r') {
        tail.pop_back();
      }
      lines.push_back(tail);
    }
  }

  return lines;
}

namespace {
// Sentinel-based quote tagging so we can preserve how a token was quoted
// through parsing stages without changing public APIs.
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
}  // namespace

std::vector<std::string> tokenize_command(const std::string& cmdline) {
  std::vector<std::string> tokens;
  tokens.reserve(16);  // Reserve space for typical command with arguments
  std::string current_token;
  current_token.reserve(128);  // Reserve space for typical token
  bool in_quotes = false;
  char quote_char = '\0';
  bool escaped = false;
  // Track how the token was quoted overall; if a token contains any single
  // quotes and no double quotes, mark as single; if any double quotes and no
  // single quotes, mark as double.
  bool token_saw_single = false;
  bool token_saw_double = false;

  for (size_t i = 0; i < cmdline.length(); ++i) {
    char c = cmdline[i];

    if (escaped) {
      // Inside double quotes, only escape specific characters per POSIX
      if (in_quotes && quote_char == '"') {
        // In double quotes, backslash only escapes: $ ` " \ newline
        if (c == '$' || c == '`' || c == '"' || c == '\\' || c == '\n') {
          current_token += c;  // Add the escaped character
        } else {
          current_token += '\\';  // Keep the backslash
          current_token += c;     // Add the character
        }
      } else {
        current_token += c;  // For single quotes or unquoted, add escaped char
      }
      escaped = false;
    } else if (c == '\\' && (!in_quotes || quote_char != '\'')) {
      // Backslash escapes next character, except within single quotes
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
    } else if ((c == '(' || c == ')') && !in_quotes) {
      if (!current_token.empty()) {
        // Apply quote tag if any
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

  // Check for unclosed quotes
  if (in_quotes) {
    throw std::runtime_error("Unclosed quote: missing closing " +
                             std::string(1, quote_char));
  }

  return tokens;
}

// Post-process tokens to merge redirection operators
std::vector<std::string> merge_redirection_tokens(
    const std::vector<std::string>& tokens) {
  std::vector<std::string> result;
  result.reserve(tokens.size());  // Reserve at least as much space as input

  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& token = tokens[i];

    // Handle 2>&1, 2>>, 2> patterns
    if (token == "2" && i + 1 < tokens.size()) {
      if (tokens[i + 1] == ">&1") {
        result.push_back("2>&1");
        i++;  // skip next token
      } else if (tokens[i + 1] == ">>" || tokens[i + 1] == ">") {
        result.push_back("2" + tokens[i + 1]);
        i++;  // skip next token
      } else {
        result.push_back(token);
      }
    }
    // Handle >>&1, >&1, >&2 patterns
    else if ((token == ">>" || token == ">") && i + 1 < tokens.size() &&
             (tokens[i + 1] == "&1" || tokens[i + 1] == "&2")) {
      result.push_back(token + tokens[i + 1]);
      i++;  // skip next token
    }
    // Handle cases where >&2 might be split as ">" "&" "2"
    else if (token == ">" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
             tokens[i + 2] == "2") {
      result.push_back(">&2");
      i += 2;  // skip next two tokens
    }
    // Handle cases where 2>&1 might be split as "2" "&" "1"
    else if (token == "2" && i + 2 < tokens.size() && tokens[i + 1] == "&" &&
             tokens[i + 2] == "1") {
      result.push_back("2>&1");
      i += 2;  // skip next two tokens
    }
    // Handle cases where 2> might be split as "2" ">"
    else if (token == "2" && i + 1 < tokens.size() &&
             (tokens[i + 1] == ">" || tokens[i + 1] == ">>")) {
      result.push_back("2" + tokens[i + 1]);
      i++;  // skip next token
    } else {
      result.push_back(token);
    }
  }

  return result;
}

std::vector<std::string> Parser::parse_command(const std::string& cmdline) {
  std::vector<std::string> args;
  args.reserve(8);  // Reserve space for typical command with arguments

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
  for (const auto& raw_arg : args) {
    const bool is_single = is_single_quoted_token(raw_arg);
    const bool is_double = is_double_quoted_token(raw_arg);
    const std::string arg = strip_quote_tag(raw_arg);
    if (arg.find('{') != std::string::npos &&
        arg.find('}') != std::string::npos) {
      std::vector<std::string> brace_expansions = expand_braces(arg);
      // NOTE: Real shells inhibit brace expansion inside quotes; keeping simple
      // behavior here unless tests require otherwise. Preserve quote tags.
      if (is_single) {
        for (auto& b : brace_expansions) {
          expanded_args.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_SINGLE +
                                  b);
        }
      } else if (is_double) {
        for (auto& b : brace_expansions) {
          expanded_args.push_back(std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE +
                                  b);
        }
      } else {
        expanded_args.insert(expanded_args.end(), brace_expansions.begin(),
                             brace_expansions.end());
      }
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
  args = expanded_args;
  // Handle special case for quoted $@ expansion before regular env var
  // expansion
  std::vector<std::string> pre_expanded_args;
  for (const auto& raw_arg : args) {
    if (is_double_quoted_token(raw_arg)) {
      std::string tmp = strip_quote_tag(raw_arg);
      // Check if this is exactly "$@" within double quotes
      if (tmp == "$@" && shell) {
        // Special handling: split into separate arguments, each double-quoted
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
    // Do not expand env vars inside single quotes; expand inside double or
    // unquoted.
    if (!is_single_quoted_token(raw_arg)) {
      std::string tmp = strip_quote_tag(raw_arg);
      expand_env_vars(tmp);
      // Reapply quote tag for double-quoted tokens so we can still skip
      // globbing later
      if (is_double_quoted_token(raw_arg)) {
        raw_arg = std::string(1, QUOTE_PREFIX) + QUOTE_DOUBLE + tmp;
      } else {
        raw_arg = tmp;
      }
    }
  }

  // Apply IFS word splitting to unquoted arguments that may contain expanded
  // variables
  std::vector<std::string> ifs_expanded_args;
  for (const auto& raw_arg : args) {
    if (!is_single_quoted_token(raw_arg) && !is_double_quoted_token(raw_arg)) {
      // Unquoted argument - apply IFS word splitting
      std::vector<std::string> split_words = split_by_ifs(raw_arg);
      ifs_expanded_args.insert(ifs_expanded_args.end(), split_words.begin(),
                               split_words.end());
    } else {
      ifs_expanded_args.push_back(raw_arg);
    }
  }
  args = ifs_expanded_args;
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

    // Inhibit tilde expansion in any quoted token
    if (has_tilde && !is_single && !is_double) {
      char* home_dir = std::getenv("HOME");
      if (home_dir) {
        std::string home_str(home_dir);
        std::string expanded_arg =
            std::regex_replace(arg, std::regex("^~"), home_str);
        tilde_expanded_args.push_back(expanded_arg);
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

  // Check for numeric range expansion {start..end}
  size_t range_pos = content.find("..");
  if (range_pos != std::string::npos) {
    std::string start_str = content.substr(0, range_pos);
    std::string end_str = content.substr(range_pos + 2);

    // Try to parse as integers
    try {
      int start = std::stoi(start_str);
      int end = std::stoi(end_str);

      if (start <= end) {
        for (int i = start; i <= end; ++i) {
          std::vector<std::string> expanded_results =
              expand_braces(prefix + std::to_string(i) + suffix);
          result.insert(result.end(), expanded_results.begin(),
                        expanded_results.end());
        }
      } else {
        // Reverse range
        for (int i = start; i >= end; --i) {
          std::vector<std::string> expanded_results =
              expand_braces(prefix + std::to_string(i) + suffix);
          result.insert(result.end(), expanded_results.begin(),
                        expanded_results.end());
        }
      }
      return result;
    } catch (const std::exception&) {
      // Not numeric, check for alphabetic range
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
          // Reverse alphabetic range
          for (char c = start_char; c >= end_char; --c) {
            std::vector<std::string> expanded_results =
                expand_braces(prefix + std::string(1, c) + suffix);
            result.insert(result.end(), expanded_results.begin(),
                          expanded_results.end());
          }
        }
        return result;
      }
      // Fall through to comma-separated expansion
    }
  }

  // Handle comma-separated options
  std::vector<std::string> options;
  size_t start = 0;
  depth = 0;

  for (size_t i = 0; i <= content.size(); ++i) {
    if (i == content.size() || (content[i] == ',' && depth == 0)) {
      options.push_back(content.substr(start, i - start));
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
    result.insert(result.end(), expanded_results.begin(),
                  expanded_results.end());
  }

  return result;
}

void Parser::expand_env_vars(std::string& arg) {
  std::string result;
  result.reserve(arg.length() * 1.5);  // Reserve extra space for expansions
  bool in_var = false;
  std::string var_name;
  var_name.reserve(64);  // Reserve space for variable names

  for (size_t i = 0; i < arg.length(); ++i) {
    // Handle escaped dollar (e.g., \$var): treat $ literally and remove escape
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
        // Handle special variables
        if (var_name == "?") {
          const char* status_env = getenv("STATUS");
          value = status_env ? status_env : "0";
        } else if (var_name == "$") {
          // Process ID
          value = std::to_string(getpid());
        } else if (var_name == "#") {
          // Number of positional parameters
          if (shell) {
            value = std::to_string(shell->get_positional_parameter_count());
          } else {
            value = "0";
          }
        } else if (var_name == "*") {
          // All positional parameters as single string
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
          // All positional parameters as separate words
          // Note: This is the basic expansion; quoted "$@" needs special
          // handling
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
          // Process ID of last background command (not implemented yet)
          value = "";
        } else if (isdigit(var_name[0]) && var_name.length() == 1) {
          // Positional parameter $1, $2, etc.
          // First check environment variables (for function parameters)
          const char* env_val = getenv(var_name.c_str());
          if (env_val) {
            value = env_val;
          } else {
            // Then check shell positional parameters (for script arguments)
            int param_num = var_name[0] - '0';
            if (shell && param_num > 0) {
              auto params = shell->get_positional_parameters();
              if (static_cast<size_t>(param_num - 1) < params.size()) {
                value = params[param_num - 1];
              }
            }
          }
        } else {
          // Regular environment variable
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
        // Don't add the character that ended the variable if it's '$' (start of
        // next variable)
        if (arg[i] != '$') {
          result += arg[i];
        } else {
          // This is a '$' starting a new variable, so we need to reprocess it
          i--;  // Back up one position so the '$' gets processed again
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
    // Handle special variables
    if (var_name == "?") {
      const char* status_env = getenv("STATUS");
      value = status_env ? status_env : "0";
    } else if (var_name == "$") {
      // Process ID
      value = std::to_string(getpid());
    } else if (var_name == "#") {
      // Number of positional parameters
      if (shell) {
        value = std::to_string(shell->get_positional_parameter_count());
      } else {
        value = "0";
      }
    } else if (var_name == "*") {
      // All positional parameters as single string
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
      // All positional parameters as separate words (same as * for basic
      // expansion)
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
      // Process ID of last background command
      const char* last_bg_pid = getenv("!");
      if (last_bg_pid) {
        value = last_bg_pid;
      } else {
        // Fall back to JobManager if no env var set
        pid_t last_pid = JobManager::instance().get_last_background_pid();
        if (last_pid > 0) {
          value = std::to_string(last_pid);
        } else {
          value = "";
        }
      }
    } else if (isdigit(var_name[0]) && var_name.length() == 1) {
      // Positional parameter $1, $2, etc.
      // First check environment variables (for function parameters)
      const char* env_val = getenv(var_name.c_str());
      if (env_val) {
        value = env_val;
      } else {
        // Then check shell positional parameters (for script arguments)
        int param_num = var_name[0] - '0';
        if (shell && param_num > 0) {
          auto params = shell->get_positional_parameters();
          if (static_cast<size_t>(param_num - 1) < params.size()) {
            value = params[param_num - 1];
          }
        }
      }
    } else {
      // Regular environment variable
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

  // Get IFS from environment, default to space, tab, newline
  const char* ifs_env = getenv("IFS");
  std::string ifs = ifs_env ? ifs_env : " \t\n";

  if (input.empty()) {
    return result;
  }

  // If IFS is empty, return the whole string as one element
  if (ifs.empty()) {
    result.push_back(input);
    return result;
  }

  std::string current_word;
  bool in_word = false;

  for (char c : input) {
    if (ifs.find(c) != std::string::npos) {
      // Found IFS character
      if (in_word) {
        result.push_back(current_word);
        current_word.clear();
        in_word = false;
      }
      // Skip consecutive IFS characters
    } else {
      // Non-IFS character
      current_word += c;
      in_word = true;
    }
  }

  // Add the last word if we were building one
  if (in_word) {
    result.push_back(current_word);
  }

  return result;
}

std::vector<Command> Parser::parse_pipeline(const std::string& command) {
  std::vector<Command> commands;
  std::vector<std::string> command_parts;

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
    } else if (command[i] == '|' && !in_quotes && paren_depth == 0) {
      if (!current.empty()) {
        command_parts.push_back(current);
        current.clear();
      }
    } else {
      current += command[i];
    }
  }

  if (!current.empty()) {
    command_parts.push_back(current);
  }

  for (const auto& cmd_str : command_parts) {
    Command cmd;
    std::string cmd_part = cmd_str;

    if (!cmd_part.empty() && cmd_part.back() == '&') {
      cmd.background = true;
      cmd_part.pop_back();
      cmd_part.erase(cmd_part.find_last_not_of(" \t\n\r") + 1);
    }

    // Special handling for subshell commands (allow leading whitespace)
    if (!cmd_part.empty()) {
      size_t lead = cmd_part.find_first_not_of(" \t\r\n");
      if (lead != std::string::npos && cmd_part[lead] == '(') {
        size_t close_paren = cmd_part.find(')', lead + 1);
        if (close_paren != std::string::npos) {
          // This is a subshell command - treat it specially
          std::string subshell_content =
              cmd_part.substr(lead + 1, close_paren - (lead + 1));
          std::string remaining = cmd_part.substr(close_paren + 1);

          // Create a special command that represents the subshell
          cmd.args.push_back("sh");
          cmd.args.push_back("-c");
          cmd.args.push_back(subshell_content);

          // Parse any redirection that comes after the subshell
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
    std::vector<std::string> filtered_args;  // may contain quote tags

    for (size_t i = 0; i < tokens.size(); ++i) {
      const std::string tok = strip_quote_tag(tokens[i]);
      if (tok == "<" && i + 1 < tokens.size()) {
        cmd.input_file = strip_quote_tag(tokens[++i]);
      } else if (tok == ">" && i + 1 < tokens.size()) {
        cmd.output_file = strip_quote_tag(tokens[++i]);
      } else if (tok == ">>" && i + 1 < tokens.size()) {
        cmd.append_file = strip_quote_tag(tokens[++i]);
      } else if (tok == "<<" && i + 1 < tokens.size()) {
        std::string delimiter = strip_quote_tag(tokens[++i]);
        // Check if this is a special here document content marker
        if (delimiter.find("__HEREDOC_CONTENT__") == 0) {
          // Extract the line number and find the corresponding content
          std::string line_num =
              delimiter.substr(19);  // after "__HEREDOC_CONTENT__"
          cmd.here_doc = delimiter;  // Store the marker for now
        } else {
          cmd.here_doc = delimiter;
        }
      } else if ((tok == "2>" || tok == "2>>") && i + 1 < tokens.size()) {
        cmd.stderr_file = strip_quote_tag(tokens[++i]);
        cmd.stderr_append = (tok == "2>>");
      } else if (tok == "2>&1") {
        cmd.stderr_to_stdout = true;
      } else if (tok == ">&2") {
        // stdout to stderr - we need a new field for this
        cmd.stdout_to_stderr = true;
      } else {
        // Preserve quote tags for later wildcard handling
        filtered_args.push_back(tokens[i]);
      }
    }

    // Expand wildcards on unquoted args only, then strip quote tags
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
    }

    commands.push_back(cmd);
  }

  return commands;
}

std::vector<Command> Parser::parse_pipeline_with_preprocessing(
    const std::string& command) {
  // Use the new preprocessor to handle complex cases
  auto preprocessed = CommandPreprocessor::preprocess(command);

  // Merge here document context: keep existing documents and add new ones
  // This preserves here docs from parse_into_lines() for script files
  for (const auto& pair : preprocessed.here_documents) {
    current_here_docs[pair.first] = pair.second;
  }

  // Check if this is a SUBSHELL{} command
  // Allow leading whitespace before SUBSHELL marker
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

        // Convert to an internal subshell command so downstream parsing
        // (pipes/redirs) continues to work: __INTERNAL_SUBSHELL__
        // "content"<remaining> Need to quote the content to preserve it as a
        // single argument
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
        // Preserve the original leading whitespace
        std::string prefix = preprocessed.processed_text.substr(0, lead);
        preprocessed.processed_text = prefix + rebuilt;
      }
    }
  }

  // Parse the preprocessed command normally
  std::vector<Command> commands = parse_pipeline(preprocessed.processed_text);

  // Resolve any here document placeholders in the commands
  for (auto& cmd : commands) {
    // Check if input_file is actually a here document placeholder
    if (!cmd.input_file.empty() &&
        cmd.input_file.find("HEREDOC_PLACEHOLDER_") == 0) {
      auto it = current_here_docs.find(cmd.input_file);
      if (it != current_here_docs.end()) {
        // Move from input_file to here_doc
        std::string content = it->second;

        // Check if variable expansion is needed (unquoted delimiter)
        if (content.length() >= 10 && content.substr(0, 10) == "__EXPAND__") {
          content = content.substr(10);  // Remove marker
          // Expand variables in here document content
          expand_env_vars(content);
        }

        cmd.here_doc = content;
        cmd.input_file.clear();  // Clear the placeholder
      }
    }

    // Also check the here_doc field for placeholders (in case of direct
    // parsing)
    if (!cmd.here_doc.empty() && current_here_docs.count(cmd.here_doc)) {
      std::string content = current_here_docs[cmd.here_doc];

      // Check if variable expansion is needed (unquoted delimiter)
      if (content.length() >= 10 && content.substr(0, 10) == "__EXPAND__") {
        content = content.substr(10);  // Remove marker
        // Expand variables in here document content
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

    // Check if the variable is readonly before allowing assignment
    if (ReadonlyManager::instance().is_readonly(var_name)) {
      std::cerr << "cjsh: " << var_name << ": readonly variable" << std::endl;
      return false;  // Reject readonly variable assignments
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
  int control_depth = 0;  // Track control structure nesting

  // Pre-scan to detect control structures and avoid splitting them
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
      // Check for control structure keywords
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

  // Reset state for actual parsing
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
