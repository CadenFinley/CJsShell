#include "plugin.h"

Plugin::Plugin(const std::filesystem::path& plugins_dir) {
    plugins_directory = plugins_dir;
    plugins_discovered = discover_plugins();
}

Plugin::~Plugin() {
    std::unique_lock lock(plugins_mutex);
    for (auto& [name, data] : loaded_plugins) {
        if (data.enabled && data.instance) {
            data.instance->shutdown();
        }
        if (data.instance && data.destroy_func) {
            data.destroy_func(data.instance);
        }
        if (data.handle) {
            dlclose(data.handle);
        }
    }
    loaded_plugins.clear();
}

bool Plugin::discover_plugins() {
    std::unique_lock discovery_lock(discovery_mutex);
    
    {
        std::shared_lock plugins_lock(plugins_mutex);
        if (plugins_discovered && !loaded_plugins.empty()) {
            return true;
        }
    }

    if (!std::filesystem::exists(plugins_directory)) {
        std::cerr << "Plugins directory does not exist: " << plugins_directory << std::endl;
        return false;
    }
    
    {
        std::unique_lock plugins_lock(plugins_mutex);
        for (auto& [name, data] : loaded_plugins) {
            if (data.enabled && data.instance) {
                data.instance->shutdown();
            }
            if (data.instance && data.destroy_func) {
                data.destroy_func(data.instance);
            }
            if (data.handle) {
                dlclose(data.handle);
            }
        }
        loaded_plugins.clear();
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(plugins_directory)) {
        std::string file_name = entry.path().filename().string();
        if (entry.path().extension() == ".so" || entry.path().extension() == ".dylib") {
            load_plugin(entry.path());
        }
    }
    
    plugins_discovered = true;
    
    return true;
}

bool Plugin::load_plugin(const std::filesystem::path& path) {
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load plugin: " << path << " - " << dlerror() << std::endl;
        return false;
    }
    
    dlerror();
    
    create_plugin_func create_func = reinterpret_cast<create_plugin_func>(dlsym(handle, "createPlugin"));
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol 'createPlugin': " << dlsym_error << std::endl;
        dlclose(handle);
        return false;
    }

    destroy_plugin_func destroy_func = reinterpret_cast<destroy_plugin_func>(dlsym(handle, "destroyPlugin"));
    dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol 'destroyPlugin': " << dlsym_error << std::endl;
        dlclose(handle);
        return false;
    }
    
    PluginApi* instance = create_func();
    if (!instance) {
        std::cerr << "Failed to create plugin instance" << std::endl;
        dlclose(handle);
        return false;
    }
    
    if (instance->get_interface_version() != PluginApi::INTERFACE_VERSION) {
        std::cerr << "Plugin interface version mismatch for " << instance->get_name() << ". Expected: " << PluginApi::INTERFACE_VERSION << ", Got: " << instance->get_interface_version() << std::endl;
        destroy_func(instance);
        dlclose(handle);
        return false;
    }
    
    std::string name = instance->get_name();
    
    {
        std::shared_lock plugins_lock(plugins_mutex);
        if (loaded_plugins.find(name) != loaded_plugins.end()) {
            std::cerr << "Plugin '" << name << "' is already loaded. Ignoring duplicate." << std::endl;
            destroy_func(instance);
            dlclose(handle);
            return false;
        }
    }
    
    plugin_data data;
    data.handle = handle;
    data.instance = instance;
    data.create_func = create_func;
    data.destroy_func = destroy_func;
    data.enabled = false;
    data.settings = instance->get_default_settings();
    
    {
        std::unique_lock plugins_lock(plugins_mutex);
        loaded_plugins[name] = std::move(data);
    }
    
    return true;
}

