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

#include "pluginapi.h"

struct plugin_data {
    void* handle;
    PluginApi* instance;
    create_plugin_func create_func;
    destroy_plugin_func destroy_func;
    bool enabled;
    std::map<std::string, std::string> settings;
};

class Plugin {
private:
    std::filesystem::path plugins_directory;
    std::unordered_map<std::string, plugin_data> loaded_plugins;
    std::unordered_map<std::string, std::vector<std::string>> subscribed_events;
    bool plugins_discovered;

    void unload_plugin(const std::string& name);

public:
    Plugin(const std::filesystem::path& plugins_dir);
    ~Plugin();
    
    bool discover_plugins();
    bool load_plugin(const std::filesystem::path& path);
    bool install_plugin(const std::filesystem::path& source_path);
    bool uninstall_plugin(const std::string& name);
    
    std::vector<std::string> get_available_plugins() const;
    std::vector<std::string> get_enabled_plugins() const;
    
    bool enable_plugin(const std::string& name);
    bool disable_plugin(const std::string& name);

    int get_interface_version() const { return PluginApi::INTERFACE_VERSION; }
    
    bool handle_plugin_command(const std::string& targeted_plugin, std::queue<std::string>& args);
    std::vector<std::string> get_plugin_commands(const std::string& name) const;
    std::string get_plugin_info(const std::string& name) const;
    
    bool update_plugin_setting(const std::string& plugin_name, const std::string& key, const std::string& value);
    std::map<std::string, std::map<std::string, std::string>> get_all_plugin_settings() const;
    
    void trigger_subscribed_global_event(const std::string& event, const std::string& event_data);
    
    PluginApi* get_plugin_instance(const std::string& name) const;
    
    void clear_plugin_cache();
    bool is_plugin_loaded(const std::string& name) const;
};
