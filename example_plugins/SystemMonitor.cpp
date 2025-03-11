#include "plugininterface.h"
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #pragma comment(lib, "psapi.lib")
#elif defined(__APPLE__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <sys/mount.h> 
    #include <mach/mach.h>
    #include <mach/processor_info.h>
    #include <mach/mach_host.h>
#else
    #include <unistd.h>
    #include <fstream>
    #include <sys/statvfs.h>
    #include <sys/sysinfo.h>
#endif

class SystemMonitor : public PluginInterface {
public:
    SystemMonitor() {
        cmds.push_back("syscheck");
        cmds.push_back("cpu");
        cmds.push_back("memory");
        cmds.push_back("disk");
        settings["refresh_rate"] = "1000ms";
    }
    ~SystemMonitor() throw() { }
    
    std::string getName() const {
        return "sysmon";
    }
    
    std::string getVersion() const {
        return "1.0";
    }
    
    std::string getDescription() const {
        return "A system monitor plugin for devtoolsterminal.";
    }
    
    std::string getAuthor() const {
        return "Caden Finley";
    }
    
    bool initialize() {
        return true;
    }
    
    void shutdown() {
        return;
    }
    
    bool handleCommand(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "No command provided." << std::endl;
            return false;
        }
        std::string cmd = args.front();
        args.pop();
        
        if (cmd == "syscheck") {
            displaySystemInfo();
            return true;
        } else if (cmd == "cpu") {
            std::cout << "CPU Usage: " << getCpuUsage() << "%" << std::endl;
            return true;
        } else if (cmd == "memory") {
            std::pair<float, float> memUsage = getMemoryUsage();
            float used = memUsage.first;
            float total = memUsage.second;
            std::cout << "Memory Usage: " << used << "MB / " << total << "MB (" 
                      << std::fixed << std::setprecision(1) << (used * 100.0 / total) << "%)" << std::endl;
            return true;
        } else if (cmd == "disk") {
            std::pair<float, float> diskUsage = getDiskUsage();
            float used = diskUsage.first;
            float total = diskUsage.second;
            std::cout << "Disk Usage: " << used << "GB / " << total << "GB (" 
                      << std::fixed << std::setprecision(1) << (used * 100.0 / total) << "%)" << std::endl;
            return true;
        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
        }
        return false;
    }
    
    std::vector<std::string> getCommands() const {
        return cmds;
    }
    
    std::map<std::string, std::string> getDefaultSettings() const {
        return settings;
    }
    
    void updateSetting(const std::string& key, const std::string& value) {
        std::cout << "SystemMonitor updated setting " << key << " to " << value << std::endl;
        settings[key] = value;
    }

private:
    std::vector<std::string> cmds;
    
    void displaySystemInfo() {
        std::cout << "===== System Monitor =====" << std::endl;
        std::cout << "CPU Usage: " << getCpuUsage() << "%" << std::endl;
        
        std::pair<float, float> memUsage = getMemoryUsage();
        float memUsed = memUsage.first;
        float memTotal = memUsage.second;
        std::cout << "Memory Usage: " << memUsed << "MB / " << memTotal << "MB (" 
                  << std::fixed << std::setprecision(1) << (memUsed * 100.0 / memTotal) << "%)" << std::endl;
        
        std::pair<float, float> diskUsage = getDiskUsage();
        float diskUsed = diskUsage.first;
        float diskTotal = diskUsage.second;
        std::cout << "Disk Usage: " << diskUsed << "GB / " << diskTotal << "GB (" 
                  << std::fixed << std::setprecision(1) << (diskUsed * 100.0 / diskTotal) << "%)" << std::endl;
        std::cout << "=========================" << std::endl;
    }
    
    float getCpuUsage() {
        float cpuUsage = 0.0f;
        
#ifdef _WIN32
        FILETIME idleTime, kernelTime, userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            static ULARGE_INTEGER lastIdleTime = {0};
            static ULARGE_INTEGER lastKernelTime = {0};
            static ULARGE_INTEGER lastUserTime = {0};
            
            ULARGE_INTEGER idle, kernel, user;
            idle.LowPart = idleTime.dwLowDateTime;
            idle.HighPart = idleTime.dwHighDateTime;
            kernel.LowPart = kernelTime.dwLowDateTime;
            kernel.HighPart = kernelTime.dwHighDateTime;
            user.LowPart = userTime.dwLowDateTime;
            user.HighPart = userTime.dwHighDateTime;
            
            if (lastIdleTime.QuadPart != 0) {
                ULONGLONG idleDiff = idle.QuadPart - lastIdleTime.QuadPart;
                ULONGLONG totalDiff = (kernel.QuadPart - lastKernelTime.QuadPart) + 
                                     (user.QuadPart - lastUserTime.QuadPart);
                
                if (totalDiff > 0) {
                    cpuUsage = 100.0f - ((float)idleDiff * 100.0f / (float)totalDiff);
                }
            }
            
            lastIdleTime = idle;
            lastKernelTime = kernel;
            lastUserTime = user;
        }
