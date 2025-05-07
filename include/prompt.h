#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "prompt_info.h"
#include "theme.h"

class Prompt {
 private:
  PromptInfo info;
  std::string replace_placeholder(const std::string& format,
                                  const std::string& placeholder,
                                  const std::string& value);

 public:
  Prompt();
  ~Prompt();
  std::string get_prompt();
  std::string get_ai_prompt();
  std::string get_title_prompt();
  std::string get_newline_prompt();
};
