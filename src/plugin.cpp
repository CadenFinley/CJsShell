#include "plugin.h"

/**
 * @brief Constructs a Plugin manager for dynamic plugin handling.
 *
 * Initializes the plugin manager with the specified plugins directory and immediately discovers available plugins.
 *
 * @param plugins_dir Path to the directory containing plugin shared libraries.
 */
Plugin::Plugin(const std::filesystem::path& plugins_dir) {
    plugins_directory = plugins_dir;
    plugins_discovered = discover_plugins();
}

/**
 * @brief Destructor that cleans up all loaded plugins and associated resources.
 *
 * Shuts down enabled plugins, destroys plugin instances, closes dynamic library handles, and clears the loaded plugins map.
 */
Plugin::~Plugin() {
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

/**
 * @brief Discovers and loads all plugins from the plugins directory.
 *
 * Scans the configured plugins directory for shared library files, unloads any currently loaded plugins, and loads all valid plugin files found. Marks plugins as discovered to avoid redundant loading.
 *
 * @return true if the plugins directory exists and discovery completes; false if the directory does not exist.
 */
bool Plugin::discover_plugins() {
    if (plugins_discovered && !loaded_plugins.empty()) {
        return true;
    }

    if (!std::filesystem::exists(plugins_directory)) {
        std::cerr << "Plugins directory does not exist: " << plugins_directory << std::endl;
        return false;
    }
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
    
    for (const auto& entry : std::filesystem::directory_iterator(plugins_directory)) {
        std::string file_name = entry.path().filename().string();
        if (entry.path().extension() == ".so" || entry.path().extension() == ".dylib") {
            load_plugin(entry.path());
        }
    }
    
    plugins_discovered = true;
    
    return true;
}

/**
 * @brief Loads a plugin shared library from the specified path and registers it if valid.
 *
 * Attempts to dynamically load a plugin from the given file path, verifies required symbols and interface version, creates an instance, and adds it to the loaded plugins map if not already present.
 *
 * @param path Filesystem path to the plugin shared library.
 * @return true if the plugin was successfully loaded and registered; false otherwise.
 */
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
    
    if (loaded_plugins.find(name) != loaded_plugins.end()) {
        std::cerr << "Plugin '" << name << "' is already loaded. Ignoring duplicate." << std::endl;
        destroy_func(instance);
        dlclose(handle);
        return false;
    }
    
    plugin_data data;
    data.handle = handle;
    data.instance = instance;
    data.create_func = create_func;
    data.destroy_func = destroy_func;
    data.enabled = false;
    data.settings = instance->get_default_settings();
    
    loaded_plugins[name] = std::move(data);
    
    return true;
}

/**
 * @brief Uninstalls a plugin by name, removing its file from the plugins directory.
 *
 * The plugin must be disabled before uninstallation. Searches for the plugin's shared library file, unloads the plugin, and deletes the file from disk.
 *
 * @param name The name of the plugin to uninstall.
 * @return true if the plugin was successfully uninstalled; false otherwise.
 */
