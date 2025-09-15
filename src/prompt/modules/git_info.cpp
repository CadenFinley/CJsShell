#include "git_info.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "cjsh.h"

GitInfo::GitInfo() {
  if (g_debug_mode)
    std::cerr << "DEBUG: GitInfo constructor" << std::endl;

  last_git_status_check =
      std::chrono::steady_clock::now() - std::chrono::seconds(30);
  is_git_status_check_running = false;
  cached_is_clean_repo = true;

  if (g_debug_mode)
    std::cerr << "DEBUG: GitInfo constructor END" << std::endl;
}

GitInfo::~GitInfo() {
  if (g_debug_mode)
    std::cerr << "DEBUG: GitInfo destructor" << std::endl;
}

std::string GitInfo::get_git_remote(const std::filesystem::path& repo_root) {
  std::string cmd =
      "git -C '" + repo_root.string() + "' remote get-url origin 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[256];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string GitInfo::get_git_tag(const std::filesystem::path& repo_root) {
  std::string cmd = "git -C '" + repo_root.string() +
                    "' describe --tags --abbrev=0 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string GitInfo::get_git_last_commit(
    const std::filesystem::path& repo_root) {
  std::string cmd = "git -C '" + repo_root.string() +
                    "' log -1 --pretty=format:%h:%s 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[256];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  return result;
}

std::string GitInfo::get_git_author(const std::filesystem::path& repo_root) {
  std::string cmd = "git -C '" + repo_root.string() +
                    "' log -1 --pretty=format:%an 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp)
    return "";
  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), fp) != NULL)
    result += buffer;
  pclose(fp);
  if (!result.empty() && result.back() == '\n')
    result.pop_back();
  return result;
}

std::string GitInfo::get_git_branch(
    const std::filesystem::path& git_head_path) {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_git_branch START for " << git_head_path.string()
              << std::endl;

  try {
    std::ifstream head_file(git_head_path);
    if (!head_file.is_open()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: get_git_branch unable to open HEAD file"
                  << std::endl;
      return "";
    }

    std::string head_contents;
    std::getline(head_file, head_contents);
    head_file.close();

    const std::string ref_prefix = "ref: refs/heads/";
    if (head_contents.substr(0, ref_prefix.length()) == ref_prefix) {
      std::string branch = head_contents.substr(ref_prefix.length());
      if (g_debug_mode)
        std::cerr << "DEBUG: get_git_branch END: " << branch << std::endl;
      return branch;
    } else {
      if (g_debug_mode)
        std::cerr << "DEBUG: get_git_branch END: detached HEAD" << std::endl;
      return head_contents.substr(0, 7);
    }
  } catch (const std::exception& e) {
    if (g_debug_mode)
      std::cerr << "DEBUG: get_git_branch exception: " << e.what() << std::endl;
    return "";
  }
}

std::string GitInfo::get_git_status(const std::filesystem::path& repo_root) {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_git_status START for " << repo_root.string()
              << std::endl;

  std::string status_symbols = "";
  std::string git_dir = repo_root.string();
  bool is_clean_repo = true;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                     now - last_git_status_check)
                     .count();

  if ((elapsed > 60 || cached_git_dir != git_dir) &&
      !is_git_status_check_running) {
    std::lock_guard<std::mutex> lock(git_status_mutex);
    if (!is_git_status_check_running) {
      is_git_status_check_running = true;

      std::string cmd =
          "git -C '" + git_dir + "' status --porcelain 2>/dev/null";
      FILE* fp = popen(cmd.c_str(), "r");
      if (fp) {
        char buffer[256];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
          result += buffer;
        }
        pclose(fp);

        if (!result.empty()) {
          is_clean_repo = false;
          status_symbols = "*";
        } else {
          is_clean_repo = true;
          status_symbols = "✓";
        }
      } else {
        is_clean_repo = true;
        status_symbols = "✓";
      }

      cached_git_dir = git_dir;
      cached_status_symbols = status_symbols;
      cached_is_clean_repo = is_clean_repo;
      last_git_status_check = now;
      is_git_status_check_running = false;
    }
  } else {
    status_symbols = cached_status_symbols;
    is_clean_repo = cached_is_clean_repo;
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_git_status END: " << status_symbols << std::endl;

  if (is_clean_repo) {
    return "✓";
  } else {
    return "*";
  }
}

