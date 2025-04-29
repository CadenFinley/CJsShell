#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include "main.h"

class Shell;

// Debug verbosity levels
enum class DebugLevel {
    NONE = 0,     // No debugging
    BASIC = 1,    // Basic command tracing
    VERBOSE = 2,  // Detailed command and variable info
    TRACE = 3     // Full execution trace with all details
};

class ShellScriptInterpreter {
public:
    ShellScriptInterpreter();
    ~ShellScriptInterpreter();

    // Execute a shell script from a file
    bool execute_script(const std::string& filename);
    
    // Execute a single line of shell script
    bool execute_line(const std::string& line);
    
    // Execute a block of shell script
    bool execute_block(const std::vector<std::string>& lines);
    
    // Set command executor callback
    void set_command_executor(std::function<bool(const std::string&, bool)> executor);
    
    // Debug control methods
    void set_debug_level(DebugLevel level);
    DebugLevel get_debug_level() const;
    void debug_print(const std::string& message, DebugLevel level = DebugLevel::BASIC) const;
    void dump_variables() const;

private:
    // Shell script parsing
    bool parse_conditional(std::vector<std::string>::iterator& it, const std::vector<std::string>::const_iterator& end);
    bool parse_loop(std::vector<std::string>::iterator& it, const std::vector<std::string>::const_iterator& end);
    bool evaluate_condition(const std::string& condition);
    
    // Variable handling
    std::string expand_variables(const std::string& str);
    std::string execute_command_substitution(const std::string& cmd);
    
    // Utility functions
    std::string trim_string(const std::string& str);
    std::vector<std::string> split_command(const std::string& cmd);
    
    // Debug related methods
    bool handle_debug_command(const std::string& command);
    std::string escape_debug_string(const std::string& input) const;
    
    // Special case handlers
    bool handle_path_helper();
    
    // Shell command execution with output capture
    std::string capture_command_output(const std::string& cmd);
    
    // State
    std::map<std::string, std::string> local_variables;
    std::function<bool(const std::string&, bool)> command_executor;
    bool in_then_block; // Flag to track if we're in a then block
    
    // Debug state
    DebugLevel debug_level;
    bool show_command_output;
    int debug_indent_level;
    mutable int cached_indent_level;        // Cache for indentation level
    mutable std::string cached_indentation; // Cached indentation string
    std::string get_indentation() const;
};
