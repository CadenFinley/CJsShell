#include "ls_command.h"

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/sysmacros.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "error_out.h"
#include "shell.h"
#include "suggestion_utils.h"

#if defined(__APPLE__) && !defined(major)
#define major(dev) ((int)(((dev) >> 24) & 0xff))
#endif
#if defined(__APPLE__) && !defined(minor)
#define minor(dev) ((int)((dev) & 0xffffff))
#endif

#define COLOR_RESET "\033[0m"
#define COLOR_BLUE "\033[34m"
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_CYAN "\033[36m"
#define COLOR_YELLOW "\033[33m"

enum FileType : uint8_t {
  TYPE_UNKNOWN = 0,
  TYPE_DIRECTORY = 1,
  TYPE_SYMLINK = 2,
  TYPE_EXECUTABLE = 3,
  TYPE_SOURCE = 4,
  TYPE_REGULAR = 5
};

static constexpr const char* file_type_colors[] = {
    COLOR_RESET, COLOR_BLUE, COLOR_CYAN, COLOR_RED, COLOR_GREEN, COLOR_RESET};

static const std::unordered_set<std::string_view> source_extensions = {
    ".cpp", ".h",     ".hpp", ".py", ".js",   ".java", ".cs", ".rb", ".php",
    ".go",  ".swift", ".ts",  ".rs", ".html", ".css",  ".c",  ".cc", ".cxx"};

static const std::unordered_set<std::string_view> executable_extensions = {
    ".so", ".dylib", ".exe"};

bool should_use_colors(Shell* shell) {
  if (!isatty(STDOUT_FILENO)) {
    return false;
  }

  if (shell && !shell->get_interactive_mode()) {
    return false;
  }

  return true;
}

struct FileInfo {
  std::filesystem::directory_entry entry;
  struct stat stat_info;
  bool stat_valid = false;
  std::string_view cached_name_view;
  std::string cached_name_storage;
  const char* cached_color = nullptr;
  uintmax_t cached_size = 0;
  bool size_calculated = false;
  uint8_t file_type = 0;

  FileInfo(const std::filesystem::directory_entry& e) : entry(e) {
    auto filename = entry.path().filename();
    if (filename.native().size() < 256) {
      cached_name_storage = filename.string();
      cached_name_view = cached_name_storage;
    } else {
      cached_name_storage = filename.string();
      cached_name_view = cached_name_storage;
    }
  }

  const std::string& get_name() const {
    if (cached_name_storage.empty()) {
      const_cast<FileInfo*>(this)->cached_name_storage =
          entry.path().filename().string();
    }
    return cached_name_storage;
  }
};

