#include "environment_info.h"

#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "cjsh.h"
#include "utils/cjsh_filesystem.h"

std::string EnvironmentInfo::get_terminal_type() {
  const char* term = getenv("TERM");
  return term ? std::string(term) : "unknown";
}

std::pair<int, int> EnvironmentInfo::get_terminal_dimensions() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return {w.ws_col, w.ws_row};
}

std::string EnvironmentInfo::get_active_language_version(
    const std::string& language) {
  std::string cmd;

  if (language == "python") {
    cmd =
        "python3 --version 2>&1 | awk '{print $2}' || python --version 2>&1 | "
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
        "perl --version 2>/dev/null | head -n 2 | tail -n 1 | awk '{print $4}' "
        "| sed 's/[()v]//g'";
  } else {
    return "";
  }

  auto cmd_result = cjsh_filesystem::FileOperations::read_command_output(cmd);
  if (cmd_result.is_error()) {
    return "";
  }

  std::string result = cmd_result.value();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }

  return result;
}

bool EnvironmentInfo::is_in_virtual_environment(std::string& env_name) {
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

int EnvironmentInfo::get_background_jobs_count() {
  std::string cmd = "jobs | wc -l";
  auto cmd_result = cjsh_filesystem::FileOperations::read_command_output(cmd);
  if (cmd_result.is_error()) {
    return 0;
  }

  int count = 0;
  try {
    count = std::stoi(cmd_result.value());
  } catch (const std::exception& e) {
    count = 0;
  }

  return count;
}

std::string EnvironmentInfo::get_shell() {
  return "cjsh";
}

std::string EnvironmentInfo::get_shell_version() {
  extern const std::string c_version;
  return c_version;
}