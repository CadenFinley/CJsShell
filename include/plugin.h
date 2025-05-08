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
#include <mutex>
#include <shared_mutex>
#include <queue>

#include "pluginapi.h"

struct plugin_data {
    void* handle;
    plugin_info_t* info;
    bool enabled;
    std::map<std::string, std::string> settings;
    
    plugin_get_info_func get_info;
    plugin_initialize_func initialize;
    plugin_shutdown_func shutdown;
    plugin_handle_command_func handle_command;
    plugin_get_commands_func get_commands;
    plugin_get_subscribed_events_func get_subscribed_events;
    plugin_get_default_settings_func get_default_settings;
    plugin_update_setting_func update_setting;
    plugin_free_memory_func free_memory;
};

class Plugin {
private:
    std::filesystem::path plugins_directory;
    std::unordered_map<std::string, plugin_data> loaded_plugins;
    std::unordered_map<std::string, std::vector<std::string>> subscribed_events;
    bool plugins_discovered;
    bool enabled;
    mutable std::shared_mutex plugins_mutex;
    mutable std::shared_mutex events_mutex;
    std::mutex discovery_mutex;

    void unload_plugin(const std::string& name);
    std::string get_current_architecture() const;
    std::string get_file_architecture(const std::filesystem::path& path) const;
    bool is_architecture_compatible(const std::string& file_arch, const std::string& current_arch) const;
    bool is_rosetta_translated() const;

public:
    Plugin(const std::filesystem::path& plugins_dir, bool enabled);
    ~Plugin();
    bool discover_plugins();
    bool load_plugin(const std::filesystem::path& path);
    bool install_plugin(const std::filesystem::path& source_path);
    bool uninstall_plugin(const std::string& name);
    std::vector<std::string> get_available_plugins() const;
    std::vector<std::string> get_enabled_plugins() const;
    bool enable_plugin(const std::string& name);
    bool disable_plugin(const std::string& name);
    int get_interface_version() const { return PLUGIN_INTERFACE_VERSION; }
    bool handle_plugin_command(const std::string& targeted_plugin, std::vector<std::string>& args);
    std::vector<std::string> get_plugin_commands(const std::string& name) const;
    std::string get_plugin_info(const std::string& name) const;
    bool update_plugin_setting(const std::string& plugin_name, const std::string& key, const std::string& value);
    std::map<std::string, std::map<std::string, std::string>> get_all_plugin_settings() const;
    void trigger_subscribed_global_event(const std::string& event, const std::string& event_data);
    plugin_data* get_plugin_data(const std::string& name);
    void clear_plugin_cache();
    bool is_plugin_loaded(const std::string& name) const;
};
