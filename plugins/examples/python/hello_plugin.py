"""
Python plugin example using ctypes to interface with CJSH

This is a wrapper Python script that generates a C shared library.
The Python code is embedded in the C library as strings and executed dynamically.

To build:
1. Run this script: python hello_plugin.py
2. It will generate and compile hello_python.c into a shared library
"""

import os
import sys
import ctypes
from ctypes import c_char_p, c_int, c_void_p, POINTER, Structure, CFUNCTYPE

# Plugin code that will be embedded in the C file
PYTHON_PLUGIN_CODE = """
import ctypes
import os

# Global state
greeting = "Hello from Python!"

def handle_command(args_count, args_ptr):
    """Handle the hello_python command"""
    global greeting
    
    print(f"{greeting}, world! (from Python plugin)")
    
    if args_count > 1:
        args = []
        for i in range(1, args_count):
            arg = ctypes.cast(args_ptr[i], ctypes.c_char_p).value.decode('utf-8')
            args.append(arg)
        print(f"You provided arguments: {' '.join(args)}")
    
    return 0  # PLUGIN_SUCCESS

def update_setting(key, value):
    """Update a plugin setting"""
    global greeting
    
    key = key.decode('utf-8')
    value = value.decode('utf-8')
    
    if key == "greeting":
        greeting = value
        return 0  # PLUGIN_SUCCESS
    
    return -2  # PLUGIN_ERROR_INVALID_ARGS
"""

# C code template for the Python plugin wrapper
C_TEMPLATE = """
#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../include/pluginapi.h"

// Embedded Python module code
static const char* python_code = 
"{python_code}";

// Plugin state
static PyObject* plugin_module = NULL;

// Static plugin info
static plugin_info_t info = {{
    "hello_python",
    "1.0.0",
    "Example plugin in Python",
    "CJSH Team",
    PLUGIN_INTERFACE_VERSION
}};

// Commands provided by this plugin
static char* commands[] = {{"hello_python", "py_hello"}};

// Events this plugin subscribes to
static char* events[] = {{"main_process_start"}};

// Default settings
static plugin_setting_t settings[] = {{
    {{"greeting", "Hello from Python!"}}
}};

// Initialize Python interpreter and load the embedded module
static int init_python() {{
    if (Py_IsInitialized()) {{
        return 0; // Already initialized
    }}
    
    Py_Initialize();
    if (!Py_IsInitialized()) {{
        fprintf(stderr, "Failed to initialize Python interpreter\\n");
        return -1;
    }}
    
    // Create a new module
    plugin_module = PyModule_New("cjsh_python_plugin");
    if (plugin_module == NULL) {{
        fprintf(stderr, "Failed to create Python module\\n");
        return -1;
    }}
    
    // Execute the embedded Python code in the module's context
    PyObject* dict = PyModule_GetDict(plugin_module);
    PyObject* result = PyRun_String(python_code, Py_file_input, dict, dict);
    if (result == NULL) {{
        PyErr_Print();
        return -1;
    }}
    Py_DECREF(result);
    
    return 0;
}}

// Cleanup Python interpreter
static void cleanup_python() {{
    if (plugin_module != NULL) {{
        Py_DECREF(plugin_module);
        plugin_module = NULL;
    }}
    
    if (Py_IsInitialized()) {{
        Py_Finalize();
    }}
}}

// Call a Python function in the module
static PyObject* call_python_func(const char* func_name, PyObject* args) {{
    if (!Py_IsInitialized() || plugin_module == NULL) {{
        return NULL;
    }}
    
    PyObject* dict = PyModule_GetDict(plugin_module);
    PyObject* func = PyDict_GetItemString(dict, func_name);
    
    if (func == NULL || !PyCallable_Check(func)) {{
        return NULL;
    }}
    
    return PyObject_CallObject(func, args);
}}

// Required plugin API functions
PLUGIN_API plugin_info_t* plugin_get_info() {{
    return &info;
}}

PLUGIN_API int plugin_initialize() {{
    printf("Hello Python plugin initializing...\\n");
    return init_python();
}}

PLUGIN_API void plugin_shutdown() {{
    printf("Hello Python plugin shutting down...\\n");
    cleanup_python();
}}

PLUGIN_API int plugin_handle_command(plugin_args_t* args) {{
    if (args == NULL || args->count < 1) {{
        return PLUGIN_ERROR_INVALID_ARGS;
    }}
    
    if (!Py_IsInitialized() || plugin_module == NULL) {{
        return PLUGIN_ERROR_GENERAL;
    }}
    
    // Call Python handler
    PyObject* pyArgs = Py_BuildValue("(iO)", args->count, 
                                   PyCapsule_New(args->args, NULL, NULL));
    PyObject* result = call_python_func("handle_command", pyArgs);
    Py_DECREF(pyArgs);
    
    if (result == NULL) {{
        PyErr_Print();
        return PLUGIN_ERROR_GENERAL;
    }}
    
    int ret_val = (int)PyLong_AsLong(result);
    Py_DECREF(result);
    
    return ret_val;
}}

PLUGIN_API char** plugin_get_commands(int* count) {{
    *count = sizeof(commands) / sizeof(char*);
    return commands;
}}

PLUGIN_API char** plugin_get_subscribed_events(int* count) {{
    *count = sizeof(events) / sizeof(char*);
    return events;
}}

PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {{
    *count = sizeof(settings) / sizeof(plugin_setting_t);
    return settings;
}}

PLUGIN_API int plugin_update_setting(const char* key, const char* value) {{
    if (!Py_IsInitialized() || plugin_module == NULL) {{
        return PLUGIN_ERROR_GENERAL;
    }}
    
    // Call Python handler
    PyObject* pyArgs = Py_BuildValue("(ss)", key, value);
    PyObject* result = call_python_func("update_setting", pyArgs);
    Py_DECREF(pyArgs);
    
    if (result == NULL) {{
        PyErr_Print();
        return PLUGIN_ERROR_GENERAL;
    }}
    
    int ret_val = (int)PyLong_AsLong(result);
    Py_DECREF(result);
    
    return ret_val;
}}

PLUGIN_API void plugin_free_memory(void* ptr) {{
    free(ptr);
}}
"""

