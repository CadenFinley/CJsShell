#include "shell_script_interpreter.h"
#include "shell.h"
#include "main.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iomanip>

ShellScriptInterpreter* g_script_interpreter = nullptr;

ShellScriptInterpreter::ShellScriptInterpreter() {
  if (g_debug_mode) std::cerr << "DEBUG: Initializing ShellScriptInterpreter" << std::endl;
  
  command_executor = [](const std::string& cmd, bool) -> bool {
    if (g_debug_mode) std::cerr << "DEBUG: Executing shell command: " << cmd << std::endl;
    return system(cmd.c_str()) == 0;
  };
  
  in_then_block = false;
  
  debug_level = DebugLevel::NONE;
  show_command_output = false;
  debug_indent_level = 0;
  cached_indent_level = -1;
}

ShellScriptInterpreter::~ShellScriptInterpreter() {
}

void ShellScriptInterpreter::set_debug_level(DebugLevel level) {
  if (g_debug_mode) std::cerr << "DEBUG: Setting script interpreter debug level to " << static_cast<int>(level) << std::endl;
  debug_level = level;
}

DebugLevel ShellScriptInterpreter::get_debug_level() const {
    return debug_level;
}

void ShellScriptInterpreter::debug_print(const std::string& message, DebugLevel level) const {
    if (static_cast<int>(debug_level) >= static_cast<int>(level)) {
        std::cerr << get_indentation() << "[DEBUG] " << message << std::endl;
    }
}

std::string ShellScriptInterpreter::get_indentation() const {
    // Cache the indentation string for performance
    if (cached_indent_level != debug_indent_level) {
        cached_indentation = std::string(debug_indent_level * 2, ' ');
        cached_indent_level = debug_indent_level;
    }
    return cached_indentation;
}

void ShellScriptInterpreter::dump_variables() const {
  if (g_debug_mode) std::cerr << "DEBUG: Dumping script interpreter variables" << std::endl;
  
  if (debug_level == DebugLevel::NONE) {
    return;
  }
    
    // Use a stringstream for more efficient string building
    std::stringstream ss;
    ss << get_indentation() << "[DEBUG] Variable dump:" << std::endl;
    
    // Find max length in one pass
    size_t max_length = 0;
    for (const auto& pair : local_variables) {
        max_length = std::max(max_length, pair.first.length());
    }
    
    // Output formatted variables
    for (const auto& pair : local_variables) {
        ss << get_indentation() << "  " << std::left 
           << std::setw(max_length + 2) << pair.first 
           << "= \"" << pair.second << "\"" << std::endl;
    }
    
    const char* env_vars[] = {
        "PATH", "HOME", "USER", "SHELL", "PWD", "OLDPWD",
        "TERM", "LANG", "LC_ALL", "DISPLAY",
        "?", "PPID", "PS1", "PS2", 
        "HOSTNAME", "OSTYPE", "MACHTYPE", "LOGNAME",
        "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "MANPATH",
        "JAVA_HOME", "PYTHONPATH", "GOPATH", "NODE_PATH"
    };
    ss << get_indentation() << "[DEBUG] Key environment variables:" << std::endl;
    for (const char* var : env_vars) {
        const char* value = getenv(var);
        if (value) {
            ss << get_indentation() << "  " << std::left 
               << std::setw(max_length + 2) << var 
               << "= \"" << value << "\"" << std::endl;
        }
    }
    std::cerr << ss.str();
}

std::string ShellScriptInterpreter::escape_debug_string(const std::string& input) const {
  if (g_debug_mode) std::cerr << "DEBUG: Escaping debug string of length " << input.length() << std::endl;
  
  std::string result;
  for (char c : input) {
    if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (c == '\t') result += "\\t";
    else if (c < 32 || c > 126) {
      char buf[5];
      snprintf(buf, sizeof(buf), "\\x%02X", static_cast<unsigned char>(c));
      result += buf;
    } else {
      result += c;
    }
  }
  return result;
}