bool Plugin::uninstall_plugin(const std::string& name) {
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it == loaded_plugins.end()) {
        std::cerr << "Plugin not found: " << name << std::endl;
        return false;
    }

    if (it->second.enabled) {
        std::cerr << "Please disable the plugin before uninstalling: " << name << std::endl;
        return false;
    }
    plugins_lock.unlock();

    std::filesystem::path plugin_path;
    for (const auto& entry : std::filesystem::directory_iterator(plugins_directory)) {
        if (entry.path().extension() != ".so" && entry.path().extension() != ".dylib") {
            continue;
        }
        
        void* temp_handle = dlopen(entry.path().c_str(), RTLD_LAZY);
        if (!temp_handle) {
            continue;
        }

        create_plugin_func create_func = (create_plugin_func)dlsym(temp_handle, "createPlugin");
        if (!create_func) {
            dlclose(temp_handle);
            continue;
        }

        PluginApi* temp_instance = create_func();
        if (temp_instance && temp_instance->get_name() == name) {
            plugin_path = entry.path();
            dlclose(temp_handle);
            break;
        }
        dlclose(temp_handle);
    }

    if (plugin_path.empty()) {
        std::cerr << "Could not find plugin file for: " << name << std::endl;
        return false;
    }

    try {
        unload_plugin(name);
        std::filesystem::remove(plugin_path);
        std::cout << "Successfully uninstalled plugin: " << name << std::endl;
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to remove plugin file: " << e.what() << std::endl;
        return false;
    }
}

void Plugin::unload_plugin(const std::string& name) {
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        if (it->second.enabled) {
            it->second.instance->shutdown();
        }
        
        if (it->second.instance && it->second.destroy_func) {
            it->second.destroy_func(it->second.instance);
        }
        
        if (it->second.handle) {
            dlclose(it->second.handle);
        }
        
        loaded_plugins.erase(it);
    }
}

std::vector<std::string> Plugin::get_available_plugins() const {
    std::shared_lock plugins_lock(plugins_mutex);
    std::vector<std::string> plugins;
    for (const auto& [name, _] : loaded_plugins) {
        plugins.push_back(name);
    }
    return plugins;
}

std::vector<std::string> Plugin::get_enabled_plugins() const {
    std::shared_lock plugins_lock(plugins_mutex);
    std::vector<std::string> plugins;
    for (const auto& [name, data] : loaded_plugins) {
        if (data.enabled) {
            plugins.push_back(name);
        }
    }
    return plugins;
}

bool Plugin::enable_plugin(const std::string& name) {
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if(it != loaded_plugins.end() && it->second.enabled){
        std::cout << "Plugin already enabled: " << name << std::endl;
        return true;
    }
    if (it != loaded_plugins.end() && !it->second.enabled) {
        if (it->second.instance->initialize()) {
            it->second.enabled = true;
            std::cout << "Enabled plugin: " << name << std::endl;
            
            std::vector<std::string> events = it->second.instance->get_subscribed_events();
            plugins_lock.unlock();
            
            trigger_subscribed_global_event("plugin_enabled", name);
            
            if(!events.empty()){
                std::unique_lock events_lock(events_mutex);
                for (const auto& event : events) {
                    subscribed_events[event].push_back(name);
                }
            }
            return true;
        } else {
            std::cerr << "Failed to initialize plugin: " << name << std::endl;
        }
    }
    return false;
}

bool Plugin::disable_plugin(const std::string& name) {
    std::vector<std::string> events;
    
    {
        std::unique_lock plugins_lock(plugins_mutex);
        auto it = loaded_plugins.find(name);
        if (it != loaded_plugins.end() && it->second.enabled) {
            it->second.instance->shutdown();
            it->second.enabled = false;
            std::cout << "Disabled plugin: " << name << std::endl;
            events = it->second.instance->get_subscribed_events();
        } else {
            return false;
        }
    }
    
    trigger_subscribed_global_event("plugin_disabled", name);
    
    {
        std::unique_lock events_lock(events_mutex);
        for (const auto& event : events) {
            auto eventIt = subscribed_events.find(event);
            if (eventIt != subscribed_events.end()) {
                eventIt->second.erase(std::remove(eventIt->second.begin(), eventIt->second.end(), name), eventIt->second.end());
            }
        }
    }
    
    return true;
}

bool Plugin::handle_plugin_command(const std::string& targetedPlugin, std::queue<std::string>& args) {
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(targetedPlugin);
    if (it != loaded_plugins.end() && it->second.enabled) {
        return it->second.instance->handle_command(args);
    }
    return false;
}

std::vector<std::string> Plugin::get_plugin_commands(const std::string& name) const {
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        return it->second.instance->get_commands();
    }
    return {};
}

