#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CJSH Plugin API
 *
 * This header defines the interface for creating plugins for CJ's Shell.
 * This version includes enhanced memory safety and error handling.
 *
 * MEMORY SAFETY GUIDELINES:
 * 1. ALWAYS implement plugin_free_memory() correctly
 * 2. NEVER return static arrays or strings from allocation functions
 * 3. Use consistent allocation/deallocation methods within your plugin
 * 4. Handle NULL pointers gracefully
 * 5. Initialize all structure members to prevent undefined behavior
 */

// Plugin interface version for compatibility checking
#define PLUGIN_INTERFACE_VERSION 3

/**
 * Error codes returned by plugin functions
 */
typedef enum {
    PLUGIN_SUCCESS = 0,
    PLUGIN_ERROR_GENERAL = -1,
    PLUGIN_ERROR_INVALID_ARGS = -2,
    PLUGIN_ERROR_NOT_IMPLEMENTED = -3,
    PLUGIN_ERROR_OUT_OF_MEMORY = -4,
    PLUGIN_ERROR_NULL_POINTER = -5
} plugin_error_t;

/**
 * String buffer structure with enhanced safety
 */
typedef struct {
    char* data;    // Null-terminated string data (MUST be heap-allocated)
    int length;    // Length of string (excluding null terminator)
    int capacity;  // Allocated capacity (for future extensions)
} plugin_string_t;

/**
 * Key-value pair for plugin settings
 */
typedef struct {
    char* key;    // Setting name (MUST be heap-allocated)
    char* value;  // Setting value (MUST be heap-allocated)
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
    char* description;      // Short description
    char* author;           // Plugin author name
    int interface_version;  // Must match PLUGIN_INTERFACE_VERSION
} plugin_info_t;

/**
 * Plugin validation result
 */
typedef struct {
    plugin_error_t status;
    char* error_message;  // Optional error message (heap-allocated if present)
} plugin_validation_t;

/**
 * Core plugin function typedefs
 */
typedef plugin_info_t* (*plugin_get_info_func)();
typedef int (*plugin_initialize_func)();
typedef void (*plugin_shutdown_func)();
typedef int (*plugin_handle_command_func)(plugin_args_t* args);
typedef char** (*plugin_get_commands_func)(int* count);
typedef char** (*plugin_get_subscribed_events_func)(int* count);
typedef plugin_setting_t* (*plugin_get_default_settings_func)(int* count);
typedef int (*plugin_update_setting_func)(const char* key, const char* value);
typedef void (*plugin_free_memory_func)(void* ptr);

/**
 * NEW: Enhanced validation function (optional but recommended)
 * Plugins can implement this to perform self-validation
 */
typedef plugin_validation_t (*plugin_validate_func)();

/**
 * Global Events System
 *
 * The shell provides a global event system that plugins can subscribe to.
 * Events are triggered at specific points in the shell's execution and
 * plugins can register to receive notifications for these events.
 *
 * To subscribe to events, implement plugin_get_subscribed_events() to return
 * an array of event names your plugin wants to receive.
 *
 * MAIN PROCESS EVENTS (triggered during shell main loop):
 *
 * 1. "main_process_pre_run"
 *    - Triggered: Before entering the main command loop
 *    - Data: Empty string ""
 *    - Purpose: Initialize resources needed for command processing
 *
 * 2. "main_process_start"
 *    - Triggered: Before prompting user for input (start of each loop
 * iteration)
 *    - Data: Empty string ""
 *    - Purpose: Prepare for new command input, update prompt variables
 *
 * 3. "main_process_command_process"
 *    - Triggered: Immediately after receiving user input
 *    - Data: The command string entered by the user
 *    - Purpose: Log commands, validate input, modify command before execution
 *
 * 4. "main_process_end"
 *    - Triggered: After command execution completes
 *    - Data: Empty string ""
 *    - Purpose: Clean up command-specific resources, update statistics
 *
 * 5. "main_process_exit"
 *    - Triggered: After leaving the main command loop (shell shutting down)
 *    - Data: Empty string ""
 *    - Purpose: Final cleanup, save state, close resources
 *
 * PLUGIN LIFECYCLE EVENTS:
 *
 * - "plugin_enabled" - When a plugin is enabled (data: plugin name)
 * - "plugin_disabled" - When a plugin is disabled (data: plugin name)
 *
 * Event data is passed as a C string. For events with no data, an empty
 * string "" is passed. Plugins should not modify the event data string.
 *
 * Example subscription:
 *
 * char** plugin_get_subscribed_events(int* count) {
 *     *count = 2;
 *     char** events = malloc(*count * sizeof(char*));
 *     events[0] = strdup("main_process_command_process");
 *     events[1] = strdup("main_process_end");
 *     return events;
 * }
 */

