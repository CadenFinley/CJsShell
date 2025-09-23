#![allow(non_snake_case)]

mod pluginapi;

use chrono::Local;
use lazy_static::lazy_static;
use libc::{c_char, c_int, c_void, free, malloc};
use pluginapi::*;
use rand::seq::SliceRandom;
use rand::thread_rng;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

// Create safe wrappers around C types and functions

// Helper function to create a Rust string from a C string
unsafe fn c_str_to_string(s: *const c_char) -> String {
    if s.is_null() {
        return String::new();
    }
    CStr::from_ptr(s).to_string_lossy().into_owned()
}

// Helper function to convert a Rust string to a heap-allocated C string
fn string_to_c_str(s: &str) -> *mut c_char {
    let cs = CString::new(s).unwrap();
    let ptr = cs.as_ptr();
    let len = s.len();
    
    unsafe {
        let new_ptr = malloc(len + 1) as *mut c_char;
        if !new_ptr.is_null() {
            ptr::copy_nonoverlapping(ptr, new_ptr, len + 1);
        }
        new_ptr
    }
}

// Helper function to create a plugin_string_t from a Rust string
fn create_plugin_string(s: &str) -> plugin_string_t {
    let data = string_to_c_str(s);
    plugin_string_t {
        data,
        length: s.len() as c_int,
        capacity: (s.len() + 1) as c_int,
    }
}

// Global state for the plugin
struct PluginState {
    settings: HashMap<String, String>,
    command_history: Vec<String>,
    start_time: Instant,
    background_thread_running: Arc<AtomicBool>,
}

lazy_static! {
    static ref PLUGIN_STATE: Mutex<PluginState> = Mutex::new(PluginState {
        settings: HashMap::new(),
        command_history: Vec::new(),
        start_time: Instant::now(),
        background_thread_running: Arc::new(AtomicBool::new(false)),
    });
}

// Callback functions for prompt variables

#[no_mangle]
extern "C" fn current_time_callback() -> plugin_string_t {
    let time_str = Local::now().format("%c").to_string();
    create_plugin_string(&time_str)
}

#[no_mangle]
extern "C" fn uptime_callback() -> plugin_string_t {
    let uptime = {
        let state = PLUGIN_STATE.lock().unwrap();
        state.start_time.elapsed().as_secs()
    };
    let uptime_str = format!("{}s", uptime);
    create_plugin_string(&uptime_str)
}

#[no_mangle]
extern "C" fn random_quote_callback() -> plugin_string_t {
    let quotes = [
        "The only way to do great work is to love what you do.",
        "Life is what happens when you're busy making other plans.",
        "The future belongs to those who believe in the beauty of their dreams.",
        "The purpose of our lives is to be happy.",
        "Get busy living or get busy dying.",
    ];
    
    let quote = quotes.choose(&mut thread_rng()).unwrap_or(&"");
    create_plugin_string(quote)
}

// Background task function
fn background_task(running: Arc<AtomicBool>) {
    while running.load(Ordering::Relaxed) {
        // In a real plugin, this could update status information, check resources, etc.
        thread::sleep(Duration::from_secs(5));
    }
}

// Helper function to join arguments into a string
fn join_args(args: &plugin_args_t, start_pos: usize, separator: &str) -> String {
    let mut result = String::new();
    
    unsafe {
        for i in start_pos..args.count as usize {
            if i > start_pos {
                result.push_str(separator);
            }
            let arg = *args.args.offset(i as isize);
            result.push_str(&c_str_to_string(arg));
        }
    }
    
    result
}

// Plugin API Implementation

