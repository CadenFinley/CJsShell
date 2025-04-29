#include "shell_script_interpreter.h"
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
    // Initialize the command executor with a default implementation
    commandExecutor = [](const std::string& cmd, bool) -> bool {
        return system(cmd.c_str()) == 0;
    };
    
    // Initialize state variables
    inThenBlock = false;
    
    // Initialize debug variables
    debugLevel = DebugLevel::NONE;
    showCommandOutput = false;
    debugIndentLevel = 0;
}

ShellScriptInterpreter::~ShellScriptInterpreter() {
}

void ShellScriptInterpreter::setDebugLevel(DebugLevel level) {
    debugLevel = level;
}

DebugLevel ShellScriptInterpreter::getDebugLevel() const {
    return debugLevel;
}

void ShellScriptInterpreter::debugPrint(const std::string& message, DebugLevel level) const {
    if (static_cast<int>(debugLevel) >= static_cast<int>(level)) {
        std::cerr << getIndentation() << "[DEBUG] " << message << std::endl;
    }
}

std::string ShellScriptInterpreter::getIndentation() const {
    return std::string(debugIndentLevel * 2, ' ');
}

void ShellScriptInterpreter::dumpVariables() const {
    if (debugLevel == DebugLevel::NONE) {
        return;
    }
    
    std::cerr << getIndentation() << "[DEBUG] Variable dump:" << std::endl;
    
    // Calculate the maximum variable name length for better formatting
    size_t maxLength = 0;
    for (const auto& pair : localVariables) {
        maxLength = std::max(maxLength, pair.first.length());
    }
    
    // Print variables with aligned output
    for (const auto& pair : localVariables) {
        std::cerr << getIndentation() << "  " << std::left 
                  << std::setw(maxLength + 2) << pair.first 
                  << "= \"" << pair.second << "\"" << std::endl;
    }
    
    // Print environment variables that might be relevant to shell scripts
    const char* envVars[] = {
        // Basic system paths
        "PATH", "HOME", "USER", "SHELL", "PWD", "OLDPWD",
        // Shell specific
        "TERM", "LANG", "LC_ALL", "DISPLAY",
        // Shell state
        "?", "PPID", "PS1", "PS2", 
        // System identification
        "HOSTNAME", "OSTYPE", "MACHTYPE", "LOGNAME",
        // Additional paths
        "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "MANPATH",
        // Programming languages
        "JAVA_HOME", "PYTHONPATH", "GOPATH", "NODE_PATH"
    };
    std::cerr << getIndentation() << "[DEBUG] Key environment variables:" << std::endl;
    for (const char* var : envVars) {
        const char* value = getenv(var);
        if (value) {
            std::cerr << getIndentation() << "  " << std::left 
                      << std::setw(maxLength + 2) << var 
                      << "= \"" << value << "\"" << std::endl;
        }
    }
}

std::string ShellScriptInterpreter::escapeDebugString(const std::string& input) const {
    std::string result;
    for (char c : input) {
        if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else if (c < 32 || c > 126) {
            // Convert control or non-ASCII characters to \xHH format
            char buf[5];
            snprintf(buf, sizeof(buf), "\\x%02X", static_cast<unsigned char>(c));
            result += buf;
        } else {
            result += c;
        }
    }
    return result;
}

bool ShellScriptInterpreter::handleDebugCommand(const std::string& command) {
    // Check if it's a debug command
    if (command == "debug on") {
        debugLevel = DebugLevel::BASIC;
        std::cerr << "Debug mode ON (basic)" << std::endl;
        return true;
    } else if (command == "debug off") {
        debugLevel = DebugLevel::NONE;
        std::cerr << "Debug mode OFF" << std::endl;
        return true;
    } else if (command == "debug verbose") {
        debugLevel = DebugLevel::VERBOSE;
        std::cerr << "Debug mode ON (verbose)" << std::endl;
        return true;
    } else if (command == "debug trace") {
        debugLevel = DebugLevel::TRACE;
        std::cerr << "Debug mode ON (trace)" << std::endl;
        return true;
    } else if (command == "debug level") {
        std::cerr << "Current debug level: " << static_cast<int>(debugLevel) << std::endl;
        return true;
    } else if (command == "debug vars") {
        dumpVariables();
        return true;
    } else if (command == "debug show_output on") {
        showCommandOutput = true;
        std::cerr << "Command output display ON" << std::endl;
        return true;
    } else if (command == "debug show_output off") {
        showCommandOutput = false;
        std::cerr << "Command output display OFF" << std::endl;
        return true;
    } else if (command == "debug safe_mode on") {
        debugPrint("Enabling safe mode - path operations will be more carefully validated", DebugLevel::BASIC);
        localVariables["CJSH_SAFE_MODE"] = "1";
        return true;
    } else if (command == "debug safe_mode off") {
        debugPrint("Disabling safe mode", DebugLevel::BASIC);
        localVariables["CJSH_SAFE_MODE"] = "0";
        return true;
    }
    
    return false;
}

