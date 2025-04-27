--[[
Lua plugin example for CJSH using LuaJIT FFI

This file will be used to generate a C wrapper that embeds the Lua code.
Run: lua hello_plugin.lua
This will generate hello_lua.c which should be compiled into a shared library.
--]]

local plugin_lua_code = [[
local ffi = require("ffi")

-- Define the plugin API structures
ffi.cdef[[
typedef struct {
    char* data;
    int length;
} plugin_string_t;

typedef struct {
    char* key;
    char* value;
} plugin_setting_t;

typedef struct {
    char** args;
    int count;
    int position;
} plugin_args_t;

typedef struct {
    char* name;
    char* version;
    char* description;
    char* author;
    int interface_version;
} plugin_info_t;
]]

-- Plugin state
local plugin = {
    greeting = "Hello from Lua!"
}

-- Handle the hello_lua command
function handle_command(args_ptr)
    local args = ffi.cast("plugin_args_t*", args_ptr)
    
    print(plugin.greeting .. ", world! (from Lua plugin)")
    
    if args.count > 1 then
        io.write("You provided arguments: ")
        for i = 1, args.count - 1 do
            local arg = ffi.string(args.args[i])
            io.write(arg .. " ")
        end
        print()
    end
    
    return 0 -- PLUGIN_SUCCESS
end

-- Update a plugin setting
function update_setting(key, value)
    key = ffi.string(key)
    value = ffi.string(value)
    
    if key == "greeting" then
        plugin.greeting = value
        return 0 -- PLUGIN_SUCCESS
    end
    
    return -2 -- PLUGIN_ERROR_INVALID_ARGS
end
]]

-- C wrapper template
local c_template = [[
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../include/pluginapi.h"

// Embedded Lua code
static const char* lua_code = 
"{lua_code}";

// Plugin state
static lua_State* L = NULL;

// Static plugin info
static plugin_info_t info = {
    "hello_lua",
    "1.0.0",
    "Example plugin in Lua",
    "CJSH Team",
    PLUGIN_INTERFACE_VERSION
};

// Commands provided by this plugin
static char* commands[] = {"hello_lua", "lua_hello"};

// Events this plugin subscribes to
static char* events[] = {"main_process_start"};

// Default settings
static plugin_setting_t settings[] = {
    {"greeting", "Hello from Lua!"}
};

// Initialize Lua state and load the embedded module
static int init_lua() {
    if (L != NULL) {
        return 0; // Already initialized
    }
    
    L = luaL_newstate();
    if (L == NULL) {
        fprintf(stderr, "Failed to create Lua state\n");
        return -1;
    }
    
    luaL_openlibs(L);
    
    // Load the Lua code
    if (luaL_dostring(L, lua_code) != 0) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        L = NULL;
        return -1;
    }
    
    return 0;
}

// Cleanup Lua state
static void cleanup_lua() {
    if (L != NULL) {
        lua_close(L);
        L = NULL;
    }
}

// Required plugin API functions
PLUGIN_API plugin_info_t* plugin_get_info() {
    return &info;
}

PLUGIN_API int plugin_initialize() {
    printf("Hello Lua plugin initializing...\n");
    return init_lua();
}

PLUGIN_API void plugin_shutdown() {
    printf("Hello Lua plugin shutting down...\n");
    cleanup_lua();
}

PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
    if (args == NULL || args->count < 1) {
        return PLUGIN_ERROR_INVALID_ARGS;
    }
    
    if (L == NULL) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    // Call Lua handler
    lua_getglobal(L, "handle_command");
    lua_pushlightuserdata(L, args);
    
    if (lua_pcall(L, 1, 1, 0) != 0) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return PLUGIN_ERROR_GENERAL;
    }
    
    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    
    return result;
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
    if (L == NULL) {
        return PLUGIN_ERROR_GENERAL;
    }
    
    // Call Lua handler
    lua_getglobal(L, "update_setting");
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    
    if (lua_pcall(L, 2, 1, 0) != 0) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return PLUGIN_ERROR_GENERAL;
    }
    
    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    
    return result;
}

PLUGIN_API void plugin_free_memory(void* ptr) {
    free(ptr);
}
]]

-- Generate C file with embedded Lua code
local function generate_c_file()
    -- Escape quote characters in the Lua code
    local escaped_code = string.gsub(plugin_lua_code, '"', '\\"')
    escaped_code = string.gsub(escaped_code, "\n", '\\n"\n"')
    
    -- Generate the C file with embedded Lua code
    local c_code = string.gsub(c_template, "{lua_code}", escaped_code)
    
    -- Write to file
    local file = io.open("hello_lua.c", "w")
    if file then
        file:write(c_code)
        file:close()
        print("Generated C file: hello_lua.c")
        return true
    else
        print("Failed to write C file")
        return false
    end
end

-- Compile the plugin
local function compile_plugin()
    local cmd
    
    -- Determine the operating system
    local os_name = io.popen("uname -s"):read("*l")
    if os_name == "Darwin" then  -- macOS
        cmd = "gcc -shared -fPIC -o hello_lua.dylib hello_lua.c -llua"
    else  -- Linux
        cmd = "gcc -shared -fPIC -o hello_lua.so hello_lua.c -llua"
    end
    
    print("Compiling with command: " .. cmd)
    local result = os.execute(cmd)
    
    if result == 0 or result == true then
        print("Successfully compiled plugin")
    else
        print("Compilation failed!")
    end
end

-- Main
if generate_c_file() then
    compile_plugin()
    print("Done. Copy the shared library to your CJSH plugins directory.")
end
]]

-- Write the generation script to a file
local file = io.open("hello_plugin.lua", "w")
if file then
    file:write(plugin_lua_code)
    file:close()
    print("Generated Lua plugin file: hello_plugin.lua")
    print("Run with: luajit hello_plugin.lua")
end
