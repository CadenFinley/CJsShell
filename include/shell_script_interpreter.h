#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

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
    bool executeScript(const std::string& filename);
    
    // Execute a single line of shell script
    bool executeLine(const std::string& line);
    
    // Parse and execute a block of shell script
    bool executeBlock(const std::vector<std::string>& lines);
    
    // Set command executor callback
    void setCommandExecutor(std::function<bool(const std::string&, bool)> executor);
    
    // Debug control methods
    void setDebugLevel(DebugLevel level);
    DebugLevel getDebugLevel() const;
    void debugPrint(const std::string& message, DebugLevel level = DebugLevel::BASIC) const;
    void dumpVariables() const;

private:
    // Shell script parsing - updated parameter types to fix const correctness
    bool parseConditional(std::vector<std::string>::iterator& it, const std::vector<std::string>::const_iterator& end);
    bool parseLoop(std::vector<std::string>::iterator& it, const std::vector<std::string>::const_iterator& end);
    bool evaluateCondition(const std::string& condition);
    
    // Variable handling
    std::string expandVariables(const std::string& str);
    std::string executeCommandSubstitution(const std::string& cmd);
    
    // Utility functions
    std::string trimString(const std::string& str);
    std::vector<std::string> splitCommand(const std::string& cmd);
    
    // Debug related methods
    bool handleDebugCommand(const std::string& command);
    std::string escapeDebugString(const std::string& input) const;
    
    // Special case handlers
    bool handlePathHelper();
    
    // State
    std::map<std::string, std::string> localVariables;
    std::function<bool(const std::string&, bool)> commandExecutor;
    bool inThenBlock; // Flag to track if we're in a then block
    
    // Debug state
    DebugLevel debugLevel;
    bool showCommandOutput;
    int debugIndentLevel;
    std::string getIndentation() const;
};

extern ShellScriptInterpreter* g_script_interpreter;
