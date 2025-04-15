#pragma once

#include <filesystem>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <sys/un.h>
#include "cron_job.h"

class DaemonManager {
public:
    DaemonManager(const std::filesystem::path& dataDirectory);
    ~DaemonManager();
    
    bool startDaemon();
    bool stopDaemon();
    bool restartDaemon();
    bool isDaemonRunning();
    bool forceUpdateCheck();
    bool refreshExecutablesCache();
    std::string getDaemonStatus();
    std::string getDaemonVersion();
    void setUpdateCheckInterval(int intervalSeconds);
    int getUpdateCheckInterval();
    bool isUpdateAvailable();
    std::string getLatestVersion();
    time_t getLastUpdateCheckTime();
    
    // Cron Job Management
    std::vector<CronJob> listCronJobs();
    CronJob getCronJob(const std::string& id);
    std::string addCronJob(const CronJob& job);
    bool updateCronJob(const CronJob& job);
    bool removeCronJob(const std::string& id);
    bool enableCronJob(const std::string& id, bool enabled);
    
    // Cron Script Management
    std::vector<std::string> listCronScripts();
    std::string getCronScript(const std::string& name);
    bool saveCronScript(const std::string& name, const std::string& content);
    bool deleteCronScript(const std::string& name);
    
private:
    std::filesystem::path dataDir;
    std::filesystem::path daemonDir;
    std::filesystem::path daemonPidFile;
    std::filesystem::path daemonLogFile;
    std::filesystem::path socketPath;
    std::filesystem::path daemonConfigFile;
    std::filesystem::path daemonPath;
    std::filesystem::path updateCacheFile;
    std::filesystem::path cronDir;
    std::filesystem::path cronScriptsDir;
    std::filesystem::path cronJobsFile;
    std::filesystem::path cronLogFile;
    
    int socketFd;
    bool socketConnected;
    
    bool connectToSocket();
    void disconnectFromSocket();
    std::string sendCommand(const std::string& command);
    
    void updateDaemonConfig();
    int getDaemonPid();
    void ensureCronDirectoriesExist();
};