// Add this new method to directly handle path_helper to avoid eval issues
bool ShellScriptInterpreter::handlePathHelper() {
    debugPrint("Directly handling path_helper command", DebugLevel::BASIC);
    
    // Execute path_helper directly and capture its output
    FILE* pipe = popen("/usr/libexec/path_helper -s", "r");
    if (!pipe) {
        debugPrint("Failed to execute path_helper", DebugLevel::BASIC);
        return false;
    }
    
    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    debugPrint("Path helper output: " + escapeDebugString(result), DebugLevel::VERBOSE);
    
    // Extract the PATH value using string manipulation
    size_t pathStart = result.find("PATH=\"") + 6;
    size_t pathEnd = result.find("\"", pathStart);
    
    if (pathStart != std::string::npos && pathEnd != std::string::npos) {
        std::string pathValue = result.substr(pathStart, pathEnd - pathStart);
        
        // Validate and set PATH
        if (!pathValue.empty()) {
            debugPrint("Setting PATH to: " + pathValue, DebugLevel::BASIC);
            
            // Set both local and environment variables
            localVariables["PATH"] = pathValue;
            setenv("PATH", pathValue.c_str(), 1);
            return true;
        }
    }
    
    // If we couldn't parse the path_helper output correctly, use default PATH
    debugPrint("ERROR: Could not parse path_helper output, using default path", DebugLevel::BASIC);
    std::string defaultPath = "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin";
    localVariables["PATH"] = defaultPath;
    setenv("PATH", defaultPath.c_str(), 1);
    
    return true;
}

bool ShellScriptInterpreter::executeScript(const std::string& filename) {
    debugPrint("Executing script: " + filename);
    
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
    
    if (debugLevel >= DebugLevel::VERBOSE) {
        debugPrint("Script content (" + std::to_string(lines.size()) + " lines):", DebugLevel::VERBOSE);
        for (size_t i = 0; i < lines.size(); i++) {
            debugPrint(std::to_string(i+1) + ": " + lines[i], DebugLevel::VERBOSE);
        }
    }
    
    return executeBlock(lines);
}

bool ShellScriptInterpreter::executeBlock(const std::vector<std::string>& lines) {
    // Create a non-const copy of the vector to work with the existing functions
    std::vector<std::string> lines_copy = lines;
    
    for (auto it = lines_copy.begin(); it != lines_copy.end(); ) {
        std::string trimmed = trimString(*it);
        
        // Skip empty lines and comments completely
        if (trimmed.empty() || trimmed[0] == '#') {
            debugPrint("Skipping empty line or comment: " + escapeDebugString(*it), DebugLevel::VERBOSE);
            ++it;
            continue;
        }
        
        // Handle if/then/else blocks
        if (trimmed.find("if ") == 0) {
            debugPrint("Found if statement: " + escapeDebugString(trimmed), DebugLevel::VERBOSE);
            
            // Check if it contains 'then' on the same line
            if (trimmed.find("; then") != std::string::npos) {
                debugPrint("Found 'then' on same line", DebugLevel::VERBOSE);
                // If it has 'then' on same line, we'll handle it in parseConditional
            }
            
            if (!parseConditional(it, lines_copy.end())) {
                return false;
            }
            continue;
        }
        
        // Handle for/while loops
        if (trimmed.find("for ") == 0 || trimmed.find("while ") == 0) {
            if (!parseLoop(it, lines_copy.end())) {
                return false;
            }
            continue;
        }
        
        // Handle regular commands
        if (!executeLine(trimmed)) {
            debugPrint("Command failed: " + escapeDebugString(trimmed), DebugLevel::BASIC);
            return false;
        }
        
        ++it;
    }
    
    return true;
}