/**
 * Helper functions provided by the shell
 */

/**
 * Enhanced memory allocation helpers
 */
char* plugin_get_plugins_home_directory();
char* plugin_get_plugin_directory(const char* plugin_name);
void plugin_free_string(char* str);

/**
 * Enhanced string utilities for safer memory management
 */
char* plugin_safe_strdup(const char* src);
plugin_string_t plugin_create_string(const char* src);
void plugin_free_plugin_string(plugin_string_t* str);

/**
 * Memory leak detection helpers (debug builds only)
 */
#ifdef PLUGIN_DEBUG
void plugin_register_allocation(void* ptr, const char* file, int line);
void plugin_register_deallocation(void* ptr, const char* file, int line);
#define PLUGIN_MALLOC(size) plugin_debug_malloc(size, __FILE__, __LINE__)
#define PLUGIN_FREE(ptr) plugin_debug_free(ptr, __FILE__, __LINE__)
#else
#define PLUGIN_MALLOC(size) malloc(size)
#define PLUGIN_FREE(ptr) free(ptr)
#endif

/**
 * Prompt variable registration
 */
typedef plugin_string_t (*plugin_get_prompt_variable_func)();
plugin_error_t plugin_register_prompt_variable(const char* name,
                                               plugin_get_prompt_variable_func func);

/**
 * Macro for defining exported functions
 */
#define PLUGIN_API __attribute__((visibility("default")))

/**
 * Example implementation:
 *
 * PLUGIN_API plugin_info_t* plugin_get_info() {
 *     static plugin_info_t info = {
 *         .name = "my_plugin",
 *         .version = "1.0.0",
 *         .description = "An example plugin",
 *         .author = "Your Name",
 *         .interface_version = PLUGIN_INTERFACE_VERSION
 *     };
 *     return &info;
 * }
 *
 * PLUGIN_API plugin_validation_t plugin_validate() {
 *     plugin_validation_t result = {PLUGIN_SUCCESS, NULL};
 *     // Perform self-validation here
 *     return result;
 * }
 *
 * PLUGIN_API int plugin_initialize() {
 *     // Initialization code with error checking
 *     return PLUGIN_SUCCESS;
 * }
 *
 * PLUGIN_API void plugin_shutdown() {
 *     // Cleanup code - free all allocated resources
 * }
 *
 * PLUGIN_API char** plugin_get_commands(int* count) {
 *     if (!count) return NULL;
 *
 *     *count = 2;
 *     char** commands = PLUGIN_MALLOC(*count * sizeof(char*));
 *     if (!commands) {
 *         *count = 0;
 *         return NULL;
 *     }
 *
 *     commands[0] = plugin_safe_strdup("mycmd");
 *     commands[1] = plugin_safe_strdup("myothercmd");
 *
 *     // Check for allocation failures
 *     if (!commands[0] || !commands[1]) {
 *         if (commands[0]) PLUGIN_FREE(commands[0]);
 *         if (commands[1]) PLUGIN_FREE(commands[1]);
 *         PLUGIN_FREE(commands);
 *         *count = 0;
 *         return NULL;
 *     }
 *
 *     return commands;
 * }
 *
 * PLUGIN_API void plugin_free_memory(void* ptr) {
 *     if (ptr) {
 *         PLUGIN_FREE(ptr);
 *     }
 * }
 */

#ifdef __cplusplus
}
#endif
