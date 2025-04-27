#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Plugin interface version for compatibility checking
#define PLUGIN_INTERFACE_VERSION 2

// Error codes
typedef enum {
    PLUGIN_SUCCESS = 0,
    PLUGIN_ERROR_GENERAL = -1,
    PLUGIN_ERROR_INVALID_ARGS = -2,
    PLUGIN_ERROR_NOT_IMPLEMENTED = -3
} plugin_error_t;

// Simple string buffer structure
typedef struct {
    char* data;
    int length;
} plugin_string_t;

// Key-value pair for settings
typedef struct {
    char* key;
    char* value;
} plugin_setting_t;

// Command arguments structure
typedef struct {
    char** args;
    int count;
    int position;
} plugin_args_t;

// Plugin info structure
typedef struct {
    char* name;
    char* version;
    char* description;
    char* author;
    int interface_version;
} plugin_info_t;

// Core plugin functions that must be implemented
typedef plugin_info_t* (*plugin_get_info_func)();
typedef int (*plugin_initialize_func)();
typedef void (*plugin_shutdown_func)();
typedef int (*plugin_handle_command_func)(plugin_args_t* args);
typedef char** (*plugin_get_commands_func)(int* count);
typedef char** (*plugin_get_subscribed_events_func)(int* count);
typedef plugin_setting_t* (*plugin_get_default_settings_func)(int* count);
typedef int (*plugin_update_setting_func)(const char* key, const char* value);
typedef void (*plugin_free_memory_func)(void* ptr);

// Helper functions provided by the shell
char* plugin_get_plugins_home_directory();
char* plugin_get_plugin_directory(const char* plugin_name);
void plugin_free_string(char* str);

// Plugin must export these symbols
#define PLUGIN_API

// Required exports for a plugin
// plugin_get_info
// plugin_initialize
// plugin_shutdown
// plugin_handle_command
// plugin_get_commands
// plugin_get_subscribed_events
// plugin_get_default_settings
// plugin_update_setting
// plugin_free_memory

#ifdef __cplusplus
}
#endif
