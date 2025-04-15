#ifndef DAEMONMANAGER_H
#define DAEMONMANAGER_H

#include <string>
#include <filesystem>

class DaemonManager {
public:
    DaemonManager(const std::filesystem::path& dataDirectory);
    
    // Daemon lifecycle management
    bool startDaemon();
    bool stopDaemon();
    bool restartDaemon();
    bool isDaemonRunning();
    
    // Daemon communication
    bool forceUpdateCheck();
    bool refreshExecutablesCache();
    std::string getDaemonStatus();
    
    // Configuration
    void setUpdateCheckInterval(int intervalSeconds);
    int getUpdateCheckInterval();
    
    // Update status
    bool isUpdateAvailable();
    std::string getLatestVersion();
    time_t getLastUpdateCheckTime();

private:
    std::filesystem::path dataDir;
    std::filesystem::path daemonPidFile;
    std::filesystem::path daemonStatusFile;
    std::filesystem::path daemonConfigFile;
    std::filesystem::path daemonPath;
    std::filesystem::path updateCacheFile;
    
    void updateDaemonConfig();
    int getDaemonPid();
};

#endif // DAEMONMANAGER_H