int ls_command(const std::vector<std::string>& args, Shell* shell) {
  std::string path = ".";
  bool show_hidden = false;
  bool show_almost_all = false;
  bool long_format = false;
  bool sort_by_size = false;
  bool reverse_order = false;
  bool sort_by_time = false;
  bool sort_by_access_time = false;
  bool sort_by_status_time = false;
  bool human_readable = false;
  bool recursive = false;
  bool one_per_line = false;
  bool show_inode = false;
  bool multi_column = false;
  bool indicator_style = false;
  bool follow_symlinks_cmdline = false;
  bool follow_all_symlinks = false;
  bool directory_only = false;
  bool unsorted = false;
  bool long_format_no_owner = false;
  bool kilobyte_blocks = false;
  bool stream_format = false;
  bool numeric_ids = false;
  bool long_format_no_group = false;
  bool append_slash = false;
  bool quote_non_printable = false;
  bool show_blocks = false;
  bool multi_column_across = false;

  bool use_colors = should_use_colors(shell);

  for (size_t i = 1; i < args.size(); i++) {
    if (args[i][0] == '-' && args[i].length() > 1 && args[i][1] != '-') {
      for (size_t j = 1; j < args[i].length(); j++) {
        switch (args[i][j]) {
          case 'a':
            show_hidden = true;
            break;
          case 'A':
            show_almost_all = true;
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
          case 'u':
            sort_by_access_time = true;
            break;
          case 'c':
            sort_by_status_time = true;
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
          case 'C':
            multi_column = true;
            break;
          case 'F':
            indicator_style = true;
            break;
          case 'H':
            follow_symlinks_cmdline = true;
            break;
          case 'L':
            follow_all_symlinks = true;
            break;
          case 'd':
            directory_only = true;
            break;
          case 'f':
            unsorted = true;
            show_hidden = true;
            break;
          case 'g':
            long_format = true;
            long_format_no_owner = true;
            break;
          case 'k':
            kilobyte_blocks = true;
            break;
          case 'm':
            stream_format = true;
            break;
          case 'n':
            long_format = true;
            numeric_ids = true;
            break;
          case 'o':
            long_format = true;
            long_format_no_group = true;
            break;
          case 'p':
            append_slash = true;
            break;
          case 'q':
            quote_non_printable = true;
            break;
          case 's':
            show_blocks = true;
            break;
          case 'x':
            multi_column_across = true;
            break;
          default:
            std::cerr << "ls: unknown option: -" << args[i][j] << std::endl;
            return 1;
        }
      }
    } else if (args[i] == "--help") {
      std::cout << "Usage: ls [OPTION]... [FILE]..." << std::endl;
      std::cout << "List information about files." << std::endl << std::endl;
      std::cout << "  -a             show all files, including hidden files"
                << std::endl;
      std::cout << "  -A             show all files except . and .."
                << std::endl;
      std::cout << "  -l             use long listing format" << std::endl;
      std::cout << "  -S             sort by file size, largest first"
                << std::endl;
      std::cout << "  -r             reverse order while sorting" << std::endl;
      std::cout << "  -t             sort by modification time, newest first"
                << std::endl;
      std::cout << "  -u             sort by access time" << std::endl;
      std::cout << "  -c             sort by status change time" << std::endl;
      std::cout << "  -h             print sizes in human readable format"
                << std::endl;
      std::cout << "  -R             list subdirectories recursively"
                << std::endl;
      std::cout << "  -1             list one file per line" << std::endl;
      std::cout << "  -i             print the inode number" << std::endl;
      std::cout << "  -C             list entries by columns" << std::endl;
      std::cout << "  -F             append indicator to entries" << std::endl;
      std::cout << "  -H             follow symlinks on command line"
                << std::endl;
      std::cout << "  -L             follow all symlinks" << std::endl;
      std::cout << "  -d             list directories themselves, not contents"
                << std::endl;
      std::cout << "  -f             do not sort, enable -a" << std::endl;
      std::cout << "  -g             long format without owner" << std::endl;
      std::cout << "  -k             use 1024-byte blocks" << std::endl;
      std::cout << "  -m             stream format with comma separators"
                << std::endl;
      std::cout << "  -n             long format with numeric IDs" << std::endl;
      std::cout << "  -o             long format without group" << std::endl;
      std::cout << "  -p             append / to directories" << std::endl;
      std::cout << "  -q             replace non-printable characters with ?"
                << std::endl;
      std::cout << "  -s             print file system block counts"
                << std::endl;
      std::cout << "  -x             list entries by lines instead of columns"
                << std::endl;
      return 0;
    } else if (args[i][0] == '-') {
      std::cerr << "ls: unknown option: " << args[i] << std::endl;
      return 1;
    } else {
      path = args[i];
    }
  }

  // If we have multiple arguments (files/directories), we need to handle them
  // separately
  std::vector<std::string> paths;
  for (size_t i = 1; i < args.size(); i++) {
    if (args[i][0] != '-' && args[i] != "--help") {
      paths.push_back(args[i]);
    }
  }

  // If no paths specified, use current directory
  if (paths.empty()) {
    paths.push_back(".");
  }

  int exit_code = 0;
  for (size_t i = 0; i < paths.size(); i++) {
    // Only show path headers when listing multiple directories or when explicit
    // -d flag is used
    if (paths.size() > 1 && !directory_only) {
      std::error_code ec;
      std::filesystem::path fs_path(paths[i]);
      bool is_dir = std::filesystem::is_directory(fs_path, ec);

      if (is_dir) {
        if (i > 0)
          std::cout << std::endl;
        std::cout << paths[i] << ":" << std::endl;
      }
    }

    int result = list_directory(
        paths[i], show_hidden, show_almost_all, long_format, sort_by_size,
        reverse_order, sort_by_time, sort_by_access_time, sort_by_status_time,
        human_readable, recursive, one_per_line, show_inode, multi_column,
        indicator_style, follow_symlinks_cmdline, follow_all_symlinks,
        directory_only, unsorted, long_format_no_owner, kilobyte_blocks,
        stream_format, numeric_ids, long_format_no_group, append_slash,
        quote_non_printable, show_blocks, multi_column_across, 0, use_colors);

    if (result != 0) {
      exit_code = result;
    }
  }

  return exit_code;
}

