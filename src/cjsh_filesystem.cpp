#include "cjsh_filesystem.h"

// For macOS _NSGetExecutablePath
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// For Linux fallback
#ifdef __linux__
#include <string.h>
extern char *program_invocation_name;
#endif

// Define the global variable
std::string cjsh_filesystem::g_cjsh_path;

// This function can be called during initialization to set the path
/**
 * @brief Determines and sets the absolute path of the current executable.
 *
 * Attempts to resolve the executable's absolute path using platform-specific methods and stores it in `cjsh_filesystem::g_cjsh_path`. Falls back to a default path or a warning message if resolution fails.
 */
void initialize_cjsh_path() {
    char path[PATH_MAX];
    // Linux-specific code
    #ifdef __linux__
    if (readlink("/proc/self/exe", path, PATH_MAX) != -1) {
        cjsh_filesystem::g_cjsh_path = path;
        return;
    }
    #endif
    
    // macOS-specific code
    #ifdef __APPLE__
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(path, &size) == 0) {
        char real_path[PATH_MAX];
        if (realpath(path, real_path) != nullptr) {
            cjsh_filesystem::g_cjsh_path = real_path;
            return;
        } else {
            cjsh_filesystem::g_cjsh_path = path;
            return;
        }
    }
    #endif
    
    // Fallback method
    if (cjsh_filesystem::g_cjsh_path.empty()) {
        #ifdef __linux__
        char* exePath = realpath(program_invocation_name, NULL);
        if (exePath) {
            cjsh_filesystem::g_cjsh_path = exePath;
            free(exePath);
            return;
        } else {
            // Default to a common location
            cjsh_filesystem::g_cjsh_path = "/usr/local/bin/cjsh";
            return;
        }
        #else
        // Default to a common location on non-Linux systems
        cjsh_filesystem::g_cjsh_path = "/usr/local/bin/cjsh";
        return;
        #endif
    }
    cjsh_filesystem::g_cjsh_path = "No path found";
    std::cerr << "Warning: Unable to determine the executable path. This program may not work correctly." << std::endl;
}
