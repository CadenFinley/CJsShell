/**
 * Node.js plugin example for CJSH
 * 
 * This file generates a C wrapper for a Node.js plugin.
 * To build:
 * 1. Run: node hello_plugin.js
 * 2. The script will generate a C file and compile it.
 * 
 * Requirements:
 * - Node.js
 * - gcc compiler
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Node.js plugin code to be embedded in C
const nodePluginCode = `
'use strict';

// Plugin state
let greeting = "Hello from Node.js!";

// Handle the hello_node command
function handleCommand(args) {
  console.log(\`\${greeting}, world! (from Node.js plugin)\`);
  
  if (args.length > 1) {
    console.log(\`You provided arguments: \${args.slice(1).join(' ')}\`);
  }
  
  return 0; // PLUGIN_SUCCESS
}

// Update a plugin setting
function updateSetting(key, value) {
  if (key === "greeting") {
    greeting = value;
    return 0; // PLUGIN_SUCCESS
  }
  
  return -2; // PLUGIN_ERROR_INVALID_ARGS
}

// Export the plugin functions
module.exports = {
  handleCommand,
  updateSetting
};
`;

// C wrapper template
const cTemplate = `
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <node_api.h>
#include <uv.h>
#include "../../../include/pluginapi.h"

// Embedded Node.js module code
static const char* node_code = 
"{node_code}";

// Plugin state
static napi_env node_env = NULL;
static napi_value node_module = NULL;
static uv_loop_t* node_loop = NULL;
static uv_thread_t node_thread;
static int node_thread_running = 0;

// Static plugin info
static plugin_info_t info = {
    "hello_node",
    "1.0.0",
    "Example plugin in Node.js",
    "CJSH Team",
    PLUGIN_INTERFACE_VERSION
};

// Commands provided by this plugin
static char* commands[] = {"hello_node", "node_hello"};

// Events this plugin subscribes to
static char* events[] = {"main_process_start"};

// Default settings
static plugin_setting_t settings[] = {
    {"greeting", "Hello from Node.js!"}
};

// Helper function to get a function from the module
static napi_value get_module_function(const char* name) {
    napi_value function;
    napi_status status = napi_get_named_property(node_env, node_module, name, &function);
    if (status != napi_ok) {
        return NULL;
    }
    
    napi_valuetype type;
    status = napi_typeof(node_env, function, &type);
    if (status != napi_ok || type != napi_function) {
        return NULL;
    }
    
    return function;
}

// Initialize Node.js environment and load the embedded module
static void node_thread_main(void* arg) {
    // Initialize Node.js
    napi_module_register_by_symbol();
    
    // Create a new environment
    napi_status status = napi_create_environment(NULL, NULL, &node_env);
    if (status != napi_ok) {
        fprintf(stderr, "Failed to create Node.js environment\\n");
        return;
    }
    
    // Create the module from embedded code
    napi_value global, exports;
    status = napi_get_global(node_env, &global);
    if (status != napi_ok) {
        fprintf(stderr, "Failed to get Node.js global object\\n");
        return;
    }
    
    status = napi_create_object(node_env, &exports);
    if (status != napi_ok) {
        fprintf(stderr, "Failed to create exports object\\n");
        return;
    }
    
    // Create a temporary file with the embedded code
    char temp_path[256];
    strcpy(temp_path, "/tmp/cjsh_node_plugin_XXXXXX.js");
    int fd = mkstemps(temp_path, 3);
    if (fd < 0) {
        fprintf(stderr, "Failed to create temporary file\\n");
        return;
    }
    
    write(fd, node_code, strlen(node_code));
    close(fd);
    
    // Load the module
    napi_value result;
    status = napi_load_module_path(node_env, temp_path, &node_module);
    if (status != napi_ok) {
        napi_get_and_clear_last_exception(node_env, &result);
        napi_value error_message;
        napi_coerce_to_string(node_env, result, &error_message);
        size_t error_size;
        char error_buf[1024];
        napi_get_value_string_utf8(node_env, error_message, error_buf, 1024, &error_size);
        fprintf(stderr, "Failed to load Node.js module: %s\\n", error_buf);
        unlink(temp_path);
        return;
    }
    
    unlink(temp_path);  // Remove temporary file
    
    // Run the event loop (this will block until the plugin is shutdown)
    node_loop = uv_default_loop();
    uv_run(node_loop, UV_RUN_DEFAULT);
    
    // Cleanup
    napi_delete_environment(node_env);
    node_env = NULL;
}

// Required plugin API functions
PLUGIN_API plugin_info_t* plugin_get_info() {
    return &info;
}

PLUGIN_API int plugin_initialize() {
    printf("Hello Node.js plugin initializing...\\n");
    
    // Start the Node.js thread
    node_thread_running = 1;
    uv_thread_create(&node_thread, node_thread_main, NULL);
    
    // Give it a moment to initialize
    usleep(100000);  // 100ms
    
    if (node_env == NULL) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    return PLUGIN_SUCCESS;
}

PLUGIN_API void plugin_shutdown() {
    printf("Hello Node.js plugin shutting down...\\n");
    
    // Stop the event loop and join the thread
    if (node_loop != NULL) {
        uv_stop(node_loop);
    }
    
    if (node_thread_running) {
        uv_thread_join(&node_thread);
        node_thread_running = 0;
    }
}

PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
    if (args == NULL || args->count < 1) {
        return PLUGIN_ERROR_INVALID_ARGS;
    }
    
    if (node_env == NULL || node_module == NULL) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    // Convert arguments to a JavaScript array
    napi_value js_args;
    napi_status status = napi_create_array_with_length(node_env, args->count, &js_args);
    if (status != napi_ok) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    for (int i = 0; i < args->count; i++) {
        napi_value js_arg;
        status = napi_create_string_utf8(node_env, args->args[i], NAPI_AUTO_LENGTH, &js_arg);
        if (status != napi_ok) {
            return PLUGIN_ERROR_GENERAL;
        }
        
        status = napi_set_element(node_env, js_args, i, js_arg);
        if (status != napi_ok) {
            return PLUGIN_ERROR_GENERAL;
        }
    }
    
    // Call the handleCommand function
    napi_value handle_command = get_module_function("handleCommand");
    if (handle_command == NULL) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    napi_value result;
    status = napi_call_function(node_env, node_module, handle_command, 1, &js_args, &result);
    if (status != napi_ok) {
        napi_value exception;
        napi_get_and_clear_last_exception(node_env, &exception);
        return PLUGIN_ERROR_GENERAL;
    }
    
    // Get the return value
    int32_t ret_val;
    status = napi_get_value_int32(node_env, result, &ret_val);
    if (status != napi_ok) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    return ret_val;
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
    if (node_env == NULL || node_module == NULL) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    // Call the updateSetting function
    napi_value update_setting = get_module_function("updateSetting");
    if (update_setting == NULL) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    napi_value args[2];
    napi_status status = napi_create_string_utf8(node_env, key, NAPI_AUTO_LENGTH, &args[0]);
    if (status != napi_ok) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    status = napi_create_string_utf8(node_env, value, NAPI_AUTO_LENGTH, &args[1]);
    if (status != napi_ok) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    napi_value result;
    status = napi_call_function(node_env, node_module, update_setting, 2, args, &result);
    if (status != napi_ok) {
        napi_value exception;
        napi_get_and_clear_last_exception(node_env, &exception);
        return PLUGIN_ERROR_GENERAL;
    }
    
    // Get the return value
    int32_t ret_val;
    status = napi_get_value_int32(node_env, result, &ret_val);
    if (status != napi_ok) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    return ret_val;
}

PLUGIN_API void plugin_free_memory(void* ptr) {
    free(ptr);
}
`;

// Generate the C file with embedded Node.js code
function generateCFile() {
    // Escape quote characters in the Node.js code
    const escapedCode = nodePluginCode
        .replace(/\\/g, '\\\\')
        .replace(/"/g, '\\"')
        .replace(/\n/g, '\\n"\n"');
    
    // Generate the C file with embedded Node.js code
    const cCode = cTemplate.replace('{node_code}', escapedCode);
    
    // Write to file
    fs.writeFileSync('hello_node.c', cCode);
    console.log('Generated C file: hello_node.c');
}

// Compile the plugin
function compilePlugin() {
    // Determine the operating system
    const isOSX = process.platform === 'darwin';
    const libExt = isOSX ? 'dylib' : 'so';
    
    // Get Node.js include paths
    const nodeIncludePath = path.dirname(process.execPath) + '/../include/node';
    
    // Compile the shared library
    const cmd = `gcc -shared -fPIC -o hello_node.${libExt} hello_node.c -I${nodeIncludePath} -lnode`;
    
    console.log(`Compiling with command: ${cmd}`);
    try {
        execSync(cmd);
        console.log(`Successfully compiled plugin: hello_node.${libExt}`);
    } catch (error) {
        console.error('Compilation failed!', error.message);
    }
}

// Main
console.log('Generating Node.js plugin...');
generateCFile();
compilePlugin();
console.log('Done. Copy the shared library to your CJSH plugins directory.');
