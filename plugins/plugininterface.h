#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>

class PluginInterface {
public:
    virtual ~PluginInterface() = default;
    
    // Core plugin methods
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getAuthor() const = 0;
    
    // Lifecycle methods
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Command handling
    virtual bool handleCommand(const std::string& command, std::vector<std::string>& args) = 0;
    virtual std::vector<std::string> getCommands() const = 0;
    
    // Settings
    virtual std::map<std::string, std::string> getDefaultSettings() const = 0;
    virtual void updateSetting(const std::string& key, const std::string& value) = 0;
};

// Plugin creation and destruction function signatures
extern "C" {
    typedef PluginInterface* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(PluginInterface*);
}

// Macros to simplify plugin implementation
#define PLUGIN_API extern "C"
#define IMPLEMENT_PLUGIN(ClassName) \
    PLUGIN_API PluginInterface* createPlugin() { return new ClassName(); } \
    PLUGIN_API void destroyPlugin(PluginInterface* plugin) { delete plugin; }
