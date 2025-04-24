#include "include/pluginmanager.h"

PluginManager::PluginManager(const std::filesystem::path& pluginsDir) 
    : pluginsDirectory(pluginsDir), pluginsDiscovered(false) {

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
    if (pluginsDiscovered && !loadedPlugins.empty()) {
        return true;
    }

    if (!std::filesystem::exists(pluginsDirectory)) {
        std::cerr << "Plugins directory does not exist: " << pluginsDirectory << std::endl;
        return false;
    }
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
    
    for (const auto& entry : std::filesystem::directory_iterator(pluginsDirectory)) {
        std::string fileName = entry.path().filename().string();
        if (entry.path().extension() == ".so" || entry.path().extension() == ".dylib") {
            loadPlugin(entry.path());
        }
    }
    
    pluginsDiscovered = true;
    
    
    
    
    
    
    
    
    
    
    
    
    
    return true;
}

bool PluginManager::loadPlugin(const std::filesystem::path& path) {
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load plugin: " << path << " - " << dlerror() << std::endl;
        return false;
    }
    
    dlerror();
    
    CreatePluginFunc createFunc = reinterpret_cast<CreatePluginFunc>(dlsym(handle, "createPlugin"));
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol 'createPlugin': " << dlsym_error << std::endl;
        dlclose(handle);
        return false;
    }

    DestroyPluginFunc destroyFunc = reinterpret_cast<DestroyPluginFunc>(dlsym(handle, "destroyPlugin"));
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
    
    if (instance->getInterfaceVersion() != PluginInterface::INTERFACE_VERSION) {
        std::cerr << "Plugin interface version mismatch for " << instance->getName() << ". Expected: " << PluginInterface::INTERFACE_VERSION << ", Got: " << instance->getInterfaceVersion() << std::endl;
        destroyFunc(instance);
        dlclose(handle);
        return false;
    }
    
    std::string name = instance->getName();
    
    if (loadedPlugins.find(name) != loadedPlugins.end()) {
        std::cerr << "Plugin '" << name << "' is already loaded. Ignoring duplicate." << std::endl;
        destroyFunc(instance);
        dlclose(handle);
        return false;
    }
    
    PluginData data;
    data.handle = handle;
    data.instance = instance;
    data.createFunc = createFunc;
    data.destroyFunc = destroyFunc;
    data.enabled = false;
    data.settings = instance->getDefaultSettings();
    
    loadedPlugins[name] = std::move(data);
    
    return true;
}

