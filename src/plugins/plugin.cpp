#include "plugin.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

#include <dlfcn.h>
#include <sys/utsname.h>

#include "cjsh.h"
#include "error_out.h"

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

static thread_local std::string current_plugin_context;

extern "C" PLUGIN_API plugin_error_t
plugin_register_prompt_variable(const char* name, plugin_get_prompt_variable_func func) {
    if (current_plugin_context.empty() || !g_plugin) {
        return PLUGIN_ERROR_GENERAL;
    }
    plugin_data* pd = g_plugin->get_plugin_data(current_plugin_context);
    if (!pd) {
        return PLUGIN_ERROR_GENERAL;
    }
    pd->prompt_variables[std::string(name)] = func;
    return PLUGIN_SUCCESS;
}

extern "C" PLUGIN_API char* plugin_safe_strdup(const char* src) {
    if (!src)
        return nullptr;
    size_t len = strlen(src);
    char* copy = static_cast<char*>(malloc(len + 1));
    if (copy) {
        memcpy(copy, src, len + 1);
    }
    return copy;
}

extern "C" PLUGIN_API plugin_string_t plugin_create_string(const char* src) {
    plugin_string_t result = {nullptr, 0, 0};
    if (!src)
        return result;

    size_t len = strlen(src);
    result.data = static_cast<char*>(malloc(len + 1));
    if (result.data) {
        memcpy(result.data, src, len + 1);
        result.length = static_cast<int>(len);
        result.capacity = static_cast<int>(len + 1);
    }
    return result;
}

extern "C" PLUGIN_API void plugin_free_plugin_string(plugin_string_t* str) {
    if (str && str->data) {
        free(str->data);
        str->data = nullptr;
        str->length = 0;
        str->capacity = 0;
    }
}

