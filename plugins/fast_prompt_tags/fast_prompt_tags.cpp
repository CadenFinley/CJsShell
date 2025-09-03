#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "pluginapi.h"

/* Available threaded prompt placeholders:
 * --------------------------------------
 * The threaded_prompt plugin provides optimized, non-blocking alternatives
 * to standard prompt variables. These variables are updated in background
 * threads for better performance and responsiveness.
 *
 * System information placeholders:
 * {FAST_CPU}     - Current CPU usage percentage (updated every 5 seconds)
 * {FAST_MEM}     - Current memory usage percentage (updated every 5 seconds)
 * {FAST_BATTERY} - Battery percentage and charging status (updated every 30
 * seconds) {FAST_TIME}    - Current time (HH:MM:SS) in 24 hour format (updated
 * every second) {FAST_DATE}    - Current date (YYYY-MM-DD) (updated every 60
 * seconds)
 *
 * Network information placeholders:
 * {FAST_IP}      - Local IP address (updated every 60 seconds)
 * {FAST_NET}     - Active network interface (updated every 60 seconds)
 *
 * Git information placeholders:
 * {FAST_GIT_STATUS}  - Git status (✓ for clean, * for dirty) (updated every 5
 * seconds) {FAST_GIT_BRANCH}  - Current Git branch (updated every 5 seconds)
 * {FAST_GIT_AHEAD}   - Number of commits ahead of remote (updated every 30
 * seconds) {FAST_GIT_BEHIND}  - Number of commits behind remote (updated every
 * 30 seconds) {FAST_GIT_STASHES} - Number of stashes in the repository (updated
 * every 30 seconds) {FAST_GIT_STAGED}  - Has staged changes (✓ or empty)
 * (updated every 5 seconds) {FAST_GIT_CHANGES} - Number of uncommitted changes
 * (updated every 5 seconds)
 */

// Thread pool for handling background data collection
class PromptThreadPool {
 private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queue_mutex;
  std::condition_variable condition;
  std::atomic<bool> stop;

 public:
  PromptThreadPool(size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i) {
      workers.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });

            if (stop && tasks.empty()) {
              return;
            }

            task = std::move(tasks.front());
            tasks.pop();
          }
          task();
        }
      });
    }
  }

  template <class F>
  void enqueue(F&& f) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
  }

  // Get reference to the stop flag for tasks to check
  std::atomic<bool>& getStopFlag() { return stop; }

  ~PromptThreadPool() {
    // Set stop flag and clear any pending tasks
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop = true;

      // Clear any pending tasks to ensure threads don't hang
      std::queue<std::function<void()>> empty_queue;
      std::swap(tasks, empty_queue);
    }

    // Notify all threads to check stop condition
    condition.notify_all();

    // Join all threads
    for (std::thread& worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }

    // Clear worker vector
    workers.clear();
  }
};

// Class to manage cached prompt information
class PromptInfoCache {
 private:
  struct CacheEntry {
    std::string value;
    std::chrono::steady_clock::time_point expires;
    bool ready;
  };

  std::unordered_map<std::string, CacheEntry> cache;
  std::mutex cache_mutex;
  PromptThreadPool thread_pool;

  // Core system info that's refreshed on a schedule
  std::vector<std::string> core_keys = {
      "CPU_USAGE", "MEM_USAGE", "BATTERY",    "TIME",      "DATE",
      "IP_LOCAL",  "NET_IFACE", "GIT_STATUS", "GIT_BRANCH"};

  // How frequently to refresh data in seconds
  std::unordered_map<std::string, int> refresh_intervals = {
      {"CPU_USAGE", 5},  {"MEM_USAGE", 5},   {"BATTERY", 30},
      {"TIME", 1},       {"DATE", 60},       {"IP_LOCAL", 60},
      {"NET_IFACE", 60}, {"GIT_STATUS", 5},  {"GIT_BRANCH", 5},
      {"GIT_AHEAD", 30}, {"GIT_BEHIND", 30}, {"GIT_STASHES", 30},
      {"GIT_STAGED", 5}, {"GIT_CHANGES", 5}};

 public:
  PromptInfoCache() : thread_pool(4) {
    // Initialize the cache with default values first
    initializeDefaults();

    // Start background threads to update core info
    for (const auto& key : core_keys) {
      refreshInfoAsync(key);
    }
  }

