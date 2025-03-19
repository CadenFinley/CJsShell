#include "../plugininterface.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <map>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <atomic>

class FileWatcherPlugin : public PluginInterface {
private:
    std::string name = "FileWatcher";
    std::string version = "1.0.0";
    std::string description = "Monitors files and directories for changes";
    std::string author = "DevToolsTerminal User";
    
    std::map<std::string, std::string> settings;
    std::thread watcherThread;
    std::atomic<bool> running;
    
    std::map<std::string, std::filesystem::file_time_type> fileTimestamps;
    std::mutex filesMutex;
    
    bool verboseMode = false;
    int watchIntervalMs = 1000;
    std::vector<std::string> monitoredPaths;
    
    void watchFiles() {
        while (running) {
            std::vector<std::string> changedFiles;
            
            {
                std::lock_guard<std::mutex> lock(filesMutex);
                for (auto& path : monitoredPaths) {
                    try {
                        if (!std::filesystem::exists(path)) {
                            continue;
                        }
                        
                        if (std::filesystem::is_directory(path)) {
                            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                                if (std::filesystem::is_regular_file(entry)) {
                                    auto lastWriteTime = std::filesystem::last_write_time(entry);
                                    std::string filePath = entry.path().string();
                                    
                                    if (fileTimestamps.find(filePath) != fileTimestamps.end()) {
                                        if (lastWriteTime != fileTimestamps[filePath]) {
                                            changedFiles.push_back(filePath);
                                            fileTimestamps[filePath] = lastWriteTime;
                                        }
                                    } else {
                                        fileTimestamps[filePath] = lastWriteTime;
                                    }
                                }
                            }
                        } else if (std::filesystem::is_regular_file(path)) {
                            auto lastWriteTime = std::filesystem::last_write_time(path);
                            
                            if (fileTimestamps.find(path) != fileTimestamps.end()) {
                                if (lastWriteTime != fileTimestamps[path]) {
                                    changedFiles.push_back(path);
                                    fileTimestamps[path] = lastWriteTime;
                                }
                            } else {
                                fileTimestamps[path] = lastWriteTime;
                            }
                        }
                    } catch (const std::filesystem::filesystem_error& e) {
                        if (verboseMode) {
                            std::cerr << "FileWatcher error: " << e.what() << std::endl;
                        }
                    }
                }
            }
            
            // Notify about changed files
            if (!changedFiles.empty()) {
                std::cout << "\n\033[1;35m[FileWatcher]\033[0m Detected changes in files:" << std::endl;
                for (const auto& file : changedFiles) {
                    std::cout << "  - " << file << std::endl;
                }
                std::cout << std::flush;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(watchIntervalMs));
        }
    }

