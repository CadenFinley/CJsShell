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
    daemonPath = dataDir / "DevToolsTerminal-Daemon";
    updateCacheFile = dataDir / "update_cache.json";
}

bool DaemonManager::startDaemon() {
    if (isDaemonRunning()) {
        std::cerr << "Daemon is found and already running." << std::endl;
        return true;
    }
    
    if (!std::filesystem::exists(daemonPath)) {
        std::cerr << "Daemon executable not found at: " << daemonPath << " You can install it at github.com/CadenFinley/repos/DevToolsTerminal-Daemon"<< std::endl;
        return false;
    }
    
    updateDaemonConfig();
    
    std::string command = daemonPath.string() + " &";
    int result = system(command.c_str());
    if (result == 0) {
        std::cerr << "Daemon started successfully." << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return isDaemonRunning();
}

bool DaemonManager::stopDaemon() {
    int pid = getDaemonPid();
    if (pid <= 0) {
        return true;
    }
    
    if (kill(pid, SIGTERM) == 0) {
        int retries = 10;
        while (retries-- > 0 && kill(pid, 0) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (retries <= 0 && kill(pid, 0) == 0) {
            kill(pid, SIGKILL);
        }
        
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
    
    if (kill(pid, 0) == 0) {
        return true;
    }
    
    if (std::filesystem::exists(daemonPidFile)) {
        std::filesystem::remove(daemonPidFile);
    }
    
    return false;
}

bool DaemonManager::forceUpdateCheck() {
    json cacheData;
    if (std::filesystem::exists(updateCacheFile)) {
        try {
            std::ifstream cacheFile(updateCacheFile);
            if (cacheFile.is_open()) {
                cacheFile >> cacheData;
                cacheFile.close();
            }
        } catch (std::exception &e) {
            cacheData = json::object();
        }
    } else {
        cacheData = json::object();
    }
    
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
        }
    }
    
    return "{\"running\": true, \"status\": \"unknown\"}";
}

std::string DaemonManager::getDaemonVersion() {
    if (std::filesystem::exists(daemonStatusFile)) {
        try {
            std::ifstream statusFile(daemonStatusFile);
            if (statusFile.is_open()) {
                json statusData;
                statusFile >> statusData;
                statusFile.close();
                
                if (statusData.contains("daemon_version")) {
                    return statusData["daemon_version"].get<std::string>();
                }
            }
        } catch (std::exception &e) {
        }
    }
    
    return "";
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
        }
    }
    
    return 86400;
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
            config = json::object();
        }
    } else {
        config = json::object();
    }
    
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