bool PluginManager::uninstallPlugin(const std::string& name) {
    auto it = loadedPlugins.find(name);
    if (it == loadedPlugins.end()) {
        std::cerr << "Plugin not found: " << name << std::endl;
        return false;
    }

    if (it->second.enabled) {
        std::cerr << "Please disable the plugin before uninstalling: " << name << std::endl;
        return false;
    }

    std::filesystem::path pluginPath;
    for (const auto& entry : std::filesystem::directory_iterator(pluginsDirectory)) {
        if (entry.path().extension() != ".so" && entry.path().extension() != ".dylib") {
            continue;
        }
        
        void* tempHandle = dlopen(entry.path().c_str(), RTLD_LAZY);
        if (!tempHandle) {
            continue;
        }

        CreatePluginFunc createFunc = (CreatePluginFunc)dlsym(tempHandle, "createPlugin");
        if (!createFunc) {
            dlclose(tempHandle);
            continue;
        }

        PluginInterface* tempInstance = createFunc();
        if (tempInstance && tempInstance->getName() == name) {
            pluginPath = entry.path();
            dlclose(tempHandle);
            break;
        }
        dlclose(tempHandle);
    }

    if (pluginPath.empty()) {
        std::cerr << "Could not find plugin file for: " << name << std::endl;
        return false;
    }

    try {
        unloadPlugin(name);
        std::filesystem::remove(pluginPath);
        std::cout << "Successfully uninstalled plugin: " << name << std::endl;
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to remove plugin file: " << e.what() << std::endl;
        return false;
    }
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
    if(it != loadedPlugins.end() && it->second.enabled){
        std::cout << "Plugin already enabled: " << name << std::endl;
        return true;
    }
    if (it != loadedPlugins.end() && !it->second.enabled) {
        if (it->second.instance->initialize()) {
            it->second.enabled = true;
            std::cout << "Enabled plugin: " << name << std::endl;
            triggerSubscribedGlobalEvent("plugin_enabled", name);
            std::vector<std::string> events = it->second.instance->getSubscribedEvents();
            if(!events.empty()){
                for (const auto& event : events) {
                    subscribedEvents[event].push_back(name);
                }
            }
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
        triggerSubscribedGlobalEvent("plugin_disabled", name);
        std::vector<std::string> events = it->second.instance->getSubscribedEvents();
        for (const auto& event : events) {
            auto eventIt = subscribedEvents.find(event);
            if (eventIt != subscribedEvents.end()) {
                eventIt->second.erase(std::remove(eventIt->second.begin(), eventIt->second.end(), name), eventIt->second.end());
            }
        }
        return true;
    }
    return false;
}

bool PluginManager::handlePluginCommand(const std::string& targetedPlugin, std::queue<std::string>& args) {
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

void PluginManager::triggerEvent(const std::string& targetPlugin, const std::string& event, const std::string& data) {
    auto it = loadedPlugins.find(targetPlugin);
    if (it != loadedPlugins.end() && it->second.enabled) {
        std::queue<std::string> args;
        args.push("event");
        args.push(event);
        args.push(data);
        it->second.instance->handleCommand(args);
    }
}

void PluginManager::triggerSubscribedGlobalEvent(const std::string& event, const std::string& eventData) {
    auto it = subscribedEvents.find(event);
    if (it == subscribedEvents.end() || it->second.empty()) {
        return;
    }
    std::queue<std::string> args;
    args.push("event");
    args.push(event);
    args.push(eventData);

    auto subscribedPlugins = it->second;
    for (const auto& pluginName : subscribedPlugins) {
        auto pluginIt = loadedPlugins.find(pluginName);
        if (pluginIt != loadedPlugins.end() && pluginIt->second.enabled) {
            std::queue<std::string> argsCopy = args;
            pluginIt->second.instance->handleCommand(argsCopy);
        }
    }
}

PluginInterface* PluginManager::getPluginInstance(const std::string& name) const {
    auto it = loadedPlugins.find(name);
    if (it != loadedPlugins.end()) {
        return it->second.instance;
    }
    return nullptr;
}

bool PluginManager::installPlugin(const std::filesystem::path& sourcePath) {
    if (!std::filesystem::exists(sourcePath)) {
        std::cerr << "Source plugin file does not exist: " << sourcePath << std::endl;
        return false;
    }

    std::string extension = sourcePath.extension().string();
    if (extension != ".so" && extension != ".dylib") {
        std::cerr << "Invalid plugin file type. Must be .so or .dylib" << std::endl;
        return false;
    }

    void* tempHandle = dlopen(sourcePath.c_str(), RTLD_LAZY);
    if (!tempHandle) {
        std::cerr << "Invalid plugin file: " << dlerror() << std::endl;
        return false;
    }

    dlerror();
    
    CreatePluginFunc createFunc = reinterpret_cast<CreatePluginFunc>(dlsym(tempHandle, "createPlugin"));
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Invalid plugin file: missing createPlugin symbol: " << dlsym_error << std::endl;
        dlclose(tempHandle);
        return false;
    }

    PluginInterface* tempInstance = nullptr;
    try {
        tempInstance = createFunc();
    } catch (const std::exception& e) {
        std::cerr << "Exception while creating plugin instance: " << e.what() << std::endl;
        dlclose(tempHandle);
        return false;
    }
    
    if (!tempInstance) {
        std::cerr << "Failed to create temporary plugin instance" << std::endl;
        dlclose(tempHandle);
        return false;
    }

    if (tempInstance->getInterfaceVersion() != PluginInterface::INTERFACE_VERSION) {
        std::cerr << "Plugin interface version mismatch for " << tempInstance->getName() 
                  << ". Expected: " << PluginInterface::INTERFACE_VERSION 
                  << ", Got: " << tempInstance->getInterfaceVersion() << std::endl;
        DestroyPluginFunc destroyFunc = reinterpret_cast<DestroyPluginFunc>(dlsym(tempHandle, "destroyPlugin"));
        if (destroyFunc) {
            destroyFunc(tempInstance);
        }
        dlclose(tempHandle);
        return false;
    }

    std::string pluginName = tempInstance->getName();
    std::string version = tempInstance->getVersion();
    
    if (isPluginLoaded(pluginName)) {
        std::cerr << "Plugin already installed: " << pluginName << std::endl;
        DestroyPluginFunc destroyFunc = reinterpret_cast<DestroyPluginFunc>(dlsym(tempHandle, "destroyPlugin"));
        if (destroyFunc) {
            destroyFunc(tempInstance);
        }
        dlclose(tempHandle);
        return false;
    }

    DestroyPluginFunc destroyFunc = reinterpret_cast<DestroyPluginFunc>(dlsym(tempHandle, "destroyPlugin"));
    if (destroyFunc) {
        destroyFunc(tempInstance);
    }
    dlclose(tempHandle);

    std::filesystem::path destPath = pluginsDirectory / sourcePath.filename();

    try {
        std::filesystem::copy(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing);
        
        if (loadPlugin(destPath)) {
            std::cout << "Successfully installed plugin: " << pluginName << " v" << version << std::endl;
            return true;
        } else {
            std::filesystem::remove(destPath);
            std::cerr << "Failed to load installed plugin" << std::endl;
            return false;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to install plugin: " << e.what() << std::endl;
        return false;
    }
}

void PluginManager::clearPluginCache() {
    pluginsDiscovered = false;
}

bool PluginManager::isPluginLoaded(const std::string& name) const {
    return loadedPlugins.find(name) != loadedPlugins.end();
}
