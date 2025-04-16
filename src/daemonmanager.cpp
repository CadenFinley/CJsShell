#include "daemonmanager.h"
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

using json = nlohmann::json;

DaemonManager::DaemonManager(const std::filesystem::path& dataDirectory)
    : dataDir(dataDirectory), socketFd(-1), socketConnected(false) {
    daemonDir = dataDir / "DTT-Daemon";
    daemonPidFile = daemonDir / ".daemon.pid";
    daemonLogFile = daemonDir / "daemon.log";
    socketPath = daemonDir / ".daemon.sock";
    daemonConfigFile = daemonDir / "daemon_config.json";

    daemonPath = dataDir / "DevToolsTerminal-Daemon";
    updateCacheFile = dataDir / "update_cache.json";
    
    cronDir = dataDir / "dtt-cron";
    cronScriptsDir = cronDir / "cron_scripts";
    cronJobsFile = cronDir / "cron_jobs.json";
    cronLogFile = cronDir / "cron_log.txt";
}

DaemonManager::~DaemonManager() {
    disconnectFromSocket();
}

bool DaemonManager::connectToSocket() {
    if (socketConnected) return true;
    
    socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd < 0) {
        return false;
    }
    
    int flags = fcntl(socketFd, F_GETFL, 0);
    fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
    
    int result = connect(socketFd, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0 && errno != EINPROGRESS) {
        close(socketFd);
        socketFd = -1;
        return false;
    }
    
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(socketFd, &writefds);
    
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    if (select(socketFd + 1, nullptr, &writefds, nullptr, &timeout) <= 0) {
        close(socketFd);
        socketFd = -1;
        return false;
    }
    
    fcntl(socketFd, F_SETFL, flags);
    
    socketConnected = true;
    return true;
}

void DaemonManager::disconnectFromSocket() {
    if (socketFd >= 0) {
        close(socketFd);
        socketFd = -1;
        socketConnected = false;
    }
}

std::string DaemonManager::sendCommand(const std::string& command) {
    if (!isDaemonRunning()) {
        return "{\"error\": \"Daemon not running\"}";
    }
    
    if (!connectToSocket()) {
        return "{\"error\": \"Could not connect to daemon socket\"}";
    }
    
    std::string cmdWithNewline = command + "\n";
    if (write(socketFd, cmdWithNewline.c_str(), cmdWithNewline.size()) <= 0) {
        disconnectFromSocket();
        return "{\"error\": \"Failed to send command\"}";
    }
    
    char buffer[4096];
    std::string response;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socketFd, &readfds);
    
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    
    if (select(socketFd + 1, &readfds, nullptr, nullptr, &timeout) <= 0) {
        disconnectFromSocket();
        return "{\"error\": \"Response timeout\"}";
    }
    
    ssize_t bytesRead = read(socketFd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
        disconnectFromSocket();
        return "{\"error\": \"Failed to read response\"}";
    }
    
    buffer[bytesRead] = '\0';
    response = buffer;
    
    return response;
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
    
    if (!std::filesystem::exists(daemonDir)) {
        std::filesystem::create_directories(daemonDir);
    }
    
    ensureCronDirectoriesExist();
    
    updateDaemonConfig();
    
    std::string command = daemonPath.string() + " &";
    int result = system(command.c_str());
    if (result == 0) {
        std::cerr << "Daemon started successfully." << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return isDaemonRunning();
}

bool DaemonManager::stopDaemon() {
    json command = {
        {"action", "stop"}
    };
    
    std::string response = sendCommand(command.dump());
    try {
        json responseJson = json::parse(response);
        return responseJson.contains("success") && responseJson["success"].get<bool>();
    } catch (std::exception &e) {
        return false;
    }
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
    json command = {
        {"action", "force_update_check"}
    };
    
    std::string response = sendCommand(command.dump());
    try {
        json responseJson = json::parse(response);
        return responseJson.contains("success") && responseJson["success"].get<bool>();
    } catch (std::exception &e) {
        return false;
    }
}

bool DaemonManager::refreshExecutablesCache() {
    json command = {
        {"action", "refresh_executables"}
    };
    
    std::string response = sendCommand(command.dump());
    try {
        json responseJson = json::parse(response);
        return responseJson.contains("success") && responseJson["success"].get<bool>();
    } catch (std::exception &e) {
        return false;
    }
}

std::string DaemonManager::getDaemonStatus() {
    json command = {
        {"action", "status"}
    };
    
    return sendCommand(command.dump());
}

std::string DaemonManager::getDaemonVersion() {
    json command = {
        {"action", "status"}
    };
    
    std::string response = sendCommand(command.dump());
    try {
        json responseJson = json::parse(response);
        if (responseJson.contains("daemon_version")) {
            return responseJson["daemon_version"].get<std::string>();
        }
    } catch (std::exception &e) {
    }
    
    return "";
}

void DaemonManager::setUpdateCheckInterval(int intervalSeconds) {
    json command = {
        {"action", "set_update_interval"},
        {"interval", intervalSeconds}
    };
    
    sendCommand(command.dump());
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

void DaemonManager::ensureCronDirectoriesExist() {
    if (!std::filesystem::exists(cronDir)) {
        std::filesystem::create_directories(cronDir);
    }
    
    if (!std::filesystem::exists(cronScriptsDir)) {
        std::filesystem::create_directories(cronScriptsDir);
    }
    
    if (!std::filesystem::exists(cronJobsFile)) {
        std::ofstream jobsFile(cronJobsFile);
        if (jobsFile.is_open()) {
            jobsFile << "[]";
            jobsFile.close();
        }
    }
    
    if (!std::filesystem::exists(cronLogFile)) {
        std::ofstream logFile(cronLogFile);
        if (logFile.is_open()) {
            logFile << "# Cron job log file\n";
            logFile.close();
        }
    }
}