Plugin::Plugin(const std::filesystem::path& plugins_dir, bool enabled, bool lazy_loading) {
    plugins_directory = plugins_dir;
    plugins_discovered = false;
    this->enabled = enabled;
    this->lazy_loading_enabled = lazy_loading;

    if (enabled) {
        if (lazy_loading_enabled) {
            cache_plugin_metadata();
            plugins_discovered = true;
        } else {
            plugins_discovered = discover_plugins();
        }
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

    {
        std::unique_lock events_lock(events_mutex);
        subscribed_events.clear();
    }
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
        std::cerr << "cjsh: plugin: discover_plugins: Plugins directory does "
                     "not exist: "
                  << plugins_directory << std::endl;
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
        print_error({ErrorType::RUNTIME_ERROR, "plugin", "Plugin loading is disabled", {}});
        return false;
    }

    std::string file_arch = get_file_architecture(path);
    std::string current_arch = get_current_architecture();

    if (!is_architecture_compatible(file_arch, current_arch)) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "plugin",
                     "Architecture mismatch for plugin: " + path.filename().string() +
                         " (plugin: " + file_arch + ", system: " + current_arch + ")",
                     {}});
        return false;
    }

    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "plugin",
                     "Failed to load plugin: " + path.string() + " - " + std::string(dlerror()),
                     {}});
        return false;
    }

    dlerror();

    plugin_get_info_func get_info =
        reinterpret_cast<plugin_get_info_func>(dlsym(handle, "plugin_get_info"));
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "plugin",
                     "Cannot load symbol 'plugin_get_info': " + std::string(dlsym_error),
                     {}});
        dlclose(handle);
        return false;
    }

    plugin_info_t* info = get_info();
    if (!info) {
        print_error({ErrorType::RUNTIME_ERROR, "plugin", "Failed to get plugin info", {}});
        dlclose(handle);
        return false;
    }

    if (info->interface_version != PLUGIN_INTERFACE_VERSION) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "plugin",
                     "Plugin interface version mismatch for " + std::string(info->name) +
                         ". Expected: " + std::to_string(PLUGIN_INTERFACE_VERSION) +
                         ", Got: " + std::to_string(info->interface_version),
                     {}});
        dlclose(handle);
        return false;
    }

    std::string name = info->name;

    {
        std::shared_lock plugins_lock(plugins_mutex);
        if (loaded_plugins.find(name) != loaded_plugins.end()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "plugin",
                         "Plugin '" + name + "' is already loaded. Ignoring duplicate",
                         {}});
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
    data.handle_command =
        reinterpret_cast<plugin_handle_command_func>(dlsym(handle, "plugin_handle_command"));
    data.get_commands =
        reinterpret_cast<plugin_get_commands_func>(dlsym(handle, "plugin_get_commands"));
    data.get_subscribed_events = reinterpret_cast<plugin_get_subscribed_events_func>(
        dlsym(handle, "plugin_get_subscribed_events"));
    data.get_default_settings = reinterpret_cast<plugin_get_default_settings_func>(
        dlsym(handle, "plugin_get_default_settings"));
    data.update_setting =
        reinterpret_cast<plugin_update_setting_func>(dlsym(handle, "plugin_update_setting"));
    data.free_memory =
        reinterpret_cast<plugin_free_memory_func>(dlsym(handle, "plugin_free_memory"));
    data.validate = reinterpret_cast<plugin_validate_func>(dlsym(handle, "plugin_validate"));

    if (!data.initialize || !data.shutdown || !data.handle_command || !data.get_commands ||
        !data.free_memory) {
        std::cerr << "Plugin " << name << " is missing required functions" << std::endl;
        dlclose(handle);
        return false;
    }

    if (data.get_default_settings) {
        int count = 0;
        plugin_setting_t* settings = data.get_default_settings(&count);

        if (settings && count > 0) {
            for (int i = 0; i < count; i++) {
                if (settings[i].key && settings[i].value) {
                    data.settings[settings[i].key] = settings[i].value;
                }
            }

            if (data.free_memory) {
                for (int i = 0; i < count; i++) {
                    if (settings[i].key) {
                        data.free_memory(settings[i].key);
                    }
                    if (settings[i].value) {
                        data.free_memory(settings[i].value);
                    }
                }
                data.free_memory(settings);
            }
        }
    }

    if (data.validate) {
        plugin_validation_t validation_result = data.validate();
        if (validation_result.status != PLUGIN_SUCCESS) {
            std::string error_msg = "Plugin validation failed";
            if (validation_result.error_message) {
                error_msg += ": " + std::string(validation_result.error_message);

                if (data.free_memory) {
                    data.free_memory(validation_result.error_message);
                }
            }
            std::cerr << "Plugin loading failed: " << name << ": " << error_msg << std::endl;
            dlclose(handle);
            return false;
        }
    }

    {
        std::unique_lock plugins_lock(plugins_mutex);
        loaded_plugins[name] = std::move(data);
    }
    return true;
}

