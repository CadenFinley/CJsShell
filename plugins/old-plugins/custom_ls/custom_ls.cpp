#include "../include/pluginapi.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <iomanip>
#include <cstring>
#include <ctime>       // For time formatting
#include <sys/stat.h>  // For file stats
#include <pwd.h>       // For user name lookup
#include <grp.h>       // For group name lookup

// Define color codes for terminal output
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"

// Static plugin information
static plugin_info_t plugin_info = {
    (char*)"custom_ls",                  // name
    (char*)"1.0.0",                      // version
    (char*)"Custom ls command with colors and formatting", // description
    (char*)"CJsShell",                   // author
    PLUGIN_INTERFACE_VERSION      // interface version
};

// Custom ls implementation
int custom_ls_command(plugin_args_t* args) {
    std::string path = "."; // Default to current directory
    bool show_hidden = false;
    bool long_format = false;  // New flag for -l option
    bool sort_by_size = false; // New flag for -S option
    
    // Process arguments (simple arg handling)
    for (int i = 1; i < args->count; i++) {
        if (strcmp(args->args[i], "-a") == 0) {
            show_hidden = true;
        } else if (strcmp(args->args[i], "-l") == 0) {
            long_format = true;  // Set long format flag
        } else if (strcmp(args->args[i], "-la") == 0 || strcmp(args->args[i], "-al") == 0) {
            show_hidden = true;
            long_format = true;  // Set long format flag
        } else if (strcmp(args->args[i], "-S") == 0) {
            sort_by_size = true;  // Set sort by size flag
        } else if (args->args[i][0] == '-') {
            std::cerr << "Unknown option: " << args->args[i] << std::endl;
            return PLUGIN_ERROR_INVALID_ARGS;
        }
        else if (args->args[i][0] != '-') {
            path = args->args[i];
        }
    }
    
    try {
        // Collect directory entries first to sort them
        std::vector<std::filesystem::directory_entry> entries;
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (!show_hidden && entry.path().filename().string()[0] == '.') {
                continue; // Skip hidden files unless -a flag is used
            }
            entries.push_back(entry);
        }
        
        // Sort entries based on flags
        if (sort_by_size) {
            // Sort by file size (largest first), directories at the top
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                bool a_is_dir = std::filesystem::is_directory(a);
                bool b_is_dir = std::filesystem::is_directory(b);
                
                // Always put directories first
                if (a_is_dir && !b_is_dir) return true;
                if (!a_is_dir && b_is_dir) return false;
                
                // If both are directories or both are files, sort by size
                if (!a_is_dir && !b_is_dir) {
                    try {
                        return std::filesystem::file_size(a) > std::filesystem::file_size(b);
                    } catch (...) {
                        // In case of error reading file size, fall back to name sorting
                        return a.path().filename() < b.path().filename();
                    }
                }
                
                // If both are directories, sort by name
                return a.path().filename() < b.path().filename();
            });
        } else {
            // Default sort: directories first, then files alphabetically
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                if (std::filesystem::is_directory(a) && !std::filesystem::is_directory(b)) {
                    return true;
                }
                if (!std::filesystem::is_directory(a) && std::filesystem::is_directory(b)) {
                    return false;
                }
                return a.path().filename() < b.path().filename();
            });
        }

        if (long_format) {
            std::cout << std::setw(12) << std::left << "Permissions"
                      << std::setw(10) << "Owner"
                      << std::setw(10) << "Group"
                      << std::setw(12) << std::right << "Size"
                      << std::setw(20) << "Modified"
                      << "  Name" << std::endl;
            std::cout << std::string(80, '-') << std::endl;
        } else {
            std::cout << std::setw(40) << std::left << "Name" 
                      << std::setw(15) << "Size" 
                      << "Type" << std::endl;
            std::cout << std::string(60, '-') << std::endl;
        }
        
        // Display entries
        for (const auto& entry : entries) {
            std::string name = entry.path().filename().string();
            std::string type;
            std::string color;
            
            if (std::filesystem::is_directory(entry)) {
                type = "Directory";
                color = COLOR_BLUE;
            } else if (std::filesystem::is_symlink(entry)) {
                type = "Symlink";
                color = COLOR_CYAN;
            } else if (entry.path().extension() == ".cpp" || 
                       entry.path().extension() == ".h" || 
                       entry.path().extension() == ".hpp" ||
                       entry.path().extension() == ".py" || 
                       entry.path().extension() == ".js" || 
                       entry.path().extension() == ".java" ||
                       entry.path().extension() == ".cs" || 
                       entry.path().extension() == ".rb" || 
                       entry.path().extension() == ".php" ||
                       entry.path().extension() == ".go" || 
                       entry.path().extension() == ".swift" || 
                       entry.path().extension() == ".ts" ||
                       entry.path().extension() == ".rs" || 
                       entry.path().extension() == ".html" || 
                       entry.path().extension() == ".css") {
                type = "Source";
                color = COLOR_GREEN;
            } else if (std::filesystem::is_regular_file(entry) && 
                      (entry.path().extension() == ".so" || 
                       entry.path().extension() == ".dylib" || 
                       entry.path().extension() == ".exe" ||
                       (std::filesystem::status(entry).permissions() & 
                        std::filesystem::perms::owner_exec) != std::filesystem::perms::none)) {
                type = "Executable";
                color = COLOR_RED;
            } else {
                type = "File";
                color = COLOR_RESET;
            }
            
            // Get file size
            std::string size_str;
            if (std::filesystem::is_regular_file(entry)) {
                uintmax_t size = std::filesystem::file_size(entry);
                if (size < 1024)
                    size_str = std::to_string(size) + " B";
                else if (size < 1024 * 1024)
                    size_str = std::to_string(size / 1024) + " KB";
                else if (size < 1024 * 1024 * 1024)
                    size_str = std::to_string(size / (1024 * 1024)) + " MB";
                else
                    size_str = std::to_string(size / (1024 * 1024 * 1024)) + " GB";
            } else {
                size_str = "-";
            }
            
            // Display entry
            if (long_format) {
                // Get file permissions, owner, group and modification time
                struct stat file_stat;
                stat(entry.path().c_str(), &file_stat);
                
                // Format permissions
                std::string perms;
                perms += (S_ISDIR(file_stat.st_mode)) ? 'd' : '-';
                perms += (file_stat.st_mode & S_IRUSR) ? 'r' : '-';
                perms += (file_stat.st_mode & S_IWUSR) ? 'w' : '-';
                perms += (file_stat.st_mode & S_IXUSR) ? 'x' : '-';
                perms += (file_stat.st_mode & S_IRGRP) ? 'r' : '-';
                perms += (file_stat.st_mode & S_IWGRP) ? 'w' : '-';
                perms += (file_stat.st_mode & S_IXGRP) ? 'x' : '-';
                perms += (file_stat.st_mode & S_IROTH) ? 'r' : '-';
                perms += (file_stat.st_mode & S_IWOTH) ? 'w' : '-';
                perms += (file_stat.st_mode & S_IXOTH) ? 'x' : '-';
                
                // Get owner and group names
                struct passwd *pw = getpwuid(file_stat.st_uid);
                struct group *gr = getgrgid(file_stat.st_gid);
                std::string owner = pw ? pw->pw_name : std::to_string(file_stat.st_uid);
                std::string group = gr ? gr->gr_name : std::to_string(file_stat.st_gid);
                
                // Format modification time
                char mod_time[20];
                strftime(mod_time, sizeof(mod_time), "%Y-%m-%d %H:%M", localtime(&file_stat.st_mtime));
                
                // Display detailed entry
                std::cout << std::setw(12) << std::left << perms
                          << std::setw(10) << owner.substr(0, 9)
                          << std::setw(10) << group.substr(0, 9)
                          << std::setw(12) << std::right << size_str
                          << std::setw(20) << mod_time
                          << "  " << color << name << COLOR_RESET << std::endl;
            } else {
                std::cout << color << std::setw(40) << std::left << name.substr(0, 39) 
                          << COLOR_RESET << std::setw(15) << size_str 
                          << type << std::endl;
            }
        }
        
        return PLUGIN_SUCCESS;
    } catch (const std::filesystem::filesystem_error& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return PLUGIN_ERROR_GENERAL;
    }
}

