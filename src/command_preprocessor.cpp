#include "command_preprocessor.h"

#include <iostream>
#include <regex>
#include <sstream>

extern bool g_debug_mode;

int CommandPreprocessor::placeholder_counter = 0;

CommandPreprocessor::PreprocessedCommand CommandPreprocessor::preprocess(
    const std::string& command) {
  PreprocessedCommand result;
  result.processed_text = command;

  if (g_debug_mode) {
    std::cerr << "DEBUG: Preprocessing command: " << command << std::endl;
  }

  // First handle here documents
  result.processed_text =
      process_here_documents(result.processed_text, result.here_documents);

  // Then handle subshells
  std::string original_text = result.processed_text;
  result.processed_text = process_subshells(result.processed_text);
  result.has_subshells = (original_text != result.processed_text);

  result.needs_special_handling =
      !result.here_documents.empty() || result.has_subshells;

  if (g_debug_mode && result.needs_special_handling) {
    std::cerr << "DEBUG: Preprocessed to: " << result.processed_text
              << std::endl;
    if (!result.here_documents.empty()) {
      std::cerr << "DEBUG: Found " << result.here_documents.size()
                << " here documents" << std::endl;
    }
    if (result.has_subshells) {
      std::cerr << "DEBUG: Processed subshells" << std::endl;
    }
  }

  return result;
}

std::string CommandPreprocessor::process_here_documents(
    const std::string& command, std::map<std::string, std::string>& here_docs) {
  std::string result = command;

  // Look for here document pattern: command << DELIMITER
  size_t here_pos = result.find("<<");
  if (here_pos == std::string::npos) {
    return result;
  }

  // Find the delimiter
  size_t delim_start = here_pos + 2;
  while (delim_start < result.size() && std::isspace(result[delim_start])) {
    delim_start++;
  }

  size_t delim_end = delim_start;
  while (delim_end < result.size() && !std::isspace(result[delim_end])) {
    delim_end++;
  }

  if (delim_start >= delim_end) {
    return result;
  }

  std::string delimiter = result.substr(delim_start, delim_end - delim_start);

  // Find the content between delimiter lines
  size_t content_start = result.find('\n', delim_end);
  if (content_start == std::string::npos) {
    return result;
  }
  content_start++;  // Skip the newline

  // Look for line containing just the delimiter
  std::string delimiter_pattern = "\n" + delimiter;
  size_t content_end = result.find(delimiter_pattern, content_start);

  if (content_end == std::string::npos) {
    return result;
  }

  // Extract here document content
  std::string content =
      result.substr(content_start, content_end - content_start);

  // Generate placeholder using a simple marker approach
  std::string placeholder =
      "HEREDOC_PLACEHOLDER_" + std::to_string(++placeholder_counter);
  here_docs[placeholder] = content;

  // Replace here document with input redirection to placeholder
  std::string before_here = result.substr(0, here_pos);
  std::string after_delimiter =
      result.substr(content_end + delimiter.length() + 1);

  result = before_here + "< " + placeholder + after_delimiter;

  if (g_debug_mode) {
    std::cerr << "DEBUG: Extracted here document with delimiter '" << delimiter
              << "' to placeholder '" << placeholder << "'" << std::endl;
  }

  return result;
}

std::string CommandPreprocessor::process_subshells(const std::string& command) {
  std::string result = command;

  // Look for subshell pattern at start: (command) possibly followed by
  // redirection
  if (result.empty() || result[0] != '(') {
    return result;
  }

  size_t close_paren = find_matching_paren(result, 0);
  if (close_paren == std::string::npos) {
    return result;  // No matching closing parenthesis
  }

  // Extract subshell content
  std::string subshell_content = result.substr(1, close_paren - 1);
  std::string remaining = result.substr(close_paren + 1);

  // Convert to sh -c equivalent
  // Need to properly escape the content for shell execution
  std::string escaped_content = subshell_content;

  // Escape single quotes by replacing ' with '\''
  size_t pos = 0;
  while ((pos = escaped_content.find('\'', pos)) != std::string::npos) {
    escaped_content.replace(pos, 1, "'\\''");
    pos += 4;
  }

  result = "sh -c '" + escaped_content + "'" + remaining;

  if (g_debug_mode) {
    std::cerr << "DEBUG: Converted subshell (" << subshell_content
              << ") to: sh -c '" << escaped_content << "'" << remaining
              << std::endl;
  }

  return result;
}

std::string CommandPreprocessor::generate_placeholder() {
  return "HEREDOC_PLACEHOLDER_" + std::to_string(++placeholder_counter);
}

size_t CommandPreprocessor::find_matching_paren(const std::string& text,
                                                size_t start_pos) {
  if (start_pos >= text.length() || text[start_pos] != '(') {
    return std::string::npos;
  }

  int depth = 0;
  for (size_t i = start_pos; i < text.length(); ++i) {
    if (is_inside_quotes(text, i)) {
      continue;
    }

    if (text[i] == '(') {
      depth++;
    } else if (text[i] == ')') {
      depth--;
      if (depth == 0) {
        return i;
      }
    }
  }

  return std::string::npos;
}

bool CommandPreprocessor::is_inside_quotes(const std::string& text,
                                           size_t pos) {
  bool in_single = false;
  bool in_double = false;
  bool escaped = false;

  for (size_t i = 0; i < pos && i < text.length(); ++i) {
    if (escaped) {
      escaped = false;
      continue;
    }

    if (text[i] == '\\') {
      escaped = true;
    } else if (text[i] == '\'' && !in_double) {
      in_single = !in_single;
    } else if (text[i] == '"' && !in_single) {
      in_double = !in_double;
    }
  }

  return in_single || in_double;
}
