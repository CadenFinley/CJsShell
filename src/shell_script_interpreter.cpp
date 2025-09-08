#include "shell_script_interpreter.h"

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

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

int ShellScriptInterpreter::execute_block(
    const std::vector<std::string>& lines) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Executing script block with " << lines.size()
              << " lines" << std::endl;

  if (g_shell == nullptr) {
    std::cerr << "Error: No shell instance available" << std::endl;
    return 1;
  }

  if (!shell_parser) {
    std::cerr << "Error: Script interpreter not initialized with a Parser"
              << std::endl;
    return 1;
  }

  auto trim = [](const std::string& s) -> std::string {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
      return "";
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
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
           access(path.c_str(), R_OK) == 0;
  };

  auto should_interpret_as_cjsh = [&](const std::string& path) -> bool {
    if (!is_readable_file(path))
      return false;
    // Heuristics: .cjsh extension, or shebang mentions cjsh, or first line
    // contains 'cjsh'
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".cjsh")
      return true;
    std::ifstream f(path);
    if (!f)
      return false;
    std::string first_line;
    std::getline(f, first_line);
    if (first_line.rfind("#!", 0) == 0 &&
        first_line.find("cjsh") != std::string::npos)
      return true;
    if (first_line.find("cjsh") != std::string::npos)
      return true;
    return false;
  };

  // Forward-declared executor to allow helper lambdas to call back into it
  std::function<int(const std::string&)> execute_simple_or_pipeline;

  execute_simple_or_pipeline = [&](const std::string& cmd_text) -> int {
    if (g_debug_mode) {
      std::cerr << "DEBUG: execute_simple_or_pipeline called with: " << cmd_text << std::endl;
    }
    // Decide between simple exec via Shell::execute_command vs
    // Exec::execute_pipeline
    std::string text = trim(strip_inline_comment(cmd_text));
    if (text.empty())
      return 0;

    // Note: SUBSHELL{} markers are now converted by the parser's
    // parse_pipeline_with_preprocessing() into an equivalent
    // "__INTERNAL_SUBSHELL__" command with any trailing redirections/pipes preserved.
    // We intentionally do not special-case SUBSHELL{} here to avoid
    // dropping following constructs like "2>&1 | ...".

    // Expand command substitution $(...) and arithmetic $((...)) without using
    // external sh
    auto capture_internal_output =
        [&](const std::string& content) -> std::string {
      // Create a unique temp file path
      char tmpl[] = "/tmp/cjsh_subst_XXXXXX";
      int fd = mkstemp(tmpl);
      if (fd >= 0)
        close(fd);
      std::string path = tmpl;

      // For command substitution, we need to execute the content with full
      // expansion and capture the output. The simplest approach is to redirect
      // stdout to a file.

      // Save current stdout
      int saved_stdout = dup(STDOUT_FILENO);

      // Open our temp file for writing
      FILE* temp_file = fopen(path.c_str(), "w");
      if (!temp_file) {
        // Fallback: use internal execution with pipe capture instead of /bin/sh
        int pipefd[2];
        if (pipe(pipefd) != 0) {
          return "";  // Unable to create pipe
        }
        
        pid_t pid = fork();
        if (pid == 0) {
          // Child process: redirect stdout to pipe and execute internally
          close(pipefd[0]);  // Close read end
          dup2(pipefd[1], STDOUT_FILENO);
          close(pipefd[1]);
          
          // Execute the content using internal execution mechanism
          int exit_code = execute_simple_or_pipeline(content);
          exit(exit_code);
        } else if (pid > 0) {
          // Parent process: read from pipe
          close(pipefd[1]);  // Close write end
          std::string result;
          char buf[4096];
          ssize_t n;
          while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
            result.append(buf, n);
          }
          close(pipefd[0]);
          
          // Wait for child to complete
          int status;
          waitpid(pid, &status, 0);
          
          // Trim trailing newlines
          while (!result.empty() &&
                 (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
          
          return result;
        } else {
          // Fork failed
          close(pipefd[0]);
          close(pipefd[1]);
          return "";
        }
      }

      // Redirect stdout to our temp file
      int temp_fd = fileno(temp_file);
      dup2(temp_fd, STDOUT_FILENO);

      // Execute the content with full expansion
      execute_simple_or_pipeline(content);

      // Restore stdout
      fflush(stdout);
      fclose(temp_file);
      dup2(saved_stdout, STDOUT_FILENO);
      close(saved_stdout);

      // Read the result
      std::ifstream ifs(path);
      std::stringstream buffer;
      buffer << ifs.rdbuf();
      std::string out = buffer.str();
      ::unlink(path.c_str());

      // Trim trailing newlines
      while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();

      return out;
    };

    auto eval_arith = [&](const std::string& expr) -> long long {
      // Shunting-yard for +,-,*,/,% and parentheses; variables via getenv
      struct Tok {
        enum T {
          NUM,
          OP,
          LP,
          RP
        } t;
        long long v;
        char op;
      };
      auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
      };
      auto prec = [](char op) {
        if (op == '+' || op == '-')
          return 1;
        if (op == '*' || op == '/' || op == '%')
          return 2;
        return 0;
      };
      auto apply = [](long long a, long long b, char op) -> long long {
        switch (op) {
          case '+':
            return a + b;
          case '-':
            return a - b;
          case '*':
            return a * b;
          case '/':
            return b == 0 ? 0 : a / b;
          case '%':
            return b == 0 ? 0 : a % b;
          default:
            return 0;
        }
      };
      std::vector<Tok> output;
      std::vector<char> ops;
      // Tokenize
      for (size_t i = 0; i < expr.size();) {
        if (is_space(expr[i])) {
          ++i;
          continue;
        }
        if (isdigit(expr[i])) {
          long long val = 0;
          size_t j = i;
          while (j < expr.size() && isdigit(expr[j])) {
            val = val * 10 + (expr[j] - '0');
            ++j;
          }
          output.push_back({Tok::NUM, val, 0});
          i = j;
          continue;
        }
        if (isalpha(expr[i]) || expr[i] == '_') {
          size_t j = i;
          while (j < expr.size() && (isalnum(expr[j]) || expr[j] == '_'))
            ++j;
          std::string name = expr.substr(i, j - i);
          const char* ev = getenv(name.c_str());
          long long val = 0;
          if (ev) {
            try {
              val = std::stoll(ev);
            } catch (...) {
              val = 0;
            }
          }
          output.push_back({Tok::NUM, val, 0});
          i = j;
          continue;
        }
        if (expr[i] == '(') {
          ops.push_back('(');
          ++i;
          continue;
        }
        if (expr[i] == ')') {
          while (!ops.empty() && ops.back() != '(') {
            char op = ops.back();
            ops.pop_back();
            output.push_back({Tok::OP, 0, op});
          }
          if (!ops.empty() && ops.back() == '(')
            ops.pop_back();
          ++i;
          continue;
        }
        char op = expr[i];
        if (op == '+' || op == '-' || op == '*' || op == '/' || op == '%') {
          while (!ops.empty() && ops.back() != '(' &&
                 prec(ops.back()) >= prec(op)) {
            char top = ops.back();
            ops.pop_back();
            output.push_back({Tok::OP, 0, top});
          }
          ops.push_back(op);
          ++i;
          continue;
        }
        // Unknown char, skip
        ++i;
      }
      while (!ops.empty()) {
        char op = ops.back();
        ops.pop_back();
        if (op != '(')
          output.push_back({Tok::OP, 0, op});
      }
      // Eval RPN
      std::vector<long long> st;
      for (auto& t : output) {
        if (t.t == Tok::NUM)
          st.push_back(t.v);
        else if (t.t == Tok::OP) {
          if (st.size() < 2) {
            st.clear();
            break;
          }
          long long b = st.back();
          st.pop_back();
          long long a = st.back();
          st.pop_back();
          st.push_back(apply(a, b, t.op));
        }
      }
      return st.empty() ? 0 : st.back();
    };

    auto expand_substitutions = [&](const std::string& in) -> std::string {
      std::string s = in;
      // First, Arithmetic $(( ... ))
      bool changed = true;
      while (changed) {
        changed = false;
        bool in_quotes = false;
        char q = '\0';
        for (size_t i = 0; i + 2 < s.size(); ++i) {
          char c = s[i];
          if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
            if (!in_quotes) {
              in_quotes = true;
              q = c;
            } else if (q == c) {
              in_quotes = false;
              q = '\0';
            }
          }
          // Allow inside double quotes ("...$((...))...") but not single quotes
          if ((!in_quotes || q == '"') && c == '$' && s[i + 1] == '(' &&
              s[i + 2] == '(') {
            // find matching )) with parenthesis depth
            size_t j = i + 3;
            int depth = 1;
            bool found = false;
            bool in_q = false;
            char qq = '\0';
            for (; j < s.size(); ++j) {
              char d = s[j];
              if ((d == '"' || d == '\'') && (j == i + 3 || s[j - 1] != '\\')) {
                if (!in_q) {
                  in_q = true;
                  qq = d;
                } else if (qq == d) {
                  in_q = false;
                  qq = '\0';
                }
              } else if (!in_q) {
                if (d == '(')
                  depth++;
                else if (d == ')') {
                  depth--;
                  if (depth == 0) {
                    if (j + 1 < s.size() && s[j + 1] == ')') {
                      found = true;
                      j++;
                    }
                    break;
                  }
                }
              }
            }
            if (found) {
              // j currently points at the second ')' closing the arithmetic.
              size_t expr_start = i + 3;
              size_t expr_len = (j > i + 4) ? (j - i - 4) : 0;
              std::string expr = s.substr(expr_start, expr_len);
              std::string repl = std::to_string(eval_arith(expr));
              s.replace(i, (j - i + 1), repl);
              changed = true;
              break;
            }
          }
        }
      }
      // Next, legacy backtick command substitution: ` ... `
      // Allowed inside double quotes, but not inside single quotes. Handles
      // simple escaping of \` within the content.
      changed = true;
      while (changed) {
        changed = false;
        bool in_quotes = false;
        char q = '\0';
        for (size_t i = 0; i < s.size(); ++i) {
          char c = s[i];
          if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
            if (!in_quotes) {
              in_quotes = true;
              q = c;
            } else if (q == c) {
              in_quotes = false;
              q = '\0';
            }
          }
          if ((!in_quotes || q == '"') && c == '`' &&
              (i == 0 || s[i - 1] != '\\')) {
            // find the matching unescaped backtick
            size_t j = i + 1;
            bool found = false;
            for (; j < s.size(); ++j) {
              if (s[j] == '\\') {  // skip escaped character
                if (j + 1 < s.size()) {
                  ++j;
                  continue;
                }
              }
              if (s[j] == '`' && s[j - 1] != '\\') {
                found = true;
                break;
              }
            }
            if (found) {
              std::string inner = s.substr(i + 1, j - (i + 1));
              // Unescape \` inside content
              std::string content;
              content.reserve(inner.size());
              for (size_t k = 0; k < inner.size(); ++k) {
                if (inner[k] == '\\' && k + 1 < inner.size() &&
                    inner[k + 1] == '`') {
                  content.push_back('`');
                  ++k;
                } else {
                  content.push_back(inner[k]);
                }
              }
              std::string repl = capture_internal_output(content);
              s.replace(i, (j - i + 1), repl);
              changed = true;
              break;
            }
          }
        }
      }
      // Then, $( ... ) command substitution. Skip $(( which was already
      // handled.
      changed = true;
      while (changed) {
        changed = false;
        bool in_quotes = false;
        char q = '\0';
        for (size_t i = 0; i + 1 < s.size(); ++i) {
          char c = s[i];
          if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
            if (!in_quotes) {
              in_quotes = true;
              q = c;
            } else if (q == c) {
              in_quotes = false;
              q = '\0';
            }
          }
          // Allow inside double quotes ("...$(...)...") but not single quotes
          if ((!in_quotes || q == '"') && c == '$' && s[i + 1] == '(') {
            if (i + 2 < s.size() && s[i + 2] == '(')
              continue;  // arithmetic already
            // find matching ) with nesting
            size_t j = i + 2;
            int depth = 1;
            bool found = false;
            bool in_q = false;
            char qq = '\0';
            for (; j < s.size(); ++j) {
              char d = s[j];
              if ((d == '"' || d == '\'') && (j == i + 2 || s[j - 1] != '\\')) {
                if (!in_q) {
                  in_q = true;
                  qq = d;
                } else if (qq == d) {
                  in_q = false;
                  qq = '\0';
                }
              } else if (!in_q) {
                if (d == '(')
                  depth++;
                else if (d == ')') {
                  depth--;
                  if (depth == 0) {
                    found = true;
                    break;
                  }
                }
              }
            }
            if (found) {
              std::string inner = s.substr(i + 2, j - (i + 2));
              std::string repl = capture_internal_output(inner);
              s.replace(i, (j - i + 1), repl);
              changed = true;
              break;
            }
          }
        }
      }
      // Finally, ${VAR-default} parameter expansion
      changed = true;
      while (changed) {
        changed = false;
        bool in_quotes = false;
        char q = '\0';
        for (size_t i = 0; i + 1 < s.size(); ++i) {
          char c = s[i];
          if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
            if (!in_quotes) {
              in_quotes = true;
              q = c;
            } else if (q == c) {
              in_quotes = false;
              q = '\0';
            }
          }
          // Parameter expansion ${VAR} or ${VAR-default}
          // Allow inside double quotes but not single quotes
          if ((!in_quotes || q == '"') && c == '$' && s[i + 1] == '{') {
            // find matching }
            size_t j = i + 2;
            int depth = 1;
            bool found = false;
            for (; j < s.size(); ++j) {
              if (s[j] == '{')
                depth++;
              else if (s[j] == '}') {
                depth--;
                if (depth == 0) {
                  found = true;
                  break;
                }
              }
            }
            if (found) {
              std::string param_expr = s.substr(i + 2, j - (i + 2));
              std::string repl;

              // Check for ${VAR-default} or ${VAR:-default} syntax
              size_t dash_pos = param_expr.find('-');
              if (dash_pos != std::string::npos) {
                std::string var_name = param_expr.substr(0, dash_pos);
                std::string default_val = param_expr.substr(dash_pos + 1);

                const char* env_val = getenv(var_name.c_str());
                if (env_val && strlen(env_val) > 0) {
                  repl = env_val;
                } else {
                  repl = default_val;
                }
              } else {
                // Simple ${VAR} expansion
                const char* env_val = getenv(param_expr.c_str());
                repl = env_val ? env_val : "";
              }

              s.replace(i, (j - i + 1), repl);
              changed = true;
              break;
            }
          }
        }
      }
      return s;
    };

    text = expand_substitutions(text);

    // If the command is exactly a readable cjsh script path, interpret it
    // Otherwise, if it's a regular readable file without exec bit, interpret as
    // well Tokenize minimally to get the program token
    std::vector<std::string> head = shell_parser->parse_command(text);
    if (!head.empty()) {
      const std::string& prog = head[0];
      if (should_interpret_as_cjsh(prog)) {
        if (g_debug_mode)
          std::cerr << "DEBUG: Interpreting script file: " << prog << std::endl;
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

    // Use enhanced pipeline parser with preprocessing to handle here docs and
    // subshells
    std::vector<Command> cmds =
        shell_parser->parse_pipeline_with_preprocessing(text);

    bool has_redir_or_pipe = cmds.size() > 1;
    if (!has_redir_or_pipe && !cmds.empty()) {
      const auto& c = cmds[0];
      has_redir_or_pipe = c.background || !c.input_file.empty() ||
                          !c.output_file.empty() || !c.append_file.empty() ||
                          c.stderr_to_stdout || !c.stderr_file.empty() ||
                          !c.here_doc.empty();
    }

    if (!has_redir_or_pipe && !cmds.empty()) {
      // Simple command path - leverage Shell::execute_command for
      // builtins/plugins/env-assignments
      const auto& c = cmds[0];

      // Check if this is an internal subshell command
      if (!c.args.empty() && c.args[0] == "__INTERNAL_SUBSHELL__") {
        // If the subshell command has redirections, we need to use pipeline execution
        // to properly handle them, not simple command execution
        bool has_redir = c.stderr_to_stdout || c.stdout_to_stderr || 
                         !c.input_file.empty() || !c.output_file.empty() || 
                         !c.append_file.empty() || !c.stderr_file.empty() || 
                         !c.here_doc.empty();
        
        if (g_debug_mode) {
          std::cerr << "DEBUG: INTERNAL_SUBSHELL has_redir=" << has_redir 
                    << " stderr_to_stdout=" << c.stderr_to_stdout << std::endl;
        }
        
        if (has_redir) {
          // Use pipeline execution to handle redirections properly
          if (g_debug_mode) {
            std::cerr << "DEBUG: Executing subshell with redirections via pipeline" << std::endl;
          }
          int exit_code = g_shell->shell_exec->execute_pipeline(cmds);
          setenv("STATUS", std::to_string(exit_code).c_str(), 1);
          return exit_code;
        } else {
          // No redirections, execute subshell content directly in same process
          // This is necessary for command substitution to work properly
          if (c.args.size() >= 2) {
            std::string subshell_content = c.args[1];
            if (g_debug_mode) {
              std::cerr << "DEBUG: Executing subshell content directly: " << subshell_content << std::endl;
            }
            int exit_code = g_shell->execute(subshell_content);
            setenv("STATUS", std::to_string(exit_code).c_str(), 1);
            return exit_code;
          } else {
            // Invalid subshell command
            return 1;
          }
        }
      } else {
        // Re-parse the original text to apply alias/env/brace/tilde expansions
        // parse_command performs alias expansion while parse_pipeline does not.
        std::vector<std::string> expanded_args =
            shell_parser->parse_command(text);
        if (expanded_args.empty())
          return 0;
        if (g_debug_mode) {
          std::cerr << "DEBUG: Simple exec: ";
          for (const auto& a : expanded_args)
            std::cerr << "[" << a << "]";
          if (c.background)
            std::cerr << " &";
          std::cerr << std::endl;
        }
        int exit_code = g_shell->execute_command(expanded_args, c.background);
        // Update STATUS environment variable for $? expansion
        setenv("STATUS", std::to_string(exit_code).c_str(), 1);
        return exit_code;
      }
    }

    // Pipeline or with redirections
    if (cmds.empty())
      return 0;
    if (g_debug_mode) {
      std::cerr << "DEBUG: Executing pipeline of size " << cmds.size()
                << std::endl;
      for (size_t i = 0; i < cmds.size(); i++) {
        const auto& cmd = cmds[i];
        std::cerr << "DEBUG: Command " << i << ": ";
        for (const auto& arg : cmd.args) {
          std::cerr << "'" << arg << "' ";
        }
        std::cerr << " stderr_to_stdout=" << cmd.stderr_to_stdout << std::endl;
      }
    }
    int exit_code = g_shell->shell_exec->execute_pipeline(cmds);
    // Update STATUS environment variable for $? expansion
    setenv("STATUS", std::to_string(exit_code).c_str(), 1);
    return exit_code;
  };

  int last_code = 0;

  // Split on single '&' (not '&&'), respecting quotes; append '&' to segment
  auto split_ampersand = [&](const std::string& s) -> std::vector<std::string> {
    std::vector<std::string> parts;
    bool in_quotes = false;
    char q = '\0';
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
      char c = s[i];
      if ((c == '"' || c == '\'') && (i == 0 || s[i - 1] != '\\')) {
        if (!in_quotes) {
          in_quotes = true;
          q = c;
        } else if (q == c) {
          in_quotes = false;
          q = '\0';
        }
        cur += c;
      } else if (!in_quotes && c == '&') {
        // If it's a double ampersand, leave for logical splitting (already
        // handled earlier)
        if (i + 1 < s.size() && s[i + 1] == '&') {
          cur += c;  // let logical splitter handle it; shouldn't happen here
        } else if (i > 0 && s[i - 1] == '>' && i + 1 < s.size() &&
                   std::isdigit(s[i + 1])) {
          // This is a redirection like >&1 or 2>&1, not a background operator
          cur += c;
        } else {
          // finalize current as background segment
          std::string seg = trim(cur);
          if (!seg.empty() && seg.back() != '&')
            seg += " &";
          if (!seg.empty())
            parts.push_back(seg);
          cur.clear();
        }
      } else {
        cur += c;
      }
    }
    std::string tail = trim(cur);
    if (!tail.empty())
      parts.push_back(tail);
    return parts;
  };

  // Minimal if/then/else/fi block handler
  auto handle_if_block = [&](const std::vector<std::string>& src_lines,
                             size_t& idx) -> int {
    // Build a clean line with comments stripped
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    // Extract condition after leading 'if'
    std::string cond_accum;
    if (first.rfind("if ", 0) == 0) {
      cond_accum = first.substr(3);
    } else if (first == "if") {
      // condition may be on following lines
    } else {
      return 1;
    }

    size_t j = idx;
    bool then_found = false;
    // If current line has '; then' suffix, split it
    auto pos = first.find("; then");  // Use find() to get the FIRST occurrence, not rfind()
    if (pos != std::string::npos) {
      // Condition is everything between 'if ' and '; then'
      cond_accum = trim(first.substr(3, pos - 3));
      then_found = true;
    } else {
      // advance until a bare 'then'
      while (!then_found && ++j < src_lines.size()) {
        std::string cur = trim(strip_inline_comment(src_lines[j]));
        if (cur == "then") {
          then_found = true;
          break;
        }
        auto p = cur.rfind("; then");
        if (p != std::string::npos) {
          if (!cond_accum.empty())
            cond_accum += " ";
          cond_accum += cur.substr(0, p);
          then_found = true;
          break;
        }
        if (!cur.empty()) {
          if (!cond_accum.empty())
            cond_accum += " ";
          cond_accum += cur;
        }
      }
    }

    if (!then_found) {
      if (debug_level >= DebugLevel::BASIC)
        std::cerr << "DEBUG: if without matching then" << std::endl;
      idx = j;
      return 1;
    }

    // Execute condition with output suppressed
    int cond_rc = 1;
    if (!cond_accum.empty()) {
      cond_rc = execute_simple_or_pipeline(cond_accum);
    }

    // If '; then' was on the same line, check if it's a complete inline
    // statement
    if (pos != std::string::npos) {
      std::string rem = trim(first.substr(pos + 6));  // after '; then'
      // Only handle as inline if there's content and it ends with fi
      if (!rem.empty()) {
        // find '; fi' at end (allow spaces): we'll search for "; fi" substring
        size_t fi_pos = rem.rfind("; fi");
        if (fi_pos == std::string::npos) {
          // try ending with 'fi'
          if (rem.size() >= 2 && rem.substr(rem.size() - 2) == "fi") {
            fi_pos = rem.size() - 2;  // assume no preceding semicolon
          }
        }

        // Check if this looks like an elif statement (complex case)
        if (rem.find("elif") != std::string::npos) {
          // Fall through to multiline processing for elif statements
        } else {
          // Only process as inline if we found 'fi' on the same line
          if (fi_pos != std::string::npos) {
            std::string body = trim(rem.substr(0, fi_pos));
            // Split optional else: look for '; else'
            std::string then_body = body;
            std::string else_body;
            size_t else_pos = body.find("; else");
            if (else_pos == std::string::npos) {
              // also allow ' else ' without leading semicolon
              size_t alt = body.find(" else ");
              if (alt != std::string::npos)
                else_pos = alt;
            }
            if (else_pos != std::string::npos) {
              then_body = trim(body.substr(0, else_pos));
              else_body = trim(body.substr(else_pos + 6));  // after '; else'
            }
            int body_rc = 0;
            if (cond_rc == 0) {
              auto cmds = shell_parser->parse_semicolon_commands(then_body);
              for (const auto& c : cmds) {
                int rc2 = execute_simple_or_pipeline(c);
                body_rc = rc2;
                if (rc2 != 0)
                  break;
              }
            } else if (!else_body.empty()) {
              auto cmds = shell_parser->parse_semicolon_commands(else_body);
              for (const auto& c : cmds) {
                int rc2 = execute_simple_or_pipeline(c);
                body_rc = rc2;
                if (rc2 != 0)
                  break;
              }
            }
            // single-line; do not advance idx
            return body_rc;
          }
        }
      }
      // If we reach here, it's a multiline if with '; then' on the first line
      // Fall through to multiline processing
    }

    // Multiline: Collect body until matching 'fi', support one else
    size_t k = j + 1;
    int depth = 1;
    bool in_else = false;
    std::vector<std::string> then_lines;
    std::vector<std::string> else_lines;
    
    // For inline if statements, we need to parse the single line differently
    if (src_lines.size() == 1 && src_lines[0].find("fi") != std::string::npos) {
      // This is a single line with the complete if statement
      // We need to parse it properly
      std::string full_line = src_lines[0];
      
      // For now, let's split it into parts and execute it as separate commands
      // This is a simplified approach - a full parser would be better
      std::vector<std::string> parts;
      
      // Extract the condition
      size_t if_pos = full_line.find("if ");
      size_t then_pos = full_line.find("; then");
      if (if_pos != std::string::npos && then_pos != std::string::npos) {
        std::string condition = trim(full_line.substr(if_pos + 3, then_pos - (if_pos + 3)));
        
        // Execute condition
        int cond_result = execute_simple_or_pipeline(condition);
        
        // Now we need to find which branch to execute
        // Look for elif/else/fi structure
        std::string remaining = trim(full_line.substr(then_pos + 6));
        
        // Simple parser for if/elif/else/fi
        std::vector<std::pair<std::string, std::string>> branches; // condition, commands
        std::string current_commands;
        
        // First branch is the 'then' part
        size_t pos = 0;
        while (pos < remaining.length()) {
          size_t elif_pos = remaining.find("; elif ", pos);
          size_t else_pos = remaining.find("; else ", pos);
          
          // Look for fi at word boundary (preceded by space or semicolon)
          size_t fi_pos = std::string::npos;
          size_t search_pos = pos;
          while (search_pos < remaining.length()) {
            size_t candidate = remaining.find("fi", search_pos);
            if (candidate == std::string::npos) break;
            
            // Check if this is a word boundary (fi at end or followed by space/semicolon)
            bool is_word_end = (candidate + 2 >= remaining.length()) || 
                              (remaining[candidate + 2] == ' ') || 
                              (remaining[candidate + 2] == ';');
            bool is_word_start = (candidate == 0) || 
                                (remaining[candidate - 1] == ' ') || 
                                (remaining[candidate - 1] == ';');
            
            if (is_word_start && is_word_end) {
              fi_pos = candidate;
              break;
            }
            search_pos = candidate + 1;
          }
          
          // Find the earliest occurrence
          size_t next_pos = std::min({elif_pos, else_pos, fi_pos});
          if (next_pos == std::string::npos) break;
          
          // Extract commands up to this point
          std::string commands = trim(remaining.substr(pos, next_pos - pos));
          
          if (elif_pos != std::string::npos && next_pos == elif_pos) {
            // Store current commands for previous condition
            if (pos == 0) {
              // This is the first 'then' branch
              if (cond_result == 0) {
                auto cmds = shell_parser->parse_semicolon_commands(commands);
                for (const auto& c : cmds) {
                  execute_simple_or_pipeline(c);
                }
                idx = 0; // Don't advance since we processed everything
                return 0;
              }
            }
            
            // Find the elif condition
            pos = next_pos + 7; // Skip "; elif "
            size_t elif_then = remaining.find("; then", pos);
            if (elif_then != std::string::npos) {
              std::string elif_cond = trim(remaining.substr(pos, elif_then - pos));
              int elif_result = execute_simple_or_pipeline(elif_cond);
              if (elif_result == 0) {
                // Execute commands after this elif's then
                size_t elif_body_start = elif_then + 6; // Skip "; then"
                
                // Find the end of this elif's body (next elif, else, or fi)
                size_t next_elif = remaining.find("; elif ", elif_body_start);
                size_t next_else = remaining.find("; else ", elif_body_start);
                size_t next_fi = std::string::npos;
                size_t search_fi = elif_body_start;
                while (search_fi < remaining.length()) {
                  size_t candidate = remaining.find("fi", search_fi);
                  if (candidate == std::string::npos) break;
                  
                  bool is_word_end = (candidate + 2 >= remaining.length()) || 
                                    (remaining[candidate + 2] == ' ') || 
                                    (remaining[candidate + 2] == ';');
                  bool is_word_start = (candidate == 0) || 
                                      (remaining[candidate - 1] == ' ') || 
                                      (remaining[candidate - 1] == ';');
                  
                  if (is_word_start && is_word_end) {
                    next_fi = candidate;
                    break;
                  }
                  search_fi = candidate + 1;
                }
                
                size_t elif_body_end = std::min({next_elif, next_else, next_fi});
                if (elif_body_end != std::string::npos) {
                  std::string elif_commands = trim(remaining.substr(elif_body_start, elif_body_end - elif_body_start));
                  auto cmds = shell_parser->parse_semicolon_commands(elif_commands);
                  for (const auto& c : cmds) {
                    execute_simple_or_pipeline(c);
                  }
                  idx = 0;
                  return 0;
                }
              }
              pos = elif_then + 6;
            }
          } else if (else_pos != std::string::npos && next_pos == else_pos) {
            // This is the else branch
            if (cond_result != 0) {
              // Execute the commands after else
              pos = next_pos + 7; // Skip "; else "
              size_t fi_end = remaining.find(" fi", pos);
              if (fi_end != std::string::npos) {
                std::string else_commands = trim(remaining.substr(pos, fi_end - pos));
                auto cmds = shell_parser->parse_semicolon_commands(else_commands);
                for (const auto& c : cmds) {
                  execute_simple_or_pipeline(c);
                }
                idx = 0;
                return 0;
              }
            }
            break;
          } else {
            // This is fi - we're done
            // If we reached fi and no previous condition was true, execute these commands
            if (commands.length() > 0) {
              auto cmds = shell_parser->parse_semicolon_commands(commands);
              for (const auto& c : cmds) {
                execute_simple_or_pipeline(c);
              }
            }
            break;
          }
        }
        
        idx = 0; // Don't advance since we processed everything
        return 0;
      }
    }
    
    while (k < src_lines.size() && depth > 0) {
      std::string cur_raw = src_lines[k];
      std::string cur = trim(strip_inline_comment(cur_raw));
      if (cur == "if" || cur.rfind("if ", 0) == 0 ||
          cur.find("; then") != std::string::npos)
        depth++;
      else if (cur == "fi") {
        depth--;
        if (depth == 0)
          break;
      } else if (depth == 1 && cur == "else") {
        in_else = true;
        k++;
        continue;
      }

      if (depth > 0) {
        if (!in_else)
          then_lines.push_back(cur_raw);
        else
          else_lines.push_back(cur_raw);
      }
      k++;
    }
    if (depth != 0) {
      if (debug_level >= DebugLevel::BASIC)
        std::cerr << "DEBUG: if without matching fi" << std::endl;
      idx = k;
      return 1;
    }

    int body_rc = 0;
    if (cond_rc == 0)
      body_rc = execute_block(then_lines);
    else if (!else_lines.empty())
      body_rc = execute_block(else_lines);

    idx = k;  // position at 'fi'
    return body_rc;
  };

  // Minimal for VAR in LIST; do ...; done handler
  auto handle_for_block = [&](const std::vector<std::string>& src_lines,
                              size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (!(first == "for" || first.rfind("for ", 0) == 0))
      return 1;

    std::string var;
    std::vector<std::string> items;

    auto parse_header = [&](const std::string& header) -> bool {
      // Tokenize header to extract var and list after 'in'
      std::vector<std::string> toks = shell_parser->parse_command(header);
      size_t i = 0;
      if (i < toks.size() && toks[i] == "for")
        ++i;
      if (i >= toks.size())
        return false;
      var = toks[i++];
      if (i < toks.size() && toks[i] == "in") {
        ++i;
        for (; i < toks.size(); ++i) {
          if (toks[i] == ";" || toks[i] == "do")
            break;
          items.push_back(toks[i]);
        }
      }
      return !var.empty();
    };

    // Inline form: for i in 1 2; do body; done
    if (first.find("; do") != std::string::npos) {
      size_t do_pos = first.find("; do");
      std::string header = trim(first.substr(0, do_pos));
      if (!parse_header(header))
        return 1;
      std::string tail = trim(first.substr(do_pos + 4));
      size_t done_pos = tail.rfind("; done");
      if (done_pos == std::string::npos)
        done_pos = tail.rfind("done");
      std::string body =
          done_pos == std::string::npos ? tail : trim(tail.substr(0, done_pos));
      int rc = 0;
      for (const auto& it : items) {
        setenv(var.c_str(), it.c_str(), 1);
        auto cmds = shell_parser->parse_semicolon_commands(body);
        for (const auto& c : cmds) {
          rc = execute_simple_or_pipeline(c);
          if (rc != 0)
            break;
        }
        if (rc != 0)
          break;
      }
      return rc;
    }

    // Multiline form
    std::string header_accum = first;
    size_t j = idx;
    bool have_do = false;
    while (!have_do && ++j < src_lines.size()) {
      std::string cur = trim(strip_inline_comment(src_lines[j]));
      if (cur == "do") {
        have_do = true;
        break;
      }
      if (cur.find("; do") != std::string::npos) {
        header_accum += " ";
        header_accum += cur;
        have_do = true;
        break;
      }
      if (!cur.empty()) {
        header_accum += " ";
        header_accum += cur;
      }
    }
    if (!parse_header(header_accum)) {
      idx = j;
      return 1;
    }
    if (!have_do) {
      idx = j;
      return 1;
    }

    // Collect body until matching 'done' with basic nesting
    std::vector<std::string> body_lines;
    size_t k = j + 1;
    int depth = 1;
    while (k < src_lines.size() && depth > 0) {
      std::string cur_raw = src_lines[k];
      std::string cur = trim(strip_inline_comment(cur_raw));
      if (cur == "for" || cur.rfind("for ", 0) == 0)
        depth++;
      else if (cur == "done") {
        depth--;
        if (depth == 0)
          break;
      }
      if (depth > 0)
        body_lines.push_back(cur_raw);
      k++;
    }
    if (depth != 0) {
      idx = k;
      return 1;
    }

    int rc = 0;
    for (const auto& it : items) {
      setenv(var.c_str(), it.c_str(), 1);
      rc = execute_block(body_lines);
      if (rc != 0)
        break;
    }
    idx = k;  // at 'done'
    return rc;
  };

  // Minimal case VALUE in PATTERN) ...; esac handler
  auto handle_case_block = [&](const std::vector<std::string>& src_lines,
                               size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (!(first == "case" || first.rfind("case ", 0) == 0))
      return 1;

    // Extract the case value
    std::string case_value;
    std::string header_accum = first;

    // Check if the entire case statement is on one line
    if (first.find(" in ") != std::string::npos &&
        first.find("esac") != std::string::npos) {
      // Single-line case statement: "case VALUE in PATTERN) COMMAND;; ... esac"
      size_t in_pos = first.find(" in ");
      std::string case_part = first.substr(0, in_pos);
      std::string patterns_part = first.substr(in_pos + 4);  // Skip " in "

      // Extract case value
      std::vector<std::string> case_toks =
          shell_parser->parse_command(case_part);
      if (case_toks.size() >= 2 && case_toks[0] == "case") {
        case_value = case_toks[1];
      }

      // Remove "esac" from the end
      size_t esac_pos = patterns_part.find("esac");
      if (esac_pos != std::string::npos) {
        patterns_part = patterns_part.substr(0, esac_pos);
      }

      // Process pattern sections
      std::vector<std::string> pattern_sections;
      size_t start = 0;
      while (start < patterns_part.length()) {
        size_t sep_pos = patterns_part.find(";;", start);
        if (sep_pos == std::string::npos) {
          if (start < patterns_part.length()) {
            pattern_sections.push_back(trim(patterns_part.substr(start)));
          }
          break;
        }
        pattern_sections.push_back(
            trim(patterns_part.substr(start, sep_pos - start)));
        start = sep_pos + 2;
      }

      // Check each pattern for a match
      int matched_exit_code = 0;
      for (const auto& section : pattern_sections) {
        if (section.empty())
          continue;

        size_t paren_pos = section.find(')');
        if (paren_pos != std::string::npos) {
          std::string pattern = trim(section.substr(0, paren_pos));
          std::string command_part = trim(section.substr(paren_pos + 1));

          // Check if pattern matches case_value
          bool pattern_matches = false;
          if (pattern == "*") {
            pattern_matches = true;  // wildcard matches everything
          } else if (pattern == case_value) {
            pattern_matches = true;  // exact match
          }

          if (pattern_matches) {
            if (!command_part.empty()) {
              matched_exit_code = execute_simple_or_pipeline(command_part);
            }
            // Stay on the same line since it's all one line
            return matched_exit_code;
          }
        }
      }

      // No match found, return 0
      return 0;
    }

    // Multi-line case statement processing
    // Parse: case VALUE in
    std::vector<std::string> header_toks =
        shell_parser->parse_command(header_accum);
    size_t tok_idx = 0;
    if (tok_idx < header_toks.size() && header_toks[tok_idx] == "case")
      ++tok_idx;
    if (tok_idx < header_toks.size()) {
      case_value = header_toks[tok_idx++];
    }

    // Check if "in" is on the same line or next lines
    bool found_in = false;
    if (tok_idx < header_toks.size() && header_toks[tok_idx] == "in") {
      found_in = true;
    }

    size_t j = idx;
    if (!found_in) {
      // Look for "in" on subsequent lines
      while (!found_in && ++j < src_lines.size()) {
        std::string cur = trim(strip_inline_comment(src_lines[j]));
        if (cur == "in") {
          found_in = true;
          break;
        }
        if (!cur.empty()) {
          header_accum += " " + cur;
          // Re-parse to check for "in"
          header_toks = shell_parser->parse_command(header_accum);
          for (size_t h = 0; h < header_toks.size(); ++h) {
            if (header_toks[h] == "in") {
              found_in = true;
              break;
            }
          }
        }
      }
    }

    if (!found_in || case_value.empty()) {
      if (debug_level >= DebugLevel::BASIC)
        std::cerr << "DEBUG: case without valid syntax" << std::endl;
      idx = j;
      return 1;
    }

    // Expand the case value (handle variables, etc.)
    // For now, just use the literal value - variable expansion would need
    // access to expand_substitutions which is defined later
    // case_value = expand_substitutions(case_value);

    // Parse case patterns and their commands until "esac"
    size_t k = j + 1;
    int matched_exit_code = 0;
    bool found_match = false;

    // Handle single-line case statement
    if (k < src_lines.size()) {
      std::string remaining_line = src_lines[k];

      // Check if this line contains "esac" - might be a single-line case
      if (remaining_line.find("esac") != std::string::npos) {
        // Single line case: parse all patterns in one line
        // Split by "))" to separate pattern sections
        std::string work_line = remaining_line;
        size_t esac_pos = work_line.find("esac");
        if (esac_pos != std::string::npos) {
          work_line = work_line.substr(0, esac_pos);  // Remove "esac"
        }

        // Split on ";;" to get pattern sections
        std::vector<std::string> pattern_sections;
        size_t start = 0;
        while (start < work_line.length()) {
          size_t sep_pos = work_line.find(";;", start);
          if (sep_pos == std::string::npos) {
            if (start < work_line.length()) {
              pattern_sections.push_back(work_line.substr(start));
            }
            break;
          }
          pattern_sections.push_back(work_line.substr(start, sep_pos - start));
          start = sep_pos + 2;
        }

        // Process each pattern section
        for (const auto& section : pattern_sections) {
          std::string pattern_line = trim(section);
          if (pattern_line.empty())
            continue;

          size_t paren_pos = pattern_line.find(')');
          if (paren_pos != std::string::npos) {
            std::string pattern = trim(pattern_line.substr(0, paren_pos));
            std::string command_part = trim(pattern_line.substr(paren_pos + 1));

            // Check if pattern matches case_value
            bool pattern_matches = false;
            if (pattern == "*") {
              pattern_matches = true;  // wildcard matches everything
            } else if (pattern == case_value) {
              pattern_matches = true;  // exact match
            } else {
              // Simple glob pattern matching for basic patterns
              if (pattern.find('*') != std::string::npos) {
                // Very basic glob: pattern like "test*" matches "testing"
                if (pattern.back() == '*') {
                  std::string prefix = pattern.substr(0, pattern.length() - 1);
                  if (case_value.substr(0, prefix.length()) == prefix) {
                    pattern_matches = true;
                  }
                } else if (pattern.front() == '*') {
                  std::string suffix = pattern.substr(1);
                  if (case_value.length() >= suffix.length() &&
                      case_value.substr(case_value.length() -
                                        suffix.length()) == suffix) {
                    pattern_matches = true;
                  }
                }
              }
            }

            if (pattern_matches && !found_match) {
              found_match = true;
              if (!command_part.empty()) {
                matched_exit_code = execute_simple_or_pipeline(command_part);
              }
              break;  // Found match, stop looking
            }
          }
        }

        // Skip to after esac
        idx = k;
        return matched_exit_code;
      }
    }

    // Multi-line case statement handling
    while (k < src_lines.size()) {
      std::string line = trim(strip_inline_comment(src_lines[k]));
      if (line.empty()) {
        k++;
        continue;
      }
      if (line == "esac") {
        break;
      }

      // Look for pattern) syntax
      size_t paren_pos = line.find(')');
      if (paren_pos != std::string::npos) {
        std::string pattern = trim(line.substr(0, paren_pos));
        std::string command_part = trim(line.substr(paren_pos + 1));

        // Check if pattern matches case_value
        bool pattern_matches = false;
        if (pattern == "*") {
          pattern_matches = true;  // wildcard matches everything
        } else if (pattern == case_value) {
          pattern_matches = true;  // exact match
        } else {
          // Simple glob pattern matching for basic patterns
          if (pattern.find('*') != std::string::npos) {
            // Very basic glob: pattern like "test*" matches "testing"
            if (pattern.back() == '*') {
              std::string prefix = pattern.substr(0, pattern.length() - 1);
              if (case_value.substr(0, prefix.length()) == prefix) {
                pattern_matches = true;
              }
            } else if (pattern.front() == '*') {
              std::string suffix = pattern.substr(1);
              if (case_value.length() >= suffix.length() &&
                  case_value.substr(case_value.length() - suffix.length()) ==
                      suffix) {
                pattern_matches = true;
              }
            }
          }
        }

        if (pattern_matches && !found_match) {
          found_match = true;

          // Execute commands until we hit ";;" or another pattern
          std::vector<std::string> case_commands;
          if (!command_part.empty()) {
            case_commands.push_back(command_part);
          }

          // Continue reading lines until ";;" or next pattern or "esac"
          k++;
          while (k < src_lines.size()) {
            std::string cmd_line = trim(strip_inline_comment(src_lines[k]));
            if (cmd_line.empty()) {
              k++;
              continue;
            }
            if (cmd_line == "esac") {
              break;
            }
            if (cmd_line == ";;" || cmd_line.find(";;") != std::string::npos) {
              // Handle ";;" terminator
              if (cmd_line != ";;") {
                // Commands before ";;" on same line
                size_t sep_pos = cmd_line.find(";;");
                std::string before_sep = trim(cmd_line.substr(0, sep_pos));
                if (!before_sep.empty()) {
                  case_commands.push_back(before_sep);
                }
              }
              break;
            }
            // Check if this line contains a new pattern (has ")" in it)
            if (cmd_line.find(')') != std::string::npos) {
              k--;  // Back up to re-process this line as a new pattern
              break;
            }
            case_commands.push_back(cmd_line);
            k++;
          }

          // Execute the matched case commands
          for (const auto& cmd : case_commands) {
            matched_exit_code = execute_simple_or_pipeline(cmd);
            if (matched_exit_code != 0)
              break;
          }

          // Skip remaining patterns
          break;
        }
      }
      k++;
    }

    // Find the closing "esac"
    while (k < src_lines.size()) {
      std::string line = trim(strip_inline_comment(src_lines[k]));
      if (line == "esac") {
        break;
      }
      k++;
    }

    idx = k;  // Position at "esac"
    return matched_exit_code;
  };

  // Minimal while CONDITION; do ...; done handler
  auto handle_while_block = [&](const std::vector<std::string>& src_lines,
                                size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (!(first == "while" || first.rfind("while ", 0) == 0))
      return 1;

    auto parse_cond_from = [&](const std::string& s, std::string& cond,
                               bool& inline_do, std::string& body_inline) {
      inline_do = false;
      body_inline.clear();
      cond.clear();
      std::string tmp = s;
      if (tmp == "while") {
        return true;
      }
      if (tmp.rfind("while ", 0) == 0)
        tmp = tmp.substr(6);
      size_t do_pos = tmp.find("; do");
      if (do_pos != std::string::npos) {
        cond = trim(tmp.substr(0, do_pos));
        inline_do = true;
        body_inline = trim(tmp.substr(do_pos + 4));
        return true;
      }
      if (tmp == "do") {
        inline_do = true;
        return true;
      }
      cond = trim(tmp);
      return true;
    };

    std::string cond;
    bool inline_do = false;
    std::string body_inline;
    parse_cond_from(first, cond, inline_do, body_inline);
    size_t j = idx;
    if (!inline_do) {
      while (++j < src_lines.size()) {
        std::string cur = trim(strip_inline_comment(src_lines[j]));
        if (cur == "do") {
          inline_do = true;
          break;
        }
        if (cur.find("; do") != std::string::npos) {
          parse_cond_from(cur, cond, inline_do, body_inline);
          break;
        }
        if (!cur.empty()) {
          if (!cond.empty())
            cond += " ";
          cond += cur;
        }
      }
    }
    if (!inline_do) {
      idx = j;
      return 1;
    }

    std::vector<std::string> body_lines;
    if (!body_inline.empty()) {
      std::string bi = body_inline;
      size_t done_pos = bi.rfind("; done");
      if (done_pos != std::string::npos)
        bi = trim(bi.substr(0, done_pos));
      body_lines = shell_parser->parse_into_lines(bi);
    } else {
      size_t k = j + 1;
      int depth = 1;
      while (k < src_lines.size() && depth > 0) {
        std::string cur_raw = src_lines[k];
        std::string cur = trim(strip_inline_comment(cur_raw));
        if (cur == "while" || cur.rfind("while ", 0) == 0)
          depth++;
        else if (cur == "done") {
          depth--;
          if (depth == 0)
            break;
        }
        if (depth > 0)
          body_lines.push_back(cur_raw);
        k++;
      }
      if (depth != 0) {
        idx = k;
        return 1;
      }
      idx = k;  // at 'done'
    }

    int rc = 0;
    int guard = 0;
    const int GUARD_MAX = 100000;
    while (true) {
      int c = 0;
      if (!cond.empty()) {
        c = execute_simple_or_pipeline(cond);
      }
      if (c != 0)
        break;
      rc = execute_block(body_lines);
      if (rc != 0)
        break;
      if (++guard > GUARD_MAX) {
        std::cerr << "cjsh: while loop aborted (guard)" << std::endl;
        rc = 1;
        break;
      }
    }
    return rc;
  };

  for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
    const auto& raw_line = lines[line_index];
    std::string line = trim(strip_inline_comment(raw_line));
    if (line.empty()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: skipping empty/comment line" << std::endl;
      continue;
    }

    // Control structure: if/then/else/fi
    if (line == "if" || line.rfind("if ", 0) == 0 ||
        line.find("; then") != std::string::npos) {
      int rc = handle_if_block(lines, line_index);
      last_code = rc;
      if (g_debug_mode)
        std::cerr << "DEBUG: if block completed with exit code: " << rc
                  << std::endl;
      // line_index now points at 'fi'; for loop will ++ to next line
      continue;
    }

    // Control structure: for ...; do ...; done
    if (line == "for" || line.rfind("for ", 0) == 0) {
      int rc = handle_for_block(lines, line_index);
      if (rc != 0)
        return rc;
      continue;
    }

    // Control structure: while ...; do ...; done
    if (line == "while" || line.rfind("while ", 0) == 0) {
      int rc = handle_while_block(lines, line_index);
      if (rc != 0)
        return rc;
      continue;
    }

    // Control structure: case ... in ... esac
    if (line == "case" || line.rfind("case ", 0) == 0) {
      int rc = handle_case_block(lines, line_index);
      last_code = rc;
      if (g_debug_mode)
        std::cerr << "DEBUG: case block completed with exit code: " << rc
                  << std::endl;
      continue;
    }

    // Function definition: name() { ... }
    if (line.find("()") != std::string::npos &&
        line.find("{") != std::string::npos) {
      size_t name_end = line.find("()");
      size_t brace_pos = line.find("{");
      if (name_end != std::string::npos && brace_pos != std::string::npos &&
          name_end < brace_pos) {
        std::string func_name = trim(line.substr(0, name_end));
        if (!func_name.empty() && func_name.find(' ') == std::string::npos) {
          // Collect body including multiline until matching '}'
          std::vector<std::string> body_lines;
          bool handled_single_line = false;
          std::string after_brace = trim(line.substr(brace_pos + 1));
          if (!after_brace.empty()) {
            // Check if the closing brace is on the same line
            size_t end_brace = after_brace.find('}');
            if (end_brace != std::string::npos) {
              std::string body_part = trim(after_brace.substr(0, end_brace));
              if (!body_part.empty())
                body_lines.push_back(body_part);
              // register function and continue
              functions[func_name] = body_lines;
              if (g_debug_mode)
                std::cerr << "DEBUG: Defined function '" << func_name
                          << "' (single-line)" << std::endl;
              // If there's trailing content after the closing '}', process it
              // now
              std::string remainder = trim(after_brace.substr(end_brace + 1));
              if (!remainder.empty()) {
                // Replace current line with the remainder and fall through
                line = remainder;
              }
              handled_single_line = true;
            } else if (!after_brace.empty()) {
              body_lines.push_back(after_brace);
            }
          }
          if (!handled_single_line) {
            // Multiline: gather until closing '}' with simple depth tracking
            int depth = 1;
            while (++line_index < lines.size() && depth > 0) {
              std::string func_line_raw = lines[line_index];
              std::string func_line = trim(strip_inline_comment(func_line_raw));
              for (char ch : func_line) {
                if (ch == '{')
                  depth++;
                else if (ch == '}')
                  depth--;
              }
              if (depth <= 0) {
                // remove any trailing content before final '}'
                size_t pos = func_line.find('}');
                if (pos != std::string::npos) {
                  std::string before = trim(func_line.substr(0, pos));
                  if (!before.empty())
                    body_lines.push_back(before);
                }
                break;
              } else if (!func_line.empty()) {
                body_lines.push_back(func_line_raw);
              }
            }
            functions[func_name] = body_lines;
            if (g_debug_mode)
              std::cerr << "DEBUG: Defined function '" << func_name << "' with "
                        << body_lines.size() << " lines" << std::endl;
            continue;
          } else {
            // Already registered single-line function; proceed to parse the
            // remainder in 'line'
          }
        }
      }
    }

    // Break the line into logical command segments (&&, ||) while preserving
    // order
    std::vector<LogicalCommand> lcmds =
        shell_parser->parse_logical_commands(line);
    if (lcmds.empty())
      continue;

    last_code = 0;
    for (size_t i = 0; i < lcmds.size(); ++i) {
      const auto& lc = lcmds[i];
      // Short-circuiting based on previous op
      if (i > 0) {
        const std::string& prev_op = lcmds[i - 1].op;
        if (prev_op == "&&" && last_code != 0) {
          if (g_debug_mode)
            std::cerr << "DEBUG: Skipping due to && short-circuit" << std::endl;
          continue;
        }
        if (prev_op == "||" && last_code == 0) {
          if (g_debug_mode)
            std::cerr << "DEBUG: Skipping due to || short-circuit" << std::endl;
          continue;
        }
      }

      // Check for command grouping (parentheses or braces) before semicolon
      // splitting
      std::string cmd_to_parse = lc.command;
      std::string trimmed_cmd = trim(strip_inline_comment(cmd_to_parse));

      // Handle grouping constructs that should be treated as a single unit
      if (!trimmed_cmd.empty() &&
          (trimmed_cmd[0] == '(' || trimmed_cmd[0] == '{')) {
        // This is a grouped command - execute it as a single unit
        int code = execute_simple_or_pipeline(cmd_to_parse);
        last_code = code;
        if (code != 0 && debug_level >= DebugLevel::BASIC) {
          std::cerr << "DEBUG: Grouped command failed (" << code << ") -> '"
                    << cmd_to_parse << "'" << std::endl;
        }
        continue;
      }

      // Handle inline if statements that should be treated as a single unit
      if ((trimmed_cmd == "if" || trimmed_cmd.rfind("if ", 0) == 0) &&
          (trimmed_cmd.find("; then") != std::string::npos) &&
          (trimmed_cmd.find(" fi") != std::string::npos ||
           trimmed_cmd.find("; fi") != std::string::npos ||
           trimmed_cmd.rfind("fi") == trimmed_cmd.length() - 2)) {
        // This is an inline if statement - execute it as a single unit
        size_t local_idx = 0;
        std::vector<std::string> one{trimmed_cmd};
        int code = handle_if_block(one, local_idx);
        last_code = code;
        if (code != 0 && debug_level >= DebugLevel::BASIC) {
          std::cerr << "DEBUG: if block failed (" << code << ") -> '"
                    << trimmed_cmd << "'" << std::endl;
        }
        continue;
      }

      // Split each logical segment further by semicolons, then single '&'
      // separators
      auto semis = shell_parser->parse_semicolon_commands(lc.command);
      if (semis.empty()) {
        last_code = 0;
        continue;
      }
      for (size_t k = 0; k < semis.size(); ++k) {
        const std::string& semi = semis[k];
        auto segs = split_ampersand(semi);
        if (segs.empty())
          segs.push_back(semi);
        for (size_t si = 0; si < segs.size(); ++si) {
          const std::string& cmd_text = segs[si];
          // Handle inline for/while blocks within this semicolon segment
          std::string t = trim(strip_inline_comment(cmd_text));
          // Case A: single token contains entire for/while with ; do ... ; done
          if ((t.rfind("for ", 0) == 0 || t == "for") &&
              t.find("; do") != std::string::npos) {
            size_t local_idx = 0;
            std::vector<std::string> one{t};
            int code = handle_for_block(one, local_idx);
            last_code = code;
            if (code != 0 && debug_level >= DebugLevel::BASIC) {
              std::cerr << "DEBUG: for block failed (" << code << ") -> '" << t
                        << "'" << std::endl;
            }
            continue;
          }
          if ((t.rfind("while ", 0) == 0 || t == "while") &&
              t.find("; do") != std::string::npos) {
            size_t local_idx = 0;
            std::vector<std::string> one{t};
            int code = handle_while_block(one, local_idx);
            last_code = code;
            if (code != 0 && debug_level >= DebugLevel::BASIC) {
              std::cerr << "DEBUG: while block failed (" << code << ") -> '"
                        << t << "'" << std::endl;
            }
            continue;
          }
          // Case B: header/body/end split across separate semicolon segments in
          // same line
          if ((t.rfind("for ", 0) == 0 || t == "for")) {
            // Parse header
            std::string var;
            std::vector<std::string> items;
            auto parse_for_header = [&](const std::string& header) -> bool {
              std::vector<std::string> toks =
                  shell_parser->parse_command(header);
              size_t i = 0;
              if (i < toks.size() && toks[i] == "for")
                ++i;
              if (i >= toks.size())
                return false;
              var = toks[i++];
              if (i < toks.size() && toks[i] == "in") {
                ++i;
                for (; i < toks.size(); ++i)
                  items.push_back(toks[i]);
              }
              return !var.empty();
            };
            if (!parse_for_header(t)) {
              last_code = 1;
              break;
            }
            // Next segment should start with 'do'
            std::string body_inline;
            size_t done_index = k + 1;
            if (done_index < semis.size()) {
              std::string next = trim(strip_inline_comment(semis[done_index]));
              if (next.rfind("do ", 0) == 0)
                body_inline = trim(next.substr(3));
              else if (next == "do")
                body_inline = "";
              else {
                last_code = 1;
                break;
              }
              // Advance to find 'done'
              size_t scan = done_index + 1;
              bool found_done = false;
              for (; scan < semis.size(); ++scan) {
                std::string seg = trim(strip_inline_comment(semis[scan]));
                if (seg == "done") {
                  found_done = true;
                  break;
                }
                // Allow accumulating more body from subsequent segments if no
                // 'done' yet
                if (!body_inline.empty())
                  body_inline += "; ";
                body_inline += seg;
              }
              if (!found_done) {
                last_code = 1;
                break;
              }
              // Execute loop
              int rc2 = 0;
              for (const auto& itv : items) {
                setenv(var.c_str(), itv.c_str(), 1);
                auto cmds2 =
                    shell_parser->parse_semicolon_commands(body_inline);
                for (const auto& c2 : cmds2) {
                  rc2 = execute_simple_or_pipeline(c2);
                  if (rc2 != 0)
                    break;
                }
                if (rc2 != 0)
                  break;
              }
              last_code = rc2;
              // Skip consumed segments: 'do'..'done'
              k = scan;  // outer loop will ++k next
              // Skip remaining split_ampersand segs for this semi
              break;
            }
          }
          if ((t.rfind("while ", 0) == 0 || t == "while")) {
            // Parse condition from header t
            std::string cond =
                (t == "while") ? std::string("") : trim(t.substr(6));
            // Next segment should start with 'do'
            size_t done_index = k + 1;
            std::string body_inline;
            if (done_index < semis.size()) {
              std::string next = trim(strip_inline_comment(semis[done_index]));
              if (next.rfind("do ", 0) == 0)
                body_inline = trim(next.substr(3));
              else if (next == "do")
                body_inline.clear();
              else {
                last_code = 1;
                break;
              }
              // Collect until 'done'
              size_t scan = done_index + 1;
              bool found_done = false;
              for (; scan < semis.size(); ++scan) {
                std::string seg = trim(strip_inline_comment(semis[scan]));
                if (seg == "done") {
                  found_done = true;
                  break;
                }
                if (!body_inline.empty())
                  body_inline += "; ";
                body_inline += seg;
              }
              if (!found_done) {
                last_code = 1;
                break;
              }
              // Execute while loop with a guard
              int rc2 = 0;
              int guard = 0;
              const int GUARD_MAX = 100000;
              while (true) {
                int cnd = 0;
                if (!cond.empty()) {
                  cnd = execute_simple_or_pipeline(cond);
                }
                if (cnd != 0)
                  break;
                auto cmds2 =
                    shell_parser->parse_semicolon_commands(body_inline);
                for (const auto& c2 : cmds2) {
                  rc2 = execute_simple_or_pipeline(c2);
                  if (rc2 != 0)
                    break;
                }
                if (rc2 != 0)
                  break;
                if (++guard > GUARD_MAX) {
                  std::cerr << "cjsh: while loop aborted (guard)" << std::endl;
                  rc2 = 1;
                  break;
                }
              }
              last_code = rc2;
              k = scan;  // skip to 'done'
              break;
            }
          }
          // Detect function invocation: first token matches defined function
          int code = 0;
          {
            std::vector<std::string> first_toks =
                shell_parser->parse_command(cmd_text);
            if (!first_toks.empty() && functions.count(first_toks[0])) {
              // Set positional params as environment variables $1..$9 minimally
              // Save originals to restore after
              std::vector<std::string> param_names;
              for (size_t pi = 1; pi < first_toks.size() && pi <= 9; ++pi) {
                std::string name = std::to_string(pi);
                param_names.push_back(name);
                setenv(name.c_str(), first_toks[pi].c_str(), 1);
              }
              // Execute function body
              code = execute_block(functions[first_toks[0]]);
              // Restore positional params (unset)
              for (const auto& n : param_names)
                unsetenv(n.c_str());
            } else {
              code = execute_simple_or_pipeline(cmd_text);
            }
          }
          last_code = code;
          if (code != 0 && debug_level >= DebugLevel::BASIC) {
            std::cerr << "DEBUG: Command failed (" << code << ") -> '"
                      << cmd_text << "'" << std::endl;
          }
        }
      }
    }

    // Only stop script execution for critical errors (127 = command not found)
    // Allow other errors to continue, as is typical in shell scripts
    if (last_code == 127) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Stopping script block due to critical error: "
                  << last_code << std::endl;
      return last_code;
    } else if (last_code != 0 && g_debug_mode) {
      std::cerr << "DEBUG: Command failed with exit code " << last_code
                << " but continuing execution" << std::endl;
    }
  }

  return last_code;
}
