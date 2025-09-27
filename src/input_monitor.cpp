#include "input_monitor.h"
#include <iostream>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>
#include <cstring>
#include "isocline/isocline.h"

extern bool g_debug_mode;

InputMonitor::InputMonitor() 
    : monitoring_active(false), should_stop(false) {
}

InputMonitor::~InputMonitor() {
    stop_monitoring();
}

void InputMonitor::start_monitoring() {
    if (monitoring_active.load()) {
        return; // Already monitoring
    }
    
    should_stop.store(false);
    monitoring_active.store(true);
    
    monitor_thread = std::thread(&InputMonitor::monitor_thread_function, this);
    
    if (g_debug_mode) {
        std::cerr << "DEBUG: InputMonitor started" << std::endl;
    }
}

void InputMonitor::stop_monitoring() {
    if (!monitoring_active.load()) {
        return;
    }
    
    should_stop.store(true);
    monitoring_active.store(false);
    
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    if (g_debug_mode) {
        std::cerr << "DEBUG: InputMonitor stopped" << std::endl;
    }
}

bool InputMonitor::has_queued_input() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return !input_queue.empty();
}

std::string InputMonitor::get_next_input() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    if (input_queue.empty()) {
        return "";
    }
    
    std::string input = input_queue.front();
    input_queue.pop();
    return input;
}

bool InputMonitor::is_monitoring() const {
    return monitoring_active.load();
}

void InputMonitor::monitor_thread_function() {
    // Check if stdin is a terminal
    if (!isatty(STDIN_FILENO)) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: InputMonitor: Not a TTY, skipping" << std::endl;
        }
        return;
    }
    
    struct termios original_termios, raw_termios;
    
    // Save original terminal settings
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: InputMonitor: Failed to get terminal attributes" << std::endl;
        }
        return;
    }
    
    std::string input_buffer;
    char ch;
    
    while (!should_stop.load()) {
        // Use select to check for input with timeout
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // 200ms timeout
        
        int ready = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            // Set up non-canonical mode to read character by character
            raw_termios = original_termios;
            raw_termios.c_lflag &= ~(ICANON | ECHO);
            raw_termios.c_cc[VMIN] = 0;
            raw_termios.c_cc[VTIME] = 1;
            
            if (tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios) == 0) {
                ssize_t bytes_read = read(STDIN_FILENO, &ch, 1);
                
                // Restore canonical mode
                tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
                
                if (bytes_read > 0) {
                    if (ch == '\n' || ch == '\r') {
                        // Complete command received
                        if (!input_buffer.empty()) {
                            std::lock_guard<std::mutex> lock(queue_mutex);
                            input_queue.push(input_buffer);
                            
                            if (g_debug_mode) {
                                std::cerr << "DEBUG: InputMonitor queued: '" 
                                          << input_buffer << "'" << std::endl;
                            }
                        }
                        input_buffer.clear();
                    } else if (ch == 3) { // Ctrl+C
                        if (g_debug_mode) {
                            std::cerr << "DEBUG: InputMonitor received Ctrl+C" << std::endl;
                        }
                        // Send interrupt signal to foreground process group
                        pid_t fg_pgid = tcgetpgrp(STDIN_FILENO);
                        if (fg_pgid != -1 && fg_pgid != getpgrp()) {
                            kill(-fg_pgid, SIGINT);
                        }
                        input_buffer.clear();
                    } else if (ch == 26) { // Ctrl+Z
                        if (g_debug_mode) {
                            std::cerr << "DEBUG: InputMonitor received Ctrl+Z" << std::endl;
                        }
                        // Send stop signal to foreground process group
                        pid_t fg_pgid = tcgetpgrp(STDIN_FILENO);
                        if (fg_pgid != -1 && fg_pgid != getpgrp()) {
                            kill(-fg_pgid, SIGTSTP);
                        }
                        input_buffer.clear();
                    } else if (ch >= 32 && ch < 127) { // Printable characters
                        input_buffer += ch;
                        // Echo the character
                        std::cout << ch << std::flush;
                    } else if (ch == 127 || ch == 8) { // Backspace
                        if (!input_buffer.empty()) {
                            input_buffer.pop_back();
                            std::cout << "\b \b" << std::flush;
                        }
                    }
                }
            }
        }
        
        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}