#[no_mangle]
pub extern "C" fn plugin_get_info() -> *mut plugin_info_t {
    // Use a Box to allocate on the heap, then leak it to keep it alive
    // (The shell doesn't call free on this, it should live for the plugin's lifetime)
    static mut INFO_INITIALIZED: bool = false;
    static mut NAME: *mut c_char = std::ptr::null_mut();
    static mut VERSION: *mut c_char = std::ptr::null_mut();
    static mut DESCRIPTION: *mut c_char = std::ptr::null_mut();
    static mut AUTHOR: *mut c_char = std::ptr::null_mut();
    
    unsafe {
        if !INFO_INITIALIZED {
            NAME = string_to_c_str("example_rust_plugin");
            VERSION = string_to_c_str("1.0.0");
            DESCRIPTION = string_to_c_str("A comprehensive plugin demonstrating all CJSH plugin features in Rust");
            AUTHOR = string_to_c_str("Caden Finley");
            INFO_INITIALIZED = true;
        }
        
        // Create a new instance each time with the static pointers
        let info = Box::new(plugin_info_t {
            name: NAME,
            version: VERSION,
            description: DESCRIPTION,
            author: AUTHOR,
            interface_version: PLUGIN_INTERFACE_VERSION,
        });
        
        Box::into_raw(info)
    }
}

#[no_mangle]
pub extern "C" fn plugin_initialize() -> c_int {
    let mut state = PLUGIN_STATE.lock().unwrap();
    
    // Try to register prompt variables
    // Note: plugin_register_prompt_variable might not be available at build time
    // but will be resolved at runtime by the shell
    unsafe {
        let result = std::panic::catch_unwind(|| {
            plugin_register_prompt_variable(
                string_to_c_str("CURRENT_TIME"),
                Some(current_time_callback),
            );
            plugin_register_prompt_variable(
                string_to_c_str("PLUGIN_UPTIME"),
                Some(uptime_callback),
            );
            plugin_register_prompt_variable(
                string_to_c_str("RANDOM_QUOTE"),
                Some(random_quote_callback),
            );
        });
        
        if result.is_err() {
            println!("Warning: Could not register prompt variables - this is normal during build");
        }
    }
    
    // Reset plugin state
    state.command_history.clear();
    state.start_time = Instant::now();
    
    // Start background thread
    let running = state.background_thread_running.clone();
    running.store(true, Ordering::Relaxed);
    
    thread::spawn(move || {
        background_task(running);
    });
    
    println!("All Features Rust Plugin initialized successfully!");
    
    plugin_error_t::PLUGIN_SUCCESS as c_int
}

#[no_mangle]
pub extern "C" fn plugin_shutdown() {
    let mut state = PLUGIN_STATE.lock().unwrap();
    
    // Stop background thread
    state.background_thread_running.store(false, Ordering::Relaxed);
    
    // Clear plugin state
    state.command_history.clear();
    
    println!("All Features Rust Plugin shut down.");
}

