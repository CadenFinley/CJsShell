#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CJSH Plugin API
 *
 * This header defines the interface for creating plugins for CJ's Shell.
 * Plugins must be compiled as shared libraries (.so on Linux, .dylib on macOS)
 * and implement all required functions listed below.
 *
 * To create a plugin:
 * 1. Include this header in your plugin source
 * 2. Implement all required functions
 * 3. Compile as a shared library
 * 4. Place the compiled library in the ~/.cjsh/plugins directory
 */

// Plugin interface version for compatibility checking
// Your plugin must report this version in plugin_get_info() to be loaded
#define PLUGIN_INTERFACE_VERSION 2

/**
 * Error codes returned by plugin functions
 * Use these values to indicate success or specific errors
 */
typedef enum {
  PLUGIN_SUCCESS = 0,              // Operation completed successfully
  PLUGIN_ERROR_GENERAL = -1,       // Generic error occurred
  PLUGIN_ERROR_INVALID_ARGS = -2,  // Invalid arguments were provided
  PLUGIN_ERROR_NOT_IMPLEMENTED =
      -3  // Function is not implemented by this plugin
} plugin_error_t;

/**
 * Simple string buffer structure
 * Used for returning string data from plugin to shell
 */
typedef struct {
  char* data;  // Null-terminated string data
  int length;  // Length of string (excluding null terminator)
} plugin_string_t;

/**
 * Key-value pair for plugin settings
 * Used to define and retrieve plugin configuration
 */
typedef struct {
  char* key;    // Setting name
  char* value;  // Setting value
} plugin_setting_t;

/**
 * Command arguments structure
 * Passed to plugin_handle_command() when a command is executed
 */
typedef struct {
  char** args;   // Array of argument strings (null-terminated)
  int count;     // Number of arguments in the array
  int position;  // Current position in processing (typically 0 at start)
} plugin_args_t;

/**
 * Plugin info structure
 * Returned by plugin_get_info() to identify the plugin
 */
typedef struct {
  char* name;             // Plugin name (must be unique)
  char* version;          // Plugin version (semver recommended)
  char* description;      // Short description of plugin functionality
  char* author;           // Plugin author name
  int interface_version;  // Must match PLUGIN_INTERFACE_VERSION
} plugin_info_t;

/**
 * Core plugin functions that must be implemented
 *
 * Each plugin must export the following functions with these exact signatures.
 * The shell will look for these symbols when loading the plugin.
 */

/**
 * Get plugin information
 * Required function: Must be implemented by all plugins
 *
 * Returns basic information about the plugin including name, version, and
 * author. The returned pointer must remain valid for the lifetime of the
 * plugin.
 *
 * @return Pointer to a plugin_info_t structure
 */
typedef plugin_info_t* (*plugin_get_info_func)();

/**
 * Initialize the plugin
 * Required function: Must be implemented by all plugins
 *
 * Called when the plugin is enabled. Perform any setup operations here.
 *
 * @return PLUGIN_SUCCESS if initialization was successful, error code otherwise
 */
typedef int (*plugin_initialize_func)();

/**
 * Shutdown the plugin
 * Required function: Must be implemented by all plugins
 *
 * Called when the plugin is disabled or the shell is exiting.
 * Clean up any resources allocated by the plugin.
 */
typedef void (*plugin_shutdown_func)();

/**
 * Handle a command invocation
 * Required function: Must be implemented by all plugins
 *
 * Called when a user executes a command registered by this plugin.
 * Parse arguments and perform the command action.
 *
 * @param args Command arguments structure
 * @return PLUGIN_SUCCESS if command executed successfully, error code otherwise
 */
typedef int (*plugin_handle_command_func)(plugin_args_t* args);

/**
 * Get commands provided by this plugin
 * Required function: Must be implemented by all plugins
 *
 * Return a list of command names that this plugin handles.
 * The shell will register these commands and route them to this plugin.
 *
 * @param count Output parameter: set to the number of commands
 * @return Array of command name strings (must remain valid)
 */
typedef char** (*plugin_get_commands_func)(int* count);

/**
 * Get events this plugin subscribes to
 * Required function: Must be implemented by all plugins
 *
 * Return a list of event names this plugin wants to receive.
 * The shell will notify the plugin when these events occur.
 *
 * Common events include:
 * - "main_process_pre_run" - Beginning of main process loop
 * - "main_process_start" - Beginning of command processing loop
 * - "main_process_end" - End of command processing loop
 * - "main_process_command_processed" - After a command is processed
 * - "plugin_enabled" - When a plugin is enabled
 * - "plugin_disabled" - When a plugin is disabled
 *
 * @param count Output parameter: set to the number of events
 * @return Array of event name strings (must remain valid)
 */
typedef char** (*plugin_get_subscribed_events_func)(int* count);

