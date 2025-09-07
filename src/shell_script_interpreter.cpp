#include "shell_script_interpreter.h"

#include <sys/stat.h>
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
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
           access(path.c_str(), R_OK) == 0;
  };

  auto should_interpret_as_cjsh = [&](const std::string& path) -> bool {
    if (!is_readable_file(path)) return false;
    // Heuristics: .cjsh extension, or shebang mentions cjsh, or first line
    // contains 'cjsh'
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".cjsh")
      return true;
    std::ifstream f(path);
    if (!f) return false;
    std::string first_line;
    std::getline(f, first_line);
    if (first_line.rfind("#!", 0) == 0 &&
        first_line.find("cjsh") != std::string::npos)
      return true;
    if (first_line.find("cjsh") != std::string::npos) return true;
    return false;
  };

  // Forward-declared executor to allow helper lambdas to call back into it
  std::function<int(const std::string&)> execute_simple_or_pipeline;

  execute_simple_or_pipeline = [&](const std::string& cmd_text) -> int {
    // Decide between simple exec via Shell::execute_command vs
    // Exec::execute_pipeline
    std::string text = trim(strip_inline_comment(cmd_text));
    if (text.empty()) return 0;

    // Expand command substitution $(...) and arithmetic $((...)) without using
    // external sh
    auto capture_internal_output =
        [&](const std::string& content) -> std::string {
      // Create a unique temp file path
      char tmpl[] = "/tmp/cjsh_subst_XXXXXX";
      int fd = mkstemp(tmpl);
      if (fd >= 0) close(fd);
      std::string path = tmpl;

      auto append_redirect = [&](const std::string& segment) -> int {
        // Avoid interfering with existing stdout redirections in simple cases
        bool has_stdout_redir = false;
        bool in_quotes = false;
        char q = '\0';
        for (size_t i = 0; i < segment.size(); ++i) {
          char c = segment[i];
          if ((c == '"' || c == '\'') && (i == 0 || segment[i - 1] != '\\')) {
            if (!in_quotes) {
              in_quotes = true;
              q = c;
            } else if (q == c) {
              in_quotes = false;
              q = '\0';
            }
          }
          if (!in_quotes && c == '>') {
            has_stdout_redir = true;
            break;
          }
        }
        std::string modified = segment;
        if (!has_stdout_redir) {
          modified += " >> ";
          modified += path;
        }
        return execute_simple_or_pipeline(modified);
      };

      // Split into logical/semicolon parts and run with append redirection
      std::vector<LogicalCommand> lcmds =
          shell_parser->parse_logical_commands(content);
      int rc = 0;
      for (size_t i = 0; i < lcmds.size(); ++i) {
        const auto& lc = lcmds[i];
        auto parts = shell_parser->parse_semicolon_commands(lc.command);
        for (const auto& part : parts) {
          rc = append_redirect(part);
          if (rc != 0) break;
        }
        if (rc != 0) break;
      }

      // Read file
      std::ifstream ifs(path);
      std::stringstream buffer;
      buffer << ifs.rdbuf();
      std::string out = buffer.str();
      // Cleanup
      ::unlink(path.c_str());
      // Trim trailing newlines
      while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
      // Fallback: if empty, try executing with /bin/sh -c and capture stdout
      // via popen
      if (out.empty()) {
        std::string cmd = "/bin/sh -c '" + content + "'";
        FILE* fp = popen(cmd.c_str(), "r");
        if (fp) {
          char buf[4096];
          size_t n;
          std::string tmpOut;
          while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
            tmpOut.append(buf, n);
          pclose(fp);
          while (!tmpOut.empty() &&
                 (tmpOut.back() == '\n' || tmpOut.back() == '\r'))
            tmpOut.pop_back();
          out = tmpOut;
        }
      }
      return out;
    };

    auto eval_arith = [&](const std::string& expr) -> long long {
      // Shunting-yard for +,-,*,/,% and parentheses; variables via getenv
      struct Tok {
        enum T { NUM, OP, LP, RP } t;
        long long v;
        char op;
      };
      auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
      };
      auto prec = [](char op) {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/' || op == '%') return 2;
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
          while (j < expr.size() && (isalnum(expr[j]) || expr[j] == '_')) ++j;
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
          if (!ops.empty() && ops.back() == '(') ops.pop_back();
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
        if (op != '(') output.push_back({Tok::OP, 0, op});
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

    // Use pipeline parser to capture redirections, background, and pipes
    std::vector<Command> cmds = shell_parser->parse_pipeline(text);
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
      // Re-parse the original text to apply alias/env/brace/tilde expansions
      // parse_command performs alias expansion while parse_pipeline does not.
      std::vector<std::string> expanded_args =
          shell_parser->parse_command(text);
      if (expanded_args.empty()) return 0;
      if (g_debug_mode) {
        std::cerr << "DEBUG: Simple exec: ";
        for (const auto& a : expanded_args) std::cerr << "[" << a << "]";
        if (c.background) std::cerr << " &";
        std::cerr << std::endl;
      }
      int exit_code = g_shell->execute_command(expanded_args, c.background);
      // Update STATUS environment variable for $? expansion
      setenv("STATUS", std::to_string(exit_code).c_str(), 1);
      return exit_code;
    }

    // Pipeline or with redirections
    if (cmds.empty()) return 0;
    if (g_debug_mode) {
      std::cerr << "DEBUG: Executing pipeline of size " << cmds.size()
                << std::endl;
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
        } else if (i > 0 && s[i - 1] == '>' && i + 1 < s.size() && std::isdigit(s[i + 1])) {
          // This is a redirection like >&1 or 2>&1, not a background operator
          cur += c;
        } else {
          // finalize current as background segment
          std::string seg = trim(cur);
          if (!seg.empty() && seg.back() != '&') seg += " &";
          if (!seg.empty()) parts.push_back(seg);
          cur.clear();
        }
      } else {
        cur += c;
      }
    }
    std::string tail = trim(cur);
    if (!tail.empty()) parts.push_back(tail);
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
    auto pos = first.rfind("; then");
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
          if (!cond_accum.empty()) cond_accum += " ";
          cond_accum += cur.substr(0, p);
          then_found = true;
          break;
        }
        if (!cur.empty()) {
          if (!cond_accum.empty()) cond_accum += " ";
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

    int cond_rc =
        cond_accum.empty() ? 1 : execute_simple_or_pipeline(cond_accum);

    // If '; then' was on the same line, parse inline until '; fi'
    if (pos != std::string::npos) {
      std::string rem = trim(first.substr(pos + 6));  // after '; then'
      // find '; fi' at end (allow spaces): we'll search for "; fi" substring
      size_t fi_pos = rem.rfind("; fi");
      if (fi_pos == std::string::npos) {
        // try ending with 'fi'
        if (rem.size() >= 2 && rem.substr(rem.size() - 2) == "fi") {
          fi_pos = rem.size() - 2;  // assume no preceding semicolon
        }
      }
      std::string body =
          fi_pos == std::string::npos ? rem : trim(rem.substr(0, fi_pos));
      // Split optional else: look for '; else'
      std::string then_body = body;
      std::string else_body;
      size_t else_pos = body.find("; else");
      if (else_pos == std::string::npos) {
        // also allow ' else ' without leading semicolon
        size_t alt = body.find(" else ");
        if (alt != std::string::npos) else_pos = alt;
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
          if (rc2 != 0) break;
        }
      } else if (!else_body.empty()) {
        auto cmds = shell_parser->parse_semicolon_commands(else_body);
        for (const auto& c : cmds) {
          int rc2 = execute_simple_or_pipeline(c);
          body_rc = rc2;
          if (rc2 != 0) break;
        }
      }
      // single-line; do not advance idx
      return body_rc;
    }

    // Multiline: Collect body until matching 'fi', support one else
    size_t k = j + 1;
    int depth = 1;
    bool in_else = false;
    std::vector<std::string> then_lines;
    std::vector<std::string> else_lines;
    while (k < src_lines.size() && depth > 0) {
      std::string cur_raw = src_lines[k];
      std::string cur = trim(strip_inline_comment(cur_raw));
      if (cur == "if")
        depth++;
      else if (cur == "fi") {
        depth--;
        if (depth == 0) break;
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
    if (!(first == "for" || first.rfind("for ", 0) == 0)) return 1;

    std::string var;
    std::vector<std::string> items;

    auto parse_header = [&](const std::string& header) -> bool {
      // Tokenize header to extract var and list after 'in'
      std::vector<std::string> toks = shell_parser->parse_command(header);
      size_t i = 0;
      if (i < toks.size() && toks[i] == "for") ++i;
      if (i >= toks.size()) return false;
      var = toks[i++];
      if (i < toks.size() && toks[i] == "in") {
        ++i;
        for (; i < toks.size(); ++i) {
          if (toks[i] == ";" || toks[i] == "do") break;
          items.push_back(toks[i]);
        }
      }
      return !var.empty();
    };

    // Inline form: for i in 1 2; do body; done
    if (first.find("; do") != std::string::npos) {
      size_t do_pos = first.find("; do");
      std::string header = trim(first.substr(0, do_pos));
      if (!parse_header(header)) return 1;
      std::string tail = trim(first.substr(do_pos + 4));
      size_t done_pos = tail.rfind("; done");
      if (done_pos == std::string::npos) done_pos = tail.rfind("done");
      std::string body =
          done_pos == std::string::npos ? tail : trim(tail.substr(0, done_pos));
      int rc = 0;
      for (const auto& it : items) {
        setenv(var.c_str(), it.c_str(), 1);
        auto cmds = shell_parser->parse_semicolon_commands(body);
        for (const auto& c : cmds) {
          rc = execute_simple_or_pipeline(c);
          if (rc != 0) break;
        }
        if (rc != 0) break;
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
        if (depth == 0) break;
      }
      if (depth > 0) body_lines.push_back(cur_raw);
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
      if (rc != 0) break;
    }
    idx = k;  // at 'done'
    return rc;
  };

  // Minimal while CONDITION; do ...; done handler
  auto handle_while_block = [&](const std::vector<std::string>& src_lines,
                                size_t& idx) -> int {
    std::string first = trim(strip_inline_comment(src_lines[idx]));
    if (!(first == "while" || first.rfind("while ", 0) == 0)) return 1;

    auto parse_cond_from = [&](const std::string& s, std::string& cond,
                               bool& inline_do, std::string& body_inline) {
      inline_do = false;
      body_inline.clear();
      cond.clear();
      std::string tmp = s;
      if (tmp == "while") {
        return true;
      }
      if (tmp.rfind("while ", 0) == 0) tmp = tmp.substr(6);
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
          if (!cond.empty()) cond += " ";
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
      if (done_pos != std::string::npos) bi = trim(bi.substr(0, done_pos));
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
          if (depth == 0) break;
        }
        if (depth > 0) body_lines.push_back(cur_raw);
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
      int c = cond.empty() ? 0 : execute_simple_or_pipeline(cond);
      if (c != 0) break;
      rc = execute_block(body_lines);
      if (rc != 0) break;
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
      if (rc != 0) {
        if (g_debug_mode)
          std::cerr << "DEBUG: if block returned non-zero: " << rc << std::endl;
        return rc;
      }
      // line_index now points at 'fi'; for loop will ++ to next line
      continue;
    }

    // Control structure: for ...; do ...; done
    if (line == "for" || line.rfind("for ", 0) == 0) {
      int rc = handle_for_block(lines, line_index);
      if (rc != 0) return rc;
      continue;
    }

    // Control structure: while ...; do ...; done
    if (line == "while" || line.rfind("while ", 0) == 0) {
      int rc = handle_while_block(lines, line_index);
      if (rc != 0) return rc;
      continue;
    }

    // Break the line into logical command segments (&&, ||) while preserving
    // order
    std::vector<LogicalCommand> lcmds =
        shell_parser->parse_logical_commands(line);
    if (lcmds.empty()) continue;

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
        if (segs.empty()) segs.push_back(semi);
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
              if (i < toks.size() && toks[i] == "for") ++i;
              if (i >= toks.size()) return false;
              var = toks[i++];
              if (i < toks.size() && toks[i] == "in") {
                ++i;
                for (; i < toks.size(); ++i) items.push_back(toks[i]);
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
                if (!body_inline.empty()) body_inline += "; ";
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
                  if (rc2 != 0) break;
                }
                if (rc2 != 0) break;
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
                if (!body_inline.empty()) body_inline += "; ";
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
                int cnd = cond.empty() ? 0 : execute_simple_or_pipeline(cond);
                if (cnd != 0) break;
                auto cmds2 =
                    shell_parser->parse_semicolon_commands(body_inline);
                for (const auto& c2 : cmds2) {
                  rc2 = execute_simple_or_pipeline(c2);
                  if (rc2 != 0) break;
                }
                if (rc2 != 0) break;
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
          int code = execute_simple_or_pipeline(cmd_text);
          last_code = code;
          if (code != 0 && debug_level >= DebugLevel::BASIC) {
            std::cerr << "DEBUG: Command failed (" << code << ") -> '"
                      << cmd_text << "'" << std::endl;
          }
        }
      }
    }

    if (last_code != 0) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Stopping script block due to exit code "
                  << last_code << std::endl;
      return last_code;
    }
  }

  return last_code;
}
