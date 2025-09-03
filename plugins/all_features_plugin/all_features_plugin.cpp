#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <memory>

// Include the plugin API header
#include "pluginapi.h"

// Create a stub for plugin_register_prompt_variable that will be resolved at runtime
// This avoids link-time errors while still allowing the function to be called at runtime
extern "C" {
    // This is a weak symbol that will be overridden by the actual implementation in the shell
    __attribute__((weak)) plugin_error_t plugin_register_prompt_variable(
        const char* name, plugin_get_prompt_variable_func func) {
        // This will never be called as the real implementation will be used at runtime
        std::cerr << "Warning: Using stub implementation of plugin_register_prompt_variable\n";
        return PLUGIN_ERROR_GENERAL;
    }
}

// Define the plugin version and name constants
#define PLUGIN_NAME "all_features_plugin"
#define PLUGIN_VERSION "1.0.0"

// Global variables for plugin state
static std::mutex plugin_mutex;
static std::map<std::string, std::string> plugin_settings;
static bool is_enabled = false;
static std::vector<std::string> command_history;
static std::thread background_thread;
static bool background_thread_running = false;

// Forward declarations of callback functions
static plugin_string_t current_time_callback();
static plugin_string_t uptime_callback();
static plugin_string_t random_quote_callback();

// Helper function to create a heap-allocated copy of a string
char* create_string_copy(const char* src) {
    if (!src) return nullptr;
    char* dest = (char*)std::malloc(std::strlen(src) + 1);
    if (dest) std::strcpy(dest, src);
    return dest;
}

// Helper function to create a heap-allocated array of string copies
char** create_string_array(const std::vector<std::string>& strings, int* count) {
    *count = strings.size();
    if (*count == 0) return nullptr;
    
    char** arr = (char**)std::malloc(*count * sizeof(char*));
    for (int i = 0; i < *count; i++) {
        arr[i] = create_string_copy(strings[i].c_str());
    }
    return arr;
}

// Helper function to join string arguments
std::string join_args(plugin_args_t* args, int start_pos = 1, const std::string& separator = " ") {
    std::string result;
    for (int i = start_pos; i < args->count; i++) {
        if (i > start_pos) result += separator;
        result += args->args[i];
    }
    return result;
}

