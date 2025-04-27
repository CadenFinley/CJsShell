#include "../include/pluginapi.h"
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <cstdlib>  // For getenv
#include <cstring>  // For memcpy, strlen
#include <unistd.h> // For pipe, dup2
#include <fcntl.h>  // For fcntl
#include <thread>
#include <atomic>
#include <mutex>
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

// Global state - much simpler than before
static std::string colorCode = "31";
static std::string SETTINGS_DIRECTORY;
static std::string USER_DATA;

// Pipe for redirecting stderr
static int stderrPipe[2] = {-1, -1};
static int originalStderr = -1;
static std::thread redirectThread;
static std::atomic<bool> shouldRedirect(false);
static std::mutex settingsMutex;

// Add a structure to track allocated memory
struct AllocatedMemory {
    void* ptr;
    AllocatedMemory* next;
};

static AllocatedMemory* allocatedMemoryList = nullptr;

// Helper function to register allocated memory
static void* registerAllocatedMemory(void* ptr) {
    if (!ptr) return nullptr;
    
    AllocatedMemory* newNode = (AllocatedMemory*)malloc(sizeof(AllocatedMemory));
    if (!newNode) return ptr; // If we can't track it, at least return the pointer
    
    newNode->ptr = ptr;
    newNode->next = allocatedMemoryList;
    allocatedMemoryList = newNode;
    
    return ptr;
}

// Helper function to check if memory was allocated by this plugin
static bool isAllocatedByPlugin(void* ptr) {
    AllocatedMemory* current = allocatedMemoryList;
    while (current) {
        if (current->ptr == ptr) {
            return true;
        }
        current = current->next;
    }
    return false;
}

