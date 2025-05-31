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

  if (g_debug_mode)
    std::cerr << "DEBUG: Detecting completion context for prefix: '" << prefix
              << "'" << std::endl;

  if (prefix_str.find('/') == 0 || prefix_str.find("./") == 0 ||
      prefix_str.find("../") == 0) {
    if (g_debug_mode) std::cerr << "DEBUG: Context detected: PATH" << std::endl;
    return CONTEXT_PATH;
  }

  if (prefix_str.find(' ') != std::string::npos) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Context detected: ARGUMENT" << std::endl;
    return CONTEXT_ARGUMENT;
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: Context detected: COMMAND" << std::endl;
  return CONTEXT_COMMAND;
}

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Command completer called with prefix: '" << prefix
              << "'" << std::endl;

  if (ic_stop_completing(cenv)) return;

  size_t prefix_len = std::strlen(prefix);
  auto cmds = g_shell->get_available_commands();
  for (const auto& cmd : cmds) {
    if (cmd.rfind(prefix, 0) == 0) {
      std::string suffix = cmd.substr(prefix_len);
      if (g_debug_mode)
        std::cerr << "DEBUG: Command completion found: '" << cmd
                  << "' (adding suffix: '" << suffix << "')" << std::endl;
      if (!ic_add_completion(cenv, suffix.c_str())) return;
    }
    if (ic_stop_completing(cenv)) return;
  }

  if (g_debug_mode && !ic_has_completions(cenv))
    std::cerr << "DEBUG: No command completions found for prefix: '" << prefix
              << "'" << std::endl;
}

void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: History completer called with prefix: '" << prefix
              << "'" << std::endl;

  if (ic_stop_completing(cenv)) return;

  size_t prefix_len = std::strlen(prefix);
  if (prefix_len == 0) {
    if (g_debug_mode)
      std::cerr << "DEBUG: History completer skipped (empty prefix)"
                << std::endl;
    return;
  }

  std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path);
  if (!history_file.is_open()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Failed to open history file: "
                << cjsh_filesystem::g_cjsh_history_path << std::endl;
    return;
  }

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

  if (g_debug_mode)
    std::cerr << "DEBUG: Found " << matches.size()
              << " history matches for prefix: '" << prefix << "'" << std::endl;

  std::sort(matches.begin(), matches.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  const size_t max_suggestions = 20;
  size_t count = 0;

  for (const auto& match : matches) {
    std::string suffix = match.first.substr(prefix_len);
    if (g_debug_mode)
      std::cerr << "DEBUG: Adding history completion: '" << match.first
                << "' (freq: " << match.second << ")" << std::endl;
    if (!ic_add_completion(cenv, suffix.c_str())) return;
    if (++count >= max_suggestions || ic_stop_completing(cenv)) return;
  }
}