#elif defined(__APPLE__)
        mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
        host_cpu_load_info_data_t cpuinfo;
        
        if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, 
                          (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {
            static unsigned long long previousUser = 0;
            static unsigned long long previousSystem = 0;
            static unsigned long long previousIdle = 0;
            static unsigned long long previousTotal = 0;
            
            unsigned long long user = cpuinfo.cpu_ticks[CPU_STATE_USER];
            unsigned long long system = cpuinfo.cpu_ticks[CPU_STATE_SYSTEM];
            unsigned long long idle = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
            unsigned long long total = user + system + idle;
            
            if (previousTotal > 0) {
                unsigned long long userDiff = user - previousUser;
                unsigned long long systemDiff = system - previousSystem;
                unsigned long long totalDiff = total - previousTotal;
                
                cpuUsage = (float)(userDiff + systemDiff) * 100.0f / (float)totalDiff;
            }
            
            previousUser = user;
            previousSystem = system;
            previousIdle = idle;
            previousTotal = total;
        }
#else
        std::ifstream stat_file("/proc/stat");
        if (stat_file.is_open()) {
            std::string line;
            if (std::getline(stat_file, line)) {
                std::string cpu;
                unsigned long long user, nice, system, idle, iowait, irq, softirq;
                std::istringstream ss(line);
                ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
                
                static unsigned long long prevIdle = 0;
                static unsigned long long prevTotal = 0;
                
                unsigned long long totalIdle = idle + iowait;
                unsigned long long total = user + nice + system + idle + iowait + irq + softirq;
                
                if (prevTotal > 0) {
                    unsigned long long diffIdle = totalIdle - prevIdle;
                    unsigned long long diffTotal = total - prevTotal;
                    cpuUsage = 100.0f * (1.0f - (float)diffIdle / (float)diffTotal);
                }
                
                prevIdle = totalIdle;
                prevTotal = total;
            }
            stat_file.close();
        }
#endif
        return cpuUsage;
    }
    
    std::pair<float, float> getMemoryUsage() {
        float used = 0.0f, total = 1.0f;
        
#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            total = static_cast<float>(memInfo.ullTotalPhys) / (1024 * 1024);
            used = total - static_cast<float>(memInfo.ullAvailPhys) / (1024 * 1024);
        }
#elif defined(__APPLE__)
        vm_size_t page_size;
        mach_port_t mach_port = mach_host_self();
        vm_statistics64_data_t vm_stats;
        mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
        
        if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
            host_statistics64(mach_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
            uint64_t free_memory = (uint64_t)vm_stats.free_count * (uint64_t)page_size;
            uint64_t used_memory = ((uint64_t)vm_stats.active_count + 
                                   (uint64_t)vm_stats.inactive_count + 
                                   (uint64_t)vm_stats.wire_count) * (uint64_t)page_size;
            
            int mib[2] = { CTL_HW, HW_MEMSIZE };
            u_int namelen = sizeof(mib) / sizeof(mib[0]);
            uint64_t memsize;
            size_t len = sizeof(memsize);
            
            if (sysctl(mib, namelen, &memsize, &len, NULL, 0) == 0) {
                total = static_cast<float>(memsize) / (1024 * 1024);
                used = static_cast<float>(used_memory) / (1024 * 1024);
            }
        }
#else
        struct sysinfo memInfo;
        if (sysinfo(&memInfo) == 0) {
            total = static_cast<float>(memInfo.totalram * memInfo.mem_unit) / (1024 * 1024);
            used = static_cast<float>((memInfo.totalram - memInfo.freeram) * memInfo.mem_unit) / (1024 * 1024);
        }
#endif
        return std::make_pair(used, total);
    }
    
    std::pair<float, float> getDiskUsage() {
        float used = 0.0f, total = 1.0f;
        
#ifdef _WIN32
        ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
        if (GetDiskFreeSpaceEx("C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
            total = static_cast<float>(totalBytes.QuadPart) / (1024 * 1024 * 1024);
            used = total - static_cast<float>(freeBytesAvailable.QuadPart) / (1024 * 1024 * 1024);
        }
#elif defined(__APPLE__)
        struct statfs stats;
        if (statfs("/", &stats) == 0) {
            total = static_cast<float>(stats.f_blocks * stats.f_bsize) / (1024 * 1024 * 1024);
            used = static_cast<float>((stats.f_blocks - stats.f_bfree) * stats.f_bsize) / (1024 * 1024 * 1024);
        }
#else
        struct statvfs statfs_buf;
        if (statvfs("/", &statfs_buf) == 0) {
            total = static_cast<float>(statfs_buf.f_blocks * statfs_buf.f_frsize) / (1024 * 1024 * 1024);
            used = static_cast<float>((statfs_buf.f_blocks - statfs_buf.f_bfree) * statfs_buf.f_frsize) / (1024 * 1024 * 1024);
        }
#endif
        return std::make_pair(used, total);
    }

protected:
    std::map<std::string, std::string> settings;
};


PLUGIN_API PluginInterface* createPlugin() { return new SystemMonitor(); }
PLUGIN_API void destroyPlugin(PluginInterface* plugin) { delete plugin; }
