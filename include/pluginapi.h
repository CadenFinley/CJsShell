#pragma once

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <functional>
#include <filesystem>

/**
 * @brief Abstract interface for plugins to integrate with the host application.
 *
 * The PluginApi class defines the required interface for all plugins, including metadata retrieval, lifecycle management, command/event handling, and settings management. Plugins must implement all pure virtual methods to be compatible with the host.
 *
 * Static members provide versioning and shared directory access for plugin data. The interface supports dynamic loading via C-style factory functions.
 */
class PluginApi {
public:
    // Plugin interface version for compatibility checking
    static constexpr int INTERFACE_VERSION = 2;
    
    // Static method to get the shared home directory for all plugins
    static std::string get_plugins_home_directory() {
        std::string home_dir = std::getenv("HOME");
        return (std::filesystem::path(home_dir) / ".cjsh_data").string();
    }
    
    //plugin has its own default constructor
    virtual ~PluginApi() {}
    
    //plugin details
    virtual std::string get_name() const = 0;
    virtual std::string get_version() const = 0;
    virtual std::string get_description() const = 0;
    virtual std::string get_author() const = 0;
    
    // Plugin interface version compatibility check
    virtual int get_interface_version() const = 0;
    
    virtual bool initialize() = 0; //enable plugin
    virtual void shutdown() = 0; //disable plugin
    
    // Get plugin-specific directory (subdirectory in the shared home)
    virtual std::string get_plugin_directory() const {
        return (std::filesystem::path(get_plugins_home_directory()) / get_name()).string();
    }
    
    //an "event main_process <phase>" command will be sent to all plugins via the handle_command method during all phases of the main_process_loop (pre_run, start, took_input: <char>, command_processed: <command>, and end)
    //an "event plugin_enabled <plugin_name>" command will be sent to all plugins via the handle_command method when a plugin is enabled
    //an "event plugin_disabled <plugin_name>" command will be sent to all plugins via the handle_command method when a plugin is disabled
    virtual bool handle_command(std::queue<std::string>& args) = 0;
    virtual std::vector<std::string> get_commands() const = 0; //the vector of immeadiatly availble commands
    virtual std::vector<std::string> get_subscribed_events() const = 0; //the vector of events that the plugin is subscribed to
    
    //plugin settings
    virtual std::map<std::string, std::string> get_default_settings() const = 0;
    virtual void update_setting(const std::string& key, const std::string& value) = 0;
};

//plugin interface
extern "C" {
    typedef PluginApi* (*create_plugin_func)();
    typedef void (*destroy_plugin_func)(PluginApi*);
}
#define PLUGIN_API extern "C"
#define IMPLEMENT_PLUGIN(ClassName) \
    PLUGIN_API PluginApi* create_plugin() { return new ClassName(); } \
    PLUGIN_API void destroy_plugin(PluginApi* plugin) { delete plugin; }
