#include "ls_command.h"

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#define COLOR_RESET "\033[0m"
#define COLOR_BLUE "\033[34m"
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_CYAN "\033[36m"
#define COLOR_YELLOW "\033[33m"

int ls_command(const std::vector<std::string>& args) {
  std::string path = ".";
  bool show_hidden = false;
  bool long_format = false;
  bool sort_by_size = false;
  bool reverse_order = false;
  bool sort_by_time = false;
  bool human_readable = false;
  bool recursive = false;
  bool one_per_line = false;
  bool show_inode = false;

  for (size_t i = 1; i < args.size(); i++) {
    if (args[i][0] == '-' && args[i].length() > 1 && args[i][1] != '-') {
      for (size_t j = 1; j < args[i].length(); j++) {
        switch (args[i][j]) {
          case 'a':
            show_hidden = true;
            break;
          case 'l':
            long_format = true;
            break;
          case 'S':
            sort_by_size = true;
            break;
          case 'r':
            reverse_order = true;
            break;
          case 't':
            sort_by_time = true;
            break;
          case 'h':
            human_readable = true;
            break;
          case 'R':
            recursive = true;
            break;
          case '1':
            one_per_line = true;
            break;
          case 'i':
            show_inode = true;
            break;
          default:
            std::cerr << "Unknown option: -" << args[i][j] << std::endl;
            return 1;
        }
      }
    } else if (args[i] == "--help") {
      std::cout << "Usage: ls [OPTION]... [FILE]..." << std::endl;
      std::cout << "List information about files." << std::endl << std::endl;
      std::cout << "  -a             show all files, including hidden files"
                << std::endl;
      std::cout << "  -l             use long listing format" << std::endl;
      std::cout << "  -S             sort by file size, largest first"
                << std::endl;
      std::cout << "  -r             reverse order while sorting" << std::endl;
      std::cout << "  -t             sort by modification time, newest first"
                << std::endl;
      std::cout << "  -h             print sizes in human readable format"
                << std::endl;
      std::cout << "  -R             list subdirectories recursively"
                << std::endl;
      std::cout << "  -1             list one file per line" << std::endl;
      std::cout << "  -i             print the inode number" << std::endl;
      return 0;
    } else if (args[i][0] == '-') {
      std::cerr << "Unknown option: " << args[i] << std::endl;
      return 1;
    } else {
      path = args[i];
    }
  }

  return list_directory(path, show_hidden, long_format, sort_by_size,
                        reverse_order, sort_by_time, human_readable, recursive,
                        one_per_line, show_inode, 0);
}

std::string format_size_human_readable(uintmax_t size) {
  const char* units[] = {"B", "K", "M", "G", "T", "P", "E"};
  int unit_index = 0;
  double size_d = static_cast<double>(size);

  while (size_d >= 1024.0 && unit_index < 6) {
    size_d /= 1024.0;
    unit_index++;
  }

  std::ostringstream result;
  if (unit_index == 0) {
    result << size;
  } else if (size_d < 10) {
    result << std::fixed << std::setprecision(1) << size_d << units[unit_index];
  } else {
    result << std::fixed << std::setprecision(0) << size_d << units[unit_index];
  }

  return result.str();
}

uintmax_t calculate_directory_size(const std::filesystem::path& dir_path) {
  uintmax_t size = 0;
  try {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path)) {
      if (entry.is_regular_file()) {
        try {
          size += std::filesystem::file_size(entry);
        } catch (...) {
          // Ignore files we can't access
        }
      }
    }
  } catch (...) {
    // Ignore directories we can't access
  }
  return size;
}

std::string format_size(uintmax_t size, bool human_readable) {
  if (human_readable) {
    return format_size_human_readable(size);
  } else {
    if (size < 1024)
      return std::to_string(size) + " B";
    else if (size < 1024 * 1024)
      return std::to_string(size / 1024) + " KB";
    else if (size < 1024 * 1024 * 1024)
      return std::to_string(size / (1024 * 1024)) + " MB";
    else
      return std::to_string(size / (1024 * 1024 * 1024)) + " GB";
  }
}

