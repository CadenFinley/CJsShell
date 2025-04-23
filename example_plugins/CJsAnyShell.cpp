#include "../src/include/plugininterface.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <cstdio>
#include <vector>
#include <fstream>
#include <filesystem>

// g++ -std=c++17 -shared -dynamiclib -fPIC CJsAnyShell.cpp -o CJsAnyShell.dylib

class ShellExecutorPlugin : public PluginInterface {
private:
    std::map<std::string, std::string> settings;
    bool isInitialized;
    std::string capturedCommand;
    std::string activeShell;
    std::vector<std::string> supportedShells;

    // Helper method to check if a shell is available
    bool isShellAvailable(const std::string& shell) {
        std::string checkCmd = "which " + shell + " > /dev/null 2>&1";
        return (system(checkCmd.c_str()) == 0);
    }

    // Lists all available shells on the system
    std::vector<std::string> getAvailableShells() {
        std::vector<std::string> available;
        for (const auto& shell : supportedShells) {
            if (isShellAvailable(shell)) {
                available.push_back(shell);
            }
        }
        return available;
    }
    
    // Ensure the plugin directory exists
    bool ensureDirectoryExists() {
        try {
            std::string dirPath = getPluginDirectory();
            if (!std::filesystem::exists(dirPath)) {
                return std::filesystem::create_directories(dirPath);
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error creating plugin directory: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Save settings to JSON file
    bool saveSettings() {
        if (!ensureDirectoryExists()) {
            return false;
        }
        
        std::string filePath = getPluginDirectory() + "/settings.json";
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open settings file for writing: " << filePath << std::endl;
            return false;
        }
        
        // Write settings as a simple JSON object
        file << "{\n";
        bool first = true;
        for (const auto& [key, value] : settings) {
            if (!first) file << ",\n";
            file << "  \"" << key << "\": \"" << value << "\"";
            first = false;
        }
        file << "\n}";
        
        file.close();
        return true;
    }
    
    // Load settings from JSON file
    bool loadSettings() {
        std::string filePath = getPluginDirectory() + "/settings.json";
        std::ifstream file(filePath);
        if (!file.is_open()) {
            // It's okay if the file doesn't exist yet
            return false;
        }
        
        std::string line, json;
        while (std::getline(file, line)) {
            json += line;
        }
        file.close();
        
        // Simple JSON parsing - this is not a full JSON parser but works for our simple settings
        size_t pos = 0;
        while ((pos = json.find("\"", pos)) != std::string::npos) {
            size_t keyStart = pos + 1;
            size_t keyEnd = json.find("\"", keyStart);
            if (keyEnd == std::string::npos) break;
            
            std::string key = json.substr(keyStart, keyEnd - keyStart);
            
            pos = json.find("\"", keyEnd + 1);
            if (pos == std::string::npos) break;
            size_t valueStart = pos + 1;
            size_t valueEnd = json.find("\"", valueStart);
            if (valueEnd == std::string::npos) break;
            
            std::string value = json.substr(valueStart, valueEnd - valueStart);
            settings[key] = value;
            
            pos = valueEnd + 1;
        }
        
        return true;
    }

public:
    ShellExecutorPlugin() : isInitialized(false) {
        // List of shells to try in order of preference
        supportedShells = {
            "bash", "zsh", "fish", "ksh", "tcsh", 
            "csh", "dash", "sh", "pwsh", "powershell"
        };
        
        // Initialize with default settings
        settings = getDefaultSettings();
        
        // Try to load saved settings
        loadSettings();
    }

    std::string getName() const {
        return "CJsAnyShell";
    }

    std::string getVersion() const {
        return "1.1.0.0";
    }

    std::string getDescription() const {
        return "A plugin to execute commands through various shells (bash, zsh, fish, ksh, tcsh, csh, dash, sh, powershell).";
    }

    std::string getAuthor() const {
        return "Caden Finley";
    }

    bool initialize() {
        isInitialized = true;
        
        // Check what shell to use based on settings
        std::string shellType = "auto";
        if (settings.find("shell_type") != settings.end()) {
            shellType = settings["shell_type"];
        }
        
        if (shellType != "auto") {
            // User specified a specific shell
            if (isShellAvailable(shellType)) {
                activeShell = shellType;
            } else {
                std::cerr << shellType << " shell is not available on this system" << std::endl;
                isInitialized = false;
                return false;
            }
        } else {
            // Auto mode - try shells in order of preference
            bool shellFound = false;
            for (const auto& shell : supportedShells) {
                if (isShellAvailable(shell)) {
                    activeShell = shell;
                    shellFound = true;
                    break;
                }
            }
            
            if (!shellFound) {
                std::cerr << "No supported shell is available on this system" << std::endl;
                isInitialized = false;
                return false;
            }
        }
        
        // List all available shells if verbose
        if (settings.find("verbose") != settings.end() && settings["verbose"] == "true") {
            auto availableShells = getAvailableShells();
            std::cout << "Available shells: ";
            for (size_t i = 0; i < availableShells.size(); i++) {
                std::cout << availableShells[i];
                if (i < availableShells.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
        }
        
        // Ensure settings are saved
        saveSettings();
        
        return true;
    }

    void shutdown() {
        isInitialized = false;
    }

    bool executeShellCommand(const std::string& command) {
        std::string shellCommand = activeShell + " -c \"" + command + "\"";
        
        if (settings.find("verbose") != settings.end() && settings["verbose"] == "true") {
            std::cout << "Executing via " << activeShell << ": " << command << std::endl;
        }
        
        int result = system(shellCommand.c_str());
        return (result == 0);
    }

    bool handleCommand(std::queue<std::string>& args) {
        if (args.empty()) return false;

        std::string cmd = args.front();
        args.pop();

        if (cmd == "event") {
            if (args.empty()) return false;
            std::string eventType = args.front();
            args.pop();
            std::string eventData = args.empty() ? "" : args.front();
            
            if (eventType == "main_process_command_processed") {
                // Capture the first word of the command as our command name
                std::istringstream iss(eventData);
                std::string firstWord;
                iss >> firstWord;

                if (!firstWord.empty() && firstWord != "cd") {
                    capturedCommand = firstWord;
                    if (settings.find("verbose") != settings.end() && settings["verbose"] == "true") {
                        std::cout << "Shell Plugin captured command: " << capturedCommand << std::endl;
                    }
                }
            }
            
            return true;
        }
        else if (cmd == capturedCommand) {
            // This is our captured command - execute it via the selected shell
            std::string fullCommand = capturedCommand;
            
            // Append any arguments
            while (!args.empty()) {
                fullCommand += " " + args.front();
                args.pop();
            }
            
            return executeShellCommand(fullCommand);
        }
        
        return false;
    }

    std::vector<std::string> getCommands() const {
        std::vector<std::string> commands;
        if (!capturedCommand.empty()) {
            commands.push_back(capturedCommand);
        }
        return commands;
    }

    std::vector<std::string> getSubscribedEvents() const {
        std::vector<std::string> events;
        events.push_back("main_process_pre_run");
        events.push_back("main_process_command_processed");
        return events;
    }

    std::map<std::string, std::string> getDefaultSettings() const {
        std::map<std::string, std::string> defaults;
        defaults["verbose"] = "true";
        defaults["shell_type"] = "auto"; // Can be any supported shell name or "auto"
        return defaults;
    }

    int getInterfaceVersion() const {
        return 2;
    }

    void updateSetting(const std::string& key, const std::string& value) {
        settings[key] = value;
        
        if (key == "shell_type" && isInitialized) {
            if (value == "auto") {
                std::cout << "Shell type set to auto-detect." << std::endl;
                initialize(); // Re-initialize to pick the best shell
            } else if (isShellAvailable(value)) {
                activeShell = value;
                std::cout << "Shell type changed to " << value << "." << std::endl;
            } else {
                std::cout << "Warning: " << value << " shell is not available. Keeping current shell: " << activeShell << std::endl;
            }
        } else {
            std::cout << "Shell Plugin setting updated - " << key << ": " << value << std::endl;
        }
        
        // Save settings after any update
        saveSettings();
    }
};

IMPLEMENT_PLUGIN(ShellExecutorPlugin)
