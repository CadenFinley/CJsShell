#pragma once

#include "plugininterface.h"
#include <string>
#include <fstream>
#include <vector>
#include <queue>
#include <map>
#include <iostream>
#include <memory>
#include <filesystem>
#include <dlfcn.h>
#include <functional>

class PluginManager {
public:
    PluginManager(const std::filesystem::path& pluginsDir);
    ~PluginManager();
    
    bool discoverPlugins();
    
    std::vector<std::string> getAvailablePlugins() const;
    std::vector<std::string> getEnabledPlugins() const;
    
    bool enablePlugin(const std::string& name);
    bool disablePlugin(const std::string& name);
    
    bool handlePluginCommand(const std::string targetedPlugin, std::queue<std::string>& args);
    
    std::string getPluginInfo(const std::string& name) const;
    std::vector<std::string> getPluginCommands(const std::string& name) const;
    
    bool updatePluginSetting(const std::string& pluginName, const std::string& key, const std::string& value);
    std::map<std::string, std::map<std::string, std::string>> getAllPluginSettings() const;
    
    void triggerEvent(const std::string& targetPlugin, const std::string& event, const std::string& data);
    
    PluginInterface* getPluginInstance(const std::string& name) const;

    bool uninstallPlugin(const std::string& name);

    bool installPlugin(const std::filesystem::path& sourcePath);

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
    
    // Helper methods
    bool loadPlugin(const std::filesystem::path& path);
    void unloadPlugin(const std::string& name);
};
