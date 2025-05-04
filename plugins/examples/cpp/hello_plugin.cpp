#include <iostream>
#include <string>
#include <cstring>
#include "../../../include/pluginapi.h"

// Plugin state in C++ class
class HelloPlugin {
private:
    std::string greeting;
    
public:
    HelloPlugin() : greeting("Hello from C++!") {}
    
    const std::string& getGreeting() const { return greeting; }
    void setGreeting(const std::string& newGreeting) { greeting = newGreeting; }
};

// Plugin singleton instance
static HelloPlugin* plugin = nullptr;

// Static data
static plugin_info_t plugin_info = {
    (char*)"hello_cpp",
    (char*)"1.0.0",
    (char*)"Example plugin in C++", 
    (char*)"CJSH Team",
    PLUGIN_INTERFACE_VERSION
};

static char* plugin_commands[] = {(char*)"hello_cpp", (char*)"hello_plus_plus"};
static char* plugin_events[] = {(char*)"main_process_start"};
static plugin_setting_t plugin_settings[] = {
    {(char*)"greeting", (char*)"Hello from C++!"}
};

// Export required functions with C linkage
extern "C" {

PLUGIN_API plugin_info_t* plugin_get_info() {
    return &plugin_info;
}

PLUGIN_API int plugin_initialize() {
    std::cout << "Hello C++ plugin initializing..." << std::endl;
    plugin = new HelloPlugin();
    return PLUGIN_SUCCESS;
}

PLUGIN_API void plugin_shutdown() {
    std::cout << "Hello C++ plugin shutting down..." << std::endl;
    delete plugin;
    plugin = nullptr;
}

PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
    if (args == NULL || args->count < 1 || plugin == nullptr) {
        return PLUGIN_ERROR_INVALID_ARGS;
    }
    
    std::cout << plugin->getGreeting() << ", world! (from C++ plugin)" << std::endl;
    
    if (args->count > 1) {
        std::cout << "You provided arguments: ";
        for (int i = 1; i < args->count; i++) {
            std::cout << args->args[i] << " ";
        }
        std::cout << std::endl;
    }
    
    return PLUGIN_SUCCESS;
}

PLUGIN_API char** plugin_get_commands(int* count) {
    *count = sizeof(plugin_commands) / sizeof(char*);
    return plugin_commands;
}

PLUGIN_API char** plugin_get_subscribed_events(int* count) {
    *count = sizeof(plugin_events) / sizeof(char*);
    return plugin_events;
}

PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
    *count = sizeof(plugin_settings) / sizeof(plugin_setting_t);
    return plugin_settings;
}

PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
    if (plugin == nullptr) return PLUGIN_ERROR_GENERAL;
    
    if (strcmp(key, "greeting") == 0) {
        plugin->setGreeting(value);
        return PLUGIN_SUCCESS;
    }
    return PLUGIN_ERROR_INVALID_ARGS;
}

PLUGIN_API void plugin_free_memory(void* ptr) {
    free(ptr);
}

} // extern "C"

// Build command:
// g++ -std=c++11 -shared -fPIC -o hello_cpp.so hello_plugin.cpp
