#include "../src/include/plugininterface.h"
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/statvfs.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstring>
#include <vector>
#include <queue>
#include <map>

class SystemMonitor : public PluginInterface {
private:
    bool monitoring;
    std::chrono::system_clock::time_point startTime;
    unsigned long commandsProcessed;
    std::map<std::string, std::string> settings;
    
    // Get CPU usage
    double getCPUUsage() {
        processor_cpu_load_info_t cpuLoad;
        mach_msg_type_number_t processorMsgCount;
        natural_t processorCount;
        
        kern_return_t err = host_processor_info(mach_host_self(),
                                              PROCESSOR_CPU_LOAD_INFO,
                                              &processorCount,
                                              (processor_info_array_t*)&cpuLoad,
                                              &processorMsgCount);
        
        if (err != KERN_SUCCESS) return -1.0;
        
        double totalUsage = 0.0;
        for (natural_t i = 0; i < processorCount; i++) {
            double total = cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                         cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM] +
                         cpuLoad[i].cpu_ticks[CPU_STATE_IDLE] +
                         cpuLoad[i].cpu_ticks[CPU_STATE_NICE];
            
            double used = cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                         cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM] +
                         cpuLoad[i].cpu_ticks[CPU_STATE_NICE];
            
            totalUsage += (used / total) * 100.0;
        }
        
        vm_deallocate(mach_task_self_,
                     (vm_address_t)cpuLoad,
                     (vm_size_t)(processorMsgCount * sizeof(*cpuLoad)));
        
        return totalUsage / processorCount;
    }
    
    // Get memory usage
    std::pair<double, double> getMemoryUsage() {
        vm_size_t pageSize;
        mach_port_t machPort = mach_host_self();
        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t count = sizeof(vmStats) / sizeof(natural_t);
        
        host_page_size(machPort, &pageSize);
        host_statistics64(machPort, HOST_VM_INFO64, (host_info64_t)&vmStats, &count);
        
        double used = (vmStats.active_count + vmStats.wire_count) * pageSize;
        double total = (vmStats.active_count + vmStats.wire_count + vmStats.inactive_count + vmStats.free_count) * pageSize;
        
        return {used / (1024.0 * 1024.0), total / (1024.0 * 1024.0)}; // Return in MB
    }
    
    // Get disk usage
    std::pair<double, double> getDiskUsage() {
        struct statvfs stats;
        if (statvfs("/", &stats) != 0) return {-1, -1};
        
        double total = (double)stats.f_blocks * stats.f_frsize;
        double free = (double)stats.f_bfree * stats.f_frsize;
        double used = total - free;
        
        return {used / (1024.0 * 1024.0 * 1024.0), total / (1024.0 * 1024.0 * 1024.0)}; // Return in GB
    }

public:
    SystemMonitor() : monitoring(false), commandsProcessed(0) {
        startTime = std::chrono::system_clock::now();
        settings = getDefaultSettings();
    }
    
    ~SystemMonitor() override {
        shutdown();
    }
    
    std::string getName() const override {
        return "SystemMonitor";
    }
    
    std::string getVersion() const override {
        return "1.0.0";
    }
    
    std::string getDescription() const override {
        return "System resource monitor for macOS with DevToolsTerminal usage tracking";
    }
    
    std::string getAuthor() const override {
        return "Caden Finley";
    }
    
    bool initialize() override {
        monitoring = true;
        return true;
    }
    
    void shutdown() override {
        monitoring = false;
    }
    
    std::vector<std::string> getCommands() const override {
        return {"sysinfo", "proginfo", "monitor"};
    }

    std::vector<std::string> getSubscribedEvents() const override {
        return {"main_process"};
    }
    
    std::map<std::string, std::string> getDefaultSettings() const override {
        return {
            {"update_interval", "5"},     // Update interval in seconds
            {"show_percentage", "true"}   // Show values as percentages
        };
    }
    
    void updateSetting(const std::string& key, const std::string& value) override {
        settings[key] = value;
    }
    
    bool handleCommand(std::queue<std::string>& args) override {
        if (args.empty()) return false;
        
        std::string command = args.front();
        args.pop();
        
        if (command == "event") {
            if (!args.empty() && args.front() == "main_process_command_processed") {
                args.pop();
                commandsProcessed++;
            }
            return true;
        }
        
        if (command == "sysinfo") {
            displaySystemInfo();
            return true;
        }
        
        if (command == "proginfo") {
            displayProgramInfo();
            return true;
        }
        
        if (command == "monitor") {
            if (!args.empty()) {
                if (args.front() == "start") {
                    startMonitoring();
                    return true;
                }
                if (args.front() == "stop") {
                    stopMonitoring();
                    return true;
                }
            }
            return false;
        }
        
        return false;
    }

    int getInterfaceVersion() const {
        return 1;
    }
    
private:
    void displaySystemInfo() {
        double cpuUsage = getCPUUsage();
        auto memUsage = getMemoryUsage();
        auto diskUsage = getDiskUsage();
        
        std::cout << "\n=== System Information ===\n";
        std::cout << "CPU Usage: " << std::fixed << std::setprecision(2) << cpuUsage << "%\n";
        std::cout << "Memory Usage: " << (memUsage.first / memUsage.second) * 100 << "% "
                  << "(Used: " << memUsage.first << "MB / Total: " << memUsage.second << "MB)\n";
        std::cout << "Disk Usage: " << (diskUsage.first / diskUsage.second) * 100 << "% "
                  << "(Used: " << diskUsage.first << "GB / Total: " << diskUsage.second << "GB)\n";
    }
    
    void displayProgramInfo() {
        auto now = std::chrono::system_clock::now();
        auto runtime = std::chrono::duration_cast<std::chrono::minutes>(now - startTime);
        
        std::cout << "\n=== DevToolsTerminal Statistics ===\n";
        std::cout << "Runtime: " << runtime.count() << " minutes\n";
        std::cout << "Commands Processed: " << commandsProcessed << "\n";
        std::cout << "Commands/Minute: " << std::fixed << std::setprecision(2)
                  << (runtime.count() > 0 ? static_cast<double>(commandsProcessed) / runtime.count() : 0)
                  << "\n";
    }
    
    void startMonitoring() {
        if (monitoring) {
            std::thread([this]() {
                while (monitoring) {
                    displaySystemInfo();
                    displayProgramInfo();
                    std::this_thread::sleep_for(
                        std::chrono::seconds(std::stoi(settings["update_interval"]))
                    );
                }
            }).detach();
            std::cout << "Monitoring started. Update interval: "
                      << settings["update_interval"] << " seconds.\n";
        }
    }
    
    void stopMonitoring() {
        monitoring = false;
        std::cout << "Monitoring stopped.\n";
    }
};

IMPLEMENT_PLUGIN(SystemMonitor)
