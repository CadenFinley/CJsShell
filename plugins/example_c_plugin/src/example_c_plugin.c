#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/pluginapi.h"

// Define plugin name and version constants
#define PLUGIN_NAME "example_c_plugin"
#define PLUGIN_VERSION "1.0.0"

// Global variables for plugin state
static int command_count = 0;
static time_t plugin_start_time;
static char current_theme[64] = "default";

// Forward declarations of prompt variable callbacks
static plugin_string_t command_counter_callback();
static plugin_string_t uptime_callback();
static plugin_string_t current_theme_callback();

// Helper function to create a heap-allocated copy of a string
char* strdup_wrapper(const char* src) {
  if (!src)
    return NULL;
  size_t len = strlen(src) + 1;
  char* dest = (char*)PLUGIN_MALLOC(len);
  if (dest) {
    memcpy(dest, src, len);
  }
  return dest;
}

//
// Prompt variable callback implementations
//

static plugin_string_t command_counter_callback() {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%d", command_count);

  plugin_string_t result;
  result.data = strdup_wrapper(buffer);
  result.length = strlen(buffer);
  result.capacity = result.length + 1;

  return result;
}

static plugin_string_t uptime_callback() {
  time_t now = time(NULL);
  double diff = difftime(now, plugin_start_time);

  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.0f seconds", diff);

  plugin_string_t result;
  result.data = strdup_wrapper(buffer);
  result.length = strlen(buffer);
  result.capacity = result.length + 1;

  return result;
}

static plugin_string_t current_theme_callback() {
  plugin_string_t result;
  result.data = strdup_wrapper(current_theme);
  result.length = strlen(current_theme);
  result.capacity = result.length + 1;

  return result;
}

//
// Plugin API Implementation
//

// Get plugin information
PLUGIN_API plugin_info_t* plugin_get_info() {
  static plugin_info_t info = {PLUGIN_NAME, PLUGIN_VERSION,
                               "A simple example plugin written in C for CJSH",
                               "GitHub Copilot", PLUGIN_INTERFACE_VERSION};
  return &info;
}

// Validate plugin (optional but recommended)
PLUGIN_API plugin_validation_t plugin_validate() {
  plugin_validation_t result = {PLUGIN_SUCCESS, NULL};
  
  // Perform self-validation here
  if (strlen(PLUGIN_NAME) == 0) {
    result.status = PLUGIN_ERROR_GENERAL;
    result.error_message = strdup_wrapper("Plugin name is empty");
    return result;
  }
  
  if (strlen(PLUGIN_VERSION) == 0) {
    result.status = PLUGIN_ERROR_GENERAL;
    result.error_message = strdup_wrapper("Plugin version is empty");
    return result;
  }
  
  return result;
}

// Initialize the plugin
PLUGIN_API int plugin_initialize() {
  // Record start time for uptime calculation
  plugin_start_time = time(NULL);

  // Reset command counter
  command_count = 0;

  // Register prompt variables
  plugin_register_prompt_variable("CMD_COUNT", command_counter_callback);
  plugin_register_prompt_variable("PLUGIN_UPTIME", uptime_callback);
  plugin_register_prompt_variable("CURRENT_THEME", current_theme_callback);

  printf("Example C Plugin initialized successfully!\n");
  return PLUGIN_SUCCESS;
}

// Shutdown the plugin
PLUGIN_API void plugin_shutdown() {
  printf("Example C Plugin shut down.\n");
}

// Handle commands
PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
  if (args->count < 1) {
    return PLUGIN_ERROR_INVALID_ARGS;
  }

  // Increment command counter
  command_count++;

  // Check which command was invoked
  if (strcmp(args->args[0], "hello") == 0) {
    printf("Hello from Example C Plugin!\n");
    return PLUGIN_SUCCESS;
  } else if (strcmp(args->args[0], "counter") == 0) {
    printf("Command counter: %d\n", command_count);
    return PLUGIN_SUCCESS;
  } else if (strcmp(args->args[0], "uptime") == 0) {
    time_t now = time(NULL);
    double diff = difftime(now, plugin_start_time);
    printf("Plugin uptime: %.0f seconds\n", diff);
    return PLUGIN_SUCCESS;
  } else if (strcmp(args->args[0], "echo") == 0) {
    printf("Arguments: ");
    for (int i = 1; i < args->count; i++) {
      printf("%s ", args->args[i]);
    }
    printf("\n");
    return PLUGIN_SUCCESS;
  } else if (strcmp(args->args[0], "theme") == 0) {
    if (args->count > 1) {
      strncpy(current_theme, args->args[1], sizeof(current_theme) - 1);
      current_theme[sizeof(current_theme) - 1] = '\0';
      printf("Theme set to: %s\n", current_theme);
    } else {
      printf("Current theme: %s\n", current_theme);
    }
    return PLUGIN_SUCCESS;
  } else if (strcmp(args->args[0], "help") == 0) {
    printf("Available commands:\n");
    printf("  hello   - Print a greeting message\n");
    printf("  counter - Show how many commands have been executed\n");
    printf("  uptime  - Show plugin uptime in seconds\n");
    printf("  echo    - Echo back all arguments\n");
    printf("  theme   - Get or set the current theme\n");
    printf("  help    - Show this help message\n");
    return PLUGIN_SUCCESS;
  }

  printf("Unknown command: %s\n", args->args[0]);
  return PLUGIN_ERROR_INVALID_ARGS;
}

