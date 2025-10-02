#include <fcntl.h>  // For fcntl
#include <signal.h>
#include <sys/ioctl.h>  // Added for TIOCSTI
#include <sys/stat.h>   // For mkdir and stat
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#include "pluginapi.h"

/**
 * Jarvis Plugin for CJ's Shell
 *
 * This plugin creates a Python subprocess that acts as an always-on AI
 * assistant. It conforms to the PLUGIN_INTERFACE_VERSION 3 requirements,
 * particularly with respect to memory management for returned arrays and
 * strings.
 */

static std::thread worker_thread;
static std::atomic<bool> running{false};
static FILE* worker_pipe = nullptr;
static pid_t worker_pid = 0;

// Required plugin information
extern "C" PLUGIN_API plugin_info_t* plugin_get_info() {
    static plugin_info_t info = {const_cast<char*>("jarvis"), const_cast<char*>("0.1.0"), const_cast<char*>("Test prompt variable plugin"),
                                 const_cast<char*>("caden finley"), PLUGIN_INTERFACE_VERSION};
    return &info;
}

// Helper function to create a heap-allocated string copy
char* create_string_copy(const char* src) {
    if (!src)
        return nullptr;
    size_t len = strlen(src) + 1;
    char* dest = (char*)PLUGIN_MALLOC(len);
    if (dest) {
        memcpy(dest, src, len);
    }
    return dest;
}