def generate_c_file():
    # Escape quote characters in the Python code
    escaped_code = PYTHON_PLUGIN_CODE.replace('"', '\\"').replace('\n', '\\n"\n"')
    
    # Generate the C file with embedded Python code
    c_code = C_TEMPLATE.format(python_code=escaped_code)
    
    # Write to file
    plugin_dir = os.path.dirname(os.path.abspath(__file__))
    c_file_path = os.path.join(plugin_dir, "hello_python.c")
    
    with open(c_file_path, "w") as f:
        f.write(c_code)
    
    print(f"Generated C file: {c_file_path}")
    return c_file_path

def compile_plugin(c_file_path):
    # Get Python include path
    python_include = os.path.join(sys.prefix, "include", f"python{sys.version_info.major}.{sys.version_info.minor}")
    
    # Determine platform-specific settings
    if sys.platform == "darwin":  # macOS
        lib_ext = "dylib"
    else:  # Linux
        lib_ext = "so"
    
    # Compile the shared library
    plugin_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(plugin_dir, f"hello_python.{lib_ext}")
    
    cmd = f"gcc -shared -fPIC -o {output_path} {c_file_path} -I{python_include} -lpython{sys.version_info.major}.{sys.version_info.minor}"
    
    print(f"Compiling with command: {cmd}")
    if os.system(cmd) == 0:
        print(f"Successfully compiled plugin: {output_path}")
    else:
        print("Compilation failed!")

if __name__ == "__main__":
    c_file_path = generate_c_file()
    compile_plugin(c_file_path)
    print("Done. Copy the shared library to your CJSH plugins directory.")
