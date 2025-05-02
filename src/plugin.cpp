#include "plugin.h"
#include "main.h"
#include <sys/utsname.h>
#include <cstdio>
#include <cstring>

Plugin::Plugin(const std::filesystem::path& plugins_dir, bool enabled) {
    plugins_directory = plugins_dir;
    plugins_discovered = false;
    this->enabled = enabled;
    
    if (enabled) {
        plugins_discovered = discover_plugins();
    }
}

Plugin::~Plugin() {
    if (!enabled) {
        return;
    }
    
    std::unique_lock lock(plugins_mutex);
    for (auto& [name, data] : loaded_plugins) {
        if (data.enabled && data.shutdown) {
            data.shutdown();
        }
        if (data.handle) {
            dlclose(data.handle);
        }
    }
    loaded_plugins.clear();
}

bool Plugin::discover_plugins() {
    if (!enabled) {
        return false;
    }
    
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
            if (data.enabled && data.shutdown) {
                data.shutdown();
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
    if (!enabled) {
        std::cerr << "Plugin loading is disabled." << std::endl;
        return false;
    }
    
    std::string file_arch = get_file_architecture(path);
    std::string current_arch = get_current_architecture();
    
    if (!is_architecture_compatible(file_arch, current_arch)) {
        std::cerr << "Architecture mismatch for plugin: " << path.filename().string() 
                  << " (plugin: " << file_arch << ", system: " << current_arch << ")" << std::endl;
        return false;
    }
    
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load plugin: " << path << " - " << dlerror() << std::endl;
        return false;
    }
    
    dlerror();
    
    plugin_get_info_func get_info = reinterpret_cast<plugin_get_info_func>(dlsym(handle, "plugin_get_info"));
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol 'plugin_get_info': " << dlsym_error << std::endl;
        dlclose(handle);
        return false;
    }

    plugin_info_t* info = get_info();
    if (!info) {
        std::cerr << "Failed to get plugin info" << std::endl;
        dlclose(handle);
        return false;
    }
    
    if (info->interface_version != PLUGIN_INTERFACE_VERSION) {
        std::cerr << "Plugin interface version mismatch for " << info->name << ". Expected: " 
                  << PLUGIN_INTERFACE_VERSION << ", Got: " << info->interface_version << std::endl;
        dlclose(handle);
        return false;
    }
    
    std::string name = info->name;
    
    {
        std::shared_lock plugins_lock(plugins_mutex);
        if (loaded_plugins.find(name) != loaded_plugins.end()) {
            std::cerr << "Plugin '" << name << "' is already loaded. Ignoring duplicate." << std::endl;
            dlclose(handle);
            return false;
        }
    }
    
    plugin_data data;
    data.handle = handle;
    data.info = info;
    data.enabled = false;
    
    data.get_info = get_info;
    data.initialize = reinterpret_cast<plugin_initialize_func>(dlsym(handle, "plugin_initialize"));
    data.shutdown = reinterpret_cast<plugin_shutdown_func>(dlsym(handle, "plugin_shutdown"));
    data.handle_command = reinterpret_cast<plugin_handle_command_func>(dlsym(handle, "plugin_handle_command"));
    data.get_commands = reinterpret_cast<plugin_get_commands_func>(dlsym(handle, "plugin_get_commands"));
    data.get_subscribed_events = reinterpret_cast<plugin_get_subscribed_events_func>(dlsym(handle, "plugin_get_subscribed_events"));
    data.get_default_settings = reinterpret_cast<plugin_get_default_settings_func>(dlsym(handle, "plugin_get_default_settings"));
    data.update_setting = reinterpret_cast<plugin_update_setting_func>(dlsym(handle, "plugin_update_setting"));
    data.free_memory = reinterpret_cast<plugin_free_memory_func>(dlsym(handle, "plugin_free_memory"));
    
    if (!data.initialize || !data.shutdown || !data.handle_command || !data.get_commands) {
        std::cerr << "Plugin " << name << " is missing required functions" << std::endl;
        dlclose(handle);
        return false;
    }
    
    if (data.get_default_settings) {
        int count = 0;
        plugin_setting_t* settings = data.get_default_settings(&count);
        for (int i = 0; i < count; i++) {
            data.settings[settings[i].key] = settings[i].value;
        }
    }
    
    {
        std::unique_lock plugins_lock(plugins_mutex);
        loaded_plugins[name] = std::move(data);
    }
    
    return true;
}

