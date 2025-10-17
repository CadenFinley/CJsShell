#include "environment_info.h"

#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

#include "cjsh.h"
#include "exec.h"

std::string get_terminal_type() {
    const char* term = getenv("TERM");
    return term ? std::string(term) : "unknown";
}

std::pair<int, int> get_terminal_dimensions() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return {w.ws_col, w.ws_row};
}

std::string get_active_language_version(const std::string& language) {
    std::string cmd;

    if (language == "python") {
        cmd =
            "python3 --version 2>&1 | awk '{print $2}' || python --version "
            "2>&1 | "
            "awk '{print $2}'";
    } else if (language == "node") {
        cmd = "node --version 2>/dev/null | sed 's/^v//'";
    } else if (language == "ruby") {
        cmd = "ruby --version 2>/dev/null | awk '{print $2}'";
    } else if (language == "go") {
        cmd = "go version 2>/dev/null | awk '{print $3}' | sed 's/go//'";
    } else if (language == "rust") {
        cmd = "rustc --version 2>/dev/null | awk '{print $2}'";
    } else if (language == "java") {
        cmd = "java -version 2>&1 | head -n 1 | awk -F'\"' '{print $2}'";
    } else if (language == "php") {
        cmd = "php --version 2>/dev/null | head -n 1 | awk '{print $2}'";
    } else if (language == "perl") {
        cmd =
            "perl --version 2>/dev/null | head -n 2 | tail -n 1 | awk '{print "
            "$4}' "
            "| sed 's/[()v]//g'";
    } else if (language == "cpp" || language == "c++" || language == "c") {
        cmd =
            "g++ --version 2>/dev/null | head -n 1 | awk '{print $3}' || "
            "clang++ "
            "--version 2>/dev/null | head -n 1 | awk '{print $3}' || gcc "
            "--version "
            "2>/dev/null | head -n 1 | awk '{print $3}'";
    } else if (language == "csharp" || language == "dotnet") {
        cmd = "dotnet --version 2>/dev/null";
    } else if (language == "kotlin") {
        cmd = "kotlin -version 2>&1 | grep -oE '[0-9]+\\.[0-9]+\\.[0-9]+'";
    } else if (language == "swift") {
        cmd =
            "swift --version 2>/dev/null | grep -oE "
            "'[0-9]+\\.[0-9]+(\\.[0-9]+)?'";
    } else if (language == "dart") {
        cmd = "dart --version 2>&1 | grep -oE '[0-9]+\\.[0-9]+\\.[0-9]+'";
    } else if (language == "scala") {
        cmd = "scala -version 2>&1 | grep -oE '[0-9]+\\.[0-9]+\\.[0-9]+'";
    } else {
        return "";
    }

    auto cmd_result = exec_utils::execute_command_for_output(cmd);
    if (!cmd_result.success) {
        return "";
    }

    std::string result = cmd_result.output;
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result;
}

bool is_in_virtual_environment(std::string& env_name) {
    // Check for Python virtual environments
    const char* venv = getenv("VIRTUAL_ENV");
    if (venv) {
        std::string venv_path(venv);
        size_t last_slash = venv_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            env_name = venv_path.substr(last_slash + 1);
        } else {
            env_name = venv_path;
        }
        return true;
    }

    // Check for Conda environments
    const char* conda_env = getenv("CONDA_DEFAULT_ENV");
    if (conda_env) {
        env_name = std::string(conda_env);
        return true;
    }

    // Check for pipenv
    const char* pipenv = getenv("PIPENV_ACTIVE");
    if (pipenv && std::string(pipenv) == "1") {
        env_name = "pipenv";
        return true;
    }

    return false;
}

int get_background_jobs_count() {
    std::string cmd = "jobs | wc -l";
    auto cmd_result = exec_utils::execute_command_for_output(cmd);
    if (!cmd_result.success) {
        return 0;
    }

    int count = 0;
    try {
        count = std::stoi(cmd_result.output);
    } catch (const std::exception& e) {
        count = 0;
    }

    return count;
}

std::string get_shell() {
    return "cjsh";
}

std::string get_shell_version() {
    return get_version();
}
