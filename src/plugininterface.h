#pragma once

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <functional>

class PluginInterface {
public:
    // Plugin interface version for compatibility checking
    static constexpr int INTERFACE_VERSION = 1;
    
    //plugin has its own default constructor
    virtual ~PluginInterface() {}
    
    //plugin details
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getAuthor() const = 0;
    
    // Plugin interface version compatibility check
    virtual int getInterfaceVersion() const = 0;
    
    virtual bool initialize() = 0; //enable plugin
    virtual void shutdown() = 0; //disable plugin
    
    //an "event main_process <phase>" command will be sent to all plugins via the handleCommand method during all phases of the mainProcessLoop (pre_run, start, took_input: <char>, command_processed: <command>, and end)
    //an "event plugin_enabled <plugin_name>" command will be sent to all plugins via the handleCommand method when a plugin is enabled
    //an "event plugin_disabled <plugin_name>" command will be sent to all plugins via the handleCommand method when a plugin is disabled
    virtual bool handleCommand(std::queue<std::string>& args) = 0;
    virtual std::vector<std::string> getCommands() const = 0; //the vector of immeadiatly availble commands
    virtual std::vector<std::string> getSubscribedEvents() const = 0; //the vector of events that the plugin is subscribed to
    
    //plugin settings
    virtual std::map<std::string, std::string> getDefaultSettings() const = 0;
    virtual void updateSetting(const std::string& key, const std::string& value) = 0;
};

//plugin interface
extern "C" {
    typedef PluginInterface* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(PluginInterface*);
}
#define PLUGIN_API extern "C"
#define IMPLEMENT_PLUGIN(ClassName) \
    PLUGIN_API PluginInterface* createPlugin() { return new ClassName(); } \
    PLUGIN_API void destroyPlugin(PluginInterface* plugin) { delete plugin; }
