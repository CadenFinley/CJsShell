#include "script_interpreter.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>
#include <fstream>

ScriptInterpreter::ScriptInterpreter(CommandExecutor exec_function) 
    : executor(exec_function) {
}

ScriptInterpreter::~ScriptInterpreter() {
}

bool ScriptInterpreter::evaluate_condition(const std::vector<std::string>& args) {
    if (args.size() < 3) return false;
    
    // Handle [ -x path ] condition
    if (args[1] == "-x") {
        return access(args[2].c_str(), X_OK) == 0;
    }
    // Handle [ -f path ] condition (file exists and is regular)
    else if (args[1] == "-f") {
        struct stat statbuf;
        if (stat(args[2].c_str(), &statbuf) == 0)
            return S_ISREG(statbuf.st_mode);
        return false;
    }
    // Handle [ -d path ] condition (directory exists)
    else if (args[1] == "-d") {
        struct stat statbuf;
        if (stat(args[2].c_str(), &statbuf) == 0)
            return S_ISDIR(statbuf.st_mode);
        return false;
    }
    
    return false;
}

std::string ScriptInterpreter::process_command_substitution(const std::string& input) {
    std::string result = input;
    size_t start = input.find('`');
    
    if (start != std::string::npos) {
        size_t end = input.find('`', start + 1);
        if (end != std::string::npos) {
            std::string command = input.substr(start + 1, end - start - 1);
            
            // Create pipes for capturing command output
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                return input;
            }
            
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                close(pipefd[0]);
                close(pipefd[1]);
                return input;
            }
            
            if (pid == 0) { // Child process
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                
                // Parse and execute the command
                std::istringstream iss(command);
                std::vector<std::string> cmd_args;
                std::string arg;
                while (iss >> arg) {
                    cmd_args.push_back(arg);
                }
                
                if (!cmd_args.empty()) {
                    std::vector<char*> c_args;
                    for (auto& a : cmd_args) {
                        c_args.push_back(const_cast<char*>(a.data()));
                    }
                    c_args.push_back(nullptr);
                    
                    execvp(cmd_args[0].c_str(), c_args.data());
                    perror("execvp");
                }
                
                _exit(EXIT_FAILURE);
            }
            
            // Parent process
            close(pipefd[1]);
            
            // Read from the pipe
            char buffer[4096];
            std::string output;
            ssize_t bytes_read;
            
            while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[bytes_read] = '\0';
                output += buffer;
            }
            
            close(pipefd[0]);
            
            // Remove trailing newlines
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }
            
            // Wait for child to finish
            int status;
            waitpid(pid, &status, 0);
            
            // Replace the command substitution with its output
            result = input.substr(0, start) + output + input.substr(end + 1);
        }
    }
    
    return result;
}

void ScriptInterpreter::execute_if_statement(const std::vector<std::string>& args) {
    // We expect: if command; then command; [else command;] fi
    if (args.size() < 4) {
        std::cerr << "cjsh: Invalid if statement" << std::endl;
        return;
    }
    
    // Find positions of "then" and "fi"
    size_t then_pos = 0, fi_pos = 0, else_pos = 0;
    for (size_t i = 1; i < args.size(); i++) {
        if (args[i] == "then") then_pos = i;
        else if (args[i] == "fi") fi_pos = i;
        else if (args[i] == "else") else_pos = i;
    }
    
    if (then_pos == 0 || fi_pos == 0 || then_pos >= fi_pos) {
        std::cerr << "cjsh: Invalid if statement syntax" << std::endl;
        return;
    }
    
    // Extract condition (everything between "if" and "then")
    std::vector<std::string> condition_cmd(args.begin() + 1, args.begin() + then_pos);
    
    if (condition_cmd[0] == "[") {
        // Handle test directly
        bool condition_result = evaluate_condition(condition_cmd);
        
        if (condition_result) {
            // Execute "then" part
            std::vector<std::string> then_cmds;
            size_t end_then = else_pos ? else_pos : fi_pos;
            
            for (size_t i = then_pos + 1; i < end_then; i++) {
                if (args[i] == ";") {
                    if (!then_cmds.empty()) {
                        executor(then_cmds);
                        then_cmds.clear();
                    }
                } else {
                    then_cmds.push_back(args[i]);
                }
            }
            
            if (!then_cmds.empty()) {
                executor(then_cmds);
            }
        } else if (else_pos > 0) {
            // Execute "else" part
            std::vector<std::string> else_cmds;
            
            for (size_t i = else_pos + 1; i < fi_pos; i++) {
                if (args[i] == ";") {
                    if (!else_cmds.empty()) {
                        executor(else_cmds);
                        else_cmds.clear();
                    }
                } else {
                    else_cmds.push_back(args[i]);
                }
            }
            
            if (!else_cmds.empty()) {
                executor(else_cmds);
            }
        }
    } else {
        // Run condition as a command and check its exit status
        executor(condition_cmd);
        // Implementation would continue depending on the command's exit status
    }
}

bool ScriptInterpreter::execute_script_file(const std::string& script_path) {
    std::ifstream script_file(script_path);
    if (!script_file.is_open()) {
        std::cerr << "cjsh: Cannot open script file: " << script_path << std::endl;
        return false;
    }
    
    std::string line;
    std::vector<std::string> current_command;
    bool in_if_block = false;
    std::vector<std::string> if_statement;
    
    while (std::getline(script_file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Process command substitution
        if (line.find('`') != std::string::npos) {
            line = process_command_substitution(line);
        }
        
        std::istringstream iss(line);
        std::string token;
        
        // If we're inside an if block, collect all tokens until 'fi'
        if (in_if_block) {
            while (iss >> token) {
                if_statement.push_back(token);
                if (token == "fi") {
                    in_if_block = false;
                    execute_if_statement(if_statement);
                    if_statement.clear();
                    break;
                }
            }
            continue;
        }
        
        // Check for if statement start
        if (line.find("if ") == 0) {
            in_if_block = true;
            if_statement.push_back("if");
            // Continue reading tokens for this line
            while (iss >> token) {
                if_statement.push_back(token);
                if (token == "fi") {
                    in_if_block = false;
                    execute_if_statement(if_statement);
                    if_statement.clear();
                    break;
                }
            }
            continue;
        }
        
        // Handle regular commands
        current_command.clear();
        while (iss >> token) {
            current_command.push_back(token);
        }
        
        if (!current_command.empty()) {
            // Handle test commands
            if (current_command[0] == "[" || current_command[0] == "test") {
                evaluate_condition(current_command); // Removed unused 'result' variable
                // Script continues regardless of result
                continue;
            }
            
            // Execute the command
            executor(current_command);
        }
    }
    
    // If we're still in an if block at EOF, that's an error
    if (in_if_block) {
        std::cerr << "cjsh: Syntax error: unexpected end of file (expecting 'fi')" << std::endl;
        return false;
    }
    
    return true;
}
