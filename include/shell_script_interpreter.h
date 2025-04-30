#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include "main.h"

class Shell;

enum class DebugLevel {
    NONE = 0,
    BASIC = 1,
    VERBOSE = 2,
    TRACE = 3
};

class ShellScriptInterpreter {
public:
    ShellScriptInterpreter();
    ~ShellScriptInterpreter();

    bool execute_script(const std::string& filename);

    bool execute_line(const std::string& line);
    
    bool execute_block(const std::vector<std::string>& lines);
    
    void set_command_executor(std::function<bool(const std::string&, bool)> executor);
    
    void set_debug_level(DebugLevel level);
    DebugLevel get_debug_level() const;
    void debug_print(const std::string& message, DebugLevel level = DebugLevel::BASIC) const;
    void dump_variables() const;

private:
    bool parse_conditional(std::vector<std::string>::iterator& it, const std::vector<std::string>::const_iterator& end);
    bool parse_loop(std::vector<std::string>::iterator& it, const std::vector<std::string>::const_iterator& end);
    bool evaluate_condition(const std::string& condition);
    
    std::string expand_variables(const std::string& str);
    std::string execute_command_substitution(const std::string& cmd);
    
    std::string trim_string(const std::string& str);
    std::vector<std::string> split_command(const std::string& cmd);
    
    bool handle_debug_command(const std::string& command);
    std::string escape_debug_string(const std::string& input) const;
    
    bool handle_path_helper();
    
    std::string capture_command_output(const std::string& cmd);
    
    std::map<std::string, std::string> local_variables;
    std::function<bool(const std::string&, bool)> command_executor;
    bool in_then_block;
    
    DebugLevel debug_level;
    bool show_command_output;
    int debug_indent_level;
    mutable int cached_indent_level;
    mutable std::string cached_indentation;
    std::string get_indentation() const;
};