  // Destructor to ensure thread pool is shut down properly
  ~PromptInfoCache() {
    // Thread pool will be automatically destroyed as a member variable
    // which triggers its destructor to stop and join all threads
  }

  void initializeDefaults() {
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Set initial default values to prevent empty or "N/A" values on first
    // render
    cache["CPU_USAGE"] = {"0", std::chrono::steady_clock::now(), true};
    cache["MEM_USAGE"] = {"0", std::chrono::steady_clock::now(), true};
    cache["BATTERY"] = {"100%", std::chrono::steady_clock::now(), true};

    auto now = std::chrono::system_clock::now();
    auto time_now = std::chrono::system_clock::to_time_t(now);
    struct tm time_info;
    localtime_r(&time_now, &time_info);

    char time_buffer[128];
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &time_info);
    cache["TIME"] = {time_buffer, std::chrono::steady_clock::now(), true};

    char date_buffer[128];
    strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", &time_info);
    cache["DATE"] = {date_buffer, std::chrono::steady_clock::now(), true};

    cache["IP_LOCAL"] = {"127.0.0.1", std::chrono::steady_clock::now(), true};
    cache["NET_IFACE"] = {"en0", std::chrono::steady_clock::now(), true};
    cache["GIT_STATUS"] = {"✓", std::chrono::steady_clock::now(), true};
    cache["GIT_BRANCH"] = {"master", std::chrono::steady_clock::now(), true};
    cache["GIT_AHEAD"] = {"0", std::chrono::steady_clock::now(), true};
    cache["GIT_BEHIND"] = {"0", std::chrono::steady_clock::now(), true};
    cache["GIT_STASHES"] = {"0", std::chrono::steady_clock::now(), true};
    cache["GIT_STAGED"] = {"0", std::chrono::steady_clock::now(), true};
    cache["GIT_CHANGES"] = {"0", std::chrono::steady_clock::now(), true};
  }

  void refreshInfoAsync(const std::string& key) {
    int interval = refresh_intervals.count(key) ? refresh_intervals[key] : 30;

    thread_pool.enqueue([this, key, interval]() {
      // Add a local atomic flag to check for stop signal
      std::atomic<bool>& threadpool_stop = thread_pool.getStopFlag();

      while (!threadpool_stop) {
        try {
          // Update the cache value here
          std::string value = fetchDataForKey(key);

          // Ensure we're not setting empty values
          if (value.empty()) {
            if (key == "CPU_USAGE" || key == "MEM_USAGE") {
              value = "0";
            } else if (key == "TIME") {
              auto now = std::chrono::system_clock::now();
              auto time_now = std::chrono::system_clock::to_time_t(now);
              struct tm time_info;
              localtime_r(&time_now, &time_info);

              char buffer[128];
              strftime(buffer, sizeof(buffer), "%H:%M:%S", &time_info);
              value = buffer;
            } else {
              value = "N/A";
            }
          }

          // Check stop flag again before updating cache
          if (threadpool_stop) break;

          {
            std::lock_guard<std::mutex> lock(cache_mutex);
            cache[key] = {value,
                          std::chrono::steady_clock::now() +
                              std::chrono::seconds(interval),
                          true};
          }
        } catch (...) {
          // Catch any exceptions during data fetching to prevent thread crashes
          if (threadpool_stop) break;

          std::lock_guard<std::mutex> lock(cache_mutex);
          cache[key].expires =
              std::chrono::steady_clock::now() + std::chrono::seconds(interval);
        }

        // Sleep with periodic checking of stop flag
        for (int i = 0; i < interval && !threadpool_stop; i++) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Final check before looping
        if (threadpool_stop) break;
      }
    });
  }

  std::string fetchDataForKey(const std::string& key) {
    // Commands updated for better error handling and consistent output

    if (key == "CPU_USAGE") {
      FILE* fp = popen(
          "top -l 1 | grep 'CPU usage' | awk '{print $3}' | tr -d '%' || echo "
          "'0'",
          "r");
      if (!fp) return "0";

      char buffer[128];
      std::string result = "";
      if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result = buffer;
      }
      pclose(fp);

      // Remove newline
      if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
      }

      return result.empty() ? "0" : result;
    } else if (key == "MEM_USAGE") {
      FILE* fp = popen(
          "ps -A -o %mem | awk '{ sum += $1 } END { print sum }' || echo '0'",
          "r");
      if (!fp) return "0";

      char buffer[128];
      std::string result = "";
      if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result = buffer;
      }
      pclose(fp);

      // Remove newline
      if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
      }

      return result.empty() ? "0" : result;
    } else if (key == "BATTERY") {
      // Get battery percentage on macOS with fallback
      FILE* fp = popen(
          "pmset -g batt | grep -Eo '\\d+%' | cut -d% -f1 || echo 'N/A'", "r");
      if (!fp) return "N/A";

      char buffer[128];
      std::string percent = "";
      if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        percent = buffer;
      }
      pclose(fp);

      // Remove newline
      if (!percent.empty() && percent[percent.length() - 1] == '\n') {
        percent.erase(percent.length() - 1);
      }

      if (percent == "N/A") return "N/A";

      // Check if charging with better error handling
      fp = popen("pmset -g batt | grep -o 'charging' || echo ''", "r");
      bool charging = false;
      if (fp) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          std::string result = buffer;
          if (!result.empty() && result[result.length() - 1] == '\n') {
            result.erase(result.length() - 1);
          }
          charging = !result.empty();
        }
        pclose(fp);
      }

      return percent + "%" + (charging ? " ⚡" : "");
    } else if (key == "TIME") {
      auto now = std::chrono::system_clock::now();
      auto time_now = std::chrono::system_clock::to_time_t(now);
      struct tm time_info;
      localtime_r(&time_now, &time_info);

      char buffer[128];
      strftime(buffer, sizeof(buffer), "%H:%M:%S", &time_info);
      return buffer;
    } else if (key == "DATE") {
      auto now = std::chrono::system_clock::now();
      auto time_now = std::chrono::system_clock::to_time_t(now);
      struct tm time_info;
      localtime_r(&time_now, &time_info);

      char buffer[128];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d", &time_info);
      return buffer;
    } else if (key == "IP_LOCAL") {
      FILE* fp = popen(
          "ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 "
          "2>/dev/null || echo 'N/A'",
          "r");
      if (!fp) return "N/A";

      char buffer[128];
      std::string result = "";
      if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result = buffer;
      }
      pclose(fp);

      // Remove newline
      if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
      }

      return result.empty() ? "N/A" : result;
    } else if (key == "NET_IFACE") {
      FILE* fp = popen(
          "route -n get default 2>/dev/null | grep interface | awk '{print "
          "$2}' || echo 'N/A'",
          "r");
      if (!fp) return "N/A";

      char buffer[128];
      std::string result = "";
      if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result = buffer;
      }
      pclose(fp);

      // Remove newline
      if (!result.empty() && result[result.length() - 1] == '\n') {
        result.erase(result.length() - 1);
      }

      return result.empty() ? "N/A" : result;
    } else if (key.find("GIT_") == 0) {
      // Detect if we're in a git repository first
      FILE* fp = popen(
          "git rev-parse --is-inside-work-tree 2>/dev/null || echo 'false'",
          "r");
      if (!fp) return "N/A";

      char buffer[128];
      std::string in_git_repo = "";
      if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        in_git_repo = buffer;
      }
      pclose(fp);

      // Remove newline
      if (!in_git_repo.empty() &&
          in_git_repo[in_git_repo.length() - 1] == '\n') {
        in_git_repo.erase(in_git_repo.length() - 1);
      }

      // If not in a git repository, return appropriate defaults
      if (in_git_repo != "true") {
        if (key == "GIT_STATUS" || key == "GIT_STAGED") {
          return "";
        } else if (key == "GIT_BRANCH") {
          return "no git";
        } else {
          return "0";
        }
      }

      // We're in a git repository, handle each key appropriately
      if (key == "GIT_STATUS") {
        // Check for unstaged changes
        FILE* fp = popen(
            "git status --porcelain 2>/dev/null | wc -l | tr -d ' ' || echo "
            "'0'",
            "r");
        if (!fp) return "✓";

        char buffer[128];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result = buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return (result == "0") ? "✓" : "*";
      } else if (key == "GIT_BRANCH") {
        FILE* fp = popen(
            "git symbolic-ref --short HEAD 2>/dev/null || git rev-parse "
            "--short HEAD 2>/dev/null || echo 'unknown'",
            "r");
        if (!fp) return "unknown";

        char buffer[128];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result = buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result.empty() ? "unknown" : result;
      } else if (key == "GIT_AHEAD") {
        FILE* fp = popen(
            "git rev-list --count @{upstream}..HEAD 2>/dev/null || echo '0'",
            "r");
        if (!fp) return "0";

        char buffer[128];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result = buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result.empty() ? "0" : result;
      } else if (key == "GIT_BEHIND") {
        FILE* fp = popen(
            "git rev-list --count HEAD..@{upstream} 2>/dev/null || echo '0'",
            "r");
        if (!fp) return "0";

        char buffer[128];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result = buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result.empty() ? "0" : result;
      } else if (key == "GIT_STASHES") {
        FILE* fp = popen(
            "git stash list 2>/dev/null | wc -l | tr -d ' ' || echo '0'", "r");
        if (!fp) return "0";

        char buffer[128];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result = buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result.empty() ? "0" : result;
      } else if (key == "GIT_STAGED") {
        FILE* fp = popen(
            "git diff --cached --name-only 2>/dev/null | wc -l | tr -d ' ' || "
            "echo '0'",
            "r");
        if (!fp) return "0";

        char buffer[128];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result = buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return (result == "0") ? "" : "✓";
      } else if (key == "GIT_CHANGES") {
        FILE* fp = popen(
            "git status --porcelain 2>/dev/null | wc -l | tr -d ' ' || echo "
            "'0'",
            "r");
        if (!fp) return "0";

        char buffer[128];
        std::string result = "";
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result = buffer;
        }
        pclose(fp);

        // Remove newline
        if (!result.empty() && result[result.length() - 1] == '\n') {
          result.erase(result.length() - 1);
        }

        return result.empty() ? "0" : result;
      }

      return "N/A";  // Default for unknown git-related info
    }

    return "N/A";
  }

  std::string getValue(const std::string& key) {
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Check if we have a cached value that's still valid
    auto it = cache.find(key);
    if (it != cache.end() && it->second.ready &&
        std::chrono::steady_clock::now() < it->second.expires) {
      return it->second.value;
    }

    // If we don't have a valid cached value, return a placeholder
    // rather than blocking to fetch new data
    if (it != cache.end()) {
      // Return the expired value while a refresh is happening in the background
      return it->second.value;
    }

    // If we don't have any cached value, create an initial one
    // and schedule a background refresh
    std::string value = "...";  // Non-blocking placeholder

    int interval = refresh_intervals.count(key) ? refresh_intervals[key] : 30;
    cache[key] = {
        value,
        std::chrono::steady_clock::now() +
            std::chrono::seconds(1),  // Short expiry to prompt quick refresh
        true};

    // Schedule background refresh if this is a new key
    if (std::find(core_keys.begin(), core_keys.end(), key) == core_keys.end()) {
      refreshInfoAsync(key);
    }

    return value;
  }
};

