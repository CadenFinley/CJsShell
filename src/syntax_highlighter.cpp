#include "syntax_highlighter.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>

#include "cjsh_filesystem.h"
#include "main.h"

const std::unordered_set<std::string> SyntaxHighlighter::basic_unix_commands_ =
    {"ls",    "cd",    "pwd",   "echo",  "cat",   "mv",     "cp",
     "rm",    "mkdir", "rmdir", "touch", "grep",  "find",   "chmod",
     "chown", "kill",  "ps",    "man",   "which", "whereis"};
std::unordered_set<std::string> SyntaxHighlighter::external_executables_;

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
    size_t len = std::strlen(input), i = 0;
    while (i < len && !std::isspace((unsigned char)input[i])) ++i;
    std::string token(input, i);

    if (token.rfind("./", 0) == 0) {
      if (!std::filesystem::exists(token) ||
          !std::filesystem::is_regular_file(token)) {
        ic_highlight(henv, 0, i, "cjsh-unknown-command");
      }
      return;
    }

    if (!token.empty()) {
      auto cmds = g_shell->get_available_commands();
      if (std::find(cmds.begin(), cmds.end(), token) == cmds.end() &&
          basic_unix_commands_.count(token) == 0 &&
          external_executables_.count(token) == 0) {
        ic_highlight(henv, 0, i, "cjsh-unknown-command");
      }
    }
}