bool Plugin::extract_plugin_metadata(const std::filesystem::path& path, plugin_metadata& metadata) {
    metadata.library_path = path;
    metadata.last_modified = std::filesystem::last_write_time(path).time_since_epoch().count();
    metadata.is_loaded = false;
    metadata.load_failed = false;

    void* temp_handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!temp_handle) {
        metadata.load_failed = true;
        return false;
    }

    plugin_get_info_func get_info =
        reinterpret_cast<plugin_get_info_func>(dlsym(temp_handle, "plugin_get_info"));
    if (!get_info) {
        dlclose(temp_handle);
        metadata.load_failed = true;
        return false;
    }

    plugin_info_t* info = get_info();
    if (!info) {
        dlclose(temp_handle);
        metadata.load_failed = true;
        return false;
    }

    metadata.name = info->name;
    metadata.version = info->version;
    metadata.description = info->description;
    metadata.author = info->author;

    plugin_free_memory_func free_memory =
        reinterpret_cast<plugin_free_memory_func>(dlsym(temp_handle, "plugin_free_memory"));

    auto release_string_array = [&](char** entries, int entry_count) {
        if (!entries || entry_count <= 0) {
            return;
        }
        if (free_memory) {
            for (int i = 0; i < entry_count; i++) {
                if (entries[i]) {
                    free_memory(entries[i]);
                }
            }
            free_memory(entries);
            return;
        }
        for (int i = 0; i < entry_count; i++) {
            if (entries[i]) {
                free(entries[i]);
            }
        }
        free(entries);
    };

    plugin_get_commands_func get_commands =
        reinterpret_cast<plugin_get_commands_func>(dlsym(temp_handle, "plugin_get_commands"));
    if (get_commands) {
        int count = 0;
        char** commands = get_commands(&count);
        if (commands && count > 0) {
            metadata.commands.clear();
            for (int i = 0; i < count; i++) {
                if (commands[i]) {
                    metadata.commands.push_back(commands[i]);
                }
            }
            release_string_array(commands, count);
        }
    }

    plugin_get_subscribed_events_func get_events =
        reinterpret_cast<plugin_get_subscribed_events_func>(
            dlsym(temp_handle, "plugin_get_subscribed_events"));
    if (get_events) {
        int count = 0;
        char** events = get_events(&count);
        if (events && count > 0) {
            metadata.events.clear();
            for (int i = 0; i < count; i++) {
                if (events[i]) {
                    metadata.events.push_back(events[i]);
                }
            }
            release_string_array(events, count);
        }
    }

    dlclose(temp_handle);

    return true;
}

bool Plugin::load_plugin_on_demand(const std::string& name) {
    {
        std::shared_lock plugins_lock(plugins_mutex);
        if (loaded_plugins.find(name) != loaded_plugins.end()) {
            return true;
        }
    }

    std::shared_lock metadata_lock(metadata_mutex);
    auto metadata_it = plugin_metadata_cache.find(name);
    if (metadata_it == plugin_metadata_cache.end()) {
        return false;
    }

    std::filesystem::path library_path = metadata_it->second.library_path;
    metadata_lock.unlock();

    bool result = load_plugin(library_path);

    if (result) {
        std::unique_lock metadata_lock(metadata_mutex);
        auto& metadata = plugin_metadata_cache[name];
        metadata.is_loaded = true;
        metadata.load_failed = false;
    }

    return result;
}

bool Plugin::is_metadata_stale(const plugin_metadata& metadata) const {
    if (!std::filesystem::exists(metadata.library_path)) {
        return true;
    }

    auto current_mtime =
        std::filesystem::last_write_time(metadata.library_path).time_since_epoch().count();
    return current_mtime != metadata.last_modified;
}

void Plugin::cache_plugin_metadata() {
    if (!enabled) {
        return;
    }

    if (!std::filesystem::exists(plugins_directory)) {
        return;
    }

    std::unique_lock metadata_lock(metadata_mutex);

    for (const auto& entry : std::filesystem::directory_iterator(plugins_directory)) {
        if (entry.path().extension() == ".so" || entry.path().extension() == ".dylib") {
            std::string filename = entry.path().filename().string();

            auto existing = plugin_metadata_cache.find(filename);
            if (existing != plugin_metadata_cache.end()) {
                if (!is_metadata_stale(existing->second)) {
                    continue;
                }
            }

            plugin_metadata metadata;
            if (extract_plugin_metadata(entry.path(), metadata)) {
                plugin_metadata_cache[metadata.name] = metadata;
            }
        }
    }
}

