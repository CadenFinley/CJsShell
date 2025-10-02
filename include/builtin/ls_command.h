#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Shell;

int ls_command(const std::vector<std::string>& args, Shell* shell = nullptr);
int list_directory(const std::string& path, bool show_hidden, bool show_almost_all,
                   bool long_format, bool sort_by_size, bool reverse_order, bool sort_by_time,
                   bool sort_by_access_time, bool sort_by_status_time, bool human_readable,
                   bool recursive, bool one_per_line, bool show_inode, bool multi_column,
                   bool indicator_style, bool follow_symlinks_cmdline, bool follow_all_symlinks,
                   bool directory_only, bool unsorted, bool long_format_no_owner,
                   bool kilobyte_blocks, bool stream_format, bool numeric_ids,
                   bool long_format_no_group, bool append_slash, bool quote_non_printable,
                   bool show_blocks, bool multi_column_across, int level);
uintmax_t calculate_directory_size(const std::filesystem::path& dir_path);
