#include "shell_script_interpreter.h"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

#include "cjsh.h"
#include "shell.h"

ShellScriptInterpreter::ShellScriptInterpreter() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing ShellScriptInterpreter" << std::endl;
  debug_level = DebugLevel::NONE;
  // Parser will be provided by Shell after construction.
  shell_parser = nullptr;
}

ShellScriptInterpreter::~ShellScriptInterpreter() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Destroying ShellScriptInterpreter" << std::endl;
}

void ShellScriptInterpreter::set_debug_level(DebugLevel level) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting script interpreter debug level to "
              << static_cast<int>(level) << std::endl;
  debug_level = level;
}

DebugLevel ShellScriptInterpreter::get_debug_level() const {
  return debug_level;
}

int ShellScriptInterpreter::execute_block(const std::vector<std::string>& lines) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Executing script block with " << lines.size()
              << " lines" << std::endl;

  if (g_shell == nullptr) {
    std::cerr << "Error: No shell instance available" << std::endl;
    return 1;
  }

  if (!shell_parser) {
    std::cerr << "Error: Script interpreter not initialized with a Parser" << std::endl;
    return 1;
  }

  auto trim = [](const std::string& s) -> std::string {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
  };

  auto strip_inline_comment = [](const std::string& s) -> std::string {
    bool in_quotes = false;
    char quote = '\0';
    for (size_t i = 0; i < s.size(); ++i) {
      char c = s[i];
      if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
        if (!in_quotes) {
          in_quotes = true;
          quote = c;
        } else if (quote == c) {
          in_quotes = false;
          quote = '\0';
        }
      } else if (!in_quotes && c == '#') {
        return s.substr(0, i);
      }
    }
    return s;
  };

  auto is_readable_file = [](const std::string& path) -> bool {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(path.c_str(), R_OK) == 0;
  };

  auto should_interpret_as_cjsh = [&](const std::string& path) -> bool {
    if (!is_readable_file(path)) return false;
    // Heuristics: .cjsh extension, or shebang mentions cjsh, or first line contains 'cjsh'
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".cjsh") return true;
    std::ifstream f(path);
    if (!f) return false;
    std::string first_line;
    std::getline(f, first_line);
    if (first_line.rfind("#!", 0) == 0 && first_line.find("cjsh") != std::string::npos) return true;
    if (first_line.find("cjsh") != std::string::npos) return true;
    return false;
  };

  auto execute_simple_or_pipeline = [&](const std::string& cmd_text) -> int {
    // Decide between simple exec via Shell::execute_command vs Exec::execute_pipeline
    std::string text = trim(strip_inline_comment(cmd_text));
    if (text.empty()) return 0;

    // If the command is exactly a readable cjsh script path, interpret it
    // Otherwise, if it's a regular readable file without exec bit, interpret as well
    // Tokenize minimally to get the program token
    std::vector<std::string> head = shell_parser->parse_command(text);
    if (!head.empty()) {
      const std::string& prog = head[0];
      if (should_interpret_as_cjsh(prog)) {
        if (g_debug_mode) std::cerr << "DEBUG: Interpreting script file: " << prog << std::endl;
        std::ifstream f(prog);
        if (!f) {
          std::cerr << "cjsh: failed to open script: " << prog << std::endl;
          return 1;
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        auto nested_lines = shell_parser->parse_into_lines(buffer.str());
        return execute_block(nested_lines);
      }
    }

    // Use pipeline parser to capture redirections, background, and pipes
    std::vector<Command> cmds = shell_parser->parse_pipeline(text);
    bool has_redir_or_pipe = cmds.size() > 1;
    if (!has_redir_or_pipe && !cmds.empty()) {
      const auto& c = cmds[0];
      has_redir_or_pipe = c.background || !c.input_file.empty() || !c.output_file.empty() || !c.append_file.empty() || c.stderr_to_stdout || !c.here_doc.empty();
    }

    if (!has_redir_or_pipe && !cmds.empty()) {
      // Simple command path - leverage Shell::execute_command for builtins/plugins/env-assignments
      const auto& c = cmds[0];
      // Re-parse the original text to apply alias/env/brace/tilde expansions
      // parse_command performs alias expansion while parse_pipeline does not.
      std::vector<std::string> expanded_args = shell_parser->parse_command(text);
      if (expanded_args.empty()) return 0;
      if (g_debug_mode) {
        std::cerr << "DEBUG: Simple exec: ";
        for (const auto& a : expanded_args) std::cerr << "[" << a << "]";
        if (c.background) std::cerr << " &";
        std::cerr << std::endl;
      }
      return g_shell->execute_command(expanded_args, c.background);
    }

    // Pipeline or with redirections
    if (cmds.empty()) return 0;
    if (g_debug_mode) {
      std::cerr << "DEBUG: Executing pipeline of size " << cmds.size() << std::endl;
    }
    return g_shell->shell_exec->execute_pipeline(cmds);
  };

  int last_code = 0;

  for (const auto& raw_line : lines) {
    std::string line = trim(strip_inline_comment(raw_line));
    if (line.empty()) {
      if (g_debug_mode) std::cerr << "DEBUG: skipping empty/comment line" << std::endl;
      continue;
    }

    // Break the line into logical command segments (&&, ||) while preserving order
    std::vector<LogicalCommand> lcmds = shell_parser->parse_logical_commands(line);
    if (lcmds.empty()) continue;

    last_code = 0;
    for (size_t i = 0; i < lcmds.size(); ++i) {
      const auto& lc = lcmds[i];
      // Short-circuiting based on previous op
      if (i > 0) {
        const std::string& prev_op = lcmds[i - 1].op;
        if (prev_op == "&&" && last_code != 0) {
          if (g_debug_mode) std::cerr << "DEBUG: Skipping due to && short-circuit" << std::endl;
          continue;
        }
        if (prev_op == "||" && last_code == 0) {
          if (g_debug_mode) std::cerr << "DEBUG: Skipping due to || short-circuit" << std::endl;
          continue;
        }
      }

      // Split each logical segment further by semicolons
      auto semis = shell_parser->parse_semicolon_commands(lc.command);
      if (semis.empty()) {
        last_code = 0;
        continue;
      }
      for (const auto& cmd_text : semis) {
        int code = execute_simple_or_pipeline(cmd_text);
        last_code = code;
        if (code != 0 && debug_level >= DebugLevel::BASIC) {
          std::cerr << "DEBUG: Command failed (" << code << ") -> '" << cmd_text << "'" << std::endl;
        }
      }
    }

    if (last_code != 0) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Stopping script block due to exit code " << last_code << std::endl;
      return last_code;
    }
  }

  return last_code;
}
