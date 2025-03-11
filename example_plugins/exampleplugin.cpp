#include "plugininterface.h"
#include <iostream>
#include <map>
#include <vector>
#include <string>

class ExamplePlugin : public PluginInterface {
public:
    ExamplePlugin() {}
    ~ExamplePlugin() throw() { }
    
    std::string getName() const { 
        return "ExamplePlugin"; 
    }
    
    std::string getVersion() const { 
        return "1.0.0"; 
    }
    
    std::string getDescription() const { 
        return "A simple example plugin for DevToolsTerminal."; 
    }
    
    std::string getAuthor() const { 
        return "Your Name"; 
    }
    
    bool initialize() { 
        std::cout << "ExamplePlugin initialized." << std::endl;
        return true; 
    }
    
    void shutdown() { 
        std::cout << "ExamplePlugin shutting down." << std::endl; 
    }
    
    bool handleCommand(std::queue<std::string>& args) {
        if (args.front() == "example") {
            args.pop();
            std::cout << "ExamplePlugin command executed." << std::endl;
            return true;
        }
        return false;
    }
    
    std::vector<std::string> getCommands() const { 
        std::vector<std::string> cmds;
        cmds.push_back("example");
        return cmds;
    }
    
    std::map<std::string, std::string> getDefaultSettings() const { 
        std::map<std::string, std::string> settings;
        settings.insert(std::make_pair("setting1", "value1"));
        settings.insert(std::make_pair("setting2", "value2"));
        return settings;
    }
    
    void updateSetting(const std::string& key, const std::string& value) { 
        std::cout << "ExamplePlugin updated setting " << key << " to " << value << std::endl;
    }
};

PLUGIN_API PluginInterface* createPlugin() { return new ExamplePlugin(); }
PLUGIN_API void destroyPlugin(PluginInterface* plugin) { delete plugin; }
