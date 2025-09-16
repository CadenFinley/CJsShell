#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <iostream>
#include <string>
#include <unordered_map>

// Performance monitoring class for measuring execution times
class PerformanceTimer {
 private:
  std::chrono::high_resolution_clock::time_point start_time;
  std::string operation_name;
  bool debug_mode;

 public:
  explicit PerformanceTimer(const std::string& name, bool debug = true)
      : operation_name(name), debug_mode(debug) {
    start_time = std::chrono::high_resolution_clock::now();
  }

  ~PerformanceTimer() {
    if (debug_mode) {
      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                          end_time - start_time)
                          .count();

      std::cerr << "PERF: " << operation_name << " took " << duration << "μs"
                << std::endl;
    }
  }

  // Get elapsed time without destroying the timer
  long get_elapsed_microseconds() const {
    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                                 start_time)
        .count();
  }

  // Reset the timer
  void reset() {
    start_time = std::chrono::high_resolution_clock::now();
  }
};

// Macro for easy performance timing with debug mode check
#define PERF_TIMER(name) PerformanceTimer timer(name, g_debug_mode)

// Performance statistics collector
class PerformanceStats {
 private:
  static constexpr size_t MAX_SAMPLES = 1000;

  struct OperationStats {
    std::string name;
    long total_time = 0;
    long min_time = LONG_MAX;
    long max_time = 0;
    size_t count = 0;
    size_t current_index = 0;
    std::array<long, MAX_SAMPLES> samples{};
  };

  std::unordered_map<std::string, OperationStats> stats;

 public:
  void record_operation(const std::string& name, long microseconds) {
    auto& op_stats = stats[name];
    op_stats.name = name;
    op_stats.total_time += microseconds;
    op_stats.min_time = std::min(op_stats.min_time, microseconds);
    op_stats.max_time = std::max(op_stats.max_time, microseconds);

    op_stats.samples[op_stats.current_index] = microseconds;
    op_stats.current_index = (op_stats.current_index + 1) % MAX_SAMPLES;
    op_stats.count++;
  }

  void print_stats() const {
    std::cerr << "=== Performance Statistics ===" << std::endl;
    for (const auto& [name, stats] : stats) {
      if (stats.count > 0) {
        long avg_time = stats.total_time / stats.count;
        std::cerr << stats.name << ": "
                  << "avg=" << avg_time << "μs, "
                  << "min=" << stats.min_time << "μs, "
                  << "max=" << stats.max_time << "μs, "
                  << "count=" << stats.count << std::endl;
      }
    }
  }

  void clear() {
    stats.clear();
  }
};

extern PerformanceStats g_perf_stats;