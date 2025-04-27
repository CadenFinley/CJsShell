use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_void};
use std::sync::Mutex;
use lazy_static::lazy_static;

// Plugin state
struct PluginState {
    greeting: String,
}

lazy_static! {
    static ref PLUGIN_STATE: Mutex<PluginState> = Mutex::new(PluginState {
        greeting: "Hello from Rust!".to_string(),
    });
}

// Plugin info (static)
static mut PLUGIN_INFO: plugin_info_t = plugin_info_t {
    name: std::ptr::null_mut(),
    version: std::ptr::null_mut(),
    description: std::ptr::null_mut(),
    author: std::ptr::null_mut(),
    interface_version: 2,
};

static mut COMMANDS: [*mut c_char; 2] = [std::ptr::null_mut(), std::ptr::null_mut()];
static mut EVENTS: [*mut c_char; 1] = [std::ptr::null_mut()];
static mut SETTINGS: [plugin_setting_t; 1] = [plugin_setting_t {
    key: std::ptr::null_mut(),
    value: std::ptr::null_mut(),
}];

// Plugin API struct definitions
#[repr(C)]
pub struct plugin_info_t {
    name: *mut c_char,
    version: *mut c_char,
    description: *mut c_char,
    author: *mut c_char,
    interface_version: c_int,
}

#[repr(C)]
pub struct plugin_setting_t {
    key: *mut c_char,
    value: *mut c_char,
}

#[repr(C)]
pub struct plugin_args_t {
    args: *mut *mut c_char,
    count: c_int,
    position: c_int,
}

// Initialize static strings
unsafe fn init_static_strings() {
    // Initialize plugin info
    PLUGIN_INFO.name = CString::new("hello_rust").unwrap().into_raw();
    PLUGIN_INFO.version = CString::new("1.0.0").unwrap().into_raw();
    PLUGIN_INFO.description = CString::new("Example plugin in Rust").unwrap().into_raw();
    PLUGIN_INFO.author = CString::new("CJSH Team").unwrap().into_raw();
    
    // Initialize commands
    COMMANDS[0] = CString::new("hello_rust").unwrap().into_raw();
    COMMANDS[1] = CString::new("rust_hello").unwrap().into_raw();
    
    // Initialize events
    EVENTS[0] = CString::new("main_process_start").unwrap().into_raw();
    
    // Initialize settings
    SETTINGS[0].key = CString::new("greeting").unwrap().into_raw();
    SETTINGS[0].value = CString::new("Hello from Rust!").unwrap().into_raw();
}

// Exported plugin API functions
#[no_mangle]
pub extern "C" fn plugin_get_info() -> *mut plugin_info_t {
    unsafe {
        init_static_strings();
        &mut PLUGIN_INFO
    }
}

#[no_mangle]
pub extern "C" fn plugin_initialize() -> c_int {
    println!("Hello Rust plugin initializing...");
    0 // PLUGIN_SUCCESS
}

#[no_mangle]
pub extern "C" fn plugin_shutdown() {
    println!("Hello Rust plugin shutting down...");
}

#[no_mangle]
pub extern "C" fn plugin_handle_command(args: *mut plugin_args_t) -> c_int {
    if args.is_null() {
        return -2; // PLUGIN_ERROR_INVALID_ARGS
    }
    
    let greeting = PLUGIN_STATE.lock().unwrap().greeting.clone();
    println!("{}, world! (from Rust plugin)", greeting);
    
    unsafe {
        let args_ref = &*args;
        if args_ref.count > 1 {
            print!("You provided arguments: ");
            for i in 1..args_ref.count {
                let arg = CStr::from_ptr(*args_ref.args.offset(i as isize));
                print!("{} ", arg.to_string_lossy());
            }
            println!();
        }
    }
    
    0 // PLUGIN_SUCCESS
}

#[no_mangle]
pub extern "C" fn plugin_get_commands(count: *mut c_int) -> *mut *mut c_char {
    unsafe {
        *count = COMMANDS.len() as c_int;
        COMMANDS.as_mut_ptr()
    }
}

#[no_mangle]
pub extern "C" fn plugin_get_subscribed_events(count: *mut c_int) -> *mut *mut c_char {
    unsafe {
        *count = EVENTS.len() as c_int;
        EVENTS.as_mut_ptr()
    }
}

#[no_mangle]
pub extern "C" fn plugin_get_default_settings(count: *mut c_int) -> *mut plugin_setting_t {
    unsafe {
        *count = SETTINGS.len() as c_int;
        SETTINGS.as_mut_ptr()
    }
}

#[no_mangle]
pub extern "C" fn plugin_update_setting(key: *const c_char, value: *const c_char) -> c_int {
    if key.is_null() || value.is_null() {
        return -2; // PLUGIN_ERROR_INVALID_ARGS
    }
    
    unsafe {
        let key_str = CStr::from_ptr(key).to_string_lossy();
        let value_str = CStr::from_ptr(value).to_string_lossy();
        
        if key_str == "greeting" {
            let mut state = PLUGIN_STATE.lock().unwrap();
            state.greeting = value_str.to_string();
            return 0; // PLUGIN_SUCCESS
        }
    }
    
    -2 // PLUGIN_ERROR_INVALID_ARGS
}

#[no_mangle]
pub extern "C" fn plugin_free_memory(ptr: *mut c_void) {
    if !ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(ptr as *mut c_char);
        }
    }
}

// Build with:
// cargo build --release
// The resulting library will be in target/release/libhello_rust.so (or .dylib on macOS)