#[no_mangle]
pub extern "C" fn plugin_handle_command(args: *mut plugin_args_t) -> c_int {
    unsafe {
        if (*args).count < 1 {
            return plugin_error_t::PLUGIN_ERROR_INVALID_ARGS as c_int;
        }
        
        let mut state = PLUGIN_STATE.lock().unwrap();
        
        // Store command in history
        let cmd = c_str_to_string(*(*args).args);
        state.command_history.push(cmd.clone());
        
        // Process different commands
        if cmd == "hello" {
            println!("Hello from All Features Rust Plugin!");
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else if cmd == "echo" {
            let text = join_args(&*args, 1, " ");
            println!("Echo: {}", text);
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else if cmd == "settings" {
            println!("Current plugin settings:");
            for (key, value) in &state.settings {
                println!("  {} = {}", key, value);
            }
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else if cmd == "history" {
            println!("Command history:");
            for (i, cmd) in state.command_history.iter().enumerate() {
                println!("  {}: {}", i, cmd);
            }
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else if cmd == "quote" {
            let quote = random_quote_callback();
            println!("Quote: {}", c_str_to_string(quote.data));
            free(quote.data as *mut c_void);
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else if cmd == "time" {
            let time = current_time_callback();
            println!("Current time: {}", c_str_to_string(time.data));
            free(time.data as *mut c_void);
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else if cmd == "uptime" {
            let uptime = uptime_callback();
            println!("Plugin uptime: {}", c_str_to_string(uptime.data));
            free(uptime.data as *mut c_void);
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else if cmd == "help" {
            println!("Available commands:");
            println!("  hello - Print a greeting");
            println!("  echo [text] - Echo back the provided text");
            println!("  settings - Show current plugin settings");
            println!("  history - Show command history");
            println!("  quote - Show a random quote");
            println!("  time - Show current time");
            println!("  uptime - Show plugin uptime");
            println!("  help - Show this help message");
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else if cmd == "event" {
            if (*args).count > 1 {
                let event = c_str_to_string(*(*args).args.offset(1));
                print!("Event received: {}\nWith args: ", event);
                
                for i in 2..(*args).count {
                    let arg = c_str_to_string(*(*args).args.offset(i as isize));
                    print!("{}{}", arg, if i < (*args).count - 1 { ", " } else { "\n" });
                }
            }
            plugin_error_t::PLUGIN_SUCCESS as c_int
        } else {
            eprintln!("Unknown command: {}", cmd);
            plugin_error_t::PLUGIN_ERROR_INVALID_ARGS as c_int
        }
    }
}

#[no_mangle]
pub extern "C" fn plugin_get_commands(count: *mut c_int) -> *mut *mut c_char {
    let commands = vec![
        "hello", "echo", "settings", "history", 
        "quote", "time", "uptime", "help"
    ];
    
    unsafe {
        *count = commands.len() as c_int;
        
        let result = malloc(commands.len() * std::mem::size_of::<*mut c_char>()) as *mut *mut c_char;
        
        for (i, cmd) in commands.iter().enumerate() {
            *result.offset(i as isize) = string_to_c_str(cmd);
        }
        
        result
    }
}

#[no_mangle]
pub extern "C" fn plugin_get_subscribed_events(count: *mut c_int) -> *mut *mut c_char {
    let events = vec![
        "main_process_pre_run", "main_process_start",
        "main_process_end", "main_process_command_processed",
        "plugin_enabled", "plugin_disabled"
    ];
    
    unsafe {
        *count = events.len() as c_int;
        
        let result = malloc(events.len() * std::mem::size_of::<*mut c_char>()) as *mut *mut c_char;
        
        for (i, event) in events.iter().enumerate() {
            *result.offset(i as isize) = string_to_c_str(event);
        }
        
        result
    }
}

#[no_mangle]
pub extern "C" fn plugin_get_default_settings(count: *mut c_int) -> *mut plugin_setting_t {
    let settings = vec![
        ("show_time_in_prompt", "true"),
        ("quote_refresh_interval", "60"),
        ("enable_background_tasks", "true"),
    ];
    
    unsafe {
        *count = settings.len() as c_int;
        
        let result = malloc(settings.len() * std::mem::size_of::<plugin_setting_t>()) as *mut plugin_setting_t;
        
        for (i, (key, value)) in settings.iter().enumerate() {
            let setting = &mut *result.offset(i as isize);
            setting.key = string_to_c_str(key);
            setting.value = string_to_c_str(value);
            
            // Store settings in our map for later use
            let mut state = PLUGIN_STATE.lock().unwrap();
            state.settings.insert(key.to_string(), value.to_string());
        }
        
        result
    }
}

#[no_mangle]
pub extern "C" fn plugin_update_setting(key: *const c_char, value: *const c_char) -> c_int {
    if key.is_null() || value.is_null() {
        return plugin_error_t::PLUGIN_ERROR_INVALID_ARGS as c_int;
    }
    
    let key_str = unsafe { c_str_to_string(key) };
    let value_str = unsafe { c_str_to_string(value) };
    
    let mut state = PLUGIN_STATE.lock().unwrap();
    
    // Update setting
    state.settings.insert(key_str.clone(), value_str.clone());
    
    // Apply setting changes
    if key_str == "enable_background_tasks" {
        let should_enable = value_str == "true";
        state.background_thread_running.store(should_enable, Ordering::Relaxed);
    }
    
    println!("Updated setting: {} = {}", key_str, value_str);
    plugin_error_t::PLUGIN_SUCCESS as c_int
}

#[no_mangle]
pub extern "C" fn plugin_free_memory(ptr: *mut c_void) {
    if !ptr.is_null() {
        unsafe {
            free(ptr);
        }
    }
}
