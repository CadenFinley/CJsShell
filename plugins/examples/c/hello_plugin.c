#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../include/pluginapi.h"

// Static plugin info
static plugin_info_t info = {
    "hello_c",
    "1.0.0",
    "Example plugin in C",
    "CJSH Team",
    PLUGIN_INTERFACE_VERSION
};

// Commands provided by this plugin
static char* commands[] = {"hello", "hello_c"};

// Events this plugin subscribes to
static char* events[] = {"main_process_start"};

// Default settings
static plugin_setting_t settings[] = {
    {"greeting", "Hello from C!"}
};

// Current greeting setting
static char greeting[256] = "Hello from C!";

// Required plugin API functions
PLUGIN_API plugin_info_t* plugin_get_info() {
    return &info;
}

PLUGIN_API int plugin_initialize() {
    printf("Hello C plugin initializing...\n");
    return PLUGIN_SUCCESS;
}

PLUGIN_API void plugin_shutdown() {
    printf("Hello C plugin shutting down...\n");
}

PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
    if (args == NULL || args->count < 1) {
        return PLUGIN_ERROR_INVALID_ARGS;
    }
    
    printf("%s, world! (from C plugin)\n", greeting);
    
    if (args->count > 1) {
        printf("You provided arguments: ");
        for (int i = 1; i < args->count; i++) {
            printf("%s ", args->args[i]);
        }
        printf("\n");
    }
    
    return PLUGIN_SUCCESS;
}

PLUGIN_API char** plugin_get_commands(int* count) {
    *count = sizeof(commands) / sizeof(char*);
    return commands;
}

PLUGIN_API char** plugin_get_subscribed_events(int* count) {
    *count = sizeof(events) / sizeof(char*);
    return events;
}

PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
    *count = sizeof(settings) / sizeof(plugin_setting_t);
    return settings;
}

PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
    if (strcmp(key, "greeting") == 0) {
        strncpy(greeting, value, sizeof(greeting) - 1);
        greeting[sizeof(greeting) - 1] = '\0';
        return PLUGIN_SUCCESS;
    }
    return PLUGIN_ERROR_INVALID_ARGS;
}

PLUGIN_API void plugin_free_memory(void* ptr) {
    free(ptr);
}

// Build command:
// gcc -shared -fPIC -o hello_c.so hello_plugin.c