// Global instance of the cache
static PromptInfoCache* g_cache = nullptr;

// Functions to register the prompt variables
static plugin_string_t cpu_usage_callback() {
  std::string result = g_cache ? g_cache->getValue("CPU_USAGE") : "0";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t memory_usage_callback() {
  std::string result = g_cache ? g_cache->getValue("MEM_USAGE") : "0";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t battery_callback() {
  std::string result = g_cache ? g_cache->getValue("BATTERY") : "N/A";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t time_callback() {
  std::string result = g_cache ? g_cache->getValue("TIME") : "00:00:00";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t date_callback() {
  std::string result = g_cache ? g_cache->getValue("DATE") : "1970-01-01";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t ip_local_callback() {
  std::string result = g_cache ? g_cache->getValue("IP_LOCAL") : "N/A";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t net_iface_callback() {
  std::string result = g_cache ? g_cache->getValue("NET_IFACE") : "N/A";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t git_status_callback() {
  std::string result = g_cache ? g_cache->getValue("GIT_STATUS") : "✓";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t git_branch_callback() {
  std::string result = g_cache ? g_cache->getValue("GIT_BRANCH") : "N/A";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t git_ahead_callback() {
  std::string result = g_cache ? g_cache->getValue("GIT_AHEAD") : "0";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t git_behind_callback() {
  std::string result = g_cache ? g_cache->getValue("GIT_BEHIND") : "0";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t git_stashes_callback() {
  std::string result = g_cache ? g_cache->getValue("GIT_STASHES") : "0";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t git_staged_callback() {
  std::string result = g_cache ? g_cache->getValue("GIT_STAGED") : "0";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

static plugin_string_t git_changes_callback() {
  std::string result = g_cache ? g_cache->getValue("GIT_CHANGES") : "0";
  char* data = (char*)std::malloc(result.length() + 1);
  std::memcpy(data, result.c_str(), result.length() + 1);
  plugin_string_t string_result = {data, static_cast<int>(result.length())};
  return string_result;
}

// Required plugin information
extern "C" PLUGIN_API plugin_info_t* plugin_get_info() {
  static plugin_info_t info = {
      (char*)"threaded_prompt", (char*)"0.3.0",
      (char*)"Fast prompt info using background threads", (char*)"Caden Finley",
      PLUGIN_INTERFACE_VERSION};
  return &info;
}

// Initialize plugin: create cache and register variables
extern "C" PLUGIN_API int plugin_initialize() {
  // Create the global cache instance
  g_cache = new PromptInfoCache();

  // Register our fast versions of standard prompt variables
  plugin_register_prompt_variable("FAST_CPU", cpu_usage_callback);
  plugin_register_prompt_variable("FAST_MEM", memory_usage_callback);
  plugin_register_prompt_variable("FAST_BATTERY", battery_callback);
  plugin_register_prompt_variable("FAST_TIME", time_callback);
  plugin_register_prompt_variable("FAST_DATE", date_callback);
  plugin_register_prompt_variable("FAST_IP", ip_local_callback);
  plugin_register_prompt_variable("FAST_NET", net_iface_callback);

  // Git-specific variables
  plugin_register_prompt_variable("FAST_GIT_STATUS", git_status_callback);
  plugin_register_prompt_variable("FAST_GIT_BRANCH", git_branch_callback);
  plugin_register_prompt_variable("FAST_GIT_AHEAD", git_ahead_callback);
  plugin_register_prompt_variable("FAST_GIT_BEHIND", git_behind_callback);
  plugin_register_prompt_variable("FAST_GIT_STASHES", git_stashes_callback);
  plugin_register_prompt_variable("FAST_GIT_STAGED", git_staged_callback);
  plugin_register_prompt_variable("FAST_GIT_CHANGES", git_changes_callback);

  return PLUGIN_SUCCESS;
}

// Clean up resources
extern "C" PLUGIN_API void plugin_shutdown() {
  // First delete the cache which will trigger the thread pool destructor
  // to properly stop and join all worker threads
  delete g_cache;
  g_cache = nullptr;

  // Give threads a moment to clean up if needed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// No commands provided by this plugin
extern "C" PLUGIN_API int plugin_handle_command(plugin_args_t* /*args*/) {
  return PLUGIN_ERROR_NOT_IMPLEMENTED;
}

extern "C" PLUGIN_API char** plugin_get_commands(int* count) {
  // Allocate heap memory for empty array to conform with API requirements
  *count = 0;
  return (char**)malloc(0);  // Return empty heap-allocated array
}

// No events subscribed
extern "C" PLUGIN_API char** plugin_get_subscribed_events(int* count) {
  // Allocate heap memory for empty array to conform with API requirements
  *count = 0;
  return (char**)malloc(0);  // Return empty heap-allocated array
}

// No settings
extern "C" PLUGIN_API plugin_setting_t* plugin_get_default_settings(
    int* count) {
  // Allocate heap memory for empty array to conform with API requirements
  *count = 0;
  return (plugin_setting_t*)malloc(0);  // Return empty heap-allocated array
}

extern "C" PLUGIN_API int plugin_update_setting(const char* /*key*/,
                                                const char* /*value*/) {
  return PLUGIN_ERROR_NOT_IMPLEMENTED;
}

// Free memory allocated by the plugin
extern "C" PLUGIN_API void plugin_free_memory(void* ptr) {
  if (ptr != nullptr) {
    std::free(ptr);
  }
}