// Required plugin functions

extern "C" {
    PLUGIN_API plugin_info_t* plugin_get_info() {
        return &plugin_info;
    }
    
    PLUGIN_API int plugin_initialize() {
        return PLUGIN_SUCCESS;
    }
    
    PLUGIN_API void plugin_shutdown() {
    }
    
    PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
        if (args->count < 1) {
            return PLUGIN_ERROR_INVALID_ARGS;
        }
        
        if (strcmp(args->args[0], "ls") == 0) {
            return custom_ls_command(args);
        }
        
        return PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    
    PLUGIN_API char** plugin_get_commands(int* count) {
        *count = 1;
        // Dynamically allocate memory for commands
        char** commands = (char**)malloc(sizeof(char*) * (*count));
        if (commands) {
            commands[0] = strdup("ls");
        }
        return commands;
    }
    
    PLUGIN_API char** plugin_get_subscribed_events(int* count) {
        *count = 0;
        return nullptr;
    }
    
    PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
        *count = 2;
        // Dynamically allocate memory for settings
        plugin_setting_t* settings = (plugin_setting_t*)malloc(sizeof(plugin_setting_t) * (*count));
        if (settings) {
            // Allocate memory for each string
            settings[0].key = strdup("show_colors");
            settings[0].value = strdup("true");
            settings[1].key = strdup("show_size");
            settings[1].value = strdup("true");
        }
        return settings;
    }
    
    PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
        // We don't need to do anything special with settings for now
        return PLUGIN_SUCCESS;
    }
    
    PLUGIN_API void plugin_free_memory(void* ptr) {
        // Add safety check before freeing memory
        if (ptr != nullptr) {
            free(ptr);
        }
    }
}
