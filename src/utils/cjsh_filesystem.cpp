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
        char real_path[PATH_MAX];
        if (realpath(path, real_path) != nullptr) {
            cjsh_filesystem::g_cjsh_path = real_path;
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
