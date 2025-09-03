// Example plugin demonstrating prompt variable registration
#include <cstring>
#include <cstdlib>
#include "pluginapi.h"

plugin_string_t generate_callback(){
  //generate random string to callback
  const char available_chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const int string_length = 10;
  char* random_string = (char*)std::malloc(string_length + 1);
  for (int i = 0; i < string_length; i++) {
    random_string[i] = available_chars[std::rand() % (sizeof(available_chars) - 1)];
  }
  random_string[string_length] = '\0';
  plugin_string_t result = { random_string, string_length };
  return result;
}

// Callback that returns the custom prompt variable value
static plugin_string_t mytag_callback() {
    plugin_string_t generated = generate_callback();
    char* data = (char*)std::malloc(generated.length + 1);
    std::memcpy(data, generated.data, generated.length);
    plugin_string_t result = { data, generated.length };
    return result;
}

// Required plugin information
extern "C" PLUGIN_API plugin_info_t* plugin_get_info() {
    static plugin_info_t info = {
        (char*)"plugin_test",           // name
        (char*)"0.1.0",                // version
        (char*)"Test prompt variable plugin", // description
        (char*)"caden finley",             // author
        PLUGIN_INTERFACE_VERSION        // interface version
    };
    return &info;
}

// Initialize plugin: register the prompt variable
extern "C" PLUGIN_API int plugin_initialize() {
    // Register {MYTAG} so it can be used in themes
    plugin_register_prompt_variable("MYTAG", mytag_callback);
    return PLUGIN_SUCCESS;
}

// Clean up if needed
extern "C" PLUGIN_API void plugin_shutdown() {
}

// No commands provided by this plugin
extern "C" PLUGIN_API int plugin_handle_command(plugin_args_t* /*args*/) {
    return PLUGIN_SUCCESS;
}
extern "C" PLUGIN_API char** plugin_get_commands(int* count) {
    *count = 0;
    return nullptr;
}

// No events subscribed
extern "C" PLUGIN_API char** plugin_get_subscribed_events(int* count) {
    *count = 0;
    return nullptr;
}

// No settings
extern "C" PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
    *count = 0;
    return nullptr;
}
extern "C" PLUGIN_API int plugin_update_setting(const char* /*key*/, const char* /*value*/) {
    return PLUGIN_ERROR_NOT_IMPLEMENTED;
}

// Free memory allocated by the plugin
extern "C" PLUGIN_API void plugin_free_memory(void* ptr) {
    std::free(ptr);
}