#pragma once

#include <string>
#include <vector>

int ls_command(const std::vector<std::string>& args);
int list_directory(const std::string& path, bool show_hidden, bool long_format,
                   bool sort_by_size, bool reverse_order, bool sort_by_time,
                   bool human_readable, bool recursive, bool one_per_line,
                   bool show_inode, int level);