bool ShellScriptInterpreter::handle_debug_command(const std::string& command) {
  if (g_debug_mode) std::cerr << "DEBUG: Handling debug command: " << command << std::endl;
  
  if (command == "debug on") {
    debug_level = DebugLevel::BASIC;
    std::cerr << "Debug mode ON (basic)" << std::endl;
    return true;
  } else if (command == "debug off") {
    debug_level = DebugLevel::NONE;
    std::cerr << "Debug mode OFF" << std::endl;
    return true;
  } else if (command == "debug verbose") {
    debug_level = DebugLevel::VERBOSE;
    std::cerr << "Debug mode ON (verbose)" << std::endl;
    return true;
  } else if (command == "debug trace") {
    debug_level = DebugLevel::TRACE;
    std::cerr << "Debug mode ON (trace)" << std::endl;
    return true;
  } else if (command == "debug level") {
    std::cerr << "Current debug level: " << static_cast<int>(debug_level) << std::endl;
    return true;
  } else if (command == "debug vars") {
    dump_variables();
    return true;
  } else if (command == "debug show_output on") {
    show_command_output = true;
    std::cerr << "Command output display ON" << std::endl;
    return true;
  } else if (command == "debug show_output off") {
    show_command_output = false;
    std::cerr << "Command output display OFF" << std::endl;
    return true;
  } else if (command == "debug safe_mode on") {
    debug_print("Enabling safe mode - path operations will be more carefully validated", DebugLevel::BASIC);
    local_variables["CJSH_SAFE_MODE"] = "1";
    return true;
  } else if (command == "debug safe_mode off") {
    debug_print("Disabling safe mode", DebugLevel::BASIC);
    local_variables["CJSH_SAFE_MODE"] = "0";
    return true;
  }
    
    return false;
}

bool ShellScriptInterpreter::execute_script(const std::string& filename) {
    debug_print("Executing script: " + filename);
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open script file: " << filename << std::endl;
        return false;
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    if (debug_level >= DebugLevel::VERBOSE) {
        debug_print("Script content (" + std::to_string(lines.size()) + " lines):", DebugLevel::VERBOSE);
        for (size_t i = 0; i < lines.size(); i++) {
            debug_print(std::to_string(i+1) + ": " + lines[i], DebugLevel::VERBOSE);
        }
    }
    
    return execute_block(lines);
}

bool ShellScriptInterpreter::execute_block(const std::vector<std::string>& lines) {
    std::vector<std::string> lines_copy = lines;
    
    for (auto it = lines_copy.begin(); it != lines_copy.end(); ) {
        std::string trimmed = trim_string(*it);
        
        if (trimmed.empty() || trimmed[0] == '#') {
            debug_print("Skipping empty line or comment: " + escape_debug_string(*it), DebugLevel::VERBOSE);
            ++it;
            continue;
        }
        
        if (trimmed.find("if ") == 0) {
            debug_print("Found if statement: " + escape_debug_string(trimmed), DebugLevel::VERBOSE);
            
            if (trimmed.find("; then") != std::string::npos) {
                debug_print("Found 'then' on same line", DebugLevel::VERBOSE);
            }
            
            if (!parse_conditional(it, lines_copy.end())) {
                return false;
            }
            continue;
        }
        
        if (trimmed.find("for ") == 0 || trimmed.find("while ") == 0) {
            if (!parse_loop(it, lines_copy.end())) {
                return false;
            }
            continue;
        }
        
        if (!execute_line(trimmed)) {
            debug_print("Command failed: " + escape_debug_string(trimmed), DebugLevel::BASIC);
            return false;
        }
        
        ++it;
    }
    
    return true;
}

