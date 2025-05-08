#include "syntax_highlighter.h"

#include <algorithm>
#include <cctype>
#include <cstring>
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
  size_t len = std::strlen(input), i = 0;
  while (i < len && !std::isspace((unsigned char)input[i])) ++i;
  std::string token(input, i);

  if (!token.empty()) {
    auto cmds = g_shell->get_available_commands();
    if (std::find(cmds.begin(), cmds.end(), token) != cmds.end()) {
      ic_highlight(henv, 0, i, "cjsh-known-command");
    } else if (basic_unix_commands_.count(token) ||
               external_executables_.count(token)) {
      ic_highlight(henv, 0, i, "cjsh-external-command");
    } else {
      ic_highlight(henv, 0, i, "cjsh-unknown-command");
    }
  }
}