// Background thread function
void background_task() {
    while (background_thread_running) {
        {
            std::lock_guard<std::mutex> lock(plugin_mutex);
            // Update some internal state - this is just an example
            // In a real plugin, this could update status information, check resources, etc.
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
    plugin_string_t result = { data, static_cast<int>(time_str.length()) };
    return result;
}

static plugin_string_t uptime_callback() {
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    
    std::string uptime_str = std::to_string(diff) + "s";
    char* data = create_string_copy(uptime_str.c_str());
    plugin_string_t result = { data, static_cast<int>(uptime_str.length()) };
    return result;
}

static plugin_string_t random_quote_callback() {
    const char* quotes[] = {
        "The only way to do great work is to love what you do.",
        "Life is what happens when you're busy making other plans.",
        "The future belongs to those who believe in the beauty of their dreams.",
        "The purpose of our lives is to be happy.",
        "Get busy living or get busy dying."
    };
    int num_quotes = sizeof(quotes) / sizeof(quotes[0]);
    int index = rand() % num_quotes;
    
    std::string quote = quotes[index];
    char* data = create_string_copy(quote.c_str());
    plugin_string_t result = { data, static_cast<int>(quote.length()) };
    return result;
}

//
// Plugin API Implementation
//

// Get plugin information
extern "C" PLUGIN_API plugin_info_t* plugin_get_info() {
    static plugin_info_t info = {
        (char*)PLUGIN_NAME,
        (char*)PLUGIN_VERSION,
        (char*)"A comprehensive plugin demonstrating all CJSH plugin features",
        (char*)"Caden Finley",
        PLUGIN_INTERFACE_VERSION
    };
    return &info;
}

// Initialize the plugin
extern "C" PLUGIN_API int plugin_initialize() {
    std::lock_guard<std::mutex> lock(plugin_mutex);
    
    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    // Register prompt variables
    plugin_register_prompt_variable("CURRENT_TIME", current_time_callback);
    plugin_register_prompt_variable("PLUGIN_UPTIME", uptime_callback);
    plugin_register_prompt_variable("RANDOM_QUOTE", random_quote_callback);
    
    // Initialize plugin state
    is_enabled = true;
    command_history.clear();
    
    // Start background thread
    background_thread_running = true;
    background_thread = std::thread(background_task);
    
    std::cout << "All Features Plugin initialized successfully!\n";
    return PLUGIN_SUCCESS;
}

// Shutdown the plugin
extern "C" PLUGIN_API void plugin_shutdown() {
    std::lock_guard<std::mutex> lock(plugin_mutex);
    
    // Stop background thread
    if (background_thread_running) {
        background_thread_running = false;
        if (background_thread.joinable()) {
            background_thread.join();
        }
    }
    
    // Clear plugin state
    is_enabled = false;
    command_history.clear();
    
    std::cout << "All Features Plugin shut down.\n";
}

// Handle commands
extern "C" PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
    if (args->count < 1) {
        return PLUGIN_ERROR_INVALID_ARGS;
    }
    
    std::lock_guard<std::mutex> lock(plugin_mutex);
    
    // Store command in history
    std::string cmd = args->args[0];
    command_history.push_back(cmd);
    
    // Process different commands
    if (strcmp(args->args[0], "hello") == 0) {
        std::cout << "Hello from All Features Plugin!\n";
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(args->args[0], "echo") == 0) {
        std::string text = join_args(args);
        std::cout << "Echo: " << text << "\n";
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(args->args[0], "settings") == 0) {
        std::cout << "Current plugin settings:\n";
        for (const auto& setting : plugin_settings) {
            std::cout << "  " << setting.first << " = " << setting.second << "\n";
        }
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(args->args[0], "history") == 0) {
        std::cout << "Command history:\n";
        for (size_t i = 0; i < command_history.size(); i++) {
            std::cout << "  " << i << ": " << command_history[i] << "\n";
        }
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(args->args[0], "quote") == 0) {
        plugin_string_t quote = random_quote_callback();
        std::cout << "Quote: " << quote.data << "\n";
        free(quote.data);
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(args->args[0], "time") == 0) {
        plugin_string_t time = current_time_callback();
        std::cout << "Current time: " << time.data << "\n";
        free(time.data);
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(args->args[0], "uptime") == 0) {
        plugin_string_t uptime = uptime_callback();
        std::cout << "Plugin uptime: " << uptime.data << "\n";
        free(uptime.data);
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(args->args[0], "help") == 0) {
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
      return PLUGIN_SUCCESS;
    }
    
    std::cerr << "Unknown command: " << args->args[0] << "\n";
    return PLUGIN_ERROR_INVALID_ARGS;
}

// Get commands provided by this plugin
extern "C" PLUGIN_API char** plugin_get_commands(int* count) {
    std::vector<std::string> commands = {
        "hello", "echo", "settings", "history", "quote", "time", "uptime", "help"
    };
    return create_string_array(commands, count);
}

// Get events this plugin subscribes to
extern "C" PLUGIN_API char** plugin_get_subscribed_events(int* count) {
    std::vector<std::string> events = {
        "main_process_start",
        "main_process_end",
        "main_process_command_processed",
        "plugin_enabled",
        "plugin_disabled"
    };
    return create_string_array(events, count);
}

// Get default plugin settings
extern "C" PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
    const int num_settings = 3;
    *count = num_settings;
    
    plugin_setting_t* settings = (plugin_setting_t*)std::malloc(num_settings * sizeof(plugin_setting_t));
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
    
    // Store settings in our map for later use
    std::lock_guard<std::mutex> lock(plugin_mutex);
    plugin_settings["show_time_in_prompt"] = "true";
    plugin_settings["quote_refresh_interval"] = "60";
    plugin_settings["enable_background_tasks"] = "true";
    
    return settings;
}

// Update a plugin setting
extern "C" PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
    if (!key || !value) {
        return PLUGIN_ERROR_INVALID_ARGS;
    }
    
    std::lock_guard<std::mutex> lock(plugin_mutex);
    std::string k(key);
    std::string v(value);
    
    // Update setting
    plugin_settings[k] = v;
    
    // Apply setting changes
    if (k == "enable_background_tasks") {
        bool should_enable = (v == "true");
        
        if (should_enable && !background_thread_running) {
            // Start background thread
            background_thread_running = true;
            if (background_thread.joinable()) {
                background_thread.join();
            }
            background_thread = std::thread(background_task);
        }
        else if (!should_enable && background_thread_running) {
            // Stop background thread
            background_thread_running = false;
            if (background_thread.joinable()) {
                background_thread.join();
            }
        }
    }
    
    std::cout << "Updated setting: " << key << " = " << value << "\n";
    return PLUGIN_SUCCESS;
}

// Free memory allocated by the plugin
extern "C" PLUGIN_API void plugin_free_memory(void* ptr) {
    if (ptr) {
        std::free(ptr);
    }
}