/**
 * Get default plugin settings
 * Required function: Must be implemented by all plugins
 *
 * Return a list of default settings for this plugin.
 * The shell will initialize the plugin with these settings.
 *
 * @param count Output parameter: set to the number of settings
 * @return Array of plugin_setting_t structures (must remain valid)
 */
typedef plugin_setting_t* (*plugin_get_default_settings_func)(int* count);

/**
 * Update a plugin setting
 * Required function: Must be implemented by all plugins
 *
 * Called when a plugin setting is changed.
 * The plugin should update its internal state accordingly.
 *
 * @param key Setting name
 * @param value New setting value
 * @return PLUGIN_SUCCESS if setting was updated, error code otherwise
 */
typedef int (*plugin_update_setting_func)(const char* key, const char* value);

/**
 * Free memory allocated by the plugin
 * Required function: Must be implemented by all plugins
 *
 * Called by the shell to free memory returned by plugin functions.
 * The plugin is responsible for properly freeing the memory it allocated.
 *
 * @param ptr Pointer to memory that should be freed
 */
typedef void (*plugin_free_memory_func)(void* ptr);

/**
 * Helper functions provided by the shell
 * These functions can be called by the plugin to interact with the shell
 */

/**
 * Get the path to the plugins home directory
 * Returns the path where all plugins are stored
 *
 * @return String containing the path (must be freed with plugin_free_string)
 */
char* plugin_get_plugins_home_directory();

/**
 * Get the path to a specific plugin's directory
 * Returns the path where a specific plugin can store its data
 *
 * @param plugin_name Name of the plugin
 * @return String containing the path (must be freed with plugin_free_string)
 */
char* plugin_get_plugin_directory(const char* plugin_name);

/**
 * Free a string allocated by the shell
 *
 * @param str String to free
 */
void plugin_free_string(char* str);
/**
 * Function pointer type for prompt variable callbacks.
 * Should return a plugin_string_t containing the variable's value.
 */
typedef plugin_string_t (*plugin_get_prompt_variable_func)();

/**
 * Register a new prompt variable provider with the shell.
 * Plugins should call this during initialization or when enabled.
 *
 * @param name Name of the variable/tag (without braces).
 * @param func Callback function returning the variable's value.
 * @return PLUGIN_SUCCESS on success, error code otherwise.
 */
plugin_error_t plugin_register_prompt_variable(
    const char* name, plugin_get_prompt_variable_func func);

/**
 * Macro for defining exported functions
 * Use this before function definitions that need to be exported
 */
#define PLUGIN_API

/**
 * Required exports for a plugin
 * Every plugin must export the following functions:
 *
 * plugin_get_info - Return information about the plugin
 * plugin_initialize - Initialize the plugin
 * plugin_shutdown - Clean up when the plugin is disabled
 * plugin_handle_command - Execute a command
 * plugin_get_commands - List all commands provided by this plugin
 * plugin_get_subscribed_events - List all events this plugin wants to receive
 * plugin_get_default_settings - List default plugin settings
 * plugin_update_setting - Handle a setting update
 * plugin_free_memory - Free memory allocated by the plugin
 *
 * Example plugin implementation:
 *
 * PLUGIN_API plugin_info_t* plugin_get_info() {
 *     static plugin_info_t info = {
 *         "my_plugin",           // name
 *         "1.0.0",               // version
 *         "An example plugin",   // description
 *         "Your Name",           // author
 *         PLUGIN_INTERFACE_VERSION  // Must match current interface version
 *     };
 *     return &info;
 * }
 *
 * PLUGIN_API int plugin_initialize() {
 *     // Setup code here
 *     return PLUGIN_SUCCESS;
 * }
 *
 * PLUGIN_API void plugin_shutdown() {
 *     // Cleanup code here
 * }
 *
 * PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
 *     // Command handling code here
 *     return PLUGIN_SUCCESS;
 * }
 *
 * PLUGIN_API char** plugin_get_commands(int* count) {
 *     static char* commands[] = {"mycmd", "myothercmd"};
 *     *count = 2;
 *     return commands;
 * }
 *
 * PLUGIN_API char** plugin_get_subscribed_events(int* count) {
 *     static char* events[] = {"main_process_start", "plugin_enabled"};
 *     *count = 2;
 *     return events;
 * }
 *
 * PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
 *     static plugin_setting_t settings[] = {
 *         {"setting1", "default1"},
 *         {"setting2", "default2"}
 *     };
 *     *count = 2;
 *     return settings;
 * }
 *
 * PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
 *     // Handle setting update
 *     return PLUGIN_SUCCESS;
 * }
 *
 * PLUGIN_API void plugin_free_memory(void* ptr) {
 *     free(ptr);
 * }
 */

#ifdef __cplusplus
}
#endif