std::string GitInfo::get_local_path(const std::filesystem::path& repo_root) {
  if (g_debug_mode)
    std::cerr << "DEBUG: get_local_path START for " << repo_root.string()
              << std::endl;

  std::filesystem::path cwd = std::filesystem::current_path();
  std::string repo_root_path = repo_root.string();
  std::string repo_root_name = repo_root.filename().string();
  std::string current_path_str = cwd.string();

  std::string result;
  if (current_path_str == repo_root_path) {
    result = repo_root_name;
  } else {
    try {
      std::filesystem::path relative_path =
          std::filesystem::relative(cwd, repo_root);
      result = repo_root_name + "/" + relative_path.string();
    } catch (const std::exception& e) {
      result = cwd.filename().string();
    }
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: get_local_path END: " << result << std::endl;
  return result;
}

int GitInfo::get_git_ahead_behind(const std::filesystem::path& repo_root,
                                  int& ahead, int& behind) {
  ahead = 0;
  behind = 0;

  std::filesystem::path git_head_path = repo_root / ".git" / "HEAD";
  std::string branch = get_git_branch(git_head_path);
  if (branch.empty()) {
    return -1;
  }

  std::string upstream_cmd = "git -C '" + repo_root.string() +
                             "' rev-parse --abbrev-ref " + branch +
                             "@{upstream} 2>/dev/null";
  FILE* upstream_fp = popen(upstream_cmd.c_str(), "r");
  if (!upstream_fp) {
    return -1;
  }

  char upstream_buffer[128];
  std::string upstream = "";
  if (fgets(upstream_buffer, sizeof(upstream_buffer), upstream_fp) != NULL) {
    upstream = upstream_buffer;
    if (!upstream.empty() && upstream.back() == '\n') {
      upstream.pop_back();
    }
  }
  pclose(upstream_fp);

  if (upstream.empty()) {
    return -1;
  }

  std::string count_cmd = "git -C '" + repo_root.string() +
                          "' rev-list --left-right --count " + branch + "..." +
                          upstream + " 2>/dev/null";
  FILE* count_fp = popen(count_cmd.c_str(), "r");
  if (!count_fp) {
    return -1;
  }

  char count_buffer[64];
  if (fgets(count_buffer, sizeof(count_buffer), count_fp) != NULL) {
    std::string count_str = count_buffer;
    size_t tab_pos = count_str.find('\t');
    if (tab_pos != std::string::npos) {
      try {
        ahead = std::stoi(count_str.substr(0, tab_pos));
        behind = std::stoi(count_str.substr(tab_pos + 1));
      } catch (const std::exception& e) {
        pclose(count_fp);
        return -1;
      }
    }
  }
  pclose(count_fp);

  return 0;
}

int GitInfo::get_git_stash_count(const std::filesystem::path& repo_root) {
  std::string cmd =
      "git -C '" + repo_root.string() + "' stash list 2>/dev/null | wc -l";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    return 0;
  }

  char buffer[32];
  int count = 0;
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    try {
      count = std::stoi(buffer);
    } catch (const std::exception& e) {
      count = 0;
    }
  }
  pclose(fp);

  return count;
}

bool GitInfo::get_git_has_staged_changes(
    const std::filesystem::path& repo_root) {
  std::string cmd =
      "git -C '" + repo_root.string() + "' diff --cached --quiet 2>/dev/null";
  int result = system(cmd.c_str());

  if (result == 0) {
    return false;
  } else if (result == 256) {
    return true;
  } else {
    return false;
  }
}

int GitInfo::get_git_uncommitted_changes(
    const std::filesystem::path& repo_root) {
  std::string cmd = "git -C '" + repo_root.string() +
                    "' status --porcelain 2>/dev/null | wc -l";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    return 0;
  }

  char buffer[32];
  int count = 0;
  if (fgets(buffer, sizeof(buffer), fp) != NULL) {
    try {
      count = std::stoi(buffer);
    } catch (const std::exception& e) {
      count = 0;
    }
  }
  pclose(fp);

  return count;
}