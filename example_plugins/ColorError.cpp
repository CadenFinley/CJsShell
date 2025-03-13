#include "plugininterface.h"
#include <iostream>
#include <string>
#include <streambuf>
#include <fstream>
#include <filesystem>
#include "nlohmann/json.hpp"

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

class ColorError : public PluginInterface {
private:
    ColoredErrorBuffer colorBuffer;
    std::streambuf* originalBuffer;
    std::string DATA_DIRECTORY = ".DTT-Data";
    std::string SETTINGS_DIRECTORY = DATA_DIRECTORY + "/color-error-setting";
    std::string USER_DATA = SETTINGS_DIRECTORY + "/color-error-settings.json";

public:
    std::string getName() const override { return "colorerror"; }
    std::string getVersion() const override { return "1.0"; }
    std::string getDescription() const override { 
        return "Colors stderr output in red"; 
    }
    std::string getAuthor() const override { return "Caden Finley"; }
    
    bool initialize() override {
        originalBuffer = std::cerr.rdbuf();
        std::cerr.rdbuf(&colorBuffer);
        
        ensureSettingsExist();
        loadSettings();
        return true;
    }
    
    void shutdown() override {
        std::cerr.rdbuf(originalBuffer);
    }
    
    bool handleCommand(std::queue<std::string>& args) override {
        if (args.empty()) return false;
        
        std::string cmd = args.front();
        args.pop();
        
        if (cmd == "setcolor" && !args.empty()) {
            std::string color = args.front();
            if (color.find_first_not_of("0123456789;") == std::string::npos) {
                updateSetting("color", color);
                return true;
            }
            std::cerr << "Invalid color code. Use ANSI color codes (e.g., 31 for red)." << std::endl;
        }
        return false;
    }
    
    std::vector<std::string> getCommands() const override {
        return {"setcolor"}; ;
    }
    
    std::map<std::string, std::string> getDefaultSettings() const override {
        return {{"color", "31"}};
    }
    
    void updateSetting(const std::string& key, const std::string& value) override {
        if (key == "color") {
            colorBuffer.setColor(value);
            
            nlohmann::json settings;
            if (fs::exists(USER_DATA)) {
                std::ifstream inFile(USER_DATA);
                if (inFile.is_open()) {
                    settings = nlohmann::json::parse(inFile);
                }
            }
            
            settings["color"] = value;
            
            std::ofstream outFile(USER_DATA);
            if (outFile.is_open()) {
                outFile << settings.dump(4);
            } else {
                std::cerr << "Failed to save settings" << std::endl;
            }
        }
    }

private:
    void ensureSettingsExist() {
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

    void loadSettings() {
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
};
IMPLEMENT_PLUGIN(ColorError)