std::string format_size_human_readable(uintmax_t size) {
  static const std::array<const char*, 7> units = {"B", "K", "M", "G",
                                                   "T", "P", "E"};
  int unit_index = 0;
  double size_d = static_cast<double>(size);

  while (size_d >= 1024.0 && unit_index < 6) {
    size_d /= 1024.0;
    unit_index++;
  }

  char buffer[16];
  if (unit_index == 0) {
    snprintf(buffer, sizeof(buffer), "%lu%s", (unsigned long)size,
             units[unit_index]);
  } else if (size_d < 10) {
    snprintf(buffer, sizeof(buffer), "%.1f%s", size_d, units[unit_index]);
  } else {
    snprintf(buffer, sizeof(buffer), "%.0f%s", size_d, units[unit_index]);
  }

  return std::string(buffer);
}

uintmax_t calculate_directory_size_for_sorting(
    const std::filesystem::path& dir_path) {
  uintmax_t size = 0;
  std::error_code ec;

  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(dir_path, ec)) {
    if (ec)
      break;

    if (entry.is_regular_file(ec) && !ec) {
      auto file_size = entry.file_size(ec);
      if (!ec) {
        size += file_size;
      }
    }
  }
  return size;
}

std::string format_blocks(uintmax_t blocks, bool human_readable) {
  if (human_readable) {
    return format_size_human_readable(blocks);
  } else {
    return std::to_string(blocks);
  }
}

std::string format_size(uintmax_t size, bool human_readable) {
  if (human_readable) {
    return format_size_human_readable(size);
  } else {
    if (size < 1024)
      return std::to_string(size) + " B";  // bytes
    else if (size < 1048576)
      return std::to_string(size >> 10) + " KB";  // kilobytes
    else if (size < 1073741824)
      return std::to_string(size >> 20) + " MB";  // megabytes
    else
      return std::to_string(size >> 30) + " GB";  // gigabytes
  }
}

bool get_file_stat(FileInfo& file_info) {
  if (!file_info.stat_valid) {
    if (stat(file_info.entry.path().c_str(), &file_info.stat_info) == 0) {
      file_info.stat_valid = true;
    }
  }
  return file_info.stat_valid;
}

void determine_file_type_and_color(FileInfo& file_info) {
  if (file_info.file_type != TYPE_UNKNOWN)
    return;

  std::error_code ec;

  auto file_status = file_info.entry.status(ec);
  if (ec) {
    file_info.file_type = TYPE_UNKNOWN;
    file_info.cached_color = COLOR_RESET;
    return;
  }

  auto file_type_val = file_status.type();

  if (file_type_val == std::filesystem::file_type::directory) {
    file_info.file_type = TYPE_DIRECTORY;
  } else if (file_type_val == std::filesystem::file_type::symlink) {
    file_info.file_type = TYPE_SYMLINK;
  } else if (file_type_val == std::filesystem::file_type::regular) {
    auto path_str = file_info.entry.path().native();
    auto last_dot = path_str.rfind('.');

    if (last_dot != std::string::npos) {
      std::string_view ext(path_str.data() + last_dot,
                           path_str.length() - last_dot);

      if (source_extensions.count(ext)) {
        file_info.file_type = TYPE_SOURCE;
      } else if (executable_extensions.count(ext)) {
        file_info.file_type = TYPE_EXECUTABLE;
      } else {
        if (get_file_stat(file_info) &&
            (file_info.stat_info.st_mode & S_IXUSR)) {
          file_info.file_type = TYPE_EXECUTABLE;
        } else {
          file_info.file_type = TYPE_REGULAR;
        }
      }
    } else {
      if (get_file_stat(file_info) && (file_info.stat_info.st_mode & S_IXUSR)) {
        file_info.file_type = TYPE_EXECUTABLE;
      } else {
        file_info.file_type = TYPE_REGULAR;
      }
    }
  } else {
    file_info.file_type = TYPE_REGULAR;
  }

  file_info.cached_color = file_type_colors[file_info.file_type];
}

