#include "pluginmanager.h"
#include <iostream>
#include <queue>
#include <fstream>


PluginManager::PluginManager(const std::filesystem::path& pluginsDir)
    : pluginsDirectory(pluginsDir) {
    
    if (!std::filesystem::exists(pluginsDirectory)) {
        std::filesystem::create_directories(pluginsDirectory);
    }
}

PluginManager::~PluginManager() {
    for (auto& [name, data] : loadedPlugins) {
        if (data.enabled && data.instance) {
            data.instance->shutdown();
        }
        if (data.instance && data.destroyFunc) {
            data.destroyFunc(data.instance);
        }
        if (data.handle) {
            dlclose(data.handle);
        }
    }
    loadedPlugins.clear();
}

bool PluginManager::discoverPlugins() {
    if (!std::filesystem::exists(pluginsDirectory)) {
        std::cerr << "Plugins directory does not exist: " << pluginsDirectory << std::endl;
        return false;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(pluginsDirectory)) {
        std::string fileName = entry.path().filename().string();
        if (entry.path().extension() == ".so" || entry.path().extension() == ".dylib") {
            loadPlugin(entry.path());
        }
    }
    
    return true;
}

bool PluginManager::loadPlugin(const std::filesystem::path& path) {
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load plugin: " << path << " - " << dlerror() << std::endl;
        return false;
    }
    
    dlerror();
    
    CreatePluginFunc createFunc = (CreatePluginFunc)dlsym(handle, "createPlugin");
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol 'createPlugin': " << dlsym_error << std::endl;
        dlclose(handle);
        return false;
    }

    DestroyPluginFunc destroyFunc = (DestroyPluginFunc)dlsym(handle, "destroyPlugin");
    dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol 'destroyPlugin': " << dlsym_error << std::endl;
        dlclose(handle);
        return false;
    }
    
    PluginInterface* instance = createFunc();
    if (!instance) {
        std::cerr << "Failed to create plugin instance" << std::endl;
        dlclose(handle);
        return false;
    }
    
    std::string name = instance->getName();
    
    PluginData data;
    data.handle = handle;
    data.instance = instance;
    data.createFunc = createFunc;
    data.destroyFunc = destroyFunc;
    data.enabled = false;
    data.settings = instance->getDefaultSettings();
    
    loadedPlugins[name] = data;
    
    std::cout << "Loaded plugin: " << name << " v" << instance->getVersion() << std::endl;
    return true;
}

void PluginManager::unloadPlugin(const std::string& name) {
    auto it = loadedPlugins.find(name);
    if (it != loadedPlugins.end()) {
        if (it->second.enabled) {
            it->second.instance->shutdown();
        }
        
        if (it->second.instance && it->second.destroyFunc) {
            it->second.destroyFunc(it->second.instance);
        }
        
        if (it->second.handle) {
            dlclose(it->second.handle);
        }
        
        loadedPlugins.erase(it);
    }
}

std::vector<std::string> PluginManager::getAvailablePlugins() const {
    std::vector<std::string> plugins;
    for (const auto& [name, _] : loadedPlugins) {
        plugins.push_back(name);
    }
    return plugins;
}

std::vector<std::string> PluginManager::getEnabledPlugins() const {
    std::vector<std::string> plugins;
    for (const auto& [name, data] : loadedPlugins) {
        if (data.enabled) {
            plugins.push_back(name);
        }
    }
    return plugins;
}

bool PluginManager::enablePlugin(const std::string& name) {
    auto it = loadedPlugins.find(name);
    if (it != loadedPlugins.end() && !it->second.enabled) {
        if (it->second.instance->initialize()) {
            it->second.enabled = true;
            std::cout << "Enabled plugin: " << name << std::endl;
            return true;
        } else {
            std::cerr << "Failed to initialize plugin: " << name << std::endl;
        }
    }
    return false;
}

bool PluginManager::disablePlugin(const std::string& name) {
    auto it = loadedPlugins.find(name);
    if (it != loadedPlugins.end() && it->second.enabled) {
        it->second.instance->shutdown();
        it->second.enabled = false;
        std::cout << "Disabled plugin: " << name << std::endl;
        return true;
    }
    return false;
}

bool PluginManager::handlePluginCommand(const std::string targetedPlugin, std::queue<std::string>& args) {
    auto it = loadedPlugins.find(targetedPlugin);
    if (it != loadedPlugins.end() && it->second.enabled) {
        return it->second.instance->handleCommand(args);
    }
    return false;
}

std::vector<std::string> PluginManager::getPluginCommands(const std::string& name) const {
    auto it = loadedPlugins.find(name);
    if (it != loadedPlugins.end()) {
        return it->second.instance->getCommands();
    }
    return {};
}

std::string PluginManager::getPluginInfo(const std::string& name) const {
    auto it = loadedPlugins.find(name);
    if (it != loadedPlugins.end()) {
        const auto& data = it->second;
        return "Name: " + name + "\n" +
               "Version: " + data.instance->getVersion() + "\n" +
               "Author: " + data.instance->getAuthor() + "\n" +
               "Description: " + data.instance->getDescription() + "\n" +
               "Status: " + (data.enabled ? "Enabled" : "Disabled");
    }
    return "Plugin not found: " + name;
}

bool PluginManager::updatePluginSetting(const std::string& pluginName, const std::string& key, const std::string& value) {
    auto it = loadedPlugins.find(pluginName);
    if (it != loadedPlugins.end()) {
        it->second.settings[key] = value;
        it->second.instance->updateSetting(key, value);
        return true;
    }
    return false;
}

std::map<std::string, std::map<std::string, std::string>> PluginManager::getAllPluginSettings() const {
    std::map<std::string, std::map<std::string, std::string>> allSettings;
    for (const auto& [name, data] : loadedPlugins) {
        allSettings[name] = data.settings;
    }
    return allSettings;
}

void PluginManager::registerEventCallback(const std::string& event, std::function<void(const std::string&)> callback) {
    eventCallbacks[event].push_back(callback);
}

void PluginManager::triggerEvent(const std::string& event, const std::string& data) {
    auto it = eventCallbacks.find(event);
    if (it != eventCallbacks.end()) {
        for (const auto& callback : it->second) {
            callback(data);
        }
    }
}
