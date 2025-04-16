#include "daemonmanager.h"
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

DaemonManager::DaemonManager(const std::filesystem::path& dataDirectory)
    : dataDir(dataDirectory) {
    daemonPidFile = dataDir / "daemon.pid";
    daemonStatusFile = dataDir / "daemon_status.json";
    daemonConfigFile = dataDir / "daemon_config.json";
    daemonPath = dataDir / "dttdaemon";
    updateCacheFile = dataDir / "update_cache.json";
}

bool DaemonManager::startDaemon() {
    if (isDaemonRunning()) {
        return true;  // Daemon already running
    }
    
    // Check if daemon executable exists
    if (!std::filesystem::exists(daemonPath)) {
        std::cerr << "Daemon executable not found at: " << daemonPath << std::endl;
        return false;
    }
    
    // Ensure the config file exists
    updateDaemonConfig();
    
    // Start the daemon
    std::string command = daemonPath.string() + " &";
    int result = system(command.c_str());
    
    // Wait a moment for the daemon to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return isDaemonRunning();
}

bool DaemonManager::stopDaemon() {
    int pid = getDaemonPid();
    if (pid <= 0) {
        return true;  // Daemon not running
    }
    
    // Send SIGTERM to the daemon
    if (kill(pid, SIGTERM) == 0) {
        // Wait for the daemon to exit
        int retries = 10;
        while (retries-- > 0 && kill(pid, 0) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Force kill if still running
        if (retries <= 0 && kill(pid, 0) == 0) {
            kill(pid, SIGKILL);
        }
        
        // Clean up PID file
        if (std::filesystem::exists(daemonPidFile)) {
            std::filesystem::remove(daemonPidFile);
        }
        
        return true;
    }
    
    return false;
}

bool DaemonManager::restartDaemon() {
    if (stopDaemon()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return startDaemon();
    }
    return false;
}

bool DaemonManager::isDaemonRunning() {
    int pid = getDaemonPid();
    if (pid <= 0) {
        return false;
    }
    
    // Check if process exists
    if (kill(pid, 0) == 0) {
        return true;
    }
    
    // PID file exists but process doesn't, clean up
    if (std::filesystem::exists(daemonPidFile)) {
        std::filesystem::remove(daemonPidFile);
    }
    
    return false;
}

bool DaemonManager::forceUpdateCheck() {
    // Not implemented in this version - would use IPC to communicate with daemon
    // For now, we'll just touch the update cache file with an old timestamp
    // to force the daemon to check on its next cycle
    
    json cacheData;
    if (std::filesystem::exists(updateCacheFile)) {
        try {
            std::ifstream cacheFile(updateCacheFile);
            if (cacheFile.is_open()) {
                cacheFile >> cacheData;
                cacheFile.close();
            }
        } catch (std::exception &e) {
            // Create new cache data
            cacheData = json::object();
        }
    } else {
        cacheData = json::object();
    }
    
    // Set the check time to a day ago
    cacheData["check_time"] = std::time(nullptr) - 86400;
    
    std::ofstream cacheFile(updateCacheFile);
    if (cacheFile.is_open()) {
        cacheFile << cacheData.dump();
        cacheFile.close();
        return true;
    }
    
    return false;
}

bool DaemonManager::refreshExecutablesCache() {
    // Similar to force update check, we'll use a simple approach for now
    // In a more sophisticated implementation, we'd use IPC to communicate with the daemon
    
    // Touch the executables cache file to trigger a refresh
    if (std::filesystem::exists(dataDir / "executables_cache.json")) {
        std::filesystem::remove(dataDir / "executables_cache.json");
    }
    
    return true;
}

std::string DaemonManager::getDaemonStatus() {
    if (!isDaemonRunning()) {
        return "{\"running\": false}";
    }
    
    if (std::filesystem::exists(daemonStatusFile)) {
        try {
            std::ifstream statusFile(daemonStatusFile);
            if (statusFile.is_open()) {
                std::string status((std::istreambuf_iterator<char>(statusFile)), std::istreambuf_iterator<char>());
                statusFile.close();
                return status;
            }
        } catch (std::exception &e) {
            // Return minimal status on error
        }
    }
    
    return "{\"running\": true, \"status\": \"unknown\"}";
}

void DaemonManager::setUpdateCheckInterval(int intervalSeconds) {
    json config;
    if (std::filesystem::exists(daemonConfigFile)) {
        try {
            std::ifstream configFile(daemonConfigFile);
            if (configFile.is_open()) {
                configFile >> config;
                configFile.close();
            }
        } catch (std::exception &e) {
            // Create new config
            config = json::object();
        }
    } else {
        config = json::object();
    }
    
    config["update_check_interval"] = intervalSeconds;
    
    std::ofstream configFile(daemonConfigFile);
    if (configFile.is_open()) {
        configFile << config.dump();
        configFile.close();
    }
}

int DaemonManager::getUpdateCheckInterval() {
    if (std::filesystem::exists(daemonConfigFile)) {
        try {
            std::ifstream configFile(daemonConfigFile);
            if (configFile.is_open()) {
                json config;
                configFile >> config;
                configFile.close();
                
                if (config.contains("update_check_interval")) {
                    return config["update_check_interval"].get<int>();
                }
            }
        } catch (std::exception &e) {
            // Return default on error
        }
    }
    
    return 86400;  // Default to 24 hours
}

bool DaemonManager::isUpdateAvailable() {
    if (std::filesystem::exists(updateCacheFile)) {
        try {
            std::ifstream cacheFile(updateCacheFile);
            if (cacheFile.is_open()) {
                json cacheData;
                cacheFile >> cacheData;
                cacheFile.close();
                
                if (cacheData.contains("update_available")) {
                    return cacheData["update_available"].get<bool>();
                }
            }
        } catch (std::exception &e) {
            // Return false on error
        }
    }
    
    return false;
}

std::string DaemonManager::getLatestVersion() {
    if (std::filesystem::exists(updateCacheFile)) {
        try {
            std::ifstream cacheFile(updateCacheFile);
            if (cacheFile.is_open()) {
                json cacheData;
                cacheFile >> cacheData;
                cacheFile.close();
                
                if (cacheData.contains("latest_version")) {
                    return cacheData["latest_version"].get<std::string>();
                }
            }
        } catch (std::exception &e) {
            // Return empty string on error
        }
    }
    
    return "";
}

time_t DaemonManager::getLastUpdateCheckTime() {
    if (std::filesystem::exists(updateCacheFile)) {
        try {
            std::ifstream cacheFile(updateCacheFile);
            if (cacheFile.is_open()) {
                json cacheData;
                cacheFile >> cacheData;
                cacheFile.close();
                
                if (cacheData.contains("check_time")) {
                    return cacheData["check_time"].get<time_t>();
                }
            }
        } catch (std::exception &e) {
            // Return 0 on error
        }
    }
    
    return 0;
}

void DaemonManager::updateDaemonConfig() {
    json config;
    if (std::filesystem::exists(daemonConfigFile)) {
        try {
            std::ifstream configFile(daemonConfigFile);
            if (configFile.is_open()) {
                configFile >> config;
                configFile.close();
            }
        } catch (std::exception &e) {
            // Create new config
            config = json::object();
        }
    } else {
        config = json::object();
    }
    
    // Set default values if not present
    if (!config.contains("update_check_interval")) {
        config["update_check_interval"] = 86400;
    }
    
    std::ofstream configFile(daemonConfigFile);
    if (configFile.is_open()) {
        configFile << config.dump();
        configFile.close();
    }
}

int DaemonManager::getDaemonPid() {
    if (!std::filesystem::exists(daemonPidFile)) {
        return -1;
    }
    
    std::ifstream pidFile(daemonPidFile);
    if (!pidFile.is_open()) {
        return -1;
    }
    
    int pid;
    pidFile >> pid;
    pidFile.close();
    
    return pid;
}
