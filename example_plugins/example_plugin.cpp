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
            
            std::cout << "Received event: " << eventType << " with data: " << eventData << std::endl;
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

    std::vector<std::string> getSubscribedEvents() const {
        std::vector<std::string> events;
        events.push_back("main_process_pre_run");
        events.push_back("main_process_start");
        events.push_back("main_process_took_input");
        events.push_back("main_process_command_processed");
        events.push_back("main_process_end");
        events.push_back("plugin_enabled");
        events.push_back("plugin_disabled");
        return events;
    }

    std::map<std::string, std::string> getDefaultSettings() const {
        std::map<std::string, std::string> defaults;
        defaults["greeting"] = "Hello";
        defaults["count"] = "5";
        return defaults;
    }

    int getInterfaceVersion() const {
        return 1;
    }

    void updateSetting(const std::string& key, const std::string& value) {
        settings[key] = value;
        std::cout << "Setting updated - " << key << ": " << value << std::endl;
    }
};

IMPLEMENT_PLUGIN(ExamplePlugin)