public:
    virtual std::string getName() const override {
        return name;
    }
    
    virtual std::string getVersion() const override {
        return version;
    }
    
    virtual std::string getDescription() const override {
        return description;
    }
    
    virtual std::string getAuthor() const override {
        return author;
    }
    
    virtual bool initialize() override {
        std::cout << "Initializing " << name << " plugin..." << std::endl;
        
        // Load settings
        verboseMode = settings["verbose"] == "true";
        
        try {
            watchIntervalMs = std::stoi(settings["watch_interval_ms"]);
        } catch (...) {
            watchIntervalMs = 1000;
        }
        
        // Parse watched paths
        std::string pathsStr = settings["watched_paths"];
        size_t pos = 0;
        while ((pos = pathsStr.find(";")) != std::string::npos) {
            std::string path = pathsStr.substr(0, pos);
            if (!path.empty()) {
                monitoredPaths.push_back(path);
            }
            pathsStr.erase(0, pos + 1);
        }
        if (!pathsStr.empty()) {
            monitoredPaths.push_back(pathsStr);
        }
        
        // Start watcher thread
        running = true;
        watcherThread = std::thread(&FileWatcherPlugin::watchFiles, this);
        
        return true;
    }
    
    virtual void shutdown() override {
        std::cout << "Shutting down " << name << " plugin..." << std::endl;
        running = false;
        if (watcherThread.joinable()) {
            watcherThread.join();
        }
    }
    
    virtual bool handleCommand(std::queue<std::string>& args) override {
        if (args.empty()) return false;
        
        std::string cmd = args.front();
        args.pop();
        
        if (cmd == "watch") {
            if (args.empty()) {
                std::cout << "Current watched paths:" << std::endl;
                if (monitoredPaths.empty()) {
                    std::cout << "  No paths being monitored." << std::endl;
                } else {
                    for (const auto& path : monitoredPaths) {
                        std::cout << "  - " << path << std::endl;
                    }
                }
                return true;
            }
            
            std::string path = args.front();
            args.pop();
            
            std::lock_guard<std::mutex> lock(filesMutex);
            if (std::find(monitoredPaths.begin(), monitoredPaths.end(), path) == monitoredPaths.end()) {
                monitoredPaths.push_back(path);
                std::cout << "Now monitoring: " << path << std::endl;
                
                // Update settings
                std::string pathsStr = "";
                for (const auto& p : monitoredPaths) {
                    if (!pathsStr.empty()) pathsStr += ";";
                    pathsStr += p;
                }
                settings["watched_paths"] = pathsStr;
            } else {
                std::cout << "Already monitoring: " << path << std::endl;
            }
            return true;
        }
        else if (cmd == "unwatch") {
            if (args.empty()) {
                std::cout << "Usage: unwatch <path>" << std::endl;
                return true;
            }
            
            std::string path = args.front();
            args.pop();
            
            std::lock_guard<std::mutex> lock(filesMutex);
            auto it = std::find(monitoredPaths.begin(), monitoredPaths.end(), path);
            if (it != monitoredPaths.end()) {
                monitoredPaths.erase(it);
                std::cout << "Stopped monitoring: " << path << std::endl;
                
                // Update settings
                std::string pathsStr = "";
                for (const auto& p : monitoredPaths) {
                    if (!pathsStr.empty()) pathsStr += ";";
                    pathsStr += p;
                }
                settings["watched_paths"] = pathsStr;
            } else {
                std::cout << "Not monitoring: " << path << std::endl;
            }
            return true;
        }
        else if (cmd == "interval") {
            if (args.empty()) {
                std::cout << "Current watch interval: " << watchIntervalMs << "ms" << std::endl;
                return true;
            }
            
            try {
                int newInterval = std::stoi(args.front());
                args.pop();
                
                if (newInterval < 100) {
                    std::cout << "Warning: Intervals under 100ms may impact performance" << std::endl;
                }
                
                watchIntervalMs = newInterval;
                settings["watch_interval_ms"] = std::to_string(watchIntervalMs);
                std::cout << "Watch interval set to " << watchIntervalMs << "ms" << std::endl;
            } catch (...) {
                std::cout << "Invalid interval. Please provide a number in milliseconds." << std::endl;
            }
            return true;
        }
        else if (cmd == "verbose") {
            if (args.empty()) {
                std::cout << "Verbose mode is " << (verboseMode ? "enabled" : "disabled") << std::endl;
                return true;
            }
            
            std::string mode = args.front();
            args.pop();
            
            if (mode == "on" || mode == "true" || mode == "enable") {
                verboseMode = true;
                settings["verbose"] = "true";
                std::cout << "Verbose mode enabled" << std::endl;
            } else if (mode == "off" || mode == "false" || mode == "disable") {
                verboseMode = false;
                settings["verbose"] = "false";
                std::cout << "Verbose mode disabled" << std::endl;
            } else {
                std::cout << "Invalid option. Use 'on' or 'off'" << std::endl;
            }
            return true;
        }
        else if (cmd == "status") {
            std::cout << "FileWatcher Status:" << std::endl;
            std::cout << "  - Running: " << (running ? "Yes" : "No") << std::endl;
            std::cout << "  - Watch interval: " << watchIntervalMs << "ms" << std::endl;
            std::cout << "  - Verbose mode: " << (verboseMode ? "Enabled" : "Disabled") << std::endl;
            std::cout << "  - Monitored paths: " << monitoredPaths.size() << std::endl;
            std::cout << "  - Tracked files: " << fileTimestamps.size() << std::endl;
            return true;
        }
        else if (cmd == "event") {
            if (args.size() < 2) return false;
            
            std::string eventType = args.front();
            args.pop();
            std::string eventData = args.front();
            args.pop();
            
            if (verboseMode) {
                std::cout << "FileWatcher received event: " << eventType << " - " << eventData << std::endl;
            }
            
            return true;
        }
        
        return false;
    }
    
    virtual std::vector<std::string> getCommands() const override {
        return {
            "watch", 
            "unwatch", 
            "interval", 
            "verbose", 
            "status"
        };
    }

    virtual int getInterfaceVersion() const override {
        return 1;
    }

    virtual std::vector<std::string> getSubscribedEvents() const override {
        return {};
    }
    
    virtual std::map<std::string, std::string> getDefaultSettings() const override {
        return {
            {"verbose", "false"},
            {"watch_interval_ms", "1000"},
            {"watched_paths", ""}
        };
    }
    
    virtual void updateSetting(const std::string& key, const std::string& value) override {
        settings[key] = value;
        
        if (key == "verbose") {
            verboseMode = (value == "true");
        } else if (key == "watch_interval_ms") {
            try {
                watchIntervalMs = std::stoi(value);
            } catch (...) {
                if (verboseMode) {
                    std::cerr << "Invalid watch interval value: " << value << std::endl;
                }
            }
        } else if (key == "watched_paths") {
            std::lock_guard<std::mutex> lock(filesMutex);
            monitoredPaths.clear();
            
            std::string pathsStr = value;
            size_t pos = 0;
            while ((pos = pathsStr.find(";")) != std::string::npos) {
                std::string path = pathsStr.substr(0, pos);
                if (!path.empty()) {
                    monitoredPaths.push_back(path);
                }
                pathsStr.erase(0, pos + 1);
            }
            if (!pathsStr.empty()) {
                monitoredPaths.push_back(pathsStr);
            }
        }
    }
};

// Export plugin creation and destruction functions
IMPLEMENT_PLUGIN(FileWatcherPlugin)
