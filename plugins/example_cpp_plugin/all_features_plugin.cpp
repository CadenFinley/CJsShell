#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Include the plugin API header
#include "pluginapi.h"

// Create a stub for plugin_register_prompt_variable that will be resolved at
// runtime This avoids link-time errors while still allowing the function to be
// called at runtime
extern "C" {
// This is a weak symbol that will be overridden by the actual implementation in
// the shell
__attribute__((weak)) plugin_error_t plugin_register_prompt_variable(
    const char* name, plugin_get_prompt_variable_func func) {
    // This will never be called as the real implementation will be used at
    // runtime
    std::cerr << "Warning: Using stub implementation of "
                 "plugin_register_prompt_variable\n";
    return PLUGIN_ERROR_GENERAL;
}
}

// Define the plugin version and name constants
#define PLUGIN_NAME "example_cpp_plugin"
#define PLUGIN_VERSION "1.0.0"

// Global variables for plugin state - using pointers to avoid destruction order
// issues
static std::mutex* plugin_mutex = nullptr;
static std::map<std::string, std::string>* plugin_settings = nullptr;
static bool is_enabled = false;
static std::vector<std::string>* command_history = nullptr;
static std::thread* background_thread = nullptr;
static bool background_thread_running = false;
static bool plugin_initialized = false;

// Forward declarations of callback functions
static plugin_string_t current_time_callback();
static plugin_string_t uptime_callback();
static plugin_string_t random_quote_callback();

// Helper function to create a heap-allocated copy of a string
char* create_string_copy(const char* src) {
    if (!src)
        return nullptr;
    char* dest = (char*)PLUGIN_MALLOC(std::strlen(src) + 1);
    if (dest)
        std::strcpy(dest, src);
    return dest;
}

// Helper function to create a heap-allocated array of string copies
char** create_string_array(const std::vector<std::string>& strings,
                           int* count) {
    *count = strings.size();
    if (*count == 0)
        return nullptr;

    char** arr = (char**)PLUGIN_MALLOC(*count * sizeof(char*));
    if (!arr) {
        *count = 0;
        return nullptr;
    }

    for (int i = 0; i < *count; i++) {
        arr[i] = create_string_copy(strings[i].c_str());
        if (!arr[i]) {
            // Free any previously allocated strings on failure
            for (int j = 0; j < i; j++) {
                PLUGIN_FREE(arr[j]);
            }
            PLUGIN_FREE(arr);
            *count = 0;
            return nullptr;
        }
    }
    return arr;
}

// Helper function to join string arguments
std::string join_args(plugin_args_t* args, int start_pos = 1,
                      const std::string& separator = " ") {
    std::string result;
    for (int i = start_pos; i < args->count; i++) {
        if (i > start_pos)
            result += separator;
        result += args->args[i];
    }
    return result;
}

