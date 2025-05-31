#include "approot_command.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "cd_command.h"
#include "cjsh_filesystem.h"

int change_to_approot(std::string& current_directory,
                     std::string& previous_directory,
                     std::string& last_terminal_output_error) {
  std::string appRootPath = cjsh_filesystem::g_cjsh_data_path.string();
  return ::change_directory(appRootPath, current_directory, previous_directory, last_terminal_output_error);
}
