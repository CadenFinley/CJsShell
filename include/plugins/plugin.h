#pragma once

#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "pluginapi.h"

struct plugin_metadata {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::filesystem::path library_path;
    std::time_t last_modified;
    std::vector<std::string> commands;
    std::vector<std::string> events;
    bool is_loaded;
    bool load_failed;

    plugin_metadata() : last_modified(0), is_loaded(false), load_failed(false) {
    }
};

struct plugin_data {
    void* handle;
    plugin_info_t* info;
    bool enabled;
    std::map<std::string, std::string> settings;

    std::unordered_map<std::string, plugin_get_prompt_variable_func>
        prompt_variables;

    plugin_get_info_func get_info;
    plugin_initialize_func initialize;
    plugin_shutdown_func shutdown;
    plugin_handle_command_func handle_command;
    plugin_get_commands_func get_commands;
    plugin_get_subscribed_events_func get_subscribed_events;
    plugin_get_default_settings_func get_default_settings;
    plugin_update_setting_func update_setting;
    plugin_free_memory_func free_memory;
    plugin_validate_func validate;
};

class Plugin {
   private:
    std::filesystem::path plugins_directory;
    std::unordered_map<std::string, plugin_data> loaded_plugins;
    std::unordered_map<std::string, plugin_metadata> plugin_metadata_cache;
    std::unordered_map<std::string, std::vector<std::string>> subscribed_events;
    bool plugins_discovered;
    bool lazy_loading_enabled;
    bool enabled;
    mutable std::shared_mutex plugins_mutex;
    mutable std::shared_mutex metadata_mutex;
    mutable std::shared_mutex events_mutex;
    std::mutex discovery_mutex;

    void unload_plugin(const std::string& name);
    bool extract_plugin_metadata(const std::filesystem::path& path,
                                 plugin_metadata& metadata);
    bool load_plugin_on_demand(const std::string& name);
    bool is_metadata_stale(const plugin_metadata& metadata) const;
    void cache_plugin_metadata();
    void load_metadata_cache();
    void save_metadata_cache();
    std::string get_current_architecture() const;
    std::string get_file_architecture(const std::filesystem::path& path) const;
    bool is_architecture_compatible(const std::string& file_arch,
                                    const std::string& current_arch) const;
    bool is_rosetta_translated() const;

   public:
    Plugin(const std::filesystem::path& plugins_dir, bool enabled,
           bool lazy_loading = true);
    ~Plugin();
    bool discover_plugins();
    bool load_plugin(const std::filesystem::path& path);
    bool uninstall_plugin(const std::string& name);
    std::vector<std::string> get_available_plugins() const;
    std::vector<std::string> get_enabled_plugins() const;
    bool is_plugin_enabled(const std::string& name) const;
    bool enable_plugin(const std::string& name);
    bool disable_plugin(const std::string& name);
    int get_interface_version() const {
        return PLUGIN_INTERFACE_VERSION;
    }
    bool handle_plugin_command(const std::string& targeted_plugin,
                               std::vector<std::string>& args);
    std::vector<std::string> get_plugin_commands(const std::string& name) const;
    std::string get_plugin_info(const std::string& name) const;
    bool update_plugin_setting(const std::string& plugin_name,
                               const std::string& key,
                               const std::string& value);
    std::map<std::string, std::map<std::string, std::string>>
    get_all_plugin_settings() const;
    void trigger_subscribed_global_event(const std::string& event,
                                         const std::string& event_data);
    plugin_data* get_plugin_data(const std::string& name);
    void clear_plugin_cache();
    bool is_plugin_loaded(const std::string& name) const;

    std::vector<std::string> get_available_commands(
        const std::string& plugin_name) const;
    bool is_lazy_loading_enabled() const {
        return lazy_loading_enabled;
    }
    void set_lazy_loading(bool enabled) {
        lazy_loading_enabled = enabled;
    }
    size_t get_loaded_plugin_count() const;
    size_t get_metadata_cache_size() const;

    bool get_enabled() const {
        return enabled;
    }
};
