fn main() {
    // Look for cjsh in common directories
    println!("cargo:rustc-link-search=native=/Users/cadenfinley/Documents/GitHub/CJsShell/build");
    println!("cargo:rustc-link-search=native=/usr/local/lib");
    println!("cargo:rustc-link-search=native=/usr/lib");
    
    // Set minimum macOS version to avoid warnings
    if cfg!(target_os = "macos") {
        println!("cargo:rustc-link-arg=-mmacosx-version-min=10.14");
    }
    
    // Don't warn about missing plugin_register_prompt_variable
    if cfg!(target_os = "macos") {
        println!("cargo:rustc-link-arg=-Wl,-undefined,dynamic_lookup");
    } else if cfg!(target_os = "linux") {
        println!("cargo:rustc-link-arg=-Wl,--allow-shlib-undefined");
    }
    
    println!("cargo:rerun-if-changed=build.rs");
}