// Background thread function
void background_task() {
    while (background_thread_running && plugin_initialized) {
        if (plugin_mutex) {
            std::lock_guard<std::mutex> lock(*plugin_mutex);
            // Update some internal state - this is just an example
            // In a real plugin, this could update status information, check
            // resources, etc.
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

// Implementation of the prompt variable callbacks
static plugin_string_t current_time_callback() {
    time_t now = time(nullptr);
    std::string time_str = ctime(&now);

    // Remove trailing newline
    if (!time_str.empty() && time_str[time_str.length() - 1] == '\n') {
        time_str.erase(time_str.length() - 1);
    }

    char* data = create_string_copy(time_str.c_str());
    plugin_string_t result = {data, static_cast<int>(time_str.length()),
                              static_cast<int>(time_str.length()) + 1};
    return result;
}

static plugin_string_t uptime_callback() {
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto diff =
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
            .count();

    std::string uptime_str = std::to_string(diff) + "s";
    char* data = create_string_copy(uptime_str.c_str());
    plugin_string_t result = {data, static_cast<int>(uptime_str.length()),
                              static_cast<int>(uptime_str.length()) + 1};
    return result;
}

static plugin_string_t random_quote_callback() {
    const char* quotes[] = {
        "The only way to do great work is to love what you do.",
        "Life is what happens when you're busy making other plans.",
        "The future belongs to those who believe in the beauty of their "
        "dreams.",
        "The purpose of our lives is to be happy.",
        "Get busy living or get busy dying."};
    int num_quotes = sizeof(quotes) / sizeof(quotes[0]);
    int index = rand() % num_quotes;

    std::string quote = quotes[index];
    char* data = create_string_copy(quote.c_str());
    plugin_string_t result = {data, static_cast<int>(quote.length()),
                              static_cast<int>(quote.length()) + 1};
    return result;
}

//
// Plugin API Implementation
//

// Get plugin information
extern "C" PLUGIN_API plugin_info_t* plugin_get_info() {
    static plugin_info_t info = {
        (char*)PLUGIN_NAME, (char*)PLUGIN_VERSION,
        (char*)"A comprehensive plugin demonstrating all CJSH plugin features",
        (char*)"Caden Finley", PLUGIN_INTERFACE_VERSION};
    return &info;
}

// Validate plugin (optional but recommended)
extern "C" PLUGIN_API plugin_validation_t plugin_validate() {
    plugin_validation_t result = {PLUGIN_SUCCESS, NULL};

    // Perform self-validation here
    if (strlen(PLUGIN_NAME) == 0) {
        result.status = PLUGIN_ERROR_GENERAL;
        result.error_message = create_string_copy("Plugin name is empty");
        return result;
    }

    if (strlen(PLUGIN_VERSION) == 0) {
        result.status = PLUGIN_ERROR_GENERAL;
        result.error_message = create_string_copy("Plugin version is empty");
        return result;
    }

    return result;
}

// Initialize the plugin
extern "C" PLUGIN_API int plugin_initialize() {
    // Initialize our dynamic objects
    if (!plugin_mutex)
        plugin_mutex = new std::mutex();
    if (!plugin_settings)
        plugin_settings = new std::map<std::string, std::string>();
    if (!command_history)
        command_history = new std::vector<std::string>();

    std::lock_guard<std::mutex> lock(*plugin_mutex);

    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Register prompt variables
    plugin_register_prompt_variable("CURRENT_TIME", current_time_callback);
    plugin_register_prompt_variable("PLUGIN_UPTIME", uptime_callback);
    plugin_register_prompt_variable("RANDOM_QUOTE", random_quote_callback);

    // Initialize plugin state
    is_enabled = true;
    plugin_initialized = true;
    command_history->clear();

    // Start background thread
    background_thread_running = true;
    if (!background_thread)
        background_thread = new std::thread();
    *background_thread = std::thread(background_task);

    std::cout << "All Features Plugin initialized successfully!\n";
    return PLUGIN_SUCCESS;
}

// Shutdown the plugin
extern "C" PLUGIN_API void plugin_shutdown() {
    // Mark as not initialized first
    plugin_initialized = false;

    // Stop background thread first, without holding the lock
    if (background_thread_running) {
        background_thread_running = false;
        if (background_thread && background_thread->joinable()) {
            background_thread->join();
        }
    }

    // Now acquire lock for cleanup
    if (plugin_mutex) {
        std::lock_guard<std::mutex> lock(*plugin_mutex);

        // Clear plugin state
        is_enabled = false;
        if (command_history) {
            command_history->clear();
        }
    }

    // Clean up dynamic objects
    if (background_thread) {
        delete background_thread;
        background_thread = nullptr;
    }
    if (command_history) {
        delete command_history;
        command_history = nullptr;
    }
    if (plugin_settings) {
        delete plugin_settings;
        plugin_settings = nullptr;
    }
    if (plugin_mutex) {
        delete plugin_mutex;
        plugin_mutex = nullptr;
    }

    std::cout << "All Features Plugin shut down.\n";
}

// Handle commands
extern "C" PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
    if (args->count < 1) {
        return PLUGIN_ERROR_INVALID_ARGS;
    }

    // Check if we're shutting down to avoid deadlocks
    if (!background_thread_running && !is_enabled) {
        return PLUGIN_SUCCESS;  // Silently ignore commands during shutdown
    }

    // Check if plugin is properly initialized
    if (!plugin_initialized || !plugin_mutex || !command_history) {
        return PLUGIN_SUCCESS;
    }

    std::lock_guard<std::mutex> lock(*plugin_mutex);

    // Double-check after acquiring lock
    if (!is_enabled) {
        return PLUGIN_SUCCESS;
    }

    // Store command in history
    std::string cmd = args->args[0];
    command_history->push_back(cmd);

    // Process different commands
    if (strcmp(args->args[0], "hello") == 0) {
        std::cout << "Hello from All Features Plugin!\n";
        return PLUGIN_SUCCESS;
    } else if (strcmp(args->args[0], "echo") == 0) {
        std::string text = join_args(args);
        std::cout << "Echo: " << text << "\n";
        return PLUGIN_SUCCESS;
    } else if (strcmp(args->args[0], "settings") == 0) {
        std::cout << "Current plugin settings:\n";
        if (plugin_settings) {
            for (const auto& setting : *plugin_settings) {
                std::cout << "  " << setting.first << " = " << setting.second
                          << "\n";
            }
        }
        return PLUGIN_SUCCESS;
    } else if (strcmp(args->args[0], "history") == 0) {
        std::cout << "Command history:\n";
        if (command_history) {
            for (size_t i = 0; i < command_history->size(); i++) {
                std::cout << "  " << i << ": " << (*command_history)[i] << "\n";
            }
        }
        return PLUGIN_SUCCESS;
    } else if (strcmp(args->args[0], "quote") == 0) {
        plugin_string_t quote = random_quote_callback();
        std::cout << "Quote: " << quote.data << "\n";
        PLUGIN_FREE(quote.data);
        return PLUGIN_SUCCESS;
    } else if (strcmp(args->args[0], "time") == 0) {
        plugin_string_t time = current_time_callback();
        std::cout << "Current time: " << time.data << "\n";
        PLUGIN_FREE(time.data);
        return PLUGIN_SUCCESS;
    } else if (strcmp(args->args[0], "uptime") == 0) {
        plugin_string_t uptime = uptime_callback();
        std::cout << "Plugin uptime: " << uptime.data << "\n";
        PLUGIN_FREE(uptime.data);
        return PLUGIN_SUCCESS;
    } else if (strcmp(args->args[0], "help") == 0) {
        std::cout << "Available commands:\n";
        std::cout << "  hello - Print a greeting\n";
        std::cout << "  echo [text] - Echo back the provided text\n";
        std::cout << "  settings - Show current plugin settings\n";
        std::cout << "  history - Show command history\n";
        std::cout << "  quote - Show a random quote\n";
        std::cout << "  time - Show current time\n";
        std::cout << "  uptime - Show plugin uptime\n";
        std::cout << "  help - Show this help message\n";
        return PLUGIN_SUCCESS;
    } else if (strcmp(args->args[0], "event") == 0) {
        std::cout << "Event received: " << args->args[1] << "\n";
        std::cout << "With args: ";
        for (int i = 2; i < args->count; i++) {
            std::cout << args->args[i] << (i < args->count - 1 ? ", " : "\n");
        }
        return PLUGIN_SUCCESS;
    }

    std::cerr << "Unknown command: " << args->args[0] << "\n";
    return PLUGIN_ERROR_INVALID_ARGS;
}

// Get commands provided by this plugin
extern "C" PLUGIN_API char** plugin_get_commands(int* count) {
    std::vector<std::string> commands = {"hello", "echo", "settings", "history",
                                         "quote", "time", "uptime",   "help"};
    return create_string_array(commands, count);
}

// Get events this plugin subscribes to
extern "C" PLUGIN_API char** plugin_get_subscribed_events(int* count) {
    std::vector<std::string> events = {
        "main_process_pre_run", "main_process_start",
        "main_process_end",     "main_process_command_processed",
        "plugin_enabled",       "plugin_disabled"};
    return create_string_array(events, count);
}

// Get default plugin settings
extern "C" PLUGIN_API plugin_setting_t* plugin_get_default_settings(
    int* count) {
    if (!count)
        return nullptr;

    const int num_settings = 3;
    *count = num_settings;

    plugin_setting_t* settings = (plugin_setting_t*)PLUGIN_MALLOC(
        num_settings * sizeof(plugin_setting_t));
    if (!settings) {
        *count = 0;
        return nullptr;
    }

    settings[0].key = create_string_copy("show_time_in_prompt");
    settings[0].value = create_string_copy("true");

    settings[1].key = create_string_copy("quote_refresh_interval");
    settings[1].value = create_string_copy("60");

    settings[2].key = create_string_copy("enable_background_tasks");
    settings[2].value = create_string_copy("true");

    // Check for allocation failures
    for (int i = 0; i < num_settings; i++) {
        if (!settings[i].key || !settings[i].value) {
            // Free any successfully allocated strings
            for (int j = 0; j <= i; j++) {
                if (settings[j].key)
                    PLUGIN_FREE(settings[j].key);
                if (settings[j].value)
                    PLUGIN_FREE(settings[j].value);
            }
            PLUGIN_FREE(settings);
            *count = 0;
            return nullptr;
        }
    }

    // Store settings in our map for later use
    if (plugin_mutex && plugin_settings) {
        std::lock_guard<std::mutex> lock(*plugin_mutex);
        (*plugin_settings)["show_time_in_prompt"] = "true";
        (*plugin_settings)["quote_refresh_interval"] = "60";
        (*plugin_settings)["enable_background_tasks"] = "true";
    }

    return settings;
}

// Update a plugin setting
extern "C" PLUGIN_API int plugin_update_setting(const char* key,
                                                const char* value) {
    if (!key || !value) {
        return PLUGIN_ERROR_INVALID_ARGS;
    }

    if (!plugin_initialized || !plugin_settings) {
        return PLUGIN_ERROR_GENERAL;
    }

    std::string k(key);
    std::string v(value);

    // Handle thread management without holding the main lock
    if (k == "enable_background_tasks") {
        bool should_enable = (v == "true");

        if (should_enable && !background_thread_running && is_enabled) {
            // Start background thread only if plugin is still enabled
            background_thread_running = true;
            if (background_thread && background_thread->joinable()) {
                background_thread->join();
            }
            if (!background_thread)
                background_thread = new std::thread();
            *background_thread = std::thread(background_task);
        } else if (!should_enable && background_thread_running) {
            // Stop background thread
            background_thread_running = false;
            if (background_thread && background_thread->joinable()) {
                background_thread->join();
            }
        }
    }

    // Update setting in map
    if (plugin_mutex && plugin_settings) {
        std::lock_guard<std::mutex> lock(*plugin_mutex);
        (*plugin_settings)[k] = v;
    }

    std::cout << "Updated setting: " << key << " = " << value << "\n";
    return PLUGIN_SUCCESS;
}

// Free memory allocated by the plugin
extern "C" PLUGIN_API void plugin_free_memory(void* ptr) {
    if (ptr) {
        PLUGIN_FREE(ptr);
    }
}
