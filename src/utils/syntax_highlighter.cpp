#include "syntax_highlighter.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>

#include "cjsh.h"
#include "cjsh_filesystem.h"

const std::unordered_set<std::string> SyntaxHighlighter::basic_unix_commands_ =
    {"ls",    "cd",    "pwd",   "echo",  "cat",   "mv",     "cp",
     "rm",    "mkdir", "rmdir", "touch", "grep",  "find",   "chmod",
     "chown", "kill",  "ps",    "man",   "which", "whereis"};
std::unordered_set<std::string> SyntaxHighlighter::external_executables_;
const std::unordered_set<std::string> SyntaxHighlighter::command_operators_ = {
    "&&", "||", "|", ";"};

void SyntaxHighlighter::initialize() {
  for (const auto& e : cjsh_filesystem::read_cached_executables()) {
    external_executables_.insert(e.filename().string());
  }
}

void SyntaxHighlighter::highlight(ic_highlight_env_t* henv, const char* input,
                                  void* /*arg*/) {
  if (!g_shell->get_menu_active() && input[0] != ':') {
    return;
  }
  size_t len = std::strlen(input);

  if (!g_shell->get_menu_active() && input[0] == ':') {
    ic_highlight(henv, 0, 1, "cjsh-colon");

    size_t i = 0;
    while (i < len && !std::isspace((unsigned char)input[i]))
      ++i;
    std::string token(input, i);

    if (token.size() > 1) {
      std::string sub = token.substr(1);
      if (sub.rfind("./", 0) == 0) {
        if (!std::filesystem::exists(sub) ||
            !std::filesystem::is_regular_file(sub)) {
          ic_highlight(henv, 1, i - 1, "cjsh-unknown-command");
        } else {
          ic_highlight(henv, 1, i - 1, "cjsh-known-command");
        }
      } else {
        auto cmds = g_shell->get_available_commands();
        if (std::find(cmds.begin(), cmds.end(), sub) != cmds.end()) {
          ic_highlight(henv, 1, i - 1, "cjsh-known-command");
        } else if (basic_unix_commands_.count(sub) > 0) {
          ic_highlight(henv, 1, i - 1, "cjsh-known-command");
        } else if (external_executables_.count(sub) > 0) {
          ic_highlight(henv, 1, i - 1, "cjsh-external-command");
        } else {
          ic_highlight(henv, 1, i - 1, "cjsh-unknown-command");
        }
      }
    }
    return;
  }

  size_t pos = 0;
  while (pos < len) {
    size_t cmd_end = pos;
    while (cmd_end < len) {
      if ((cmd_end + 1 < len && input[cmd_end] == '&' &&
           input[cmd_end + 1] == '&') ||
          (cmd_end + 1 < len && input[cmd_end] == '|' &&
           input[cmd_end + 1] == '|') ||
          input[cmd_end] == '|' || input[cmd_end] == ';') {
        break;
      }
      cmd_end++;
    }

    size_t cmd_start = pos;
    while (cmd_start < cmd_end &&
           std::isspace((unsigned char)input[cmd_start])) {
      cmd_start++;
    }

    std::string cmd_str(input + cmd_start, cmd_end - cmd_start);

    size_t token_end = 0;
    while (token_end < cmd_str.length() &&
           !std::isspace((unsigned char)cmd_str[token_end])) {
      token_end++;
    }

    std::string token = token_end > 0 ? cmd_str.substr(0, token_end) : "";

    bool is_sudo_command = (token == "sudo");

    if (!token.empty()) {
      if (token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0 || token.rfind("~/", 0) == 0 || token.rfind("-/", 0) == 0 || token[0] == '/' || token.find('/') != std::string::npos) {
        std::string path_to_check = token;
        if (token.rfind("~/", 0) == 0) {
          path_to_check = cjsh_filesystem::g_user_home_path.string() + token.substr(1);
        } else if (token.rfind("-/", 0) == 0) {
          std::string prev_dir = g_shell->get_previous_directory();
          if (!prev_dir.empty()) {
            path_to_check = prev_dir + token.substr(1);
          }
        } else if (token[0] != '/' && token.rfind("./", 0) != 0 && token.rfind("../", 0) != 0 && token.rfind("~/", 0) != 0 && token.rfind("-/", 0) != 0) {
          path_to_check = std::filesystem::current_path().string() + "/" + token;
        }
        if (std::filesystem::exists(path_to_check)) {
          ic_highlight(henv, cmd_start, token_end, "cjsh-known-command");
        } else {
          ic_highlight(henv, cmd_start, token_end, "cjsh-unknown-command");
        }
      } else {
        auto cmds = g_shell->get_available_commands();
        if (std::find(cmds.begin(), cmds.end(), token) != cmds.end()) {
          ic_highlight(henv, cmd_start, token_end, "cjsh-known-command");
        } else if (basic_unix_commands_.count(token) > 0) {
          ic_highlight(henv, cmd_start, token_end, "cjsh-known-command");
        } else if (external_executables_.count(token) > 0) {
          ic_highlight(henv, cmd_start, token_end, "cjsh-external-command");
        } else {
          ic_highlight(henv, cmd_start, token_end, "cjsh-unknown-command");
        }
      }
    } // <-- Correct closing for if (!token.empty())

    bool is_cd_command = (token == "cd");
    size_t arg_start = token_end;

    while (arg_start < cmd_str.length()) {
      while (arg_start < cmd_str.length() &&
             std::isspace((unsigned char)cmd_str[arg_start])) {
        arg_start++;
      }
      if (arg_start >= cmd_str.length())
        break;

      size_t arg_end = arg_start;
      while (arg_end < cmd_str.length() &&
             !std::isspace((unsigned char)cmd_str[arg_end])) {
        arg_end++;
      }

      std::string arg = cmd_str.substr(arg_start, arg_end - arg_start);

      if (is_sudo_command && arg_start == token_end + 1) {
        if (arg.rfind("./", 0) == 0) {
          if (!std::filesystem::exists(arg) ||
              !std::filesystem::is_regular_file(arg)) {
            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                         "cjsh-unknown-command");
          } else {
            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                         "cjsh-known-command");
          }
        } else {
          auto cmds = g_shell->get_available_commands();
          if (std::find(cmds.begin(), cmds.end(), arg) != cmds.end()) {
            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                         "cjsh-known-command");
          } else if (basic_unix_commands_.count(arg) > 0) {
            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                         "cjsh-known-command");
          } else if (external_executables_.count(arg) > 0) {
            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                         "cjsh-external-command");
          } else {
            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                         "cjsh-unknown-command");
          }
        }
      }

      if (is_cd_command && (arg == "~" || arg == "-")) {
        ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                     "cjsh-path-exists");
      } else if (is_cd_command || arg[0] == '/' || arg.rfind("./", 0) == 0 ||
                 arg.rfind("../", 0) == 0 || arg.rfind("~/", 0) == 0 ||
                 arg.rfind("-/", 0) == 0 || arg.find('/') != std::string::npos) {
        std::string path_to_check = arg;

        if (arg.rfind("~/", 0) == 0) {
          path_to_check =
              cjsh_filesystem::g_user_home_path.string() + arg.substr(1);
        } else if (arg.rfind("-/", 0) == 0) {
          std::string prev_dir = g_shell->get_previous_directory();
          if (!prev_dir.empty()) {
            path_to_check = prev_dir + arg.substr(1);
          }
        } else if (is_cd_command && arg[0] != '/' && arg.rfind("./", 0) != 0 &&
                   arg.rfind("../", 0) != 0 && arg.rfind("~/", 0) != 0 &&
                   arg.rfind("-/", 0) != 0) {
          path_to_check = std::filesystem::current_path().string() + "/" + arg;
        }

        if (std::filesystem::exists(path_to_check)) {
          ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                       "cjsh-path-exists");
        } else {
          ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                       "cjsh-path-not-exists");
        }
      }

      arg_start = arg_end;
    }

    pos = cmd_end;
    if (pos < len) {
      if (pos + 1 < len && input[pos] == '&' && input[pos + 1] == '&') {
        ic_highlight(henv, pos, 2, "cjsh-operator");
        pos += 2;
      } else if (pos + 1 < len && input[pos] == '|' && input[pos + 1] == '|') {
        ic_highlight(henv, pos, 2, "cjsh-operator");
        pos += 2;
      } else if (input[pos] == '|') {
        ic_highlight(henv, pos, 1, "cjsh-operator");
        pos += 1;
      } else if (input[pos] == ';') {
        ic_highlight(henv, pos, 1, "cjsh-operator");
        pos += 1;
      } else {
        pos += 1;
      }
    }
  }
}