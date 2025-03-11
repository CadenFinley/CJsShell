#pragma once

#include "plugininterface.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <dlfcn.h> // For dynamic library loading on UNIX/Mac

class PluginManager {
public:
    PluginManager(const std::filesystem::path& pluginsDir);
    ~PluginManager();
    
    // Discover and load plugins from the plugins directory
    bool discoverPlugins();
    
    // Get information about available plugins
    std::vector<std::string> getAvailablePlugins() const;
    std::vector<std::string> getEnabledPlugins() const;
    
    // Enable/disable plugins
    bool enablePlugin(const std::string& name);
    bool disablePlugin(const std::string& name);
    
    // Plugin commands
    bool handlePluginCommand(const std::string& command, const std::vector<std::string>& args);
    
    // Get plugin info
    std::string getPluginInfo(const std::string& name) const;
    
    // Plugin settings
    bool updatePluginSetting(const std::string& pluginName, const std::string& key, const std::string& value);
    std::map<std::string, std::map<std::string, std::string>> getAllPluginSettings() const;
    
    // Event system
    void registerEventCallback(const std::string& event, std::function<void(const std::string&)> callback);
    void triggerEvent(const std::string& event, const std::string& data);

private:
    struct PluginData {
        void* handle;                // Library handle
        PluginInterface* instance;   // Plugin instance
        CreatePluginFunc createFunc; // Creation function
        DestroyPluginFunc destroyFunc; // Destruction function
        bool enabled;                // Enabled status
        std::map<std::string, std::string> settings; // Plugin settings
    };
    
    std::filesystem::path pluginsDirectory;
    std::map<std::string, PluginData> loadedPlugins;
    std::map<std::string, std::vector<std::function<void(const std::string&)>>> eventCallbacks;
    
    // Helper methods
    bool loadPlugin(const std::filesystem::path& path);
    void unloadPlugin(const std::string& name);
};
