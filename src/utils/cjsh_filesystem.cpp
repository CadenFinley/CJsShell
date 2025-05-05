#include "cjsh_filesystem.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef __linux__
#include <string.h>
extern char *program_invocation_name;
#endif

cjsh_filesystem::fs::path cjsh_filesystem::g_cjsh_path;

bool initialize_cjsh_path() {
    char path[PATH_MAX];
    #ifdef __linux__
    if (readlink("/proc/self/exe", path, PATH_MAX) != -1) {
        cjsh_filesystem::g_cjsh_path = path;
        return true;
    }
    #endif
    
    #ifdef __APPLE__
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(path, &size) == 0) {
        char* resolved_path = realpath(path, NULL);
        if (resolved_path != nullptr) {
            cjsh_filesystem::g_cjsh_path = resolved_path;
            free(resolved_path);
            return true;
        } else {
            cjsh_filesystem::g_cjsh_path = path;
            return true;
        }
    }
    #endif
    
    if (cjsh_filesystem::g_cjsh_path.empty()) {
        #ifdef __linux__
        char* exePath = realpath(program_invocation_name, NULL);
        if (exePath) {
            cjsh_filesystem::g_cjsh_path = exePath;
            free(exePath);
            return true;
        } else {
            cjsh_filesystem::g_cjsh_path = "/usr/local/bin/cjsh";
            return true;
        }
        #else
        cjsh_filesystem::g_cjsh_path = "/usr/local/bin/cjsh";
        return true;
        #endif
    }
    cjsh_filesystem::g_cjsh_path = "No path found";
    return false;
}

void temp_cjsh_migration() {
    namespace fs = cjsh_filesystem::fs;
    fs::path old_path = cjsh_filesystem::g_user_home_path / ".cjsh";
    if (fs::exists(old_path) && fs::is_directory(old_path)) {
        std::cout << "Found old ~/.cjsh directory. Migrating to ~/.config/cjsh..." << std::endl;
        
        // Create ~/.config directory if it doesn't exist
        fs::path config_dir = cjsh_filesystem::g_user_home_path / ".config";
        if (!fs::exists(config_dir)) {
            try {
                fs::create_directory(config_dir);
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error creating ~/.config directory: " << e.what() << std::endl;
                return;
            }
        }
        
        // Create ~/.config/cjsh if it doesn't exist
        if (!fs::exists(cjsh_filesystem::g_cjsh_data_path)) {
            try {
                fs::create_directory(cjsh_filesystem::g_cjsh_data_path);
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error creating ~/.config/cjsh directory: " << e.what() << std::endl;
                return;
            }
        }
        
        try {
            // Copy all contents from old directory to new directory
            for (const auto& entry : fs::directory_iterator(old_path)) {
                fs::path target = cjsh_filesystem::g_cjsh_data_path / entry.path().filename();
                
                if (fs::is_directory(entry.path())) {
                    fs::copy(entry.path(), target, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                } else {
                    fs::copy(entry.path(), target, fs::copy_options::overwrite_existing);
                }
            }
            
            std::cout << "Successfully migrated all data from ~/.cjsh to ~/.config/cjsh" << std::endl;
            
            // Optionally, you can remove the old directory after successful migration
            // fs::remove_all(old_path);
            // std::cout << "Removed old ~/.cjsh directory" << std::endl;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error migrating ~/.cjsh to ~/.config/cjsh: " << e.what() << std::endl;
        }
    }
}

bool initialize_cjsh_directories() {
    try {
        namespace fs = cjsh_filesystem::fs;
        if (!fs::exists(cjsh_filesystem::g_cjsh_data_path)) {
            fs::create_directories(cjsh_filesystem::g_cjsh_data_path);
        }
        
        // Check for old .cjsh directory in home path, not current directory
        fs::path old_cjsh_path = cjsh_filesystem::g_user_home_path / ".cjsh";
        if (fs::exists(old_cjsh_path)) {
            temp_cjsh_migration();
            std::cout << "The new directory is located at: " << cjsh_filesystem::g_cjsh_data_path << std::endl;
        }
        if (!fs::exists(cjsh_filesystem::g_cjsh_plugin_path)) {
            fs::create_directories(cjsh_filesystem::g_cjsh_plugin_path);
        }
        if (!fs::exists(cjsh_filesystem::g_cjsh_theme_path)) {
            fs::create_directories(cjsh_filesystem::g_cjsh_theme_path);
        }
        if (!fs::exists(cjsh_filesystem::g_cjsh_colors_path)) {
            fs::create_directories(cjsh_filesystem::g_cjsh_colors_path);
        }
        return true;
    } catch (const cjsh_filesystem::fs::filesystem_error& e) {
        std::cerr << "Error creating cjsh directories: " << e.what() << std::endl;
        return false;
    }
}