// Helper function to remove pointer from tracked list
static void removeFromAllocatedList(void* ptr) {
    AllocatedMemory* current = allocatedMemoryList;
    AllocatedMemory* prev = nullptr;
    
    while (current) {
        if (current->ptr == ptr) {
            if (prev) {
                prev->next = current->next;
            } else {
                allocatedMemoryList = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Helper function to read from pipe and write colored output to original stderr
static void redirectFunction() {
    char buffer[4096];
    ssize_t bytesRead;
    
    std::string colorStart = "\033[" + colorCode + "m";
    std::string colorEnd = "\033[0m";
    
    while (shouldRedirect) {
        // Use a non-blocking read with a small timeout to allow clean shutdown
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(stderrPipe[0], &readSet);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout
        
        int ready = select(stderrPipe[0] + 1, &readSet, NULL, NULL, &timeout);
        
        if (ready > 0) {
            bytesRead = read(stderrPipe[0], buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                
                // Critical section for accessing shared colorCode
                std::string currentColorStart;
                {
                    std::lock_guard<std::mutex> lock(settingsMutex);
                    currentColorStart = "\033[" + colorCode + "m";
                }
                
                // Write the colored output to the original stderr
                write(originalStderr, currentColorStart.c_str(), currentColorStart.size());
                write(originalStderr, buffer, bytesRead);
                write(originalStderr, colorEnd.c_str(), colorEnd.size());
            }
        }
    }
}

// Helper functions for settings
static void ensureSettingsExist() {
    if (!fs::exists(SETTINGS_DIRECTORY)) {
        fs::create_directories(SETTINGS_DIRECTORY);
    }
    
    if (!fs::exists(USER_DATA)) {
        nlohmann::json defaultSettings = {
            {"color", "31"}
        };
        std::ofstream file(USER_DATA);
        if (file.is_open()) {
            file << defaultSettings.dump(4);
        } else {
            // Since stderr is redirected, write directly to the original
            std::string errorMsg = "Failed to create settings file\n";
            write(originalStderr, errorMsg.c_str(), errorMsg.size());
        }
    }
}

static void loadSettings() {
    try {
        if (fs::exists(USER_DATA)) {
            std::ifstream file(USER_DATA);
            if (!file.is_open()) {
                std::string errorMsg = "Failed to open settings file\n";
                write(originalStderr, errorMsg.c_str(), errorMsg.size());
                return;
            }
            
            nlohmann::json settings = nlohmann::json::parse(file);
            if (settings.contains("color")) {
                std::string color = settings["color"];
                if (color.find_first_not_of("0123456789;") == std::string::npos) {
                    std::lock_guard<std::mutex> lock(settingsMutex);
                    colorCode = color;
                }
            }
        }
    } catch (const std::exception& e) {
        std::string errorMsg = "Error loading settings: ";
        errorMsg += e.what();
        errorMsg += "\n";
        write(originalStderr, errorMsg.c_str(), errorMsg.size());
    }
}

// Required plugin API functions
extern "C" {
    PLUGIN_API plugin_info_t* plugin_get_info() {
        static char name[] = "ColorError";
        static char version[] = "1.0";
        static char description[] = "Colors stderr output in red"; 
        static char author[] = "Caden Finley";
        
        static plugin_info_t info = {
            name,                     // name
            version,                  // version
            description,              // description
            author,                   // author
            PLUGIN_INTERFACE_VERSION  // interface version
        };
        return &info;  // Simply return the address of the static variable
    }

    PLUGIN_API int plugin_initialize() {
        const char* home = getenv("HOME");
        if (!home) return PLUGIN_ERROR_GENERAL;
        
        SETTINGS_DIRECTORY = std::string(home) + "/.cjsh/plugins/ColorError";
        USER_DATA = SETTINGS_DIRECTORY + "/settings.json";
        
        ensureSettingsExist();
        loadSettings();
        
        // Create pipe for stderr redirection
        if (pipe(stderrPipe) != 0) {
            return PLUGIN_ERROR_GENERAL;
        }
        
        // Save original stderr
        originalStderr = dup(STDERR_FILENO);
        if (originalStderr == -1) {
            close(stderrPipe[0]);
            close(stderrPipe[1]);
            return PLUGIN_ERROR_GENERAL;
        }
        
        // Redirect stderr to our pipe
        if (dup2(stderrPipe[1], STDERR_FILENO) == -1) {
            close(stderrPipe[0]);
            close(stderrPipe[1]);
            close(originalStderr);
            return PLUGIN_ERROR_GENERAL;
        }
        
        // Set non-blocking mode on the pipe
        int flags = fcntl(stderrPipe[0], F_GETFL, 0);
        fcntl(stderrPipe[0], F_SETFL, flags | O_NONBLOCK);
        
        // Start background thread for processing
        shouldRedirect = true;
        redirectThread = std::thread(redirectFunction);
        
        return PLUGIN_SUCCESS;
    }

    PLUGIN_API void plugin_shutdown() {
        // Stop redirection thread
        shouldRedirect = false;
        if (redirectThread.joinable()) {
            redirectThread.join();
        }
        
        // Restore original stderr
        if (originalStderr != -1) {
            dup2(originalStderr, STDERR_FILENO);
            close(originalStderr);
            originalStderr = -1;
        }
        
        // Close pipes
        if (stderrPipe[0] != -1) {
            close(stderrPipe[0]);
            stderrPipe[0] = -1;
        }
        
        if (stderrPipe[1] != -1) {
            close(stderrPipe[1]);
            stderrPipe[1] = -1;
        }
        
        // Free any remaining allocated memory
        AllocatedMemory* current = allocatedMemoryList;
        while (current) {
            AllocatedMemory* next = current->next;
            free(current->ptr);
            free(current);
            current = next;
        }
        allocatedMemoryList = nullptr;
    }

    PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
        if (!args || args->count < 1) return PLUGIN_ERROR_INVALID_ARGS;
        
        std::string cmd = args->args[0];
        
        if (cmd == "setcolor" && args->count > 1) {
            std::string color = args->args[1];
            if (color.find_first_not_of("0123456789;") != std::string::npos) {
                std::string errorMsg = "Invalid color code. Use ANSI color codes (e.g., 31 for red).\n";
                write(originalStderr, errorMsg.c_str(), errorMsg.size());
                return PLUGIN_ERROR_INVALID_ARGS;
            }
            
            // Update color code
            {
                std::lock_guard<std::mutex> lock(settingsMutex);
                colorCode = color;
            }
            
            // Save to settings file
            try {
                nlohmann::json settings;
                if (fs::exists(USER_DATA)) {
                    std::ifstream inFile(USER_DATA);
                    if (inFile.is_open()) {
                        settings = nlohmann::json::parse(inFile);
                    }
                }
                
                settings["color"] = color;
                
                std::ofstream outFile(USER_DATA);
                if (outFile.is_open()) {
                    outFile << settings.dump(4);
                    return PLUGIN_SUCCESS;
                } else {
                    std::string errorMsg = "Failed to save settings\n";
                    write(originalStderr, errorMsg.c_str(), errorMsg.size());
                    return PLUGIN_ERROR_GENERAL;
                }
            } catch (const std::exception& e) {
                std::string errorMsg = "Error updating settings: ";
                errorMsg += e.what();
                errorMsg += "\n";
                write(originalStderr, errorMsg.c_str(), errorMsg.size());
                return PLUGIN_ERROR_GENERAL;
            }
        }
        return PLUGIN_ERROR_NOT_IMPLEMENTED;
    }

    PLUGIN_API char** plugin_get_commands(int* count) {
        static char command[] = "setcolor";
        static char* commands[] = {command};
        *count = 1;
        return commands;
    }

    PLUGIN_API char** plugin_get_subscribed_events(int* count) {
        *count = 0;
        return nullptr;
    }

    PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
        static char key[] = "color";
        static char value[] = "31";
        static plugin_setting_t settings[] = {
            {key, value}
        };
        *count = 1;
        return settings;
    }

    PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
        std::string keyStr(key);
        
        if (keyStr == "color") {
            std::string valueStr(value);
            if (valueStr.find_first_not_of("0123456789;") != std::string::npos) {
                return PLUGIN_ERROR_INVALID_ARGS;
            }
            
            // Thread-safe update of color
            {
                std::lock_guard<std::mutex> lock(settingsMutex);
                colorCode = valueStr;
            }
            
            // Save to settings file
            try {
                nlohmann::json settings;
                if (fs::exists(USER_DATA)) {
                    std::ifstream inFile(USER_DATA);
                    if (inFile.is_open()) {
                        settings = nlohmann::json::parse(inFile);
                    }
                }
                
                settings["color"] = valueStr;
                
                std::ofstream outFile(USER_DATA);
                if (outFile.is_open()) {
                    outFile << settings.dump(4);
                    return PLUGIN_SUCCESS;
                } else {
                    return PLUGIN_ERROR_GENERAL;
                }
            } catch (const std::exception& e) {
                return PLUGIN_ERROR_GENERAL;
            }
        }
        
        return PLUGIN_ERROR_NOT_IMPLEMENTED;
    }

    PLUGIN_API void plugin_free_memory(void* ptr) {
        if (ptr && isAllocatedByPlugin(ptr)) {
            removeFromAllocatedList(ptr);
            free(ptr);
        }
        // Don't free memory not allocated by this plugin
    }
}
