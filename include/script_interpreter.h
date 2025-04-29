#pragma once
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <functional>

class ScriptInterpreter {
private:
    // Callback function type for executing commands
    using CommandExecutor = std::function<void(const std::vector<std::string>&)>;
    
    // Reference to the command executor function
    CommandExecutor executor;

public:
    ScriptInterpreter(CommandExecutor exec_function);
    ~ScriptInterpreter();

    // Script parsing and interpretation functions
    bool evaluate_condition(const std::vector<std::string>& args);
    std::string process_command_substitution(const std::string& input);
    void execute_if_statement(const std::vector<std::string>& args);
    
    // New method to execute an entire shell script file
    bool execute_script_file(const std::string& script_path);
};