bool ShellScriptInterpreter::execute_line(const std::string& line) {
    std::string trimmed = trim_string(line);
    
    if (trimmed.empty()) {
        debug_print("Skipping empty line", DebugLevel::VERBOSE);
        return true;
    }
    
    try {
        debug_print("Executing line: " + escape_debug_string(trimmed), DebugLevel::BASIC);
        
        if (trimmed[0] == '#') {
            debug_print("Skipping comment", DebugLevel::VERBOSE);
            return true;
        }
        
        // Handle command line style arguments (--flag style)
        if (trimmed.size() >= 2 && trimmed[0] == '-' && trimmed[1] == '-') {
            debug_print("Found startup argument: " + trimmed, DebugLevel::BASIC);
            g_startup_args.push_back(trimmed);
            debug_print("Added to startup args: " + trimmed, DebugLevel::VERBOSE);
            return true;
        }
        
        // Handle variable assignment
        size_t equals_pos = trimmed.find('=');
        if (equals_pos != std::string::npos && 
            (trimmed.find(' ') > equals_pos || trimmed.find(' ') == std::string::npos)) {
            std::string var_name = trim_string(trimmed.substr(0, equals_pos));
            std::string var_value = trim_string(trimmed.substr(equals_pos + 1));
            
            debug_print("Variable assignment: " + var_name + "=" + var_value, DebugLevel::VERBOSE);
            
            if ((var_value.front() == '"' && var_value.back() == '"') ||
                (var_value.front() == '\'' && var_value.back() == '\'')) {
                var_value = var_value.substr(1, var_value.size() - 2);
                debug_print("Removed quotes: " + var_value, DebugLevel::TRACE);
            }
            
            bool is_export = false;
            if (var_name.find("export ") == 0) {
                is_export = true;
                var_name = trim_string(var_name.substr(7));
                debug_print("Export variable: " + var_name, DebugLevel::VERBOSE);
            }
            
            std::string orig_value = var_value;
            var_value = expand_variables(var_value);
            
            if (orig_value != var_value && debug_level >= DebugLevel::VERBOSE) {
                debug_print("Expanded value: " + orig_value + " -> " + var_value, DebugLevel::VERBOSE);
            }
            
            if (var_name == "PATH" && var_value.empty()) {
                debug_print("WARNING: Attempt to set PATH to empty string, using default value instead", DebugLevel::BASIC);
                var_value = "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin";
            }
            
            local_variables[var_name] = var_value;
            
            if (is_export) {
                try {
                    setenv(var_name.c_str(), var_value.c_str(), 1);
                    debug_print("Set environment variable: " + var_name + "=" + var_value, DebugLevel::VERBOSE);
                } catch (const std::exception& e) {
                    debug_print("ERROR: Failed to set environment variable: " + var_name + " - " + e.what(), DebugLevel::BASIC);
                    return false;
                }
            }
            
            return true;
        }
        
        // Handle if/then statements
        if (trimmed.find("if ") == 0) {
            size_t then_pos = trimmed.find("; then");
            if (then_pos != std::string::npos) {
                std::string condition = trim_string(trimmed.substr(3, then_pos - 3));
                debug_print("Evaluating condition: " + condition, DebugLevel::VERBOSE);
                bool result = evaluate_condition(condition);
                debug_print("Condition result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
                
                local_variables["?"] = result ? "0" : "1";
                return true;
            }
            
            return true;
        }
        
        if (trimmed == "then") {
            debug_print("Then statement", DebugLevel::VERBOSE);
            return true;
        }
        
        // Handle eval commands
        if (trimmed.find("eval ") == 0) {
            std::string cmd = trim_string(trimmed.substr(4));
            debug_print("Eval command: " + escape_debug_string(cmd), DebugLevel::VERBOSE);
            
            if (cmd.front() == '`' && cmd.back() == '`') {
                cmd = cmd.substr(1, cmd.size() - 2);
                debug_print("Backtick command: " + escape_debug_string(cmd), DebugLevel::VERBOSE);
                
                try {
                    std::string expanded_cmd = expand_variables(cmd);
                    if (expanded_cmd != cmd && debug_level >= DebugLevel::VERBOSE) {
                        debug_print("Expanded command: " + escape_debug_string(expanded_cmd), DebugLevel::VERBOSE);
                    }
                    
                    debug_print("Executing backtick command", DebugLevel::VERBOSE);
                    std::string result = execute_command_substitution(expanded_cmd);
                    
                    result = trim_string(result);
                    debug_print("Command result: " + escape_debug_string(result), DebugLevel::VERBOSE);
                    
                    // Use global Shell to execute the result if available
                    if (g_shell) {
                        debug_print("Using shell to execute result: " + escape_debug_string(result), DebugLevel::VERBOSE);
                        g_shell->execute_command(result, true);
                        return g_shell->get_last_exit_code() == 0;
                    } else {
                        return command_executor(result, true);
                    }
                    
                } catch (const std::exception& e) {
                    debug_print("ERROR in eval command: " + std::string(e.what()), DebugLevel::BASIC);
                    return false;
                }
            }
            
            try {
                std::string expanded_cmd = expand_variables(cmd);
                if (expanded_cmd != cmd && debug_level >= DebugLevel::VERBOSE) {
                    debug_print("Expanded eval command: " + expanded_cmd, DebugLevel::VERBOSE);
                }
                
                debug_print("Executing eval command", DebugLevel::BASIC);
                
                // Use global Shell to execute if available
                if (g_shell) {
                    debug_print("Using shell to execute eval command: " + escape_debug_string(expanded_cmd), DebugLevel::VERBOSE);
                    g_shell->execute_command(expanded_cmd, true);
                    return g_shell->get_last_exit_code() == 0;
                } else {
                    return command_executor(expanded_cmd, true);
                }
            } catch (const std::exception& e) {
                debug_print("ERROR in eval command: " + std::string(e.what()), DebugLevel::BASIC);
                return false;
            }
        }
        
        std::string expanded_cmd = expand_variables(trimmed);
        if (expanded_cmd != trimmed && debug_level >= DebugLevel::VERBOSE) {
            debug_print("Expanded command: " + expanded_cmd, DebugLevel::VERBOSE);
        }
        
        debug_print("Executing command", DebugLevel::BASIC);
        
        // Use global Shell to execute if available
        if (g_shell) {
            debug_print("Using shell to execute command: " + escape_debug_string(expanded_cmd), DebugLevel::VERBOSE);
            g_shell->execute_command(expanded_cmd, true);
            return g_shell->get_last_exit_code() == 0;
        } else {
            return command_executor(expanded_cmd, true);
        }
    } catch (const std::exception& e) {
        debug_print("ERROR: Exception in execute_line: " + std::string(e.what()), DebugLevel::BASIC);
        return false;
    }
}

bool ShellScriptInterpreter::evaluate_condition(const std::string& condition) {
    debug_print("Evaluating condition: " + escape_debug_string(condition), DebugLevel::VERBOSE);
    
    if (condition.front() == '[' && condition.back() == ']') {
        std::string test_condition = trim_string(condition.substr(1, condition.size() - 2));
        debug_print("Test condition: " + escape_debug_string(test_condition), DebugLevel::VERBOSE);
        
        if (test_condition.find("-x ") == 0) {
            std::string file_path = expand_variables(trim_string(test_condition.substr(3)));
            debug_print("Checking if file is executable: " + file_path, DebugLevel::VERBOSE);
            bool result = access(file_path.c_str(), X_OK) == 0;
            debug_print("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        if (test_condition.find("-e ") == 0) {
            std::string file_path = expand_variables(trim_string(test_condition.substr(3)));
            debug_print("Checking if file exists: " + file_path, DebugLevel::VERBOSE);
            bool result = access(file_path.c_str(), F_OK) == 0;
            debug_print("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        if (test_condition.find("-r ") == 0) {
            std::string file_path = expand_variables(trim_string(test_condition.substr(3)));
            debug_print("Checking if file is readable: " + file_path, DebugLevel::VERBOSE);
            bool result = access(file_path.c_str(), R_OK) == 0;
            debug_print("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        size_t eq_pos = test_condition.find(" = ");
        if (eq_pos != std::string::npos) {
            std::string lhs = expand_variables(trim_string(test_condition.substr(0, eq_pos)));
            std::string rhs = expand_variables(trim_string(test_condition.substr(eq_pos + 3)));
            debug_print("Comparing strings: '" + lhs + "' = '" + rhs + "'", DebugLevel::VERBOSE);
            bool result = lhs == rhs;
            debug_print("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        size_t neq_pos = test_condition.find(" != ");
        if (neq_pos != std::string::npos) {
            std::string lhs = expand_variables(trim_string(test_condition.substr(0, neq_pos)));
            std::string rhs = expand_variables(trim_string(test_condition.substr(neq_pos + 4)));
            debug_print("Comparing strings: '" + lhs + "' != '" + rhs + "'", DebugLevel::VERBOSE);
            bool result = lhs != rhs;
            debug_print("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        if (test_condition.find("-n ") == 0) {
            std::string value = expand_variables(trim_string(test_condition.substr(3)));
            debug_print("Checking if string is non-empty: '" + value + "'", DebugLevel::VERBOSE);
            bool result = !value.empty();
            debug_print("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        if (test_condition.find("-z ") == 0) {
            std::string value = expand_variables(trim_string(test_condition.substr(3)));
            debug_print("Checking if string is empty: '" + value + "'", DebugLevel::VERBOSE);
            bool result = value.empty();
            debug_print("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        debug_print("Unrecognized test condition: " + test_condition, DebugLevel::BASIC);
        return false;
    }
    
    std::string expanded_condition = expand_variables(condition);
    debug_print("Executing condition command: " + expanded_condition, DebugLevel::VERBOSE);
    
    FILE* pipe = popen(expanded_condition.c_str(), "r");
    if (!pipe) {
        debug_print("Failed to execute condition command", DebugLevel::BASIC);
        return false;
    }
    
    int status = pclose(pipe);
    bool result = status == 0;
    debug_print("Command returned: " + std::to_string(status) + ", result: " + 
               std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
    return result;
}

bool ShellScriptInterpreter::parse_conditional(std::vector<std::string>::iterator& it, 
                                             const std::vector<std::string>::const_iterator& end) {
    std::string line = *it;
    std::string condition;
    bool condition_met = false;
    
    debug_print("Parsing conditional: " + escape_debug_string(line), DebugLevel::VERBOSE);
    
    size_t then_pos = line.find("; then");
    if (then_pos != std::string::npos) {
        condition = trim_string(line.substr(3, then_pos - 3));
        debug_print("Extracted condition from same line: " + escape_debug_string(condition), DebugLevel::VERBOSE);
        condition_met = evaluate_condition(condition);
        in_then_block = true;
    } else {
        condition = trim_string(line.substr(3));
        debug_print("Extracted condition from separate line: " + escape_debug_string(condition), DebugLevel::VERBOSE);
        condition_met = evaluate_condition(condition);
        in_then_block = false;
    }
    
    debug_print("Condition result: " + std::string(condition_met ? "true" : "false"), DebugLevel::VERBOSE);
    
    local_variables["?"] = condition_met ? "0" : "1";
    
    bool executing_block = condition_met;
    bool found_else = false;
    
    std::vector<std::string> current_block;
    
    ++it;
    
    while (it != end) {
        std::string current_line = trim_string(*it);
        
        debug_print("Conditional processing line: " + escape_debug_string(current_line), DebugLevel::TRACE);
        
        if (current_line == "then") {
            debug_print("Found 'then' statement", DebugLevel::VERBOSE);
            in_then_block = true;
            ++it;
            continue;
        } else if (current_line == "else") {
            if (condition_met && !current_block.empty()) {
                debug_print("Executing 'then' block", DebugLevel::VERBOSE);
                execute_block(current_block);
            }
            
            current_block.clear();
            executing_block = !condition_met;
            found_else = true;
            ++it;
            continue;
        } else if (current_line == "fi") {
            if (executing_block && !current_block.empty()) {
                debug_print("Executing final block", DebugLevel::VERBOSE);
                execute_block(current_block);
            }
            
            ++it;
            return true;
        } else if (current_line.find("elif ") == 0) {
            if (condition_met && !current_block.empty()) {
                execute_block(current_block);
            }
            
            if (!condition_met && !found_else) {
                condition = trim_string(current_line.substr(5));
                condition_met = evaluate_condition(condition);
                executing_block = condition_met;
            } else {
                executing_block = false;
            }
            
            current_block.clear();
            ++it;
            continue;
        }
        
        if (in_then_block && executing_block) {
            debug_print("Adding line to conditional block: " + escape_debug_string(current_line), DebugLevel::TRACE);
            current_block.push_back(current_line);
        } else {
            debug_print("Skipping line in inactive block: " + escape_debug_string(current_line), DebugLevel::TRACE);
        }
        
        ++it;
    }
    
    std::cerr << "Error: Unexpected end of if block (missing fi)" << std::endl;
    return false;
}

bool ShellScriptInterpreter::parse_loop(std::vector<std::string>::iterator& it, 
                                      const std::vector<std::string>::const_iterator& end) {
    std::string loop_line = *it;
    bool is_for_loop = loop_line.find("for ") == 0;
    
    std::vector<std::string> loop_body;
    
    bool in_do_block = false;
    ++it;
    
    while (it != end) {
        std::string line = trim_string(*it);
        
        if (line == "do") {
            in_do_block = true;
            ++it;
            continue;
        } else if (line == "done") {
            break;
        } else if (in_do_block) {
            loop_body.push_back(line);
        }
        
        ++it;
    }
    
    if (it == end) {
        std::cerr << "Error: Unexpected end of loop (missing done)" << std::endl;
        return false;
    }
    
    if (is_for_loop) {
        std::string for_decl = trim_string(loop_line.substr(4));
        size_t in_pos = for_decl.find(" in ");
        
        if (in_pos != std::string::npos) {
            std::string var_name = trim_string(for_decl.substr(0, in_pos));
            std::string value_list = trim_string(for_decl.substr(in_pos + 4));
            
            std::vector<std::string> values = split_command(value_list);
            
            for (const auto& value : values) {
                local_variables[var_name] = value;
                
                execute_block(loop_body);
            }
        }
    } else {
        std::string while_condition = trim_string(loop_line.substr(6));
        
        while (evaluate_condition(while_condition)) {
            execute_block(loop_body);
        }
    }
    
    ++it;
    return true;
}

std::string ShellScriptInterpreter::expand_variables(const std::string& str) {
    if (str.empty()) {
        debug_print("Empty string for variable expansion", DebugLevel::TRACE);
        return str;
    }
    
    std::string result = str;
    size_t pos = 0;
    
    debug_print("Expanding variables in: " + escape_debug_string(str), DebugLevel::TRACE);
    
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end_pos = result.find("}", pos + 2);
        if (end_pos == std::string::npos) break;
        
        std::string var_name = result.substr(pos + 2, end_pos - pos - 2);
        std::string var_value;
        
        auto it = local_variables.find(var_name);
        if (it != local_variables.end()) {
            var_value = it->second;
        } else {
            const char* env_value = getenv(var_name.c_str());
            if (env_value) {
                var_value = env_value;
            }
        }
        
        result.replace(pos, end_pos - pos + 1, var_value);
        pos += var_value.length();
    }
    
    pos = 0;
    while ((pos = result.find('$', pos)) != std::string::npos) {
        if (pos + 1 < result.size() && result[pos + 1] == '{') {
            pos += 2;
            continue;
        }
        
        size_t end_pos = pos + 1;
        while (end_pos < result.size() && 
               (isalnum(result[end_pos]) || result[end_pos] == '_')) {
            end_pos++;
        }
        
        if (end_pos > pos + 1) {
            std::string var_name = result.substr(pos + 1, end_pos - pos - 1);
            std::string var_value;
            
            auto it = local_variables.find(var_name);
            if (it != local_variables.end()) {
                var_value = it->second;
            } else {
                const char* env_value = getenv(var_name.c_str());
                if (env_value) {
                    var_value = env_value;
                }
            }
            
            result.replace(pos, end_pos - pos, var_value);
            pos += var_value.length();
        } else {
            pos++;
        }
    }
    
    pos = 0;
    while ((pos = result.find("$(", pos)) != std::string::npos) {
        size_t depth = 1;
        size_t end_pos = pos + 2;
        
        while (end_pos < result.size() && depth > 0) {
            if (result[end_pos] == '(') depth++;
            else if (result[end_pos] == ')') depth--;
            end_pos++;
        }
        
        if (depth == 0) {
            std::string cmd = result.substr(pos + 2, end_pos - pos - 3);
            std::string cmd_output = execute_command_substitution(cmd);
            
            std::stringstream ss(cmd_output);
            std::string processed_output;
            std::string output_line;
            
            while (std::getline(ss, output_line)) {
                std::string trimmed_line = trim_string(output_line);
                if (!trimmed_line.empty()) {
                    if (!processed_output.empty()) {
                        processed_output += " ";
                    }
                    processed_output += trimmed_line;
                }
            }
            
            debug_print("Processed backtick output: " + escape_debug_string(processed_output), DebugLevel::VERBOSE);
            result.replace(pos, end_pos - pos + 1, processed_output);
            pos += processed_output.length();
        } else {
            pos += 2;
        }
    }
    
    pos = 0;
    while ((pos = result.find('`', pos)) != std::string::npos) {
        size_t end_pos = result.find('`', pos + 1);
        if (end_pos == std::string::npos) {
            debug_print("Warning: Unmatched backtick at position " + std::to_string(pos), DebugLevel::BASIC);
            break;
        }
        
        std::string cmd = result.substr(pos + 1, end_pos - pos - 1);
        debug_print("Backtick command: " + escape_debug_string(cmd), DebugLevel::VERBOSE);
        
        std::string expanded_cmd = expand_variables(cmd);
        if (expanded_cmd != cmd) {
            debug_print("Expanded backtick command: " + escape_debug_string(expanded_cmd), DebugLevel::VERBOSE);
        }
        
        std::string cmd_output = execute_command_substitution(expanded_cmd);
        
        std::stringstream ss(cmd_output);
        std::string processed_output;
        std::string output_line;
        
        while (std::getline(ss, output_line)) {
            std::string trimmed_line = trim_string(output_line);
            if (!trimmed_line.empty()) {
                if (!processed_output.empty()) {
                    processed_output += " ";
                }
                processed_output += trimmed_line;
            }
        }
        
        debug_print("Processed backtick output: " + escape_debug_string(processed_output), DebugLevel::VERBOSE);
        result.replace(pos, end_pos - pos + 1, processed_output);
        pos += processed_output.length();
    }
    
    if (result.length() == 1) {
        char c = result[0];
        if (c == '\n' || c == '\r' || (c < 32 && c != '\t')) {
            debug_print("Filtered control character in expansion: " + escape_debug_string(result), DebugLevel::VERBOSE);
            return "";
        }
    }
    
    debug_print("Expanded result: " + escape_debug_string(result), DebugLevel::TRACE);
    return result;
}

void ShellScriptInterpreter::set_command_executor(std::function<bool(const std::string&, bool)> executor) {
    command_executor = executor;
}

std::string ShellScriptInterpreter::execute_command_substitution(const std::string& cmd) {
    if (trim_string(cmd).empty()) {
        debug_print("Skipping empty command substitution", DebugLevel::VERBOSE);
        return "";
    }
    
    debug_print("Command substitution: " + escape_debug_string(cmd), DebugLevel::VERBOSE);
    
    if (cmd.find("/usr/libexec/path_helper") != std::string::npos) {
        debug_print("Executing path_helper command with extra caution", DebugLevel::VERBOSE);
    }
    
    // If we have a global shell reference, use it to capture command output
    if (g_shell) {
        return capture_command_output(cmd);
    }
    
    // Fall back to the original implementation if no shell is available
    FILE* pipe = nullptr;
    try {
        pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::string error_msg = "Error executing command: " + escape_debug_string(cmd);
            debug_print(error_msg, DebugLevel::BASIC);
            std::cerr << error_msg << std::endl;
            return "";
        }
        
        std::string result;
        char buffer[128];
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        
        int status = pclose(pipe);
        pipe = nullptr;
        
        if (status != 0) {
            debug_print("Command returned non-zero status: " + std::to_string(status), DebugLevel::VERBOSE);
        }
        
        if (result.empty() && cmd.find("path_helper") != std::string::npos) {
            debug_print("WARNING: path_helper command returned empty result", DebugLevel::BASIC);
            return "PATH=\"/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin\"; export PATH;";
        }
        
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        
        debug_print("Command substitution result: " + escape_debug_string(result), DebugLevel::VERBOSE);
        return result;
    }
    catch (const std::exception& e) {
        debug_print("ERROR in command substitution: " + std::string(e.what()), DebugLevel::BASIC);
        if (pipe != nullptr) {
            pclose(pipe);
        }
        return "";
    }
}

// New method to capture command output using Shell's execution mechanism
std::string ShellScriptInterpreter::capture_command_output(const std::string& cmd) {
    debug_print("Capturing output for command: " + escape_debug_string(cmd), DebugLevel::VERBOSE);
    
    // We need to redirect output to a temporary file
    char temp_filename[256];
    snprintf(temp_filename, sizeof(temp_filename), "/tmp/cjsh_cmd_output_%d", getpid());
    
    // Create the redirection command
    std::string redirect_cmd = cmd + " > " + temp_filename + " 2>&1";
    debug_print("Redirected command: " + escape_debug_string(redirect_cmd), DebugLevel::VERBOSE);
    
    // Execute the command with redirection using the global shell
    if (g_shell) {
        g_shell->execute_command(redirect_cmd, true);
    } else {
        std::cerr << "Error: No shell available for command execution" << std::endl;
    }
    
    // Read the output from the temporary file
    std::ifstream file(temp_filename);
    std::string result;
    std::string line;
    
    if (file.is_open()) {
        while (std::getline(file, line)) {
            result += line + "\n";
        }
        file.close();
        
        // Remove the temporary file
        unlink(temp_filename);
        
        // Remove trailing newlines
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
    } else {
        debug_print("Failed to open temporary output file", DebugLevel::BASIC);
    }
    
    debug_print("Captured output: " + escape_debug_string(result), DebugLevel::VERBOSE);
    return result;
}

std::string ShellScriptInterpreter::trim_string(const std::string& str) {
    if (str.empty()) {
        return "";
    }
    
    bool only_whitespace = true;
    for (char c : str) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            only_whitespace = false;
            break;
        }
    }
    
    if (only_whitespace) {
        return "";
    }
    
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos) {
        return "";
    }
    
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> ShellScriptInterpreter::split_command(const std::string& cmd) {
    std::vector<std::string> result;
    std::stringstream ss(cmd);
    std::string item;
    
    bool in_quotes = false;
    char quote_char = '\0';
    std::string current;
    
    for (char c : cmd) {
        if (c == '"' || c == '\'') {
            if (!in_quotes) {
                in_quotes = true;
                quote_char = c;
            } else if (c == quote_char) {
                in_quotes = false;
                quote_char = '\0';
            } else {
                current += c;
            }
        } else if (c == ' ' && !in_quotes) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        result.push_back(current);
    }
    
    return result;
}