// Get commands provided by this plugin
PLUGIN_API char** plugin_get_commands(int* count) {
  if (!count) return NULL;
  
  *count = 6;

  // Allocate memory for the array of command strings
  char** commands = (char**)PLUGIN_MALLOC(*count * sizeof(char*));
  if (!commands) {
    *count = 0;
    return NULL;
  }

  // Allocate memory for each command string
  commands[0] = strdup_wrapper("hello");
  commands[1] = strdup_wrapper("counter");
  commands[2] = strdup_wrapper("uptime");
  commands[3] = strdup_wrapper("echo");
  commands[4] = strdup_wrapper("theme");
  commands[5] = strdup_wrapper("help");

  // Check for allocation failures
  for (int i = 0; i < *count; i++) {
    if (!commands[i]) {
      // Free any successfully allocated strings
      for (int j = 0; j < i; j++) {
        PLUGIN_FREE(commands[j]);
      }
      PLUGIN_FREE(commands);
      *count = 0;
      return NULL;
    }
  }

  return commands;
}

// Get events this plugin subscribes to
PLUGIN_API char** plugin_get_subscribed_events(int* count) {
  if (!count) return NULL;
  
  *count = 4;

  // Allocate memory for the array of event strings
  char** events = (char**)PLUGIN_MALLOC(*count * sizeof(char*));
  if (!events) {
    *count = 0;
    return NULL;
  }

  // Allocate memory for each event string
  events[0] = strdup_wrapper("main_process_start");
  events[1] = strdup_wrapper("main_process_end");
  events[2] = strdup_wrapper("plugin_enabled");
  events[3] = strdup_wrapper("plugin_disabled");

  // Check for allocation failures
  for (int i = 0; i < *count; i++) {
    if (!events[i]) {
      // Free any successfully allocated strings
      for (int j = 0; j < i; j++) {
        PLUGIN_FREE(events[j]);
      }
      PLUGIN_FREE(events);
      *count = 0;
      return NULL;
    }
  }

  return events;
}

// Get default plugin settings
PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
  if (!count) return NULL;
  
  *count = 2;

  // Allocate memory for the settings array
  plugin_setting_t* settings =
      (plugin_setting_t*)PLUGIN_MALLOC(*count * sizeof(plugin_setting_t));
  if (!settings) {
    *count = 0;
    return NULL;
  }

  // Set up the settings
  settings[0].key = strdup_wrapper("default_theme");
  settings[0].value = strdup_wrapper("default");

  settings[1].key = strdup_wrapper("display_command_count");
  settings[1].value = strdup_wrapper("true");

  // Check for allocation failures
  if (!settings[0].key || !settings[0].value || 
      !settings[1].key || !settings[1].value) {
    if (settings[0].key) PLUGIN_FREE(settings[0].key);
    if (settings[0].value) PLUGIN_FREE(settings[0].value);
    if (settings[1].key) PLUGIN_FREE(settings[1].key);
    if (settings[1].value) PLUGIN_FREE(settings[1].value);
    PLUGIN_FREE(settings);
    *count = 0;
    return NULL;
  }

  return settings;
}

// Update a plugin setting
PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
  if (!key || !value) {
    return PLUGIN_ERROR_INVALID_ARGS;
  }

  if (strcmp(key, "default_theme") == 0) {
    strncpy(current_theme, value, sizeof(current_theme) - 1);
    current_theme[sizeof(current_theme) - 1] = '\0';
    printf("Theme set to: %s\n", current_theme);
    return PLUGIN_SUCCESS;
  }

  printf("Unknown setting: %s\n", key);
  return PLUGIN_ERROR_INVALID_ARGS;
}

// Free memory allocated by the plugin
PLUGIN_API void plugin_free_memory(void* ptr) {
  if (ptr) {
    PLUGIN_FREE(ptr);
  }
}