std::string Plugin::get_plugin_info(const std::string& name) const {
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        const auto& data = it->second;
        return "Name: " + name + "\n" +
               "Version: " + data.instance->get_version() + "\n" +
               "Author: " + data.instance->get_author() + "\n" +
               "Description: " + data.instance->get_description() + "\n" +
               "Status: " + (data.enabled ? "Enabled" : "Disabled");
    }
    return "Plugin not found: " + name;
}

bool Plugin::update_plugin_setting(const std::string& pluginName, const std::string& key, const std::string& value) {
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(pluginName);
    if (it != loaded_plugins.end()) {
        it->second.settings[key] = value;
        it->second.instance->update_setting(key, value);
        return true;
    }
    return false;
}

std::map<std::string, std::map<std::string, std::string>> Plugin::get_all_plugin_settings() const {
    std::shared_lock plugins_lock(plugins_mutex);
    std::map<std::string, std::map<std::string, std::string>> allSettings;
    for (const auto& [name, data] : loaded_plugins) {
        allSettings[name] = data.settings;
    }
    return allSettings;
}

void Plugin::trigger_subscribed_global_event(const std::string& event, const std::string& eventData) {
    std::vector<std::string> subscribedPlugins;
    
    {
        std::shared_lock events_lock(events_mutex);
        auto it = subscribed_events.find(event);
        if (it == subscribed_events.end() || it->second.empty()) {
            return;
        }
        subscribedPlugins = it->second;
    }
    
    std::queue<std::string> args;
    args.push("event");
    args.push(event);
    args.push(eventData);

    for (const auto& pluginName : subscribedPlugins) {
        std::shared_lock plugins_lock(plugins_mutex);
        auto pluginIt = loaded_plugins.find(pluginName);
        if (pluginIt != loaded_plugins.end() && pluginIt->second.enabled) {
            std::queue<std::string> argsCopy = args;
            plugins_lock.unlock();
            pluginIt->second.instance->handle_command(argsCopy);
        }
    }
}

PluginApi* Plugin::get_plugin_instance(const std::string& name) const {
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        return it->second.instance;
    }
    return nullptr;
}

bool Plugin::install_plugin(const std::filesystem::path& sourcePath) {
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
    
    create_plugin_func createFunc = reinterpret_cast<create_plugin_func>(dlsym(tempHandle, "createPlugin"));
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Invalid plugin file: missing createPlugin symbol: " << dlsym_error << std::endl;
        dlclose(tempHandle);
        return false;
    }

    PluginApi* tempInstance = nullptr;
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

    if (tempInstance->get_interface_version() != PluginApi::INTERFACE_VERSION) {
        std::cerr << "Plugin interface version mismatch for " << tempInstance->get_name() 
                  << ". Expected: " << PluginApi::INTERFACE_VERSION 
                  << ", Got: " << tempInstance->get_interface_version() << std::endl;
        destroy_plugin_func destroyFunc = reinterpret_cast<destroy_plugin_func>(dlsym(tempHandle, "destroyPlugin"));
        if (destroyFunc) {
            destroyFunc(tempInstance);
        }
        dlclose(tempHandle);
        return false;
    }

    std::string pluginName = tempInstance->get_name();
    std::string version = tempInstance->get_version();
    
    {
        std::shared_lock plugins_lock(plugins_mutex);
        if (is_plugin_loaded(pluginName)) {
            std::cerr << "Plugin already installed: " << pluginName << std::endl;
            destroy_plugin_func destroyFunc = reinterpret_cast<destroy_plugin_func>(dlsym(tempHandle, "destroyPlugin"));
            if (destroyFunc) {
                destroyFunc(tempInstance);
            }
            dlclose(tempHandle);
            return false;
        }
    }

    destroy_plugin_func destroyFunc = reinterpret_cast<destroy_plugin_func>(dlsym(tempHandle, "destroyPlugin"));
    if (destroyFunc) {
        destroyFunc(tempInstance);
    }
    dlclose(tempHandle);

    std::filesystem::path destPath = plugins_directory / sourcePath.filename();

    try {
        std::filesystem::copy(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing);
        
        if (load_plugin(destPath)) {
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

void Plugin::clear_plugin_cache() {
    std::unique_lock discovery_lock(discovery_mutex);
    plugins_discovered = false;
}

bool Plugin::is_plugin_loaded(const std::string& name) const {
    std::shared_lock plugins_lock(plugins_mutex);
    return loaded_plugins.find(name) != loaded_plugins.end();
}