bool Plugin::uninstall_plugin(const std::string& name) {
    if (!enabled) {
        return false;
    }
    
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

        plugin_get_info_func get_info = reinterpret_cast<plugin_get_info_func>(dlsym(temp_handle, "plugin_get_info"));
        if (!get_info) {
            dlclose(temp_handle);
            continue;
        }

        plugin_info_t* temp_info = get_info();
        if (temp_info && std::string(temp_info->name) == name) {
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
    if (!enabled) {
        return;
    }
    
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        if (it->second.enabled && it->second.shutdown) {
            it->second.shutdown();
        }
        
        if (it->second.handle) {
            dlclose(it->second.handle);
        }
        
        loaded_plugins.erase(it);
    }
}

std::vector<std::string> Plugin::get_available_plugins() const {
    if (!enabled) {
        return {};
    }
    
    std::shared_lock plugins_lock(plugins_mutex);
    std::vector<std::string> plugins;
    for (const auto& [name, _] : loaded_plugins) {
        plugins.push_back(name);
    }
    return plugins;
}

std::vector<std::string> Plugin::get_enabled_plugins() const {
    if (!enabled) {
        return {};
    }
    
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
    if (!enabled) {
        std::cerr << "Plugin system is disabled" << std::endl;
        return false;
    }
    
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if(it != loaded_plugins.end() && it->second.enabled){
        std::cout << "Plugin already enabled: " << name << std::endl;
        return true;
    }
    if (it != loaded_plugins.end() && !it->second.enabled) {
        if (it->second.initialize() == PLUGIN_SUCCESS) {
            it->second.enabled = true;
            std::cout << "Enabled plugin: " << name << std::endl;
            
            int count = 0;
            char** events = nullptr;
            if (it->second.get_subscribed_events) {
                events = it->second.get_subscribed_events(&count);
            }
            plugins_lock.unlock();
            
            trigger_subscribed_global_event("plugin_enabled", name);
            
            if(events && count > 0){
                std::unique_lock events_lock(events_mutex);
                for (int i = 0; i < count; i++) {
                    subscribed_events[events[i]].push_back(name);
                }
                if (it->second.free_memory) {
                    for (int i = 0; i < count; i++) {
                        it->second.free_memory(events[i]);
                    }
                    it->second.free_memory(events);
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
    if (!enabled) {
        return false;
    }
    
    std::vector<std::string> events_to_unsubscribe;
    
    {
        std::unique_lock plugins_lock(plugins_mutex);
        auto it = loaded_plugins.find(name);
        if (it != loaded_plugins.end() && it->second.enabled) {
            it->second.shutdown();
            it->second.enabled = false;
            std::cout << "Disabled plugin: " << name << std::endl;
            
            int count = 0;
            char** events = nullptr;
            if (it->second.get_subscribed_events) {
                events = it->second.get_subscribed_events(&count);
            }
            
            if (events && count > 0) {
                for (int i = 0; i < count; i++) {
                    events_to_unsubscribe.push_back(events[i]);
                }
                if (it->second.free_memory) {
                    for (int i = 0; i < count; i++) {
                        it->second.free_memory(events[i]);
                    }
                    it->second.free_memory(events);
                }
            }
        } else {
            return false;
        }
    }
    
    trigger_subscribed_global_event("plugin_disabled", name);
    
    {
        std::unique_lock events_lock(events_mutex);
        for (const auto& event : events_to_unsubscribe) {
            auto eventIt = subscribed_events.find(event);
            if (eventIt != subscribed_events.end()) {
                eventIt->second.erase(std::remove(eventIt->second.begin(), eventIt->second.end(), name), eventIt->second.end());
            }
        }
    }
    
    return true;
}

bool Plugin::handle_plugin_command(const std::string& targeted_plugin, std::vector<std::string>& args) {
    if (!enabled) {
        return false;
    }
    
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(targeted_plugin);
    if (it != loaded_plugins.end() && it->second.enabled) {
        plugin_args_t args_struct;
        args_struct.count = args.size();
        args_struct.args = new char*[args.size()];
        args_struct.position = 0;
        
        for (size_t i = 0; i < args.size(); i++) {
            args_struct.args[i] = strdup(args[i].c_str());
        }
        
        int result = it->second.handle_command(&args_struct);
        
        for (int i = 0; i < args_struct.count; i++) {
            free(args_struct.args[i]);
        }
        delete[] args_struct.args;
        
        return result == PLUGIN_SUCCESS;
    }
    return false;
}

std::vector<std::string> Plugin::get_plugin_commands(const std::string& name) const {
    if (!enabled) {
        return {};
    }
    
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end() && it->second.get_commands) {
        int count = 0;
        char** commands = it->second.get_commands(&count);
        
        std::vector<std::string> result;
        for (int i = 0; i < count; i++) {
            result.push_back(commands[i]);
        }
        
        if (it->second.free_memory) {
            for (int i = 0; i < count; i++) {
                it->second.free_memory(commands[i]);
            }
            it->second.free_memory(commands);
        }
        
        return result;
    }
    return {};
}

std::string Plugin::get_plugin_info(const std::string& name) const {
    if (!enabled) {
        return "Plugin system is disabled";
    }
    
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        const auto& data = it->second;
        return "Name: " + std::string(data.info->name) + "\n" +
               "Version: " + std::string(data.info->version) + "\n" +
               "Author: " + std::string(data.info->author) + "\n" +
               "Description: " + std::string(data.info->description) + "\n" +
               "Status: " + (data.enabled ? "Enabled" : "Disabled");
    }
    return "Plugin not found: " + name;
}

bool Plugin::update_plugin_setting(const std::string& plugin_name, const std::string& key, const std::string& value) {
    if (!enabled) {
        return false;
    }
    
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(plugin_name);
    if (it != loaded_plugins.end()) {
        it->second.settings[key] = value;
        if (it->second.update_setting) {
            return it->second.update_setting(key.c_str(), value.c_str()) == PLUGIN_SUCCESS;
        }
        return true;
    }
    return false;
}

std::map<std::string, std::map<std::string, std::string>> Plugin::get_all_plugin_settings() const {
    if (!enabled) {
        return {};
    }
    
    std::shared_lock plugins_lock(plugins_mutex);
    std::map<std::string, std::map<std::string, std::string>> allSettings;
    for (const auto& [name, data] : loaded_plugins) {
        allSettings[name] = data.settings;
    }
    return allSettings;
}

void Plugin::trigger_subscribed_global_event(const std::string& event, const std::string& event_data) {
    if (g_debug_mode) {
        std::cerr << "DEBUG: Triggering plugin event: " << event 
                  << " with data: " << event_data 
                  << " for " << get_enabled_plugins().size() << " plugins" 
                  << std::endl;
    }

    if (!enabled) {
        return;
    }
    
    std::vector<std::string> subscribedPlugins;
    
    {
        std::shared_lock events_lock(events_mutex);
        auto it = subscribed_events.find(event);
        if (it == subscribed_events.end() || it->second.empty()) {
            return;
        }
        subscribedPlugins = it->second;
    }
    
    plugin_args_t args;
    args.count = 3;
    args.args = new char*[3];
    args.position = 0;
    
    args.args[0] = strdup("event");
    args.args[1] = strdup(event.c_str());
    args.args[2] = strdup(event_data.c_str());

    if (g_debug_mode && args.count > 0) {
        std::cerr << "DEBUG: Created event args with " << args.count << " arguments" << std::endl;
    }

    for (const auto& plugin_name : subscribedPlugins) {
        std::shared_lock plugins_lock(plugins_mutex);
        auto plugin_it = loaded_plugins.find(plugin_name);
        if (plugin_it != loaded_plugins.end() && plugin_it->second.enabled) {
            plugins_lock.unlock();
            plugin_it->second.handle_command(&args);
        }
    }
    
    for (int i = 0; i < args.count; i++) {
        free(args.args[i]);
    }
    delete[] args.args;
}

plugin_data* Plugin::get_plugin_data(const std::string& name) {
    if (!enabled) {
        return nullptr;
    }
    
    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end()) {
        return &it->second;
    }
    return nullptr;
}

bool Plugin::install_plugin(const std::filesystem::path& source_path) {
    if (!enabled) {
        std::cerr << "Plugin system is disabled" << std::endl;
        return false;
    }
    
    if (!std::filesystem::exists(source_path)) {
        std::cerr << "Source plugin file does not exist: " << source_path << std::endl;
        return false;
    }

    std::string extension = source_path.extension().string();
    if (extension != ".so" && extension != ".dylib") {
        std::cerr << "Invalid plugin file type. Must be .so or .dylib" << std::endl;
        return false;
    }

    void* temp_handle = dlopen(source_path.c_str(), RTLD_LAZY);
    if (!temp_handle) {
        std::cerr << "Invalid plugin file: " << dlerror() << std::endl;
        return false;
    }

    dlerror();
    
    plugin_get_info_func get_info = reinterpret_cast<plugin_get_info_func>(dlsym(temp_handle, "plugin_get_info"));
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Invalid plugin file: missing plugin_get_info symbol: " << dlsym_error << std::endl;
        dlclose(temp_handle);
        return false;
    }

    plugin_info_t* temp_info = nullptr;
    try {
        temp_info = get_info();
    } catch (const std::exception& e) {
        std::cerr << "Exception while getting plugin info: " << e.what() << std::endl;
        dlclose(temp_handle);
        return false;
    }
    
    if (!temp_info) {
        std::cerr << "Failed to get plugin info" << std::endl;
        dlclose(temp_handle);
        return false;
    }

    if (temp_info->interface_version != PLUGIN_INTERFACE_VERSION) {
        std::cerr << "Plugin interface version mismatch for " << temp_info->name 
                  << ". Expected: " << PLUGIN_INTERFACE_VERSION 
                  << ", Got: " << temp_info->interface_version << std::endl;
        dlclose(temp_handle);
        return false;
    }

    std::string plugin_name = temp_info->name;
    std::string version = temp_info->version;
    
    {
        std::shared_lock plugins_lock(plugins_mutex);
        if (is_plugin_loaded(plugin_name)) {
            std::cerr << "Plugin already installed: " << plugin_name << std::endl;
            dlclose(temp_handle);
            return false;
        }
    }

    dlclose(temp_handle);

    std::filesystem::path dest_path = plugins_directory / source_path.filename();

    try {
        std::filesystem::copy(source_path, dest_path, std::filesystem::copy_options::overwrite_existing);
        
        if (load_plugin(dest_path)) {
            std::cout << "Successfully installed plugin: " << plugin_name << " v" << version << std::endl;
            return true;
        } else {
            std::filesystem::remove(dest_path);
            std::cerr << "Failed to load installed plugin" << std::endl;
            return false;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to install plugin: " << e.what() << std::endl;
        return false;
    }
}

void Plugin::clear_plugin_cache() {
    if (!enabled) {
        return;
    }
    
    std::unique_lock discovery_lock(discovery_mutex);
    plugins_discovered = false;
}

bool Plugin::is_plugin_loaded(const std::string& name) const {
    if (!enabled) {
        return false;
    }
    
    std::shared_lock plugins_lock(plugins_mutex);
    return loaded_plugins.find(name) != loaded_plugins.end();
}

std::string Plugin::get_current_architecture() const {
    struct utsname system_info;
    uname(&system_info);
    
    std::string arch = system_info.machine;
    
    if (arch == "x86_64" || arch == "amd64")
        return "x86_64";
    else if (arch == "arm64" || arch == "aarch64")
        return "arm64";
    
    return arch;
}

std::string Plugin::get_file_architecture(const std::filesystem::path& path) const {
    std::string result = "unknown";
    
    std::string cmd = "file -b " + path.string();
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return result;
    }
    
    char buffer[128];
    std::string output = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL) {
            output += buffer;
        }
    }
    pclose(pipe);
    
    if (output.find("x86_64") != std::string::npos) {
        result = "x86_64";
    } else if (output.find("arm64") != std::string::npos || output.find("ARM64") != std::string::npos || 
              output.find("aarch64") != std::string::npos) {
        result = "arm64";
    }
    
    return result;
}

bool Plugin::is_architecture_compatible(const std::string& file_arch, const std::string& current_arch) const {
    if (file_arch == current_arch)
        return true;
    
    #ifdef __APPLE__
    if (current_arch == "arm64" && file_arch == "x86_64") {
        int rosetta_check = system("arch -x86_64 true 2>/dev/null");
        return rosetta_check == 0;
    }
    #endif
    
    return false;
}