bool Plugin::uninstall_plugin(const std::string& name) {
    if (!enabled) {
        return false;
    }

    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it == loaded_plugins.end()) {
        print_error({ErrorType::COMMAND_NOT_FOUND, "plugin", "Plugin not found: " + name, {}});
        return false;
    }

    if (it->second.enabled) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "plugin",
                     "Please disable the plugin before uninstalling: " + name,
                     {}});
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

        plugin_get_info_func get_info =
            reinterpret_cast<plugin_get_info_func>(dlsym(temp_handle, "plugin_get_info"));
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
        std::cerr << "cjsh: plugin: uninstall_plugin: Could not find plugin "
                     "file for: "
                  << name << std::endl;
        return false;
    }

    try {
        unload_plugin(name);
        std::filesystem::remove(plugin_path);
        std::cout << "Successfully uninstalled plugin: " << name << std::endl;
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "cjsh: plugin: uninstall_plugin: Failed to remove plugin file: " << e.what()
                  << std::endl;
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

    std::vector<std::string> plugins;

    if (lazy_loading_enabled) {
        std::shared_lock metadata_lock(metadata_mutex);
        for (const auto& [name, metadata] : plugin_metadata_cache) {
            if (!metadata.load_failed) {
                plugins.push_back(name);
            }
        }
    } else {
        std::shared_lock plugins_lock(plugins_mutex);
        for (const auto& [name, _] : loaded_plugins) {
            plugins.push_back(name);
        }
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
        print_error({ErrorType::RUNTIME_ERROR, "plugin", "Plugin system is disabled", {}});
        return false;
    }

    plugin_initialize_func init_func = nullptr;
    {
        std::unique_lock plugins_lock(plugins_mutex);
        auto it = loaded_plugins.find(name);
        if (it != loaded_plugins.end() && it->second.enabled) {
            std::cout << "Plugin already enabled: " << name << std::endl;
            return true;
        }

        if (lazy_loading_enabled && it == loaded_plugins.end()) {
            plugins_lock.unlock();
            if (!load_plugin_on_demand(name)) {
                print_error(
                    {ErrorType::RUNTIME_ERROR, "plugin", "Failed to load plugin: " + name, {}});
                return false;
            }
            plugins_lock.lock();
            it = loaded_plugins.find(name);
        }

        if (it == loaded_plugins.end()) {
            print_error({ErrorType::COMMAND_NOT_FOUND, "plugin", "Plugin not found: " + name, {}});
            return false;
        }

        init_func = it->second.initialize;
    }

    current_plugin_context = name;
    bool init_ok = (init_func && init_func() == PLUGIN_SUCCESS);
    current_plugin_context.clear();
    if (!init_ok) {
        print_error(
            {ErrorType::RUNTIME_ERROR, "plugin", "Failed to initialize plugin: " + name, {}});
        return false;
    }

    int count = 0;
    char** events = nullptr;
    {
        std::unique_lock plugins_lock(plugins_mutex);
        auto it = loaded_plugins.find(name);
        if (it == loaded_plugins.end()) {
            std::cerr << "Plugin not found after initialization: " << name << std::endl;
            return false;
        }
        it->second.enabled = true;
        if (!g_startup_active) {
            std::cout << "Enabled plugin: " << name << std::endl;
        }

        if (it->second.get_subscribed_events) {
            events = it->second.get_subscribed_events(&count);
        }
    }
    trigger_subscribed_global_event("plugin_enabled", name);

    if (events && count > 0) {
        std::unique_lock events_lock(events_mutex);
        for (int i = 0; i < count; i++) {
            subscribed_events[events[i]].push_back(name);
        }
    }

    if (events && count > 0) {
        std::unique_lock plugins_lock(plugins_mutex);
        auto it = loaded_plugins.find(name);
        if (it != loaded_plugins.end() && it->second.free_memory) {
            for (int i = 0; i < count; i++) {
                it->second.free_memory(events[i]);
            }
            it->second.free_memory(events);
        }
    }
    return true;
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
                eventIt->second.erase(
                    std::remove(eventIt->second.begin(), eventIt->second.end(), name),
                    eventIt->second.end());
            }
        }
    }
    return true;
}

