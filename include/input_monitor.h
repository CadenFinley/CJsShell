#pragma once
#ifndef INPUT_MONITOR_H
#define INPUT_MONITOR_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

class InputMonitor {
public:
    InputMonitor();
    ~InputMonitor();
    
    // Start monitoring for input during command execution
    void start_monitoring();
    
    // Stop monitoring
    void stop_monitoring();
    
    // Check if we have any queued input
    bool has_queued_input();
    
    // Get the next queued input command
    std::string get_next_input();
    
    // Check if monitoring is active
    bool is_monitoring() const;
    
private:
    void monitor_thread_function();
    
    std::thread monitor_thread;
    std::atomic<bool> monitoring_active;
    std::atomic<bool> should_stop;
    
    std::queue<std::string> input_queue;
    std::mutex queue_mutex;
};

#endif // INPUT_MONITOR_H