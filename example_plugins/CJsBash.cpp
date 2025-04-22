#include "../src/include/plugininterface.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <cstdio>

// g++ -std=c++17 -shared -dynamiclib -fPIC CJsBash.cpp -o CJsBash.dylib

class BashExecutorPlugin : public PluginInterface {
private:
    std::map<std::string, std::string> settings;
    bool isInitialized;
    std::string capturedCommand;

public:
    BashExecutorPlugin() : isInitialized(false) {}

    std::string getName() const {
        return "CJsBash";
    }

    std::string getVersion() const {
        return "1.0.0.0";
    }

    std::string getDescription() const {
        return "A plugin to use Bash shell command processor and executor.";
    }

    std::string getAuthor() const {
        return "Caden Finley";
    }

    bool initialize() {
        std::cout << "Bash Executor Plugin initialized" << std::endl;
        isInitialized = true;
        
        if (system("which bash > /dev/null 2>&1") != 0) {
            std::cerr << "Bash shell is not available on this system" << std::endl;
            isInitialized = false;
            return false;
        }
        
        return true;
    }

    void shutdown() {
        std::cout << "Bash Executor Plugin shutting down" << std::endl;
        isInitialized = false;
    }

    bool executeBashCommand(const std::string& command) {
        std::string bashCommand = "bash -c \"" + command + "\"";
        
        if (settings.find("verbose") != settings.end() && settings["verbose"] == "true") {
            std::cout << "Executing via Bash: " << command << std::endl;
        }
        
        int result = system(bashCommand.c_str());
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
                        std::cout << "Bash Plugin captured command: " << capturedCommand << std::endl;
                    }
                }
            }
            
            return true;
        }
        else if (cmd == capturedCommand) {
            // This is our captured command - execute it via Bash
            std::string fullCommand = capturedCommand;
            
            // Append any arguments
            while (!args.empty()) {
                fullCommand += " " + args.front();
                args.pop();
            }
            
            return executeBashCommand(fullCommand);
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
        return defaults;
    }

    int getInterfaceVersion() const {
        return 2;
    }

    void updateSetting(const std::string& key, const std::string& value) {
        settings[key] = value;
        std::cout << "Bash Plugin setting updated - " << key << ": " << value << std::endl;
    }
};

IMPLEMENT_PLUGIN(BashExecutorPlugin)