bool Plugin::uninstall_plugin(const std::string& name) {
    auto it = loaded_plugins.find(name);
    if (it == loaded_plugins.end()) {
        std::cerr << "Plugin not found: " << name << std::endl;
        return false;
    }

    if (it->second.enabled) {
        std::cerr << "Please disable the plugin before uninstalling: " << name << std::endl;
        return false;
    }

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

/**
 * @brief Unloads a plugin by name, shutting it down if enabled and releasing all associated resources.
 *
 * If the plugin is enabled, it is first shut down. The plugin instance is destroyed, the dynamic library handle is closed, and the plugin is removed from the loaded plugins map. No action is taken if the plugin is not found.
 */
void Plugin::unload_plugin(const std::string& name) {
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

/**
 * @brief Returns the names of all currently loaded plugins.
 *
 * @return std::vector<std::string> List of loaded plugin names.
 */
std::vector<std::string> Plugin::get_available_plugins() const {
    std::vector<std::string> plugins;
    for (const auto& [name, _] : loaded_plugins) {
        plugins.push_back(name);
    }
    return plugins;
}

/**
 * @brief Returns a list of currently enabled plugin names.
 *
 * @return std::vector<std::string> Names of all enabled plugins.
 */
std::vector<std::string> Plugin::get_enabled_plugins() const {
    std::vector<std::string> plugins;
    for (const auto& [name, data] : loaded_plugins) {
        if (data.enabled) {
            plugins.push_back(name);
        }
    }
    return plugins;
}

/**
 * @brief Enables a loaded plugin by name.
 *
 * Initializes the specified plugin if it is not already enabled, marks it as enabled, triggers the "plugin_enabled" global event, and subscribes the plugin to its declared events.
 *
 * @param name The name of the plugin to enable.
 * @return true if the plugin was successfully enabled or was already enabled; false otherwise.
 */
bool Plugin::enable_plugin(const std::string& name) {
    auto it = loaded_plugins.find(name);
    if(it != loaded_plugins.end() && it->second.enabled){
        std::cout << "Plugin already enabled: " << name << std::endl;
        return true;
    }
    if (it != loaded_plugins.end() && !it->second.enabled) {
        if (it->second.instance->initialize()) {
            it->second.enabled = true;
            std::cout << "Enabled plugin: " << name << std::endl;
            trigger_subscribed_global_event("plugin_enabled", name);
            std::vector<std::string> events = it->second.instance->get_subscribed_events();
            if(!events.empty()){
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

/**
 * @brief Disables an enabled plugin by name.
 *
 * Shuts down the specified plugin if it is currently enabled, updates its status, triggers the "plugin_disabled" global event, and unsubscribes it from all events.
 *
 * @param name The name of the plugin to disable.
 * @return true if the plugin was successfully disabled; false if the plugin was not found or was already disabled.
 */
bool Plugin::disable_plugin(const std::string& name) {
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end() && it->second.enabled) {
        it->second.instance->shutdown();
        it->second.enabled = false;
        std::cout << "Disabled plugin: " << name << std::endl;
        trigger_subscribed_global_event("plugin_disabled", name);
        std::vector<std::string> events = it->second.instance->get_subscribed_events();
        for (const auto& event : events) {
            auto eventIt = subscribed_events.find(event);
            if (eventIt != subscribed_events.end()) {
                eventIt->second.erase(std::remove(eventIt->second.begin(), eventIt->second.end(), name), eventIt->second.end());
            }
        }
        return true;
    }
    return false;
}

/**
 * @brief Forwards a command and its arguments to the specified enabled plugin.
 *
 * If the targeted plugin is loaded and enabled, passes the argument queue to the plugin's command handler and returns the result. Returns false if the plugin is not found or not enabled.
 *
 * @param targetedPlugin Name of the plugin to receive the command.
 * @param args Queue of command arguments to be processed by the plugin.
 * @return true if the plugin handled the command successfully; false otherwise.
 */
bool Plugin::handle_plugin_command(const std::string& targetedPlugin, std::queue<std::string>& args) {
    auto it = loaded_plugins.find(targetedPlugin);
    if (it != loaded_plugins.end() && it->second.enabled) {
        return it->second.instance->handle_command(args);
    }
    return false;
}

/**
 * @brief Retrieves the list of commands supported by a specified plugin.
 *
 * @param name The name of the plugin.
 * @return std::vector<std::string> A list of command names, or an empty list if the plugin is not found.
 */
std::vector<std::string> Plugin::get_plugin_commands(const std::string& name) const {
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        return it->second.instance->get_commands();
    }
    return {};
}

/**
 * @brief Retrieves detailed information about a loaded plugin.
 *
 * Returns a formatted string containing the plugin's name, version, author, description, and enabled status. If the plugin is not found, returns a not found message.
 *
 * @param name The name of the plugin to query.
 * @return std::string Formatted plugin information or a not found message.
 */
std::string Plugin::get_plugin_info(const std::string& name) const {
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

/**
 * @brief Updates a specific setting for a loaded plugin.
 *
 * Modifies the internal settings map for the specified plugin and notifies the plugin instance of the updated setting.
 *
 * @param pluginName Name of the plugin to update.
 * @param key Setting key to update.
 * @param value New value for the setting.
 * @return true if the plugin was found and the setting was updated; false otherwise.
 */
bool Plugin::update_plugin_setting(const std::string& pluginName, const std::string& key, const std::string& value) {
    auto it = loaded_plugins.find(pluginName);
    if (it != loaded_plugins.end()) {
        it->second.settings[key] = value;
        it->second.instance->update_setting(key, value);
        return true;
    }
    return false;
}

/**
 * @brief Retrieves the settings for all loaded plugins.
 *
 * @return A map where each key is a plugin name and the value is a map of that plugin's settings.
 */
std::map<std::string, std::map<std::string, std::string>> Plugin::get_all_plugin_settings() const {
    std::map<std::string, std::map<std::string, std::string>> allSettings;
    for (const auto& [name, data] : loaded_plugins) {
        allSettings[name] = data.settings;
    }
    return allSettings;
}

/**
 * @brief Triggers a global event for all subscribed and enabled plugins.
 *
 * Invokes the command handler of each enabled plugin subscribed to the specified event, passing the event name and data as arguments.
 *
 * @param event Name of the global event to trigger.
 * @param eventData Data associated with the event.
 */
void Plugin::trigger_subscribed_global_event(const std::string& event, const std::string& eventData) {
    auto it = subscribed_events.find(event);
    if (it == subscribed_events.end() || it->second.empty()) {
        return;
    }
    std::queue<std::string> args;
    args.push("event");
    args.push(event);
    args.push(eventData);

    auto subscribedPlugins = it->second;
    for (const auto& pluginName : subscribedPlugins) {
        auto pluginIt = loaded_plugins.find(pluginName);
        if (pluginIt != loaded_plugins.end() && pluginIt->second.enabled) {
            std::queue<std::string> argsCopy = args;
            pluginIt->second.instance->handle_command(argsCopy);
        }
    }
}

/**
 * @brief Retrieves the instance pointer for a loaded plugin by name.
 *
 * @param name The name of the plugin.
 * @return PluginApi* Pointer to the plugin instance, or nullptr if the plugin is not loaded.
 */
PluginApi* Plugin::get_plugin_instance(const std::string& name) const {
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        return it->second.instance;
    }
    return nullptr;
}

/**
 * @brief Installs a new plugin from the specified file path.
 *
 * Validates the plugin file, checks for interface compatibility and duplicate installation, copies the plugin to the plugins directory, and loads it into the system. Cleans up and reports errors if installation fails at any step.
 *
 * @param sourcePath Path to the plugin shared library file (.so or .dylib) to install.
 * @return true if the plugin was successfully installed and loaded; false otherwise.
 */
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
    
    if (is_plugin_loaded(pluginName)) {
        std::cerr << "Plugin already installed: " << pluginName << std::endl;
        destroy_plugin_func destroyFunc = reinterpret_cast<destroy_plugin_func>(dlsym(tempHandle, "destroyPlugin"));
        if (destroyFunc) {
            destroyFunc(tempInstance);
        }
        dlclose(tempHandle);
        return false;
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

/**
 * @brief Resets the plugin discovery state to force rediscovery on the next operation.
 *
 * Marks the internal flag so that plugins will be rediscovered and reloaded when required.
 */
void Plugin::clear_plugin_cache() {
    plugins_discovered = false;
}

/**
 * @brief Checks if a plugin with the given name is currently loaded.
 *
 * @param name The name of the plugin to check.
 * @return true if the plugin is loaded, false otherwise.
 */
bool Plugin::is_plugin_loaded(const std::string& name) const {
    return loaded_plugins.find(name) != loaded_plugins.end();
}
