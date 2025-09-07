#pragma once

#include <map>
#include <string>
#include <vector>

/**
 * CommandPreprocessor handles complex shell constructs that need special
 * processing before normal parsing can occur. This includes:
 * - Here documents (<<)
 * - Subshells (command substitution with parentheses)
 * - Complex redirection patterns
 *
 * This separates preprocessing logic from the main parser and command
 * execution, making the codebase cleaner and more maintainable.
 */
class CommandPreprocessor {
 public:
  struct PreprocessedCommand {
    std::string processed_text;
    std::map<std::string, std::string>
        here_documents;  // placeholder -> content
    bool has_subshells = false;
    bool needs_special_handling = false;
  };

  /**
   * Preprocess a command string, handling here documents and subshells
   * @param command The raw command string
   * @return PreprocessedCommand with processed text and metadata
   */
  static PreprocessedCommand preprocess(const std::string& command);

 private:
  /**
   * Extract and replace here documents with placeholders
   * @param command Command string to process
   * @param here_docs Map to store here document content
   * @return Modified command string with placeholders
   */
  static std::string process_here_documents(
      const std::string& command,
      std::map<std::string, std::string>& here_docs);

  /**
   * Convert subshell syntax to executable commands
   * @param command Command string to process
   * @return Modified command string with subshells converted
   */
  static std::string process_subshells(const std::string& command);

  /**
   * Generate a unique placeholder for here document content
   * @return Unique placeholder string
   */
  static std::string generate_placeholder();

  /**
   * Find matching parentheses considering quotes and escaping
   * @param text Text to search
   * @param start_pos Starting position
   * @return Position of matching closing parenthesis or string::npos
   */
  static size_t find_matching_paren(const std::string& text, size_t start_pos);

  /**
   * Find matching braces considering quotes and escaping
   * @param text Text to search
   * @param start_pos Starting position
   * @return Position of matching closing brace or string::npos
   */
  static size_t find_matching_brace(const std::string& text, size_t start_pos);

  /**
   * Check if a position in the string is inside quotes
   * @param text The string to check
   * @param pos Position to check
   * @return true if position is inside quotes
   */
  static bool is_inside_quotes(const std::string& text, size_t pos);

  static int placeholder_counter;
};