std::string format_posix_time(time_t mtime) {
  time_t now = time(nullptr);
  struct tm* tm_info = localtime(&mtime);
  char buffer[32];

  if (now - mtime > 6 * 30 * 24 * 60 * 60 || mtime > now) {
    strftime(buffer, sizeof(buffer), "%b %e  %Y", tm_info);
  } else {
    strftime(buffer, sizeof(buffer), "%b %e %H:%M", tm_info);
  }

  return std::string(buffer);
}

std::string quote_filename(const std::string& filename,
                           bool quote_non_printable) {
  if (!quote_non_printable)
    return filename;

  std::string result;
  for (char c : filename) {
    if (isprint(c) && c != '\t') {
      result += c;
    } else {
      result += '?';
    }
  }
  return result;
}

static void build_permissions_fast(char* perms, mode_t mode) {
  if (S_ISDIR(mode))
    perms[0] = 'd';
  else if (S_ISLNK(mode))
    perms[0] = 'l';
  else if (S_ISBLK(mode))
    perms[0] = 'b';
  else if (S_ISCHR(mode))
    perms[0] = 'c';
  else if (S_ISFIFO(mode))
    perms[0] = 'p';
  else if (S_ISSOCK(mode))
    perms[0] = 's';
  else
    perms[0] = '-';

  perms[1] = (mode & S_IRUSR) ? 'r' : '-';
  perms[2] = (mode & S_IWUSR) ? 'w' : '-';
  if (mode & S_ISUID) {
    perms[3] = (mode & S_IXUSR) ? 's' : 'S';
  } else {
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
  }

  perms[4] = (mode & S_IRGRP) ? 'r' : '-';
  perms[5] = (mode & S_IWGRP) ? 'w' : '-';
  if (mode & S_ISGID) {
    perms[6] = (mode & S_IXGRP) ? 's' : 'S';
  } else {
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
  }

  perms[7] = (mode & S_IROTH) ? 'r' : '-';
  perms[8] = (mode & S_IWOTH) ? 'w' : '-';
  if (mode & S_ISVTX) {
    perms[9] = (mode & S_IXOTH) ? 't' : 'T';
  } else {
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
  }

  perms[10] = '\0';
}

std::string get_file_indicator(const std::filesystem::directory_entry& entry,
                               bool indicator_style, bool append_slash) {
  std::error_code ec;

  if (entry.is_directory(ec)) {
    return "/";
  }

  if (!indicator_style && !append_slash)
    return "";

  if (entry.is_symlink(ec))
    return "@";

  struct stat st;
  if (stat(entry.path().c_str(), &st) == 0) {
    if (S_ISFIFO(st.st_mode))
      return "|";
    if (st.st_mode & S_IXUSR)
      return "*";
  }

  return "";
}

uintmax_t get_block_count(const struct stat& st, bool kilobyte_blocks) {
  uintmax_t block_size = kilobyte_blocks ? 1024 : 512;
  return (st.st_size + block_size - 1) / block_size;
}

time_t get_sort_time(const struct stat& st, bool sort_by_access_time,
                     bool sort_by_status_time) {
  if (sort_by_access_time)
    return st.st_atime;
  if (sort_by_status_time)
    return st.st_ctime;
  return st.st_mtime;
}

