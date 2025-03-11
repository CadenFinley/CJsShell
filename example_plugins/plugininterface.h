#pragma once

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <functional>

class PluginInterface {
public:
    virtual ~PluginInterface() {}
    
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getAuthor() const = 0;
    
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    virtual bool handleCommand(std::queue<std::string>& args) = 0;
    virtual std::vector<std::string> getCommands() const = 0;
    
    virtual std::map<std::string, std::string> getDefaultSettings() const = 0;
    virtual void updateSetting(const std::string& key, const std::string& value) = 0;
};

extern "C" {
    typedef PluginInterface* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(PluginInterface*);
}
#define PLUGIN_API extern "C"
#define IMPLEMENT_PLUGIN(ClassName) \
    PLUGIN_API PluginInterface* createPlugin() { return new ClassName(); } \
    PLUGIN_API void destroyPlugin(PluginInterface* plugin) { delete plugin; }
