#include "../include/pluginapi.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

// Basic plugin information
static plugin_info_t plugin_info = {
    strdup("HelloPlugin"),                  // name
    strdup("1.0.0"),                        // version
    strdup("GitHub Copilot"),               // author
    strdup("A simple hello world plugin"),  // description
    PLUGIN_INTERFACE_VERSION                // interface_version
};

// Default settings
static plugin_setting_t default_settings[] = {
    {strdup("greeting"), strdup("Hello from the plugin!")},
    {strdup("farewell"), strdup("Goodbye from the plugin!")}
};

// Current greeting (can be changed through settings)
static std::string current_greeting = "Hello from the plugin!";
static std::string current_farewell = "Goodbye from the plugin!";

// Commands this plugin supports
static const char* supported_commands[] = {
    "hello",
    "greet",
    "farewell"
};

// Events this plugin subscribes to - now including all main process loop events
static const char* subscribed_events[] = {
    "plugin_enabled",
    "plugin_disabled",
    "main_process_pre_run",
    "main_process_start",
    "main_process_command_processed",
    "main_process_end"
};

// Plugin API Implementation

extern "C" {

plugin_info_t* plugin_get_info() {
    return &plugin_info;
}

int plugin_initialize() {
    std::cout << "HelloPlugin initialized!" << std::endl;
    return PLUGIN_SUCCESS;
}

int plugin_shutdown() {
    std::cout << "HelloPlugin shutdown!" << std::endl;
    return PLUGIN_SUCCESS;
}

int plugin_handle_command(plugin_args_t* args) {
    if (args->count < 1) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    // Check if this is an event
    if (strcmp(args->args[0], "event") == 0 && args->count >= 3) {
        const char* event_name = args->args[1];
        const char* event_data = args->args[2];
        
        if (strcmp(event_name, "plugin_enabled") == 0) {
            std::cout << "HelloPlugin noticed that plugin '" << event_data << "' was enabled!" << std::endl;
            return PLUGIN_SUCCESS;
        }
        else if (strcmp(event_name, "plugin_disabled") == 0) {
            std::cout << "HelloPlugin noticed that plugin '" << event_data << "' was disabled!" << std::endl;
            return PLUGIN_SUCCESS;
        }
        else if (strcmp(event_name, "main_process_pre_run") == 0) {
            std::cout << "HelloPlugin: Main process loop is about to start" << std::endl;
            return PLUGIN_SUCCESS;
        }
        else if (strcmp(event_name, "main_process_start") == 0) {
            // This would be too verbose to output every time
            // std::cout << "HelloPlugin: Main process loop iteration starting" << std::endl;
            return PLUGIN_SUCCESS;
        }
        else if (strcmp(event_name, "main_process_command_processed") == 0) {
            std::cout << "HelloPlugin: Command processed: " << event_data << std::endl;
            return PLUGIN_SUCCESS;
        }
        else if (strcmp(event_name, "main_process_end") == 0) {
            // This would be too verbose to output every time
            // std::cout << "HelloPlugin: Main process loop iteration ending" << std::endl;
            return PLUGIN_SUCCESS;
        }
        
        return PLUGIN_ERROR_GENERAL;
    }
    
    // Handle regular commands
    const char* command = args->args[args->position];
    
    if (strcmp(command, "hello") == 0) {
        std::cout << current_greeting << std::endl;
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(command, "greet") == 0) {
        if (args->count > args->position + 1) {
            std::cout << current_greeting << " " << args->args[args->position + 1] << "!" << std::endl;
        } else {
            std::cout << current_greeting << " user!" << std::endl;
        }
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(command, "farewell") == 0) {
        std::cout << current_farewell << std::endl;
        return PLUGIN_SUCCESS;
    }
    
    return PLUGIN_ERROR_GENERAL;
}

char** plugin_get_commands(int* count) {
    *count = sizeof(supported_commands) / sizeof(supported_commands[0]);
    char** commands = new char*[*count];
    
    for (int i = 0; i < *count; i++) {
        commands[i] = strdup(supported_commands[i]);
    }
    
    return commands;
}

char** plugin_get_subscribed_events(int* count) {
    *count = sizeof(subscribed_events) / sizeof(subscribed_events[0]);
    char** events = new char*[*count];
    
    for (int i = 0; i < *count; i++) {
        events[i] = strdup(subscribed_events[i]);
    }
    
    return events;
}

plugin_setting_t* plugin_get_default_settings(int* count) {
    *count = sizeof(default_settings) / sizeof(default_settings[0]);
    return default_settings;
}

int plugin_update_setting(const char* key, const char* value) {
    if (strcmp(key, "greeting") == 0) {
        current_greeting = value;
        return PLUGIN_SUCCESS;
    }
    else if (strcmp(key, "farewell") == 0) {
        current_farewell = value;
        return PLUGIN_SUCCESS;
    }
    
    return PLUGIN_ERROR_GENERAL;
}

void plugin_free_memory(void* ptr) {
    free(ptr);
}

} // extern "C"
