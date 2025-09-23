#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int};

// Replicate the C API from pluginapi.h

#[repr(C)]
pub enum plugin_error_t {
    PLUGIN_SUCCESS = 0,
    PLUGIN_ERROR_GENERAL = -1,
    PLUGIN_ERROR_INVALID_ARGS = -2,
    PLUGIN_ERROR_NOT_IMPLEMENTED = -3,
    PLUGIN_ERROR_OUT_OF_MEMORY = -4,
    PLUGIN_ERROR_NULL_POINTER = -5,
}

#[repr(C)]
pub struct plugin_string_t {
    pub data: *mut c_char,
    pub length: c_int,
    pub capacity: c_int,
}

#[repr(C)]
pub struct plugin_setting_t {
    pub key: *mut c_char,
    pub value: *mut c_char,
}

#[repr(C)]
pub struct plugin_args_t {
    pub args: *mut *mut c_char,
    pub count: c_int,
    pub position: c_int,
}

#[repr(C)]
pub struct plugin_info_t {
    pub name: *mut c_char,
    pub version: *mut c_char,
    pub description: *mut c_char,
    pub author: *mut c_char,
    pub interface_version: c_int,
}

#[repr(C)]
pub struct plugin_validation_t {
    pub status: plugin_error_t,
    pub error_message: *mut c_char,
}

// Type for prompt variable callback functions
pub type plugin_get_prompt_variable_func = Option<unsafe extern "C" fn() -> plugin_string_t>;

// External functions provided by the shell
extern "C" {
    pub fn plugin_register_prompt_variable(
        name: *const c_char, 
        func: plugin_get_prompt_variable_func
    ) -> plugin_error_t;
    
    pub fn plugin_get_plugins_home_directory() -> *mut c_char;
    pub fn plugin_get_plugin_directory(plugin_name: *const c_char) -> *mut c_char;
    pub fn plugin_free_string(str: *mut c_char);
}

// Plugin interface version
pub const PLUGIN_INTERFACE_VERSION: i32 = 3;
