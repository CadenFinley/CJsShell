#include "completions.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "../vendor/isocline/include/isocline.h"
#include "cjsh_filesystem.h"
#include "main.h"
#include "shell.h"
#include "syntax_highlighter.h"

std::map<std::string, int> g_completion_frequency;
enum CompletionContext { CONTEXT_COMMAND, CONTEXT_ARGUMENT, CONTEXT_PATH };

CompletionContext detect_completion_context(const char* prefix) {
  std::string prefix_str(prefix);

  if (prefix_str.find('/') == 0 || prefix_str.find("./") == 0 ||
      prefix_str.find("../") == 0) {
    return CONTEXT_PATH;
  }

  if (prefix_str.find(' ') != std::string::npos) {
    return CONTEXT_ARGUMENT;
  }

  return CONTEXT_COMMAND;
}

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (ic_stop_completing(cenv)) return;

  size_t prefix_len = std::strlen(prefix);
  auto cmds = g_shell->get_available_commands();
  for (const auto& cmd : cmds) {
    if (cmd.rfind(prefix, 0) == 0) {
      std::string suffix = cmd.substr(prefix_len);
      if (!ic_add_completion(cenv, suffix.c_str())) return;
    }
    if (ic_stop_completing(cenv)) return;
  }
}

void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (ic_stop_completing(cenv)) return;

  size_t prefix_len = std::strlen(prefix);
  if (prefix_len == 0) return;
  std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path);
  if (!history_file.is_open()) return;

  std::string line;
  std::vector<std::pair<std::string, int>> matches;

  while (std::getline(history_file, line)) {
    if (line.rfind(prefix, 0) == 0 && line != prefix) {
      if (g_completion_frequency.find(line) == g_completion_frequency.end()) {
        g_completion_frequency[line] = 1;
      }
      matches.push_back({line, g_completion_frequency[line]});
    }
  }

  std::sort(matches.begin(), matches.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  const size_t max_suggestions = 20;
  size_t count = 0;

  for (const auto& match : matches) {
    std::string suffix = match.first.substr(prefix_len);
    if (!ic_add_completion(cenv, suffix.c_str())) return;
    if (++count >= max_suggestions || ic_stop_completing(cenv)) return;
  }
}

void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (ic_stop_completing(cenv)) return;
  ic_complete_filename(cenv, prefix, '/', nullptr, nullptr);
}

void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (ic_stop_completing(cenv)) return;
  CompletionContext context = detect_completion_context(prefix);

  switch (context) {
    case CONTEXT_COMMAND:
      cjsh_history_completer(cenv, prefix);
      if (ic_has_completions(cenv) && ic_stop_completing(cenv)) return;

      cjsh_command_completer(cenv, prefix);
      if (ic_has_completions(cenv) && ic_stop_completing(cenv)) return;

      cjsh_filename_completer(cenv, prefix);
      break;

    case CONTEXT_PATH:
      cjsh_filename_completer(cenv, prefix);
      break;

    case CONTEXT_ARGUMENT:
      cjsh_filename_completer(cenv, prefix);
      break;
  }
}

void initialize_completion_system() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing completion system" << std::endl;

  ic_style_def("cjsh-known-command", "bold color=#00FF00");
  ic_style_def("cjsh-unknown-command", "bold color=#FF0000");
  ic_style_def("cjsh-external-command", "bold color=#00FF00");

  // for (const auto& e : cjsh_filesystem::read_cached_executables()) {
  //     external_executables.insert(e.filename().string());
  // }

  ic_set_default_completer(cjsh_default_completer, NULL);

  SyntaxHighlighter::initialize();
  ic_set_default_highlighter(SyntaxHighlighter::highlight, NULL);

  ic_enable_completion_preview(true);
  ic_enable_hint(true);
  ic_set_hint_delay(0);
  ic_enable_highlight(true);
  ic_enable_history_duplicates(false);
  ic_enable_inline_help(false);
  ic_enable_multiline_indent(false);
  ic_set_prompt_marker("", NULL);
  ic_enable_auto_tab(true);
  ic_set_history(cjsh_filesystem::g_cjsh_history_path.c_str(), -1);
  ic_enable_completion_preview(true);
}

void update_completion_frequency(const std::string& command) {
  if (!command.empty()) {
    g_completion_frequency[command]++;
  }
}
