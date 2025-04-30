#include "../include/pluginapi.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <cstdio>
#include <vector>
#include <fstream>
#include <filesystem>
#include <map>
#include <string>
#include <cstring>

// Local implementations of shell helper functions that replace the missing API functions
static std::string g_pluginDirectory = "";

#ifdef __APPLE__
    #include <mach-o/dyld.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <linux/limits.h>
#endif

// Get the directory where the plugin is located
static std::string getPluginHomeDirectory() {
    char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.cjsh/plugins";
    }
    return "./plugins"; // Fallback
}

// Get the root .cjsh directory
static std::string getCJshRootDirectory() {
    char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.cjsh";
    }
    return "./.cjsh"; // Fallback
}

// Local implementation to replace plugin_get_plugin_directory
static char* local_plugin_get_plugin_directory(const char* plugin_name) {
    std::string dirPath = getPluginHomeDirectory() + "/" + plugin_name;
    return strdup(dirPath.c_str());
}

// Local implementation to replace plugin_free_string
static void local_plugin_free_string(char* str) {
    free(str);
}

// Global state variables
static std::map<std::string, std::string> g_settings;
static bool g_isInitialized = false;
static std::string g_capturedCommand;
static std::string g_activeShell;
static std::vector<std::string> g_supportedShells = {
    "bash", "zsh", "fish", "ksh", "tcsh", 
    "csh", "dash", "sh", "pwsh", "powershell"
};

// Memory tracking for safe shutdown
static plugin_info_t* g_pluginInfo = nullptr;
static char** g_eventsList = nullptr;
static plugin_setting_t* g_defaultSettings = nullptr;
static bool g_memoryAllocated = false;

// Helper method to check if a shell is available
static bool isShellAvailable(const std::string& shell) {
    std::string checkCmd = "which " + shell + " > /dev/null 2>&1";
    return (system(checkCmd.c_str()) == 0);
}

// Lists all available shells on the system
static std::vector<std::string> getAvailableShells() {
    std::vector<std::string> available;
    for (const auto& shell : g_supportedShells) {
        if (isShellAvailable(shell)) {
            available.push_back(shell);
        }
    }
    return available;
}

// Ensure the root .cjsh directory and plugin directory exists
static bool ensureDirectoryExists() {
    try {
        std::string rootDir = getCJshRootDirectory();
        std::string pluginDir = rootDir + "/AnyShell";
        
        if (!std::filesystem::exists(rootDir)) {
            if (!std::filesystem::create_directories(rootDir))
                return false;
        }
        
        if (!std::filesystem::exists(pluginDir)) {
            return std::filesystem::create_directories(pluginDir);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directories: " << e.what() << std::endl;
        return false;
    }
}

// Save settings to JSON file
static bool saveSettings() {
    if (!ensureDirectoryExists()) {
        return false;
    }
    
    std::string filePath = getCJshRootDirectory() + "/AnyShell/settings.json";
    
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open settings file for writing: " << filePath << std::endl;
        return false;
    }
    
    // Write settings as a simple JSON object
    file << "{\n";
    bool first = true;
    for (const auto& [key, value] : g_settings) {
        if (!first) file << ",\n";
        file << "  \"" << key << "\": \"" << value << "\"";
        first = false;
    }
    file << "\n}";
    
    file.close();
    return true;
}

// Load settings from JSON file
static bool loadSettings() {
    std::string filePath = getCJshRootDirectory() + "/AnyShell/settings.json";
    
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
        g_settings[key] = value;
        
        pos = valueEnd + 1;
    }
    
    return true;
}

// Execute a shell command
static bool executeShellCommand(const std::string& command) {
    std::string shellCommand = g_activeShell + " -c \"" + command + "\"";
    
    if (g_settings.find("verbose") != g_settings.end() && g_settings["verbose"] == "true") {
        std::cout << "Executing via " << g_activeShell << ": " << command << std::endl;
    }
    
    int result = system(shellCommand.c_str());
    return (result == 0);
}

