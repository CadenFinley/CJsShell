#include "../include/pluginapi.h"
#include <iostream>
#include <string>
#include <streambuf>
#include <fstream>
#include <filesystem>
#include <cstdlib>  // For getenv
#include "nlohmann/json.hpp"

// Forward declaration of plugin API functions with consistent linkage
extern "C" {
    PLUGIN_API int plugin_update_setting(const char* key, const char* value);
}

namespace fs = std::filesystem;

class ColoredErrorBuffer : public std::streambuf {
private:
    std::streambuf* original;
    std::string colorCode = "31";

protected:
    virtual int_type overflow(int_type c) {
        if (c != EOF) {
            if (original->sputc('\033') == EOF) return EOF;
            if (original->sputc('[') == EOF) return EOF;
            for (char digit : colorCode) {
                if (original->sputc(digit) == EOF) return EOF;
            }
            if (original->sputc('m') == EOF) return EOF;
            if (original->sputc(c) == EOF) return EOF;
            if (original->sputc('\033') == EOF) return EOF;
            if (original->sputc('[') == EOF) return EOF;
            if (original->sputc('0') == EOF) return EOF;
            if (original->sputc('m') == EOF) return EOF;
            return c;
        }
        return EOF;
    }

public:
    ColoredErrorBuffer() : original(std::cerr.rdbuf()) {}
    std::streambuf* getOriginal() { return original; }
    void setColor(const std::string& code) { colorCode = code; }
};

// Global state that was previously in the class
static ColoredErrorBuffer colorBuffer;
static std::streambuf* originalBuffer = nullptr;
static std::string SETTINGS_DIRECTORY;
static std::string USER_DATA;

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
            std::cerr << "Failed to create settings file" << std::endl;
        }
    }
}

static void loadSettings() {
    try {
        if (fs::exists(USER_DATA)) {
            std::ifstream file(USER_DATA);
            if (!file.is_open()) {
                std::cerr << "Failed to open settings file" << std::endl;
                return;
            }
            nlohmann::json settings = nlohmann::json::parse(file);
            if (settings.contains("color")) {
                std::string color = settings["color"];
                if (color.find_first_not_of("0123456789;") == std::string::npos) {
                    colorBuffer.setColor(color);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading settings: " << e.what() << std::endl;
    }
}

// Required plugin API functions
extern "C" {
    PLUGIN_API plugin_info_t* plugin_get_info() {
        static plugin_info_t info = {
            const_cast<char*>("ColorError"),       // name
            const_cast<char*>("1.0"),              // version
            const_cast<char*>("Colors stderr output in red"), // description
            const_cast<char*>("Caden Finley"),     // author
            PLUGIN_INTERFACE_VERSION               // interface version
        };
        return &info;
    }

    PLUGIN_API int plugin_initialize() {
        // Use a fallback approach for getting plugin directory since we can't call the helper function directly
        const char* home = getenv("HOME");
        if (home) {
            SETTINGS_DIRECTORY = std::string(home) + "/.cjsh_data/plugins/ColorError";
            USER_DATA = SETTINGS_DIRECTORY + "/color-error-settings.json";
            
            originalBuffer = std::cerr.rdbuf();
            std::cerr.rdbuf(&colorBuffer);
            
            ensureSettingsExist();
            loadSettings();
            return PLUGIN_SUCCESS;
        }
        return PLUGIN_ERROR_GENERAL;
    }

    PLUGIN_API void plugin_shutdown() {
        if (originalBuffer) {
            std::cerr.rdbuf(originalBuffer);
            originalBuffer = nullptr;
        }
    }

    PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
        if (!args || args->count < 1) return PLUGIN_ERROR_INVALID_ARGS;
        
        std::string cmd = args->args[0];
        
        if (cmd == "setcolor" && args->count > 1) {
            std::string color = args->args[1];
            if (color.find_first_not_of("0123456789;") == std::string::npos) {
                // Use the local implementation rather than treating it as an external function
                return plugin_update_setting("color", color.c_str());
            }
            std::cerr << "Invalid color code. Use ANSI color codes (e.g., 31 for red)." << std::endl;
            return PLUGIN_ERROR_INVALID_ARGS;
        }
        return PLUGIN_ERROR_NOT_IMPLEMENTED;
    }

    PLUGIN_API char** plugin_get_commands(int* count) {
        static char* commands[] = {const_cast<char*>("setcolor")};
        *count = 1;
        return commands;
    }

    PLUGIN_API char** plugin_get_subscribed_events(int* count) {
        *count = 0;
        return nullptr;
    }

    PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
        static plugin_setting_t settings[] = {
            {const_cast<char*>("color"), const_cast<char*>("31")}
        };
        *count = 1;
        return settings;
    }

    PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
        std::string keyStr(key);
        std::string valueStr(value);
        
        if (keyStr == "color") {
            if (valueStr.find_first_not_of("0123456789;") != std::string::npos) {
                return PLUGIN_ERROR_INVALID_ARGS;
            }
            
            colorBuffer.setColor(valueStr);
            
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
                    std::cerr << "Failed to save settings" << std::endl;
                    return PLUGIN_ERROR_GENERAL;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error updating settings: " << e.what() << std::endl;
                return PLUGIN_ERROR_GENERAL;
            }
        }
        
        return PLUGIN_ERROR_NOT_IMPLEMENTED;
    }

    PLUGIN_API void plugin_free_memory(void* ptr) {
        free(ptr);
    }
}
