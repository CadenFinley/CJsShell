#include "plugininterface.h"
#include <iostream>

class ExamplePlugin : public PluginInterface {
private:
    std::map<std::string, std::string> settings;
    bool isInitialized;

public:
    ExamplePlugin() : isInitialized(false) {}

    std::string getName() const {
        return "ExamplePlugin";
    }

    std::string getVersion() const {
        return "1.0.0";
    }

    std::string getDescription() const {
        return "A comprehensive example plugin demonstrating all features";
    }

    std::string getAuthor() const {
        return "Caden Finley";
    }

    bool initialize() {
        std::cout << "ExamplePlugin initializing..." << std::endl;
        std::cout << "Using greeting: " << settings["greeting"] << std::endl;
        isInitialized = true;
        return true;
    }

    void shutdown() {
        std::cout << "ExamplePlugin shutting down..." << std::endl;
        isInitialized = false;
    }

    bool handleCommand(std::queue<std::string>& args) {
        if (args.empty()) return false;

        std::string cmd = args.front();
        args.pop();

        if (cmd == "hello") {
            std::cout << settings["greeting"] << " from ExamplePlugin!" << std::endl;
            return true;
        }
        else if (cmd == "count") {
            int count = std::stoi(settings["count"]);
            for (int i = 1; i <= count; i++) {
                std::cout << i << std::endl;
            }
            return true;
        }
        else if (cmd == "event") {
            if (args.empty()) return false;
            std::string eventType = args.front();
            args.pop();
            std::string eventData = args.empty() ? "" : args.front();
            
            if (eventType == "main_process") {
                std::cout << "example_plugin recognized main_process: " << eventData << std::endl;
            }
            else if (eventType == "plugin_enabled") {
                std::cout << "example_plugin recognized enabled: " << eventData << std::endl;
            }
            else if (eventType == "plugin_disabled") {
                std::cout << "example_plugin recognized disabled: " << eventData << std::endl;
            }
            return true;
        }
        return false;
    }

    std::vector<std::string> getCommands() const {
        std::vector<std::string> commands;
        commands.push_back("hello");
        commands.push_back("count");
        return commands;
    }

    std::map<std::string, std::string> getDefaultSettings() const {
        std::map<std::string, std::string> defaults;
        defaults["greeting"] = "Hello";
        defaults["count"] = "5";
        return defaults;
    }

    void updateSetting(const std::string& key, const std::string& value) {
        settings[key] = value;
        std::cout << "Setting updated - " << key << ": " << value << std::endl;
    }
};

IMPLEMENT_PLUGIN(ExamplePlugin)
