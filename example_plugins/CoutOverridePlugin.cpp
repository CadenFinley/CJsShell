#include "plugininterface.h"
#include <iostream>
#include <streambuf>
#include <vector>
#include <map>
#include <queue>
#include <string>

class CustomCoutBuffer : public std::streambuf {
    std::streambuf* original;
    std::string colorCode;
    bool atLineStart;
    bool enabled;
protected:
    int overflow(int c) {
        if (!enabled) {
            return original->sputc(c);
        }
        
        if (atLineStart && !colorCode.empty()) {
            for (size_t i = 0; i < colorCode.size(); ++i) {
                original->sputc(colorCode[i]);
            }
            atLineStart = false;
        }
        if (c != EOF) { 
            original->sputc(c);
            if(c == '\n'){
                atLineStart = true;
            }
        }
        return c;
    }
public:
    CustomCoutBuffer(std::streambuf* orig)
      : original(orig), colorCode(""), atLineStart(true), enabled(true) {}
      
    void setEnabled(bool state) {
        enabled = state;
        // Reset to default when disabling
        if (!enabled && !colorCode.empty()) {
            // Write reset code to ensure no colors persist
            std::string resetCode = "\033[0m";
            for (size_t i = 0; i < resetCode.length(); ++i) {
                original->sputc(resetCode[i]);
            }
        }
        atLineStart = true;
    }
    
    void setColor(const std::string& code) {
        colorCode = code;
        atLineStart = true;  // Force color application at next output
    }

    std::streambuf* getOriginal() {
        return original;
    }
};

class CoutOverridePlugin : public PluginInterface {
    std::streambuf* originalCoutBuffer;
    CustomCoutBuffer* customBuffer;
    // New members for std::cerr override
    std::streambuf* originalCerrBuffer;
    CustomCoutBuffer* customCerrBuffer;
    // New members for terminal output override (std::clog)
    std::streambuf* originalClogBuffer;
    CustomCoutBuffer* customClogBuffer;
    // New member to track color setting in plugin
    std::string currentColor;
public:
    CoutOverridePlugin() 
      : originalCoutBuffer(nullptr), customBuffer(nullptr),
        originalCerrBuffer(nullptr), customCerrBuffer(nullptr),
        originalClogBuffer(nullptr), customClogBuffer(nullptr),
        currentColor("reset") {}
    ~CoutOverridePlugin() {}

    std::string getName() const { return "coutovrde"; }
    std::string getVersion() const { return "1.0"; }
    std::string getDescription() const { return "Overrides std::cout, std::cerr, std::clog"; }
    std::string getAuthor() const { return "Caden Finley";}

    bool initialize() {
        // Override std::cout
        originalCoutBuffer = std::cout.rdbuf();
        customBuffer = new CustomCoutBuffer(originalCoutBuffer);
        customBuffer->setColor("\033[32m");
        customBuffer->setEnabled(true);
        std::cout.rdbuf(customBuffer);

        // Override std::cerr with default red
        originalCerrBuffer = std::cerr.rdbuf();
        customCerrBuffer = new CustomCoutBuffer(originalCerrBuffer);
        customCerrBuffer->setColor("\033[31m");
        customCerrBuffer->setEnabled(true);
        std::cerr.rdbuf(customCerrBuffer);

        // Override std::clog with default yellow
        originalClogBuffer = std::clog.rdbuf();
        customClogBuffer = new CustomCoutBuffer(originalClogBuffer);
        customClogBuffer->setColor("\033[33m");
        customClogBuffer->setEnabled(true);
        std::clog.rdbuf(customClogBuffer);
        return true;
    }

    void shutdown() {
        // First disable coloring before restoration
        if (customBuffer) {
            customBuffer->setEnabled(false);
        }
        if (customCerrBuffer) {
            customCerrBuffer->setEnabled(false);
        }
        if (customClogBuffer) {
            customClogBuffer->setEnabled(false);
        }
        
        // Restore original stream buffers
        if (originalCoutBuffer) {
            std::cout.rdbuf(originalCoutBuffer);
            originalCoutBuffer = nullptr;
        }
        if (originalCerrBuffer) {
            std::cerr.rdbuf(originalCerrBuffer);
            originalCerrBuffer = nullptr;
        }
        if (originalClogBuffer) {
            std::clog.rdbuf(originalClogBuffer);
            originalClogBuffer = nullptr;
        }
        
        // Delete custom buffers after restoring original streams
        if (customBuffer) {
            delete customBuffer;
            customBuffer = nullptr;
        }
        if (customCerrBuffer) {
            delete customCerrBuffer;
            customCerrBuffer = nullptr;
        }
        if (customClogBuffer) {
            delete customClogBuffer;
            customClogBuffer = nullptr;
        }
    }

    bool handleCommand(std::queue<std::string>& args) {
        return false;
    }

    std::vector<std::string> getCommands() const {
        std::vector<std::string> cmds;
        cmds.push_back("coutoverride");
        return cmds;
    }

    std::map<std::string, std::string> getDefaultSettings() const {
        std::map<std::string, std::string> settings;
        settings["color"] = "green";
        // Default for cerr override
        settings["cerr_color"] = "red";
        // Default for terminal output (clog) override
        settings["clog_color"] = "yellow";
        return settings;
    }
    
    void updateSetting(const std::string& key, const std::string& value) {
        if(key == "color" && customBuffer) {
            currentColor = value;
            if(value == "red")
                customBuffer->setColor("\033[1;31m");
            else if(value == "green")
                customBuffer->setColor("\033[32m");
            else if(value == "blue")
                customBuffer->setColor("\033[34m");
            else if(value == "reset")
                customBuffer->setColor("\033[0m");
            else
                customBuffer->setColor("");
        }
        else if(key == "cerr_color" && customCerrBuffer) {
            if(value == "red")
                customCerrBuffer->setColor("\033[1;31m");
            else if(value == "green")
                customCerrBuffer->setColor("\033[32m");
            else if(value == "blue")
                customCerrBuffer->setColor("\033[34m");
            else if(value == "reset")
                customCerrBuffer->setColor("\033[0m");
            else
                customCerrBuffer->setColor("");
        }
        else if(key == "clog_color" && customClogBuffer) {
            if(value == "yellow")
                customClogBuffer->setColor("\033[1;31m");
            else if(value == "red")
                customClogBuffer->setColor("\033[31m");
            else if(value == "green")
                customClogBuffer->setColor("\033[32m");
            else if(value == "blue")
                customClogBuffer->setColor("\033[34m");
            else if(value == "reset")
                customClogBuffer->setColor("\033[0m");
            else
                customClogBuffer->setColor("");
        }
    }
};

// Add this to ensure the destructor is called even if the plugin system fails
IMPLEMENT_PLUGIN(CoutOverridePlugin)