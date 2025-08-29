#include <fcntl.h>  // For fcntl
#include <signal.h>
#include <sys/ioctl.h>  // Added for TIOCSTI
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#include "main.h"
#include "pluginapi.h"

static std::thread worker_thread;
static std::atomic<bool> running{false};
static FILE* worker_pipe = nullptr;
static pid_t worker_pid = 0;

// Required plugin information
extern "C" PLUGIN_API plugin_info_t* plugin_get_info() {
  static plugin_info_t info = {(char*)"jarvis", (char*)"0.1.0",
                               (char*)"Test prompt variable plugin",
                               (char*)"caden finley", PLUGIN_INTERFACE_VERSION};
  return &info;
}

// Helper to start Python process with a pipe
static FILE* start_python_worker(const char* cmd, pid_t& pid_out) {
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    perror("pipe");
    return nullptr;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return nullptr;
  }

  if (pid == 0) {
    // Child: redirect stdout to pipe
    close(pipefd[0]);  // close read end
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    execlp("python3", "python3", cmd, (char*)NULL);
    _exit(1);  // if execlp fails
  }

  // Parent: read from pipe
  close(pipefd[1]);  // close write end
  FILE* fp = fdopen(pipefd[0], "r");
  if (!fp) {
    perror("fdopen");
    kill(pid, SIGTERM);
    waitpid(pid, nullptr, 0);
    return nullptr;
  }

  pid_out = pid;
  return fp;
}

extern "C" PLUGIN_API int plugin_initialize() {
  running = true;
  // std::cerr << "[jarvis] Starting jarvis\n";

  const char* home = getenv("HOME");
  if (!home) {
    std::cerr << "[jarvis] Failed to get HOME environment variable\n";
    return PLUGIN_ERROR_GENERAL;
  }

  std::string script_path =
      std::string(home) + "/.config/cjsh/Jarvis/jarvis.py";

  worker_pipe = start_python_worker(script_path.c_str(), worker_pid);
  if (!worker_pipe) {
    std::cerr << "[jarvis] Failed to start jarvis\n";
    return PLUGIN_ERROR_GENERAL;
  }

  worker_thread = std::thread([] {
    char buffer[256];
    while (running && fgets(buffer, sizeof(buffer), worker_pipe)) {
      std::string line(buffer);
      if (!line.empty() && line.back() == '\n') line.pop_back();
      if (!line.empty()) {
        if (line[0] == '[' || line[0] == ' ') {
          std::cerr << line << std::endl;
          continue;
        }
        std::cout << "\n" << line << std::endl;
        std::string status_str;
        status_str = std::to_string(g_shell->do_ai_request(line));

        // if (g_shell->get_menu_active()) {
        //   status_str = std::to_string(g_shell->execute_command(line));
        // } else {
        //   if (line[0] == ':') {
        //     line.erase(0, 1);
        //     status_str = std::to_string(g_shell->execute_command(line));
        //   } else {
        //     status_str = std::to_string(g_shell->do_ai_request(line));
        //   }
        // }
      }
    }
  });

  std::cerr << "[jarvis] I am up and running sir.\n";
  return PLUGIN_SUCCESS;
}

extern "C" PLUGIN_API void plugin_shutdown() {
  running = false;

  // Kill Python process
  if (worker_pid > 0) {
    kill(worker_pid, SIGTERM);
    waitpid(worker_pid, nullptr, 0);
    worker_pid = 0;
  }

  // Close pipe
  if (worker_pipe) {
    fclose(worker_pipe);
    worker_pipe = nullptr;
  }

  // Wait for thread to finish
  if (worker_thread.joinable()) worker_thread.join();

  std::cerr << "[jarvis] Shutdown complete\n";
}

// Commands and events remain empty
extern "C" PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
  return PLUGIN_ERROR_NOT_IMPLEMENTED;
}
extern "C" PLUGIN_API char** plugin_get_commands(int* count) {
  *count = 0;
  return nullptr;
}
extern "C" PLUGIN_API char** plugin_get_subscribed_events(int* count) {
  *count = 0;
  return nullptr;
}
extern "C" PLUGIN_API plugin_setting_t* plugin_get_default_settings(
    int* count) {
  *count = 0;
  return nullptr;
}
extern "C" PLUGIN_API int plugin_update_setting(const char* key,
                                                const char* value) {
  return PLUGIN_ERROR_NOT_IMPLEMENTED;
}
extern "C" PLUGIN_API void plugin_free_memory(void* ptr) { std::free(ptr); }