void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Filename completer called with prefix: '" << prefix
              << "'" << std::endl;

  if (ic_stop_completing(cenv)) return;

  std::string prefix_str(prefix);
  size_t last_space = prefix_str.find_last_of(" \t");

  bool has_tilde = false;
  bool has_dash = false;
  std::string prefix_before = "";
  std::string special_part = "";

  if (last_space != std::string::npos && last_space + 1 < prefix_str.length()) {
    if (prefix_str[last_space + 1] == '~') {
      has_tilde = true;
      prefix_before = prefix_str.substr(0, last_space + 1);
      special_part = prefix_str.substr(last_space + 1);
    } else if (prefix_str[last_space + 1] == '-' &&
               (prefix_str.length() == last_space + 2 ||
                prefix_str[last_space + 2] == '/')) {
      has_dash = true;
      prefix_before = prefix_str.substr(0, last_space + 1);
      special_part = prefix_str.substr(last_space + 1);
    }
  } else if (prefix_str[0] == '~') {
    has_tilde = true;
    special_part = prefix_str;
  } else if (prefix_str[0] == '-' &&
             (prefix_str.length() == 1 || prefix_str[1] == '/')) {
    has_dash = true;
    special_part = prefix_str;
  }

  // Handle tilde expansion
  if (has_tilde && (special_part.length() == 1 || special_part[1] == '/')) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Processing tilde completion: '" << special_part
                << "'" << std::endl;

    std::string path_after_tilde =
        special_part.length() > 1 ? special_part.substr(2) : "";
    std::string dir_to_complete = cjsh_filesystem::g_user_home_path.string();

    if (special_part.length() > 1) {
      dir_to_complete += "/" + path_after_tilde;
    }

    namespace fs = std::filesystem;
    fs::path dir_path;
    std::string match_prefix;

    if (special_part.back() == '/') {
      dir_path = dir_to_complete;
      match_prefix = "";
    } else {
      size_t last_slash = dir_to_complete.find_last_of('/');
      if (last_slash != std::string::npos) {
        dir_path = dir_to_complete.substr(0, last_slash);
        match_prefix = dir_to_complete.substr(last_slash + 1);
      } else {
        dir_path = dir_to_complete;
        match_prefix = "";
      }
    }

    if (g_debug_mode) {
      std::cerr << "DEBUG: Looking in directory: '" << dir_path << "'"
                << std::endl;
      std::cerr << "DEBUG: Matching prefix: '" << match_prefix << "'"
                << std::endl;
    }

    try {
      if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          std::string filename = entry.path().filename().string();

          if (match_prefix.empty() || filename.find(match_prefix) == 0) {
            std::string completion_suffix;

            if (match_prefix.empty()) {
              completion_suffix = filename;
            } else {
              completion_suffix = filename.substr(match_prefix.length());
            }

            if (entry.is_directory()) {
              completion_suffix += "/";
            }

            if (g_debug_mode)
              std::cerr << "DEBUG: Adding tilde completion: '"
                        << completion_suffix << "'" << std::endl;

            if (!ic_add_completion(cenv, completion_suffix.c_str())) return;
          }
        }
      }
    } catch (const std::exception& e) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Error reading directory: " << e.what()
                  << std::endl;
    }

    return;
  }

  else if (has_dash && (special_part.length() == 1 || special_part[1] == '/')) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Processing dash completion for previous directory: '"
                << special_part << "'" << std::endl;

    std::string path_after_dash =
        special_part.length() > 1 ? special_part.substr(2) : "";
    std::string dir_to_complete = g_shell->get_previous_directory();

    if (dir_to_complete.empty()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: No previous directory set" << std::endl;
      return;
    }

    if (special_part.length() > 1) {
      dir_to_complete += "/" + path_after_dash;
    }

    namespace fs = std::filesystem;
    fs::path dir_path;
    std::string match_prefix;

    if (special_part.back() == '/') {
      dir_path = dir_to_complete;
      match_prefix = "";
    } else {
      size_t last_slash = dir_to_complete.find_last_of('/');
      if (last_slash != std::string::npos) {
        dir_path = dir_to_complete.substr(0, last_slash);
        match_prefix = dir_to_complete.substr(last_slash + 1);
      } else {
        dir_path = dir_to_complete;
        match_prefix = "";
      }
    }

    if (g_debug_mode) {
      std::cerr << "DEBUG: Looking in directory: '" << dir_path << "'"
                << std::endl;
      std::cerr << "DEBUG: Matching prefix: '" << match_prefix << "'"
                << std::endl;
    }

    try {
      if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          std::string filename = entry.path().filename().string();

          if (match_prefix.empty() || filename.find(match_prefix) == 0) {
            std::string completion_suffix;

            if (match_prefix.empty()) {
              completion_suffix = filename;
            } else {
              completion_suffix = filename.substr(match_prefix.length());
            }

            if (entry.is_directory()) {
              completion_suffix += "/";
            }

            if (g_debug_mode)
              std::cerr << "DEBUG: Adding dash completion: '"
                        << completion_suffix << "'" << std::endl;

            if (!ic_add_completion(cenv, completion_suffix.c_str())) return;
          }
        }
      }
    } catch (const std::exception& e) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Error reading directory: " << e.what()
                  << std::endl;
    }

    return;
  }

  ic_complete_filename(cenv, prefix, '/', nullptr, nullptr);

  if (g_debug_mode) {
    if (ic_has_completions(cenv))
      std::cerr << "DEBUG: Filename completions found for prefix: '" << prefix
                << "'" << std::endl;
    else
      std::cerr << "DEBUG: No filename completions found for prefix: '"
                << prefix << "'" << std::endl;
  }
}

void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Default completer called with prefix: '" << prefix
              << "'" << std::endl;

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

  // ic_style_def("cjsh-known-command", "bold color=#00FF00");
  ic_style_def("cjsh-unknown-command", "bold color=#FF0000");
  // ic_style_def("cjsh-external-command", "bold color=#00FF00");
  ic_style_def("cjsh-colon", "bold color=#00FFFF");
  ic_style_def("cjsh-path-exists", "color=#00FF00");
  ic_style_def("cjsh-path-not-exists", "color=#FF0000");
  ic_style_def("cjsh-operator",
               "bold color=#FFCC00");  // Operator style for &&, ||, |, ;

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
  ic_enable_multiline(true);
  ic_set_prompt_marker("", NULL);
  ic_enable_auto_tab(true);
  ic_set_history(cjsh_filesystem::g_cjsh_history_path.c_str(), -1);
  ic_enable_completion_preview(true);
}

void update_completion_frequency(const std::string& command) {
  if (g_debug_mode) {
    if (!command.empty())
      std::cerr << "DEBUG: Updating completion frequency for command: '"
                << command << "'" << std::endl;
    else
      std::cerr << "DEBUG: Skipped updating frequency (empty command)"
                << std::endl;
  }

  if (!command.empty()) {
    g_completion_frequency[command]++;
  }
}
