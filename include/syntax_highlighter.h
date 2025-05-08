#pragma once

#include <string>
#include <unordered_set>

#include "../vendor/isocline/include/isocline.h"

class SyntaxHighlighter {
 public:
  static void initialize();

  static void highlight(ic_highlight_env_t* henv, const char* input, void* arg);

 private:
  static const std::unordered_set<std::string> basic_unix_commands_;
  static std::unordered_set<std::string> external_executables_;
};