int list_directory(const std::string& path, bool show_hidden,
                   bool show_almost_all, bool long_format, bool sort_by_size,
                   bool reverse_order, bool sort_by_time,
                   bool sort_by_access_time, bool sort_by_status_time,
                   bool human_readable, bool recursive, bool one_per_line,
                   bool show_inode, bool multi_column, bool indicator_style,
                   bool follow_symlinks_cmdline, bool follow_all_symlinks,
                   bool directory_only, bool unsorted,
                   bool long_format_no_owner, bool kilobyte_blocks,
                   bool stream_format, bool numeric_ids,
                   bool long_format_no_group, bool append_slash,
                   bool quote_non_printable, bool show_blocks,
                   bool multi_column_across, int level, bool use_colors) {
  try {
    std::vector<FileInfo> entries;

    std::error_code ec;
    std::filesystem::path fs_path(path);

    // Check if the path is a regular file or directory
    if (!std::filesystem::exists(fs_path, ec)) {
      auto suggestions = suggestion_utils::generate_ls_suggestions(
          path, std::filesystem::current_path().string());
      ErrorInfo error = {
          ErrorType::FILE_NOT_FOUND, "ls",
          "cannot access '" + path + "': No such file or directory",
          suggestions};
      print_error(error);
      return 1;
    }

    if (std::filesystem::is_regular_file(fs_path, ec) ||
        std::filesystem::is_symlink(fs_path, ec) ||
        (!std::filesystem::is_directory(fs_path, ec) && !ec)) {
      // Handle individual file
      entries.emplace_back(std::filesystem::directory_entry(fs_path));
    } else if (std::filesystem::is_directory(fs_path, ec)) {
      // Handle directory
      auto dir_iter = std::filesystem::directory_iterator(path, ec);
      if (ec) {
        std::cerr << "ls: cannot open directory '" << path
                  << "': " << ec.message() << std::endl;
        return 1;
      }

      entries.reserve(32);

      if (directory_only) {
        entries.emplace_back(std::filesystem::directory_entry(path));
      } else {
        if (show_hidden) {
          std::filesystem::path current_path = std::filesystem::absolute(path);
          entries.emplace_back(std::filesystem::directory_entry(current_path));
          entries.back().cached_name_storage = ".";

          std::filesystem::path parent_path = current_path.parent_path();
          entries.emplace_back(std::filesystem::directory_entry(parent_path));
          entries.back().cached_name_storage = "..";
        }

        for (const auto& entry : dir_iter) {
          const auto& path = entry.path();
          const std::string& filename = path.filename().string();

          if (!show_hidden && !show_almost_all && filename[0] == '.') {
            continue;
          }

          if (show_almost_all && (filename == "." || filename == "..")) {
            continue;
          }

          entries.emplace_back(entry);
        }
      }
    } else {
      auto suggestions = suggestion_utils::generate_ls_suggestions(
          path, std::filesystem::current_path().string());
      ErrorInfo error = {ErrorType::FILE_NOT_FOUND, "ls",
                         "cannot access '" + path + "': " + ec.message(),
                         suggestions};
      print_error(error);
      return 1;
    }

    if (!unsorted) {
      auto compare_entries = [&](const FileInfo& a, const FileInfo& b) {
        std::error_code ec_a, ec_b;
        bool a_is_dir = a.entry.is_directory(ec_a);
        bool b_is_dir = b.entry.is_directory(ec_b);

        if (!reverse_order) {
          if (a_is_dir && !b_is_dir)
            return true;
          if (!a_is_dir && b_is_dir)
            return false;
        }

        if (sort_by_time || sort_by_access_time || sort_by_status_time) {
          FileInfo& a_mut = const_cast<FileInfo&>(a);
          FileInfo& b_mut = const_cast<FileInfo&>(b);

          if (get_file_stat(a_mut) && get_file_stat(b_mut)) {
            time_t a_time = get_sort_time(a_mut.stat_info, sort_by_access_time,
                                          sort_by_status_time);
            time_t b_time = get_sort_time(b_mut.stat_info, sort_by_access_time,
                                          sort_by_status_time);

            if (a_time != b_time) {
              return reverse_order ? (a_time < b_time) : (a_time > b_time);
            }
          }
        } else if (sort_by_size) {
          FileInfo& a_mut = const_cast<FileInfo&>(a);
          FileInfo& b_mut = const_cast<FileInfo&>(b);

          if (!a_mut.size_calculated) {
            std::error_code ec;
            if (a_is_dir) {
              a_mut.cached_size =
                  calculate_directory_size_for_sorting(a.entry.path());
            } else {
              a_mut.cached_size = a.entry.file_size(ec);
              if (ec)
                a_mut.cached_size = 0;
            }
            a_mut.size_calculated = true;
          }
          if (!b_mut.size_calculated) {
            std::error_code ec;
            if (b_is_dir) {
              b_mut.cached_size =
                  calculate_directory_size_for_sorting(b.entry.path());
            } else {
              b_mut.cached_size = b.entry.file_size(ec);
              if (ec)
                b_mut.cached_size = 0;
            }
            b_mut.size_calculated = true;
          }

          if (a_mut.cached_size != b_mut.cached_size) {
            return reverse_order ? (a_mut.cached_size < b_mut.cached_size)
                                 : (a_mut.cached_size > b_mut.cached_size);
          }
        }

        return reverse_order ? (a.get_name() > b.get_name())
                             : (a.get_name() < b.get_name());
      };

      std::sort(entries.begin(), entries.end(), compare_entries);
    }

    std::ios::sync_with_stdio(false);

    bool use_long_format =
        long_format && !multi_column && !stream_format && !multi_column_across;
    bool use_stream_format = stream_format && !use_long_format;

    if (recursive && level > 0) {
      std::cout << "\n" << std::string(level, ' ') << path << ":" << std::endl;
    } else if (recursive) {
      std::cout << path << ":" << std::endl;
    }

    uintmax_t total_blocks = 0;
    if (use_long_format || show_blocks) {
      for (auto& file_info : entries) {
        if (get_file_stat(file_info)) {
          total_blocks += get_block_count(file_info.stat_info, kilobyte_blocks);
        }
      }

      if (use_long_format) {
        if (human_readable) {
          std::cout << "total " << format_size_human_readable(total_blocks)
                    << std::endl;
        } else {
          std::cout << "total " << total_blocks << std::endl;
        }
      }
    }

    if (use_stream_format) {
      bool first = true;
      for (auto& file_info : entries) {
        if (!first)
          std::cout << ", ";
        first = false;

        determine_file_type_and_color(file_info);
        std::string name =
            quote_filename(file_info.get_name(), quote_non_printable);

        if (use_colors) {
          std::cout << file_info.cached_color;
        }
        std::cout << name;
        if (use_colors) {
          std::cout << COLOR_RESET;
        }
        std::cout << get_file_indicator(file_info.entry, indicator_style,
                                        append_slash);
      }
      if (!entries.empty())
        std::cout << std::endl;
      return 0;
    }

    for (size_t i = 0; i < entries.size(); i++) {
      auto& file_info = entries[i];
      determine_file_type_and_color(file_info);

      std::string name =
          quote_filename(file_info.get_name(), quote_non_printable);
      std::string size_str = "-";
      std::error_code ec;

      if (file_info.entry.is_regular_file(ec) && !ec) {
        if (!file_info.size_calculated) {
          file_info.cached_size = file_info.entry.file_size(ec);
          file_info.size_calculated = true;
        }
        if (!ec) {
          size_str = format_size(file_info.cached_size, human_readable);
        } else {
          size_str = "???";
        }
      } else if (file_info.entry.is_directory(ec) && !ec) {
        if (sort_by_size && file_info.size_calculated) {
          size_str = format_size(file_info.cached_size, human_readable);
        } else {
          if (get_file_stat(file_info)) {
            size_str = format_size(file_info.stat_info.st_size, human_readable);
          } else {
            size_str = "???";
          }
        }
      }

      if (use_long_format) {
        if (!get_file_stat(file_info)) {
          continue;
        }

        char perms[11];
        build_permissions_fast(perms, file_info.stat_info.st_mode);

        std::string owner, group;
        if (numeric_ids) {
          owner = std::to_string(file_info.stat_info.st_uid);
          group = std::to_string(file_info.stat_info.st_gid);
        } else {
          struct passwd* pw = getpwuid(file_info.stat_info.st_uid);
          struct group* gr = getgrgid(file_info.stat_info.st_gid);
          owner = pw ? pw->pw_name : std::to_string(file_info.stat_info.st_uid);
          group = gr ? gr->gr_name : std::to_string(file_info.stat_info.st_gid);
        }

        time_t display_time = get_sort_time(
            file_info.stat_info, sort_by_access_time, sort_by_status_time);
        std::string time_str = format_posix_time(display_time);

        if (show_inode) {
          printf("%8lu ", (unsigned long)file_info.stat_info.st_ino);
        }

        if (show_blocks) {
          uintmax_t blocks =
              get_block_count(file_info.stat_info, kilobyte_blocks);
          if (human_readable) {
            std::string blocks_formatted =
                format_blocks(blocks, human_readable);
            printf("%8s ", blocks_formatted.c_str());
          } else {
            printf("%4lu ", (unsigned long)blocks);
          }
        }

        printf("%s %3u ", perms, (unsigned int)file_info.stat_info.st_nlink);

        if (!long_format_no_owner) {
          printf("%-8s ", owner.c_str());
        }

        if (!long_format_no_group) {
          printf("%-8s ", group.c_str());
        }

        if (S_ISCHR(file_info.stat_info.st_mode) ||
            S_ISBLK(file_info.stat_info.st_mode)) {
          printf("%3u, %3u ", major(file_info.stat_info.st_rdev),
                 minor(file_info.stat_info.st_rdev));
        } else {
          if (human_readable) {
            std::string size_formatted =
                format_size_human_readable(file_info.stat_info.st_size);
            printf("%8s ", size_formatted.c_str());
          } else {
            printf("%8lu ", (unsigned long)file_info.stat_info.st_size);
          }
        }

        printf("%s ", time_str.c_str());

        if (use_colors) {
          std::cout << file_info.cached_color;
        }
        std::cout << name;
        if (use_colors) {
          std::cout << COLOR_RESET;
        }

        if (file_info.entry.is_symlink(ec) && !ec) {
          std::error_code link_ec;
          auto target =
              std::filesystem::read_symlink(file_info.entry.path(), link_ec);
          if (!link_ec) {
            std::cout << " -> ";
            if (use_colors) {
              std::error_code target_ec;
              std::filesystem::path target_path =
                  file_info.entry.path().parent_path() / target;
              if (std::filesystem::is_directory(target_path, target_ec)) {
                std::cout << COLOR_BLUE;
              } else if (std::filesystem::is_regular_file(target_path,
                                                          target_ec)) {
                struct stat target_st;
                if (stat(target_path.c_str(), &target_st) == 0 &&
                    (target_st.st_mode & S_IXUSR)) {
                  std::cout << COLOR_RED;
                }
              }
            }
            std::cout << target.string();
            if (use_colors) {
              std::cout << COLOR_RESET;
            }
          }
        }

        std::cout << std::endl;
      } else {
        std::string output;

        if (show_inode && get_file_stat(file_info)) {
          output += std::to_string(file_info.stat_info.st_ino) + " ";
        }

        if (show_blocks && get_file_stat(file_info)) {
          uintmax_t blocks =
              get_block_count(file_info.stat_info, kilobyte_blocks);
          output += format_blocks(blocks, human_readable) + " ";
        }

        if (use_colors) {
          output += file_info.cached_color;
        }
        output += name;
        if (use_colors) {
          output += COLOR_RESET;
        }
        output +=
            get_file_indicator(file_info.entry, indicator_style, append_slash);

        std::cout << output << std::endl;
      }
    }

    if (recursive && !directory_only) {
      for (const auto& file_info : entries) {
        std::error_code ec;
        if (file_info.entry.is_directory(ec) && !ec &&
            !file_info.entry.is_symlink(ec)) {
          list_directory(
              file_info.entry.path().string(), show_hidden, show_almost_all,
              long_format, sort_by_size, reverse_order, sort_by_time,
              sort_by_access_time, sort_by_status_time, human_readable,
              recursive, one_per_line, show_inode, multi_column,
              indicator_style, follow_symlinks_cmdline, follow_all_symlinks,
              directory_only, unsorted, long_format_no_owner, kilobyte_blocks,
              stream_format, numeric_ids, long_format_no_group, append_slash,
              quote_non_printable, show_blocks, multi_column_across, level + 1,
              use_colors);
        }
      }
    }

    return 0;
  } catch (const std::filesystem::filesystem_error& ex) {
    std::cerr << "ls: " << ex.what() << std::endl;
    return 1;
  }
}