int list_directory(const std::string& path, bool show_hidden, bool long_format,
                   bool sort_by_size, bool reverse_order, bool sort_by_time,
                   bool human_readable, bool recursive, bool one_per_line,
                   bool show_inode, int level) {
  try {
    std::vector<std::filesystem::directory_entry> entries;
    
    // Add . and .. entries when showing hidden files
    if (show_hidden) {
      // Add current directory (.)
      std::filesystem::path current_path = std::filesystem::absolute(path);
      entries.emplace_back(current_path);
      
      // Add parent directory (..)
      std::filesystem::path parent_path = current_path.parent_path();
      entries.emplace_back(parent_path);
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      if (!show_hidden && entry.path().filename().string()[0] == '.') {
        continue;
      }
      entries.push_back(entry);
    }

    auto compare_entries = [&](const auto& a, const auto& b) {
      bool a_is_dir = std::filesystem::is_directory(a);
      bool b_is_dir = std::filesystem::is_directory(b);

      if (!reverse_order) {
        if (a_is_dir && !b_is_dir)
          return true;
        if (!a_is_dir && b_is_dir)
          return false;
      }

      if (sort_by_time) {
        struct stat a_stat, b_stat;
        memset(&a_stat, 0, sizeof(a_stat));
        memset(&b_stat, 0, sizeof(b_stat));
        stat(a.path().c_str(), &a_stat);
        stat(b.path().c_str(), &b_stat);
        if (a_stat.st_mtime != b_stat.st_mtime) {
          return reverse_order ? (a_stat.st_mtime < b_stat.st_mtime)
                               : (a_stat.st_mtime > b_stat.st_mtime);
        }
      } else if (sort_by_size && !a_is_dir && !b_is_dir) {
        try {
          uintmax_t a_size = std::filesystem::file_size(a);
          uintmax_t b_size = std::filesystem::file_size(b);
          if (a_size != b_size) {
            return reverse_order ? (a_size < b_size) : (a_size > b_size);
          }
        } catch (...) {
        }
      }

      return reverse_order ? (a.path().filename() > b.path().filename())
                           : (a.path().filename() < b.path().filename());
    };

    std::sort(entries.begin(), entries.end(), compare_entries);

    if (recursive && level > 0) {
      std::cout << "\n" << std::string(level, ' ') << path << ":" << std::endl;
    } else if (recursive) {
      std::cout << path << ":" << std::endl;
    }

    if (long_format) {
      if (show_inode) {
        std::cout << std::setw(10) << std::left << "Inode";
      }
      std::cout << std::setw(12) << std::left << "Permissions" << std::setw(3)
                << "Lnk" << std::setw(10) << "Owner" << std::setw(10) << "Group"
                << std::setw(12) << std::right << "Size" << std::setw(20)
                << "Modified"
                << "  Name" << std::endl;
      std::cout << std::string(80, '-') << std::endl;
    } else if (!one_per_line) {
      if (show_inode) {
        std::cout << std::setw(10) << std::left << "Inode";
      }
      std::cout << std::setw(40) << std::left << "Name" << std::setw(15)
                << "Size"
                << "Type" << std::endl;
      std::cout << std::string(60, '-') << std::endl;
    }

    for (const auto& entry : entries) {
      std::string name = entry.path().filename().string();
      
      // Handle special cases for . and .. directories
      std::filesystem::path current_path = std::filesystem::absolute(path);
      if (show_hidden && entry.path() == current_path) {
        name = ".";
      } else if (show_hidden && entry.path() == current_path.parent_path()) {
        name = "..";
      }
      
      std::string type;
      std::string color;

      if (std::filesystem::is_directory(entry)) {
        type = "Directory";
        color = COLOR_BLUE;
      } else if (std::filesystem::is_symlink(entry)) {
        type = "Symlink";
        color = COLOR_CYAN;
      } else if (entry.path().extension() == ".cpp" ||
                 entry.path().extension() == ".h" ||
                 entry.path().extension() == ".hpp" ||
                 entry.path().extension() == ".py" ||
                 entry.path().extension() == ".js" ||
                 entry.path().extension() == ".java" ||
                 entry.path().extension() == ".cs" ||
                 entry.path().extension() == ".rb" ||
                 entry.path().extension() == ".php" ||
                 entry.path().extension() == ".go" ||
                 entry.path().extension() == ".swift" ||
                 entry.path().extension() == ".ts" ||
                 entry.path().extension() == ".rs" ||
                 entry.path().extension() == ".html" ||
                 entry.path().extension() == ".css") {
        type = "Source";
        color = COLOR_GREEN;
      } else if (std::filesystem::is_regular_file(entry) &&
                 (entry.path().extension() == ".so" ||
                  entry.path().extension() == ".dylib" ||
                  entry.path().extension() == ".exe" ||
                  (std::filesystem::status(entry).permissions() &
                   std::filesystem::perms::owner_exec) !=
                      std::filesystem::perms::none)) {
        type = "Executable";
        color = COLOR_RED;
      } else {
        type = "File";
        color = COLOR_RESET;
      }

      std::string size_str = "-";
      if (std::filesystem::is_regular_file(entry)) {
        try {
          uintmax_t size = std::filesystem::file_size(entry);
          size_str = format_size(size, human_readable);
        } catch (...) {
          size_str = "???";
        }
      } else if (std::filesystem::is_directory(entry)) {
        try {
          uintmax_t size = calculate_directory_size(entry.path());
          size_str = format_size(size, human_readable);
        } catch (...) {
          size_str = "???";
        }
      }

      struct stat file_stat;
      memset(&file_stat, 0, sizeof(file_stat));
      stat(entry.path().c_str(), &file_stat);

      if (long_format) {
        std::string perms;
        perms += (S_ISDIR(file_stat.st_mode)) ? 'd' : '-';
        perms += (file_stat.st_mode & S_IRUSR) ? 'r' : '-';
        perms += (file_stat.st_mode & S_IWUSR) ? 'w' : '-';
        perms += (file_stat.st_mode & S_IXUSR) ? 'x' : '-';
        perms += (file_stat.st_mode & S_IRGRP) ? 'r' : '-';
        perms += (file_stat.st_mode & S_IWGRP) ? 'w' : '-';
        perms += (file_stat.st_mode & S_IXGRP) ? 'x' : '-';
        perms += (file_stat.st_mode & S_IROTH) ? 'r' : '-';
        perms += (file_stat.st_mode & S_IWOTH) ? 'w' : '-';
        perms += (file_stat.st_mode & S_IXOTH) ? 'x' : '-';

        struct passwd* pw = getpwuid(file_stat.st_uid);
        struct group* gr = getgrgid(file_stat.st_gid);
        std::string owner = pw ? pw->pw_name : std::to_string(file_stat.st_uid);
        std::string group = gr ? gr->gr_name : std::to_string(file_stat.st_gid);

        char mod_time[20];
        strftime(mod_time, sizeof(mod_time), "%Y-%m-%d %H:%M",
                 localtime(&file_stat.st_mtime));

        if (show_inode) {
          std::cout << std::setw(10) << std::left << file_stat.st_ino;
        }
        std::cout << std::setw(12) << std::left << perms << std::setw(3)
                  << std::right << file_stat.st_nlink << std::setw(10)
                  << std::left << owner.substr(0, 9) << std::setw(10)
                  << group.substr(0, 9) << std::setw(12) << std::right
                  << size_str << std::setw(20) << mod_time << "  " << color
                  << name << COLOR_RESET;

        if (std::filesystem::is_symlink(entry)) {
          try {
            std::cout << " -> "
                      << std::filesystem::read_symlink(entry).string();
          } catch (...) {
            std::cout << " -> [broken link]";
          }
        }

        std::cout << std::endl;
      } else if (one_per_line) {
        if (show_inode) {
          std::cout << std::setw(10) << std::left << file_stat.st_ino << " ";
        }
        std::cout << color << name << COLOR_RESET << std::endl;
      } else {
        if (show_inode) {
          std::cout << std::setw(10) << std::left << file_stat.st_ino;
        }
        std::cout << color << std::setw(40) << std::left << name.substr(0, 39)
                  << COLOR_RESET << std::setw(15) << size_str << type
                  << std::endl;
      }
    }

    if (recursive) {
      for (const auto& entry : entries) {
        if (std::filesystem::is_directory(entry) &&
            !std::filesystem::is_symlink(entry)) {
          list_directory(entry.path().string(), show_hidden, long_format,
                         sort_by_size, reverse_order, sort_by_time,
                         human_readable, recursive, one_per_line, show_inode,
                         level + 1);
        }
      }
    }

    return 0;
  } catch (const std::filesystem::filesystem_error& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }
}
