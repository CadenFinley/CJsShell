#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <iostream>
#include <dlfcn.h>
#include <algorithm>
#include <unordered_map>
#include <memory>

#include "plugininterface.h"

struct PluginData {
    void* handle;
    PluginInterface* instance;
    CreatePluginFunc createFunc;
    DestroyPluginFunc destroyFunc;
    bool enabled;
    std::map<std::string, std::string> settings;
};

class PluginManager {
private:
    std::filesystem::path pluginsDirectory;
    std::unordered_map<std::string, PluginData> loadedPlugins;
    std::unordered_map<std::string, std::vector<std::string>> subscribedEvents;
    bool pluginsDiscovered;

    void unloadPlugin(const std::string& name);

public:
    PluginManager(const std::filesystem::path& pluginsDir);
    ~PluginManager();
    
    bool discoverPlugins();
    bool loadPlugin(const std::filesystem::path& path);
    bool installPlugin(const std::filesystem::path& sourcePath);
    bool uninstallPlugin(const std::string& name);
    
    std::vector<std::string> getAvailablePlugins() const;
    std::vector<std::string> getEnabledPlugins() const;
    
    bool enablePlugin(const std::string& name);
    bool disablePlugin(const std::string& name);

    int getInterfaceVersion() const { return PluginInterface::INTERFACE_VERSION; }
    
    bool handlePluginCommand(const std::string& targetedPlugin, std::queue<std::string>& args);
    std::vector<std::string> getPluginCommands(const std::string& name) const;
    std::string getPluginInfo(const std::string& name) const;
    
    bool updatePluginSetting(const std::string& pluginName, const std::string& key, const std::string& value);
    std::map<std::string, std::map<std::string, std::string>> getAllPluginSettings() const;
    
    void triggerEvent(const std::string& targetPlugin, const std::string& event, const std::string& data);
    void triggerSubscribedGlobalEvent(const std::string& event, const std::string& eventData);
    
    PluginInterface* getPluginInstance(const std::string& name) const;
    
    void clearPluginCache();
    bool isPluginLoaded(const std::string& name) const;
};