bool ShellScriptInterpreter::executeLine(const std::string& line) {
    std::string trimmed = trimString(line);
    
    // Additional safety check - don't execute empty lines
    if (trimmed.empty()) {
        debugPrint("Skipping empty line", DebugLevel::VERBOSE);
        return true;
    }
    
    try {
        debugPrint("Executing line: " + escapeDebugString(trimmed), DebugLevel::BASIC);
        
        // Skip comments
        if (trimmed[0] == '#') {
            debugPrint("Skipping comment", DebugLevel::VERBOSE);
            return true;
        }
        
        // Check for debug commands first
        if (trimmed.find("debug ") == 0 || trimmed == "debug") {
            return handleDebugCommand(trimmed);
        }
        
        // Special case for path_helper - this is causing the crashes
        if (trimmed.find("eval") == 0 && trimmed.find("path_helper") != std::string::npos) {
            debugPrint("Detected path_helper eval command, using special handling", DebugLevel::BASIC);
            return handlePathHelper();
        }
        
        // Handle variable assignment
        size_t equalsPos = trimmed.find('=');
        if (equalsPos != std::string::npos && 
            (trimmed.find(' ') > equalsPos || trimmed.find(' ') == std::string::npos)) {
            std::string varName = trimString(trimmed.substr(0, equalsPos));
            std::string varValue = trimString(trimmed.substr(equalsPos + 1));
            
            debugPrint("Variable assignment: " + varName + "=" + varValue, DebugLevel::VERBOSE);
            
            // Handle quoted values
            if ((varValue.front() == '"' && varValue.back() == '"') ||
                (varValue.front() == '\'' && varValue.back() == '\'')) {
                varValue = varValue.substr(1, varValue.size() - 2);
                debugPrint("Removed quotes: " + varValue, DebugLevel::TRACE);
            }
            
            // Handle export command
            bool isExport = false;
            if (varName.find("export ") == 0) {
                isExport = true;
                varName = trimString(varName.substr(7));
                debugPrint("Export variable: " + varName, DebugLevel::VERBOSE);
            }
            
            // Expand variables in the value
            std::string origValue = varValue;
            varValue = expandVariables(varValue);
            
            if (origValue != varValue && debugLevel >= DebugLevel::VERBOSE) {
                debugPrint("Expanded value: " + origValue + " -> " + varValue, DebugLevel::VERBOSE);
            }
            
            // Special handling for PATH (sanitize value, don't allow empty PATH)
            if (varName == "PATH" && varValue.empty()) {
                debugPrint("WARNING: Attempt to set PATH to empty string, using default value instead", DebugLevel::BASIC);
                varValue = "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin";
            }
            
            // Set local variable
            localVariables[varName] = varValue;
            
            // Set environment variable if export
            if (isExport) {
                try {
                    setenv(varName.c_str(), varValue.c_str(), 1);
                    debugPrint("Set environment variable: " + varName + "=" + varValue, DebugLevel::VERBOSE);
                } catch (const std::exception& e) {
                    debugPrint("ERROR: Failed to set environment variable: " + varName + " - " + e.what(), DebugLevel::BASIC);
                    return false;
                }
            }
            
            return true;
        }
        
        // Handle if statements with 'then' on the same line
        if (trimmed.find("if ") == 0) {
            // Check if 'then' is on the same line
            size_t thenPos = trimmed.find("; then");
            if (thenPos != std::string::npos) {
                // Extract the condition part
                std::string condition = trimString(trimmed.substr(3, thenPos - 3));
                debugPrint("Evaluating condition: " + condition, DebugLevel::VERBOSE);
                bool result = evaluateCondition(condition);
                debugPrint("Condition result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
                
                // Store the result for use by subsequent 'then' block
                localVariables["?"] = result ? "0" : "1";
                return true;
            }
            
            // This is handled at a higher level in executeBlock
            return true;
        }
        
        // Handle single 'then' statement
        if (trimmed == "then") {
            debugPrint("Then statement", DebugLevel::VERBOSE);
            return true;
        }
        
        // Handle command substitution with eval
        if (trimmed.find("eval ") == 0) {
            std::string cmd = trimString(trimmed.substr(4));
            debugPrint("Eval command: " + escapeDebugString(cmd), DebugLevel::VERBOSE);
            
            // Handle backtick command substitution before expanding variables
            if (cmd.front() == '`' && cmd.back() == '`') {
                cmd = cmd.substr(1, cmd.size() - 2);
                debugPrint("Backtick command: " + escapeDebugString(cmd), DebugLevel::VERBOSE);
                
                // If this is a path_helper command, use the dedicated handler
                if (cmd.find("path_helper") != std::string::npos) {
                    debugPrint("Using special path_helper handler", DebugLevel::BASIC);
                    return handlePathHelper();
                }
                
                try {
                    // First expand variables in the command
                    std::string expandedCmd = expandVariables(cmd);
                    if (expandedCmd != cmd && debugLevel >= DebugLevel::VERBOSE) {
                        debugPrint("Expanded command: " + escapeDebugString(expandedCmd), DebugLevel::VERBOSE);
                    }
                    
                    // Execute the command and get its output
                    debugPrint("Executing backtick command", DebugLevel::VERBOSE);
                    std::string result = executeCommandSubstitution(expandedCmd);
                    
                    // Trim the result to remove any trailing newlines
                    result = trimString(result);
                    debugPrint("Command result: " + escapeDebugString(result), DebugLevel::VERBOSE);
                    
                    // Execute the command output directly
                    return commandExecutor(result, true);
                    
                } catch (const std::exception& e) {
                    debugPrint("ERROR in eval command: " + std::string(e.what()), DebugLevel::BASIC);
                    return false;
                }
            }
            
            // For non-backtick eval, expand variables and execute
            try {
                std::string expandedCmd = expandVariables(cmd);
                if (expandedCmd != cmd && debugLevel >= DebugLevel::VERBOSE) {
                    debugPrint("Expanded eval command: " + expandedCmd, DebugLevel::VERBOSE);
                }
                
                debugPrint("Executing eval command", DebugLevel::BASIC);
                return commandExecutor(expandedCmd, true);
            } catch (const std::exception& e) {
                debugPrint("ERROR in eval command: " + std::string(e.what()), DebugLevel::BASIC);
                return false;
            }
        }
        
        // Expand variables in the command
        std::string expandedCmd = expandVariables(trimmed);
        if (expandedCmd != trimmed && debugLevel >= DebugLevel::VERBOSE) {
            debugPrint("Expanded command: " + expandedCmd, DebugLevel::VERBOSE);
        }
        
        // Execute the command
        debugPrint("Executing command", DebugLevel::BASIC);
        return commandExecutor(expandedCmd, true);
    } catch (const std::exception& e) {
        debugPrint("ERROR: Exception in executeLine: " + std::string(e.what()), DebugLevel::BASIC);
        return false;
    }
}

bool ShellScriptInterpreter::evaluateCondition(const std::string& condition) {
    debugPrint("Evaluating condition: " + escapeDebugString(condition), DebugLevel::VERBOSE);
    
    // Handle test command format: [ condition ]
    if (condition.front() == '[' && condition.back() == ']') {
        std::string testCondition = trimString(condition.substr(1, condition.size() - 2));
        debugPrint("Test condition: " + escapeDebugString(testCondition), DebugLevel::VERBOSE);
        
        // Handle -x file test (file exists and is executable)
        if (testCondition.find("-x ") == 0) {
            std::string filePath = expandVariables(trimString(testCondition.substr(3)));
            debugPrint("Checking if file is executable: " + filePath, DebugLevel::VERBOSE);
            bool result = access(filePath.c_str(), X_OK) == 0;
            debugPrint("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        // Handle file existence test: -e file
        if (testCondition.find("-e ") == 0) {
            std::string filePath = expandVariables(trimString(testCondition.substr(3)));
            debugPrint("Checking if file exists: " + filePath, DebugLevel::VERBOSE);
            bool result = access(filePath.c_str(), F_OK) == 0;
            debugPrint("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        // Handle file readable test: -r file
        if (testCondition.find("-r ") == 0) {
            std::string filePath = expandVariables(trimString(testCondition.substr(3)));
            debugPrint("Checking if file is readable: " + filePath, DebugLevel::VERBOSE);
            bool result = access(filePath.c_str(), R_OK) == 0;
            debugPrint("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        // Handle string equality: str1 = str2
        size_t eqPos = testCondition.find(" = ");
        if (eqPos != std::string::npos) {
            std::string lhs = expandVariables(trimString(testCondition.substr(0, eqPos)));
            std::string rhs = expandVariables(trimString(testCondition.substr(eqPos + 3)));
            debugPrint("Comparing strings: '" + lhs + "' = '" + rhs + "'", DebugLevel::VERBOSE);
            bool result = lhs == rhs;
            debugPrint("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        // Handle string inequality: str1 != str2
        size_t neqPos = testCondition.find(" != ");
        if (neqPos != std::string::npos) {
            std::string lhs = expandVariables(trimString(testCondition.substr(0, neqPos)));
            std::string rhs = expandVariables(trimString(testCondition.substr(neqPos + 4)));
            debugPrint("Comparing strings: '" + lhs + "' != '" + rhs + "'", DebugLevel::VERBOSE);
            bool result = lhs != rhs;
            debugPrint("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        // Handle non-empty string test: -n str
        if (testCondition.find("-n ") == 0) {
            std::string value = expandVariables(trimString(testCondition.substr(3)));
            debugPrint("Checking if string is non-empty: '" + value + "'", DebugLevel::VERBOSE);
            bool result = !value.empty();
            debugPrint("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        // Handle empty string test: -z str
        if (testCondition.find("-z ") == 0) {
            std::string value = expandVariables(trimString(testCondition.substr(3)));
            debugPrint("Checking if string is empty: '" + value + "'", DebugLevel::VERBOSE);
            bool result = value.empty();
            debugPrint("Result: " + std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
            return result;
        }
        
        // If we get here, it's an unrecognized test condition
        debugPrint("Unrecognized test condition: " + testCondition, DebugLevel::BASIC);
        return false;
    }
    
    // For any other condition, execute it and check the return code
    std::string expandedCondition = expandVariables(condition);
    debugPrint("Executing condition command: " + expandedCondition, DebugLevel::VERBOSE);
    
    FILE* pipe = popen(expandedCondition.c_str(), "r");
    if (!pipe) {
        debugPrint("Failed to execute condition command", DebugLevel::BASIC);
        return false;
    }
    
    int status = pclose(pipe);
    bool result = status == 0;
    debugPrint("Command returned: " + std::to_string(status) + ", result: " + 
               std::string(result ? "true" : "false"), DebugLevel::VERBOSE);
    return result;
}

bool ShellScriptInterpreter::parseConditional(std::vector<std::string>::iterator& it, 
                                             const std::vector<std::string>::const_iterator& end) {
    std::string line = *it;
    std::string condition;
    bool conditionMet = false;
    
    debugPrint("Parsing conditional: " + escapeDebugString(line), DebugLevel::VERBOSE);
    
    // Handle 'if condition; then' on the same line
    size_t thenPos = line.find("; then");
    if (thenPos != std::string::npos) {
        // Extract just the condition part, skipping the "if " prefix
        condition = trimString(line.substr(3, thenPos - 3));
        debugPrint("Extracted condition from same line: " + escapeDebugString(condition), DebugLevel::VERBOSE);
        conditionMet = evaluateCondition(condition);
        inThenBlock = true;
    } else {
        // For separate line syntax, just extract the condition, skipping the "if " prefix
        condition = trimString(line.substr(3));
        debugPrint("Extracted condition from separate line: " + escapeDebugString(condition), DebugLevel::VERBOSE);
        conditionMet = evaluateCondition(condition);
        inThenBlock = false;
    }
    
    debugPrint("Condition result: " + std::string(conditionMet ? "true" : "false"), DebugLevel::VERBOSE);
    
    // Store result for use by the shell
    localVariables["?"] = conditionMet ? "0" : "1";
    
    bool executingBlock = conditionMet;
    bool foundElse = false;
    
    std::vector<std::string> currentBlock;
    
    ++it; // Move past the if line
    
    while (it != end) {
        std::string currentLine = trimString(*it);
        
        debugPrint("Conditional processing line: " + escapeDebugString(currentLine), DebugLevel::TRACE);
        
        if (currentLine == "then") {
            debugPrint("Found 'then' statement", DebugLevel::VERBOSE);
            inThenBlock = true;
            ++it;
            continue;
        } else if (currentLine == "else") {
            // Execute the then block if condition was met
            if (conditionMet && !currentBlock.empty()) {
                debugPrint("Executing 'then' block", DebugLevel::VERBOSE);
                executeBlock(currentBlock);
            }
            
            currentBlock.clear();
            executingBlock = !conditionMet;
            foundElse = true;
            ++it;
            continue;
        } else if (currentLine == "fi") {
            // Execute the current block if it should be executed
            if (executingBlock && !currentBlock.empty()) {
                debugPrint("Executing final block", DebugLevel::VERBOSE);
                executeBlock(currentBlock);
            }
            
            // Move past fi
            ++it;
            return true;
        } else if (currentLine.find("elif ") == 0) {
            // Execute the then block if condition was met
            if (conditionMet && !currentBlock.empty()) {
                executeBlock(currentBlock);
            }
            
            // Only evaluate the elif condition if previous conditions failed
            if (!conditionMet && !foundElse) {
                condition = trimString(currentLine.substr(5));
                conditionMet = evaluateCondition(condition);
                executingBlock = conditionMet;
            } else {
                executingBlock = false;
            }
            
            currentBlock.clear();
            ++it;
            continue;
        }
        
        if (inThenBlock && executingBlock) {
            debugPrint("Adding line to conditional block: " + escapeDebugString(currentLine), DebugLevel::TRACE);
            currentBlock.push_back(currentLine);
        } else {
            debugPrint("Skipping line in inactive block: " + escapeDebugString(currentLine), DebugLevel::TRACE);
        }
        
        ++it;
    }
    
    std::cerr << "Error: Unexpected end of if block (missing fi)" << std::endl;
    return false;
}

bool ShellScriptInterpreter::parseLoop(std::vector<std::string>::iterator& it, 
                                      const std::vector<std::string>::const_iterator& end) {
    std::string loopLine = *it;
    bool isForLoop = loopLine.find("for ") == 0;
    
    std::vector<std::string> loopBody;
    
    // Find the loop body between do and done
    bool inDoBlock = false;
    ++it; // Move past for/while line
    
    while (it != end) {
        std::string line = trimString(*it);
        
        if (line == "do") {
            inDoBlock = true;
            ++it;
            continue;
        } else if (line == "done") {
            // Found the end of the loop
            break;
        } else if (inDoBlock) {
            loopBody.push_back(line);
        }
        
        ++it;
    }
    
    if (it == end) {
        std::cerr << "Error: Unexpected end of loop (missing done)" << std::endl;
        return false;
    }
    
    // Process the loop based on its type
    if (isForLoop) {
        // Parse "for var in values" format
        std::string forDecl = trimString(loopLine.substr(4));
        size_t inPos = forDecl.find(" in ");
        
        if (inPos != std::string::npos) {
            std::string varName = trimString(forDecl.substr(0, inPos));
            std::string valueList = trimString(forDecl.substr(inPos + 4));
            
            // Split the values
            std::vector<std::string> values = splitCommand(valueList);
            
            // Execute the loop body for each value
            for (const auto& value : values) {
                // Set the loop variable
                localVariables[varName] = value;
                
                // Execute the loop body
                executeBlock(loopBody);
            }
        }
    } else {
        // While loop
        std::string whileCondition = trimString(loopLine.substr(6));
        
        while (evaluateCondition(whileCondition)) {
            executeBlock(loopBody);
        }
    }
    
    // Move past done
    ++it;
    return true;
}

std::string ShellScriptInterpreter::expandVariables(const std::string& str) {
    // Early return for empty strings
    if (str.empty()) {
        debugPrint("Empty string for variable expansion", DebugLevel::TRACE);
        return str;
    }
    
    std::string result = str;
    size_t pos = 0;
    
    debugPrint("Expanding variables in: " + escapeDebugString(str), DebugLevel::TRACE);
    
    // Replace ${VAR} style variables
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t endPos = result.find("}", pos + 2);
        if (endPos == std::string::npos) break;
        
        std::string varName = result.substr(pos + 2, endPos - pos - 2);
        std::string varValue;
        
        // Check local variables first
        auto it = localVariables.find(varName);
        if (it != localVariables.end()) {
            varValue = it->second;
        } else {
            // Then check environment variables
            const char* envValue = getenv(varName.c_str());
            if (envValue) {
                varValue = envValue;
            }
        }
        
        result.replace(pos, endPos - pos + 1, varValue);
        pos += varValue.length();
    }
    
    // Replace $VAR style variables
    pos = 0;
    while ((pos = result.find('$', pos)) != std::string::npos) {
        // Skip if it's ${
        if (pos + 1 < result.size() && result[pos + 1] == '{') {
            pos += 2;
            continue;
        }
        
        // Find the end of the variable name
        size_t endPos = pos + 1;
        while (endPos < result.size() && 
               (isalnum(result[endPos]) || result[endPos] == '_')) {
            endPos++;
        }
        
        if (endPos > pos + 1) {
            std::string varName = result.substr(pos + 1, endPos - pos - 1);
            std::string varValue;
            
            // Check local variables first
            auto it = localVariables.find(varName);
            if (it != localVariables.end()) {
                varValue = it->second;
            } else {
                // Then check environment variables
                const char* envValue = getenv(varName.c_str());
                if (envValue) {
                    varValue = envValue;
                }
            }
            
            result.replace(pos, endPos - pos, varValue);
            pos += varValue.length();
        } else {
            pos++;
        }
    }
    
    // Handle command substitution $(command)
    pos = 0;
    while ((pos = result.find("$(", pos)) != std::string::npos) {
        // Find the matching closing parenthesis
        size_t depth = 1;
        size_t endPos = pos + 2;
        
        while (endPos < result.size() && depth > 0) {
            if (result[endPos] == '(') depth++;
            else if (result[endPos] == ')') depth--;
            endPos++;
        }
        
        if (depth == 0) {
            std::string cmd = result.substr(pos + 2, endPos - pos - 3);
            std::string cmdOutput = executeCommandSubstitution(cmd);
            
            // Process output to handle newlines properly
            std::stringstream ss(cmdOutput);
            std::string processedOutput;
            std::string outputLine;
            
            while (std::getline(ss, outputLine)) {
                std::string trimmedLine = trimString(outputLine);
                if (!trimmedLine.empty()) {
                    if (!processedOutput.empty()) {
                        processedOutput += " "; // Replace newlines with spaces
                    }
                    processedOutput += trimmedLine;
                }
            }
            
            debugPrint("Processed backtick output: " + escapeDebugString(processedOutput), DebugLevel::VERBOSE);
            result.replace(pos, endPos - pos + 1, processedOutput);
            pos += processedOutput.length();
        } else {
            pos += 2;
        }
    }
    
    // Handle backtick command substitution `command`
    pos = 0;
    while ((pos = result.find('`', pos)) != std::string::npos) {
        size_t endPos = result.find('`', pos + 1);
        if (endPos == std::string::npos) {
            debugPrint("Warning: Unmatched backtick at position " + std::to_string(pos), DebugLevel::BASIC);
            break;
        }
        
        std::string cmd = result.substr(pos + 1, endPos - pos - 1);
        debugPrint("Backtick command: " + escapeDebugString(cmd), DebugLevel::VERBOSE);
        
        // First expand any variables in the command itself
        std::string expandedCmd = expandVariables(cmd);
        if (expandedCmd != cmd) {
            debugPrint("Expanded backtick command: " + escapeDebugString(expandedCmd), DebugLevel::VERBOSE);
        }
        
        std::string cmdOutput = executeCommandSubstitution(expandedCmd);
        
        // Process output to handle newlines properly
        std::stringstream ss(cmdOutput);
        std::string processedOutput;
        std::string outputLine;
        
        while (std::getline(ss, outputLine)) {
            std::string trimmedLine = trimString(outputLine);
            if (!trimmedLine.empty()) {
                if (!processedOutput.empty()) {
                    processedOutput += " "; // Replace newlines with spaces
                }
                processedOutput += trimmedLine;
            }
        }
        
        debugPrint("Processed backtick output: " + escapeDebugString(processedOutput), DebugLevel::VERBOSE);
        result.replace(pos, endPos - pos + 1, processedOutput);
        pos += processedOutput.length();
    }
    
    // Special case: if we're just processing a single character, verify it's not a newline or control character
    if (result.length() == 1) {
        char c = result[0];
        if (c == '\n' || c == '\r' || (c < 32 && c != '\t')) {
            debugPrint("Filtered control character in expansion: " + escapeDebugString(result), DebugLevel::VERBOSE);
            return "";
        }
    }
    
    debugPrint("Expanded result: " + escapeDebugString(result), DebugLevel::TRACE);
    return result;
}

void ShellScriptInterpreter::setCommandExecutor(std::function<bool(const std::string&, bool)> executor) {
    commandExecutor = executor;
}

std::string ShellScriptInterpreter::executeCommandSubstitution(const std::string& cmd) {
    // Skip empty commands
    if (trimString(cmd).empty()) {
        debugPrint("Skipping empty command substitution", DebugLevel::VERBOSE);
        return "";
    }
    
    debugPrint("Command substitution: " + escapeDebugString(cmd), DebugLevel::VERBOSE);
    
    // Additional safety for PATH manipulation commands
    if (cmd.find("/usr/libexec/path_helper") != std::string::npos) {
        debugPrint("Executing path_helper command with extra caution", DebugLevel::VERBOSE);
    }
    
    // Create a pipe to execute the command
    FILE* pipe = nullptr;
    try {
        pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::string errorMsg = "Error executing command: " + escapeDebugString(cmd);
            debugPrint(errorMsg, DebugLevel::BASIC);
            std::cerr << errorMsg << std::endl;
            return "";
        }
        
        std::string result;
        char buffer[128];
        
        // Read the command output
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        
        // Check the command status
        int status = pclose(pipe);
        pipe = nullptr; // Set to null after pclose
        
        if (status != 0) {
            debugPrint("Command returned non-zero status: " + std::to_string(status), DebugLevel::VERBOSE);
        }
        
        // Special handling for common commands that might appear to fail
        if (result.empty() && cmd.find("path_helper") != std::string::npos) {
            debugPrint("WARNING: path_helper command returned empty result", DebugLevel::BASIC);
            return "PATH=\"/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin\"; export PATH;";
        }
        
        // Clean up the result by removing trailing newlines
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        
        debugPrint("Command substitution result: " + escapeDebugString(result), DebugLevel::VERBOSE);
        return result;
    }
    catch (const std::exception& e) {
        debugPrint("ERROR in command substitution: " + std::string(e.what()), DebugLevel::BASIC);
        if (pipe != nullptr) {
            pclose(pipe);
        }
        return "";
    }
}

std::string ShellScriptInterpreter::trimString(const std::string& str) {
    // Check for empty strings first
    if (str.empty()) {
        return "";
    }
    
    // Check if string is only whitespace
    bool onlyWhitespace = true;
    for (char c : str) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            onlyWhitespace = false;
            break;
        }
    }
    
    if (onlyWhitespace) {
        return "";
    }
    
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos) {
        return "";
    }
    
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> ShellScriptInterpreter::splitCommand(const std::string& cmd) {
    std::vector<std::string> result;
    std::stringstream ss(cmd);
    std::string item;
    
    bool inQuotes = false;
    char quoteChar = '\0';
    std::string current;
    
    for (char c : cmd) {
        if (c == '"' || c == '\'') {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            } else if (c == quoteChar) {
                inQuotes = false;
                quoteChar = '\0';
            } else {
                current += c;
            }
        } else if (c == ' ' && !inQuotes) {
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