// Validate plugin (optional but recommended)
extern "C" PLUGIN_API plugin_validation_t plugin_validate() {
    plugin_validation_t result = {PLUGIN_SUCCESS, nullptr};

    // Perform self-validation here
    if (worker_pid > 0 && kill(worker_pid, 0) != 0) {
        result.status = PLUGIN_ERROR_GENERAL;
        result.error_message = create_string_copy("Worker process is not running");
        return result;
    }

    return result;
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

    std::string jarvis_dir = std::string(home) + "/.config/cjsh/Jarvis";
    std::string script_path = jarvis_dir + "/jarvis.py";

    // Check if script directory exists, create if it doesn't
    struct stat dir_info;
    if (stat(jarvis_dir.c_str(), &dir_info) != 0) {
        std::cerr << "[jarvis] Creating Jarvis directory: " << jarvis_dir << std::endl;
        if (mkdir(jarvis_dir.c_str(), 0755) != 0) {
            std::cerr << "[jarvis] Failed to create Jarvis directory" << std::endl;
            return PLUGIN_ERROR_GENERAL;
        }
    }

    // Check if script exists, create if it doesn't
    if (access(script_path.c_str(), F_OK) != 0) {
        std::cerr << "[jarvis] Creating jarvis.py script" << std::endl;
        FILE* script_file = fopen(script_path.c_str(), "w");
        if (!script_file) {
            std::cerr << "[jarvis] Failed to create jarvis.py" << std::endl;
            return PLUGIN_ERROR_GENERAL;
        }

        // Write the Python script content
        fprintf(script_file, "#!/usr/bin/env python3\n");
        fprintf(script_file, "import sys, queue, json, time, random\n");
        fprintf(script_file, "import sounddevice as sd\n");
        fprintf(script_file, "import vosk\n");
        fprintf(script_file, "import os\n\n");
        fprintf(script_file, "HOTWORD = \"jarvis\"   # customize hotword\n");
        fprintf(script_file, "ACTIVE_TIMEOUT = 2   # seconds after hotword to stay active\n\n");
        fprintf(script_file, "q = queue.Queue()\n\n");
        fprintf(script_file, "def callback(indata, frames, time_info, status):\n");
        fprintf(script_file, "    # if status:\n");
        fprintf(script_file, "    #     print(status, file=sys.stderr)\n");
        fprintf(script_file, "    q.put(bytes(indata))\n\n");
        fprintf(script_file, "def main():\n");
        fprintf(script_file, "    # Set environment variables to suppress logs\n");
        fprintf(script_file,
                "    os.environ[\"VOSK_LOG_LEVEL\"] = \"0\"  # 0 = no logs, 1 = "
                "errors, 2 = warnings, 3 = info\n");
        fprintf(script_file,
                "    os.environ[\"KALDI_LOG_LEVEL\"] = \"0\"  # Completely silence "
                "Kaldi logs\n");
        fprintf(script_file, "    \n");
        fprintf(script_file, "    # Jarvis response messages in Iron Man style\n");
        fprintf(script_file, "    jarvis_responses = [\n");
        fprintf(script_file, "        \"I'm listening sir.\",\n");
        fprintf(script_file, "        \"At your service, sir.\",\n");
        fprintf(script_file, "        \"How may I assist you today, sir?\",\n");
        fprintf(script_file, "        \"Ready and waiting, sir.\",\n");
        fprintf(script_file, "        \"Processing your request, sir.\",\n");
        fprintf(script_file, "        \"Standing by for instructions, sir.\",\n");
        fprintf(script_file, "        \"I'm all ears, sir.\",\n");
        fprintf(script_file, "        \"What can I do for you, sir?\",\n");
        fprintf(script_file, "        \"Awaiting your instructions, sir.\",\n");
        fprintf(script_file, "        \"How can I be of assistance, sir?\",\n");
        fprintf(script_file, "    ]\n");
        fprintf(script_file, "    \n");
        fprintf(script_file, "    # Additional settings to silence all Vosk/Kaldi logs\n");
        fprintf(script_file, "    if hasattr(vosk, \"SetLogLevel\"):\n");
        fprintf(script_file, "        vosk.SetLogLevel(-1)  # Set to lowest possible level\n");
        fprintf(script_file, "    \n");
        fprintf(script_file,
                "    # Redirect stderr temporarily during model loading to "
                "suppress logs\n");
        fprintf(script_file, "    original_stderr = sys.stderr\n");
        fprintf(script_file, "    sys.stderr = open(os.devnull, 'w')\n");
        fprintf(script_file, "    \n");
        fprintf(script_file,
                "    model_path = "
                "os.path.expanduser(\"~/.config/cjsh/Jarvis/"
                "vosk-model-small-en-us-0.15\")\n");
        fprintf(script_file, "    model = vosk.Model(model_path)\n");
        fprintf(script_file, "    rec = vosk.KaldiRecognizer(model, 16000)\n");
        fprintf(script_file, "    \n");
        fprintf(script_file, "    # Restore stderr\n");
        fprintf(script_file, "    sys.stderr.close()\n");
        fprintf(script_file, "    sys.stderr = original_stderr\n\n");
        fprintf(script_file, "    active = False\n");
        fprintf(script_file, "    last_active = 0\n\n");
        fprintf(script_file, "    with sd.RawInputStream(samplerate=16000, blocksize=8000,\n");
        fprintf(script_file, "                           dtype=\"int16\", channels=1,\n");
        fprintf(script_file, "                           callback=callback):\n");
        fprintf(script_file,
                "        #print(\"[system] Ready. Say 'jarvis' to wake me up.\", "
                "file=sys.stderr)\n");
        fprintf(script_file, "        while True:\n");
        fprintf(script_file, "            data = q.get()\n\n");
        fprintf(script_file, "            if rec.AcceptWaveform(data):\n");
        fprintf(script_file, "                result = json.loads(rec.Result())\n");
        fprintf(script_file, "                if \"text\" in result:\n");
        fprintf(script_file, "                    text = result[\"text\"].strip().lower()\n");
        fprintf(script_file, "                    if not text:\n");
        fprintf(script_file, "                        continue\n\n");
        fprintf(script_file, "                    if not active and HOTWORD in text:\n");
        fprintf(script_file,
                "                        # Select a random response when Jarvis is "
                "activated\n");
        fprintf(script_file,
                "                        response = "
                "random.choice(jarvis_responses)\n");
        fprintf(script_file,
                "                        print(f\"\\n[jarvis] {response}\", "
                "file=sys.stderr)\n");
        fprintf(script_file, "                        active = True\n");
        fprintf(script_file, "                        last_active = time.time()\n");
        fprintf(script_file, "                        continue\n\n");
        fprintf(script_file, "                    if active:\n");
        fprintf(script_file,
                "                        # Only print the actual command to stdout "
                "(no prefixes)\n");
        fprintf(script_file,
                "                        # This will be treated as a command to "
                "execute\n");
        fprintf(script_file, "                        print(text)  # forward command\n");
        fprintf(script_file, "                        sys.stdout.flush()\n");
        fprintf(script_file,
                "                        last_active = time.time()  # reset "
                "timeout\n");
        fprintf(script_file, "            else:\n");
        fprintf(script_file, "                # Handle partial results (streaming speech)\n");
        fprintf(script_file, "                part = json.loads(rec.PartialResult())\n");
        fprintf(script_file, "                if \"partial\" in part:\n");
        fprintf(script_file, "                    text = part[\"partial\"].strip().lower()\n");
        fprintf(script_file, "                    if active and text:\n");
        fprintf(script_file,
                "                        # optional: print partials for real-time "
                "feedback\n");
        fprintf(script_file,
                "                        # print(f\"(partial) {text}\", "
                "file=sys.stderr)\n");
        fprintf(script_file, "                        last_active = time.time()\n\n");
        fprintf(script_file, "            # Timeout handling\n");
        fprintf(script_file,
                "            if active and (time.time() - last_active > "
                "ACTIVE_TIMEOUT):\n");
        fprintf(script_file, "                active = False\n");
        fprintf(script_file,
                "                #print(\"[hotword] timeout, listening again\", "
                "file=sys.stderr)\n\n");
        fprintf(script_file, "if __name__ == \"__main__\":\n");
        fprintf(script_file, "    try:\n");
        fprintf(script_file, "        main()\n");
        fprintf(script_file, "    except KeyboardInterrupt:\n");
        fprintf(script_file, "        pass\n");

        fclose(script_file);

        // Make the script executable
        chmod(script_path.c_str(), 0755);

        std::cout << "[jarvis] Created jarvis.py script" << std::endl;

        // Check if the model directory exists and warn if it doesn't
        std::string model_dir = jarvis_dir + "/vosk-model-small-en-us-0.15";
        if (access(model_dir.c_str(), F_OK) != 0) {
            std::cout << "[jarvis] Warning: Voice model not found at " << model_dir << std::endl;
            std::cout << "[jarvis] Please download the Vosk model and extract it to "
                         "this location"
                      << std::endl;
        }
    }

    worker_pipe = start_python_worker(script_path.c_str(), worker_pid);
    if (!worker_pipe) {
        std::cerr << "[jarvis] Failed to start jarvis\n";
        return PLUGIN_ERROR_GENERAL;
    }

    worker_thread = std::thread([] {
        char buffer[256];
        while (running && fgets(buffer, sizeof(buffer), worker_pipe)) {
            std::string line(buffer);
            if (!line.empty() && line.back() == '\n')
                line.pop_back();
            if (!line.empty()) {
                if (line[0] == '[' || line[0] == ' ') {
                    std::cerr << line << std::endl;
                    continue;
                }
                std::cout << "\n" << line << std::endl;
                std::string status_str;
                status_str = std::to_string(g_shell->do_ai_request(line));
                reprint_prompt();

                // if (g_shell->get_menu_active()) {
                //   status_str =
                //   std::to_string(g_shell->execute_command(line));
                // } else {
                //   if (line[0] == ':') {
                //     line.erase(0, 1);
                //     status_str =
                //     std::to_string(g_shell->execute_command(line));
                //   } else {
                //     status_str =
                //     std::to_string(g_shell->do_ai_request(line));
                //   }
                // }
            }
        }
    });

    std::cout << "\n[jarvis] I am up and running sir.\n";
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
    if (worker_thread.joinable())
        worker_thread.join();

    std::cerr << "[jarvis] Shutdown complete\n";
}

// Commands and events remain empty
extern "C" PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
    return PLUGIN_ERROR_NOT_IMPLEMENTED;
}
extern "C" PLUGIN_API char** plugin_get_commands(int* count) {
    // No commands provided by this plugin
    *count = 0;
    return nullptr;
}
extern "C" PLUGIN_API char** plugin_get_subscribed_events(int* count) {
    // No events subscribed by this plugin
    *count = 0;
    return nullptr;
}
extern "C" PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
    // No settings provided by this plugin
    *count = 0;
    return nullptr;
}
extern "C" PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
    return PLUGIN_ERROR_NOT_IMPLEMENTED;
}
extern "C" PLUGIN_API void plugin_free_memory(void* ptr) {
    // Free memory allocated by this plugin
    if (ptr) {
        PLUGIN_FREE(ptr);
    }
}