// Exported plugin functions
extern "C" {
    PLUGIN_API plugin_info_t* plugin_get_info() {
        if (g_pluginInfo == nullptr) {
            // Allocate info structure
            g_pluginInfo = (plugin_info_t*)malloc(sizeof(plugin_info_t));
            if (g_pluginInfo) {
                // Allocate strings that will persist for the lifetime of the plugin
                g_pluginInfo->name = strdup("AnyShell");
                g_pluginInfo->version = strdup("1.1.0.0");
                g_pluginInfo->description = strdup("A plugin to execute commands through various shells (bash, zsh, fish, ksh, tcsh, csh, dash, sh, powershell).");
                g_pluginInfo->author = strdup("Caden Finley");
                g_pluginInfo->interface_version = PLUGIN_INTERFACE_VERSION;
                g_memoryAllocated = true;
            }
        }
        
        return g_pluginInfo;
    }

    PLUGIN_API int plugin_initialize() {
        // Initialize with default settings if not already loaded
        if (g_settings.empty()) {
            g_settings["verbose"] = "true";
            g_settings["shell_type"] = "auto";
            
            // Try to load saved settings
            loadSettings();
        }
        
        // Check what shell to use based on settings
        std::string shellType = "auto";
        if (g_settings.find("shell_type") != g_settings.end()) {
            shellType = g_settings["shell_type"];
        }
        
        if (shellType != "auto") {
            // User specified a specific shell
            if (isShellAvailable(shellType)) {
                g_activeShell = shellType;
            } else {
                std::cerr << shellType << " shell is not available on this system" << std::endl;
                return PLUGIN_ERROR_GENERAL;
            }
        } else {
            // Auto mode - try shells in order of preference
            bool shellFound = false;
            for (const auto& shell : g_supportedShells) {
                if (isShellAvailable(shell)) {
                    g_activeShell = shell;
                    shellFound = true;
                    break;
                }
            }
            
            if (!shellFound) {
                std::cerr << "No supported shell is available on this system" << std::endl;
                return PLUGIN_ERROR_GENERAL;
            }
        }
        
        // List all available shells if verbose
        if (g_settings.find("verbose") != g_settings.end() && g_settings["verbose"] == "true") {
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
        g_isInitialized = true;
        
        return PLUGIN_SUCCESS;
    }

    PLUGIN_API void plugin_shutdown() {
        // Mark as not initialized first
        g_isInitialized = false;
        
        // Save settings before shutdown
        if (!g_settings.empty()) {
            saveSettings();
        }
        
        // Free allocated memory for plugin info
        if (g_memoryAllocated && g_pluginInfo != nullptr) {
            free(g_pluginInfo->name);
            free(g_pluginInfo->version);
            free(g_pluginInfo->description);
            free(g_pluginInfo->author);
            free(g_pluginInfo);
            g_pluginInfo = nullptr;
        }
        
        // Free allocated memory for events list
        if (g_memoryAllocated && g_eventsList != nullptr) {
            free(g_eventsList[0]);
            free(g_eventsList[1]);
            free(g_eventsList);
            g_eventsList = nullptr;
        }
        
        // Free allocated memory for default settings
        if (g_memoryAllocated && g_defaultSettings != nullptr) {
            free(g_defaultSettings[0].key);
            free(g_defaultSettings[0].value);
            free(g_defaultSettings[1].key);
            free(g_defaultSettings[1].value);
            free(g_defaultSettings);
            g_defaultSettings = nullptr;
        }
        
        // Reset memory allocation flag
        g_memoryAllocated = false;
        
        // Clear captured command
        g_capturedCommand.clear();
    }

    PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
        if (args->count == 0 || args->position >= args->count) {
            return PLUGIN_ERROR_INVALID_ARGS;
        }
        
        std::string cmd = args->args[args->position];
        args->position++;
        
        if (cmd == "event") {
            if (args->position >= args->count) {
                return PLUGIN_ERROR_INVALID_ARGS;
            }
            
            std::string eventType = args->args[args->position];
            args->position++;
            
            std::string eventData = "";
            if (args->position < args->count) {
                eventData = args->args[args->position];
                args->position++;
            }
            
            if (eventType == "main_process_command_processed") {
                // Capture the first word of the command as our command name
                std::istringstream iss(eventData);
                std::string firstWord;
                iss >> firstWord;
                
                if (!firstWord.empty() && firstWord != "cd") {
                    g_capturedCommand = firstWord;
                    if (g_settings.find("verbose") != g_settings.end() && g_settings["verbose"] == "true") {
                        std::cout << "Shell Plugin captured command: " << g_capturedCommand << std::endl;
                    }
                }
            }
            
            return PLUGIN_SUCCESS;
        }
        else if (cmd == g_capturedCommand) {
            // This is our captured command - execute it via the selected shell
            std::string fullCommand = g_capturedCommand;
            
            // Append any arguments
            while (args->position < args->count) {
                fullCommand += " " + std::string(args->args[args->position]);
                args->position++;
            }
            
            return executeShellCommand(fullCommand) ? PLUGIN_SUCCESS : PLUGIN_ERROR_GENERAL;
        }
        
        return PLUGIN_ERROR_NOT_IMPLEMENTED;
    }

    PLUGIN_API char** plugin_get_commands(int* count) {
        // First clear any captured commands if they're empty
        if (g_capturedCommand.empty()) {
            *count = 0;
            return nullptr;
        }
        
        // Allocate memory for the array of pointers
        // The shell expects this to be allocated with malloc
        char** commands = (char**)malloc(sizeof(char*));
        if (!commands) {
            *count = 0;
            return nullptr;
        }
        
        // Allocate memory for the command string
        commands[0] = (char*)malloc(g_capturedCommand.size() + 1);
        if (!commands[0]) {
            free(commands);
            *count = 0;
            return nullptr;
        }
        
        // Copy the command string
        strcpy(commands[0], g_capturedCommand.c_str());
        
        *count = 1;
        return commands;
    }

    PLUGIN_API char** plugin_get_subscribed_events(int* count) {
        if (g_eventsList == nullptr && !g_memoryAllocated) {
            // Allocate memory for the array of pointers
            g_eventsList = (char**)malloc(2 * sizeof(char*));
            if (g_eventsList) {
                // Allocate and copy each string
                g_eventsList[0] = strdup("main_process_pre_run");
                g_eventsList[1] = strdup("main_process_command_processed");
                g_memoryAllocated = true;
            }
        }
        
        *count = (g_eventsList != nullptr) ? 2 : 0;
        return g_eventsList;
    }

    PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
        if (g_defaultSettings == nullptr && !g_memoryAllocated) {
            // Allocate memory for the settings array
            g_defaultSettings = (plugin_setting_t*)malloc(2 * sizeof(plugin_setting_t));
            if (g_defaultSettings) {
                g_defaultSettings[0].key = strdup("verbose");
                g_defaultSettings[0].value = strdup("true");
                g_defaultSettings[1].key = strdup("shell_type");
                g_defaultSettings[1].value = strdup("auto");
                g_memoryAllocated = true;
            }
        }
        
        *count = (g_defaultSettings != nullptr) ? 2 : 0;
        return g_defaultSettings;
    }

    PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
        std::string keyStr(key);
        std::string valueStr(value);
        
        g_settings[keyStr] = valueStr;
        
        if (keyStr == "shell_type" && g_isInitialized) {
            if (valueStr == "auto") {
                std::cout << "Shell type set to auto-detect." << std::endl;
                plugin_initialize(); // Re-initialize to pick the best shell
            } else if (isShellAvailable(valueStr)) {
                g_activeShell = valueStr;
                std::cout << "Shell type changed to " << valueStr << "." << std::endl;
            } else {
                std::cout << "Warning: " << valueStr << " shell is not available. Keeping current shell: " << g_activeShell << std::endl;
            }
        } else {
            std::cout << "Shell Plugin setting updated - " << keyStr << ": " << valueStr << std::endl;
        }
        
        // Save settings after any update
        saveSettings();
        
        return PLUGIN_SUCCESS;
    }

    PLUGIN_API void plugin_free_memory(void* ptr) {
        free(ptr);
    }
}