bool Plugin::handle_plugin_command(const std::string& targeted_plugin,
                                   std::vector<std::string>& args) {
    if (!enabled) {
        return false;
    }

    std::shared_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(targeted_plugin);

    if (lazy_loading_enabled && it == loaded_plugins.end()) {
        plugins_lock.unlock();
        if (load_plugin_on_demand(targeted_plugin)) {
            plugins_lock.lock();
            it = loaded_plugins.find(targeted_plugin);
        } else {
            return false;
        }
    }

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

    if (lazy_loading_enabled) {
        std::shared_lock metadata_lock(metadata_mutex);
        auto metadata_it = plugin_metadata_cache.find(name);
        if (metadata_it != plugin_metadata_cache.end()) {
            return metadata_it->second.commands;
        }
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

bool Plugin::update_plugin_setting(const std::string& plugin_name, const std::string& key,
                                   const std::string& value) {
    if (!enabled) {
        return false;
    }

    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(plugin_name);
    if (it != loaded_plugins.end()) {
        it->second.settings[key] = value;
        if (it->second.update_setting) {
            bool result = it->second.update_setting(key.c_str(), value.c_str()) == PLUGIN_SUCCESS;
            return result;
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

void Plugin::trigger_subscribed_global_event(const std::string& event,
                                             const std::string& event_data) {
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

    for (const auto& plugin_name : subscribedPlugins) {
        std::shared_lock plugins_lock(plugins_mutex);
        auto plugin_it = loaded_plugins.find(plugin_name);
        if (plugin_it != loaded_plugins.end() && plugin_it->second.enabled) {
            plugins_lock.unlock();
            current_plugin_context = plugin_name;
            plugin_it->second.handle_command(&args);
            current_plugin_context.clear();
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

void Plugin::clear_plugin_cache() {
    if (!enabled) {
        return;
    }

    std::unique_lock discovery_lock(discovery_mutex);
    plugins_discovered = false;

    {
        std::unique_lock events_lock(events_mutex);
        subscribed_events.clear();
    }
}

bool Plugin::is_plugin_loaded(const std::string& name) const {
    if (!enabled) {
        return false;
    }

    std::shared_lock plugins_lock(plugins_mutex);
    bool result = loaded_plugins.find(name) != loaded_plugins.end();
    return result;
}

std::string Plugin::get_current_architecture() const {
    struct utsname system_info;
    uname(&system_info);

    std::string arch = system_info.machine;

    if (arch == "x86_64" || arch == "amd64") {
        return "x86_64";
    } else if (arch == "arm64" || arch == "aarch64") {
        return "arm64";
    }
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
        if (fgets(buffer, 128, pipe) != nullptr) {
            output += buffer;
        }
    }
    pclose(pipe);

    if (output.find("x86_64") != std::string::npos) {
        result = "x86_64";
    } else if (output.find("arm64") != std::string::npos ||
               output.find("ARM64") != std::string::npos ||
               output.find("aarch64") != std::string::npos) {
        result = "arm64";
    }
    return result;
}

bool Plugin::is_architecture_compatible(const std::string& file_arch,
                                        const std::string& current_arch) const {
    if (file_arch == current_arch) {
        return true;
    }

#ifdef __APPLE__
    if (current_arch == "arm64" && file_arch == "x86_64") {
        bool rosetta = is_rosetta_translated();
        return rosetta;
    }
#endif

    return false;
}

bool Plugin::is_rosetta_translated() const {
#ifdef __APPLE__
    int ret = 0;
    size_t size = sizeof(ret);
    if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) != -1) {
        return ret == 1;
    }
#endif
    return false;
}

std::vector<std::string> Plugin::get_available_commands(const std::string& plugin_name) const {
    if (!enabled) {
        return {};
    }

    if (lazy_loading_enabled) {
        std::shared_lock metadata_lock(metadata_mutex);
        auto metadata_it = plugin_metadata_cache.find(plugin_name);
        if (metadata_it != plugin_metadata_cache.end()) {
            return metadata_it->second.commands;
        }
    }

    return get_plugin_commands(plugin_name);
}

size_t Plugin::get_loaded_plugin_count() const {
    std::shared_lock plugins_lock(plugins_mutex);
    return loaded_plugins.size();
}

size_t Plugin::get_metadata_cache_size() const {
    std::shared_lock metadata_lock(metadata_mutex);
    return plugin_metadata_cache.size();
}
