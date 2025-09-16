#include "builtin.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iomanip>

#include "ai_command.h"
#include "aihelp_command.h"
#include "alias_command.h"
#include "approot_command.h"
#include "cd_command.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "echo_command.h"
#include "eval_command.h"
#include "exit_command.h"
#include "export_command.h"
#include "getopts_command.h"
#include "hash_command.h"
#include "help_command.h"
#include "history_command.h"
#include "job_control.h"
#include "loop_control_commands.h"
#include "ls_command.h"
#include "plugin_command.h"
#include "printf_command.h"
#include "prompt_test_command.h"
#include "pwd_command.h"
#include "read_command.h"
#include "readonly_command.h"
#include "set_command.h"
#include "source_command.h"
#include "startup_flag_command.h"
#include "syntax_command.h"
#include "test_command.h"
#include "theme_command.h"
#include "times_command.h"
#include "trap_command.h"
#include "type_command.h"
#include "umask_command.h"
#include "version_command.h"

#define PRINT_ERROR(MSG)                             \
  do {                                               \
    last_terminal_output_error = (MSG);              \
    std::cerr << last_terminal_output_error << '\n'; \
  } while (0)

Built_ins::Built_ins() : shell(nullptr) {
  // Reserve space for builtins to avoid rehashing
  builtins.reserve(64);

  builtins = {
      {"echo",
       [](const std::vector<std::string>& args) {
         return ::echo_command(args);
       }},
      {"printf",
       [](const std::vector<std::string>& args) {
         return ::printf_command(args);
       }},
      {"pwd",
       [](const std::vector<std::string>& args) {
         return ::pwd_command(args);
       }},
      {"cd",
       [this](const std::vector<std::string>& args) {
         return ::change_directory_with_bookmarks(
             args.size() > 1 ? args[1] : "", current_directory,
             previous_directory, last_terminal_output_error,
             directory_bookmarks);
       }},
      {"..",
       [this](const std::vector<std::string>& args) {
         (void)args;
         return ::change_directory("..", current_directory, previous_directory,
                                   last_terminal_output_error);
       }},
      {"ls",
       [this](const std::vector<std::string>& args) {
         return ::ls_command(args, shell);
       }},
      {"alias",
       [this](const std::vector<std::string>& args) {
         return ::alias_command(args, shell);
       }},
      {"export",
       [this](const std::vector<std::string>& args) {
         return ::export_command(args, shell);
       }},
      {"unalias",
       [this](const std::vector<std::string>& args) {
         return ::unalias_command(args, shell);
       }},
      {"unset",
       [this](const std::vector<std::string>& args) {
         return ::unset_command(args, shell);
       }},
      {"set",
       [this](const std::vector<std::string>& args) {
         return ::set_command(args, shell);
       }},
      {"shift",
       [this](const std::vector<std::string>& args) {
         return ::shift_command(args, shell);
       }},
      {"break",
       [](const std::vector<std::string>& args) {
         return ::break_command(args);
       }},
      {"continue",
       [](const std::vector<std::string>& args) {
         return ::continue_command(args);
       }},
      {"return",
       [](const std::vector<std::string>& args) {
         return ::return_command(args);
       }},
      {"ai",
       [this](const std::vector<std::string>& args) {
         return ::ai_command(args, this);
       }},
      {"source",
       [](const std::vector<std::string>& args) {
         return ::source_command(args);
       }},
      {"login-startup-arg",
       [](const std::vector<std::string>& args) {
         // while this is a builtin, it is only intended for internal use during
         // startup for the .cjprofile and not for interactive use
         return ::startup_flag_command(args);
       }},
      {".",
       [](const std::vector<std::string>& args) {
         return ::source_command(args);
       }},
      {"theme",
       [](const std::vector<std::string>& args) {
         return ::theme_command(args);
       }},
      {"plugin",
       [](const std::vector<std::string>& args) {
         return ::plugin_command(args);
       }},
      {"help",
       [](const std::vector<std::string>&) { return ::help_command(); }},
      {"approot",
       [this](const std::vector<std::string>&) {
         return ::change_to_approot(current_directory, previous_directory,
                                    last_terminal_output_error);
       }},
      {"aihelp",
       [](const std::vector<std::string>& args) {
         return ::aihelp_command(args);
       }},
      {"version",
       [](const std::vector<std::string>& args) {
         return ::version_command(args);
       }},
      {"eval",
       [this](const std::vector<std::string>& args) {
         return ::eval_command(args, shell);
       }},
      {"syntax",
       [this](const std::vector<std::string>& args) {
         return ::syntax_command(args, shell);
       }},
      {"history",
       [](const std::vector<std::string>& args) {
         return ::history_command(args);
       }},
      {"exit",
       [](const std::vector<std::string>& args) {
         return ::exit_command(args);
       }},
      {"quit",
       [](const std::vector<std::string>& args) {
         return ::exit_command(args);
       }},
      {"terminal",
       [this](const std::vector<std::string>& args) {
         (void)args;
         shell->set_menu_active(true);
         return 0;
       }},
      {"prompt_test",
       [](const std::vector<std::string>& args) {
         extern int prompt_test_command(const std::vector<std::string>&);
         return prompt_test_command(args);
       }},
      {"test",
       [](const std::vector<std::string>& args) {
         return ::test_command(args);
       }},
      {"[",
       [](const std::vector<std::string>& args) {
         return ::test_command(args);
       }},
      {"exec",
       [this](const std::vector<std::string>& args) {
         if (args.size() <= 1) {
           // exec with no arguments should succeed
           return 0;
         }

         // Check if this is a file descriptor operation (e.g., exec 3< file)
         std::vector<std::string> exec_args;
         bool has_fd_operations = false;

         for (size_t i = 1; i < args.size(); ++i) {
           const std::string& arg = args[i];

           // Check for file descriptor redirection patterns: N< file, N> file,
           // N<&M, N>&M
           if (arg.size() > 1 && std::isdigit(arg[0])) {
             size_t fd_end = 0;
             while (fd_end < arg.size() && std::isdigit(arg[fd_end])) {
               fd_end++;
             }

             if (fd_end > 0 && fd_end < arg.size()) {
               int fd_num = std::stoi(arg.substr(0, fd_end));
               std::string op = arg.substr(fd_end);

               if (op == "<" && i + 1 < args.size()) {
                 // exec N< file - open file for reading on fd N
                 std::string filename = args[i + 1];
                 int file_fd = open(filename.c_str(), O_RDONLY);
                 if (file_fd == -1) {
                   PRINT_ERROR("exec: " + filename + ": " + strerror(errno));
                   return 1;
                 }
                 if (fd_num != file_fd) {
                   if (dup2(file_fd, fd_num) == -1) {
                     PRINT_ERROR("exec: dup2 failed: " +
                                 std::string(strerror(errno)));
                     close(file_fd);
                     return 1;
                   }
                   close(file_fd);
                 }
                 has_fd_operations = true;
                 i++;  // Skip the filename argument
                 continue;
               } else if (op == ">" && i + 1 < args.size()) {
                 // exec N> file - open file for writing on fd N
                 std::string filename = args[i + 1];
                 int file_fd =
                     open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                 if (file_fd == -1) {
                   PRINT_ERROR("exec: " + filename + ": " + strerror(errno));
                   return 1;
                 }
                 if (fd_num != file_fd) {
                   if (dup2(file_fd, fd_num) == -1) {
                     PRINT_ERROR("exec: dup2 failed: " +
                                 std::string(strerror(errno)));
                     close(file_fd);
                     return 1;
                   }
                   close(file_fd);
                 }
                 has_fd_operations = true;
                 i++;  // Skip the filename argument
                 continue;
               } else if (op.find("<&") == 0 && op.size() > 2) {
                 // exec N<&M - duplicate input fd M to fd N
                 try {
                   int src_fd = std::stoi(op.substr(2));
                   if (dup2(src_fd, fd_num) == -1) {
                     PRINT_ERROR("exec: dup2 failed: " +
                                 std::string(strerror(errno)));
                     return 1;
                   }
                   has_fd_operations = true;
                   continue;
                 } catch (const std::exception&) {
                   // Fall through to add as regular argument
                 }
               } else if (op.find(">&") == 0 && op.size() > 2) {
                 // exec N>&M - duplicate output fd M to fd N
                 try {
                   int src_fd = std::stoi(op.substr(2));
                   if (dup2(src_fd, fd_num) == -1) {
                     PRINT_ERROR("exec: dup2 failed: " +
                                 std::string(strerror(errno)));
                     return 1;
                   }
                   has_fd_operations = true;
                   continue;
                 } catch (const std::exception&) {
                   // Fall through to add as regular argument
                 }
               }
             }
           }

           // If not a file descriptor operation, add to exec args
           exec_args.push_back(arg);
         }

         // If we only had file descriptor operations and no command, return
         // success
         if (has_fd_operations && exec_args.empty()) {
           return 0;
         }

         // If we have a command to execute, run it
         if (!exec_args.empty()) {
           // For now, we'll simulate exec by executing the command
           // In a real implementation, this would replace the shell process
           return shell->execute_command(exec_args, false);
         }

         return 0;
       }},
      {":", [](const std::vector<std::string>&) { return 0; }},
      {"if",
       [this](const std::vector<std::string>& args) {
         if (args.size() < 2) {
           PRINT_ERROR("if: syntax error");
           return 2;
         }

         std::string full_cmd;
         for (size_t i = 1; i < args.size(); ++i) {
           if (i > 1)
             full_cmd += " ";
           full_cmd += args[i];
         }

         size_t then_pos = full_cmd.find("; then ");
         size_t fi_pos = full_cmd.rfind("; fi");

         if (then_pos == std::string::npos || fi_pos == std::string::npos) {
           PRINT_ERROR("if: syntax error - expected '; then' and '; fi'");
           return 2;
         }

         std::string condition = full_cmd.substr(0, then_pos);
         std::string then_cmd =
             full_cmd.substr(then_pos + 7, fi_pos - (then_pos + 7));

         int cond_result = shell->execute(condition);

         if (cond_result == 0) {
           return shell->execute(then_cmd);
         }

         return 0;  // if condition failed, return success
       }},
      {"__INTERNAL_SUBSHELL__",
       [this](const std::vector<std::string>& args) {
         if (args.size() < 2)
           return 1;

         // Execute the subshell content in a subprocess
         // This is necessary for proper redirection handling
         std::string subshell_content = args[1];

         pid_t pid = fork();
         if (pid == -1) {
           perror("fork failed in subshell");
           return 1;
         }

         if (pid == 0) {
           // Child process: execute the subshell content
           int exit_code = shell->execute(subshell_content);
           _exit(exit_code);
         } else {
           // Parent process: wait for child
           int status;
           if (waitpid(pid, &status, 0) == -1) {
             perror("waitpid failed in subshell");
             return 1;
           }

           if (WIFEXITED(status)) {
             return WEXITSTATUS(status);
           } else if (WIFSIGNALED(status)) {
             return 128 + WTERMSIG(status);
           } else {
             return 1;
           }
         }
       }},
      {"trap",
       [](const std::vector<std::string>& args) {
         return ::trap_command(args);
       }},
      {"jobs",
       [](const std::vector<std::string>& args) {
         return ::jobs_command(args);
       }},
      {"fg",
       [](const std::vector<std::string>& args) { return ::fg_command(args); }},
      {"bg",
       [](const std::vector<std::string>& args) { return ::bg_command(args); }},
      {"wait",
       [](const std::vector<std::string>& args) {
         return ::wait_command(args);
       }},
      {"kill",
       [](const std::vector<std::string>& args) {
         return ::kill_command(args);
       }},
      {"readonly",
       [this](const std::vector<std::string>& args) {
         return ::readonly_command(args, shell);
       }},
      {"read",
       [this](const std::vector<std::string>& args) {
         return ::read_command(args, shell);
       }},
      {"umask",
       [](const std::vector<std::string>& args) {
         return ::umask_command(args);
       }},
      {"getopts",
       [this](const std::vector<std::string>& args) {
         return ::getopts_command(args, shell);
       }},
      {"times",
       [](const std::vector<std::string>& args) {
         return ::times_command(args, nullptr);
       }},
      {"type",
       [this](const std::vector<std::string>& args) {
         return ::type_command(args, shell);
       }},
      {"hash",
       [](const std::vector<std::string>& args) {
         return ::hash_command(args, nullptr);
       }},
  };
}

int Built_ins::builtin_command(const std::vector<std::string>& args) {
  if (args.empty())
    return 1;

  auto it = builtins.find(args[0]);
  if (it != builtins.end()) {
    if (args[0] == "cd" && args.size() == 1) {
      return ::change_directory_with_bookmarks(
          "", current_directory, previous_directory, last_terminal_output_error,
          directory_bookmarks);
    }
    int status = it->second(args);
    return status;
  }
  PRINT_ERROR("cjsh: '" + args[0] + "': command not found");
  return 127;
}

int Built_ins::is_builtin_command(const std::string& cmd) const {
  return !cmd.empty() && builtins.find(cmd) != builtins.end();
}

int Built_ins::do_ai_request(const std::string& prompt) {
  return ::ai_command({"ai", prompt}, this);
}

void Built_ins::add_directory_bookmark(const std::string& dir_path) {
  std::filesystem::path path(dir_path);
  std::string basename = path.filename().string();
  if (!basename.empty() && basename != "." && basename != "..") {
    directory_bookmarks[basename] = dir_path;
  }
}

std::string Built_ins::find_bookmark_path(
    const std::string& bookmark_name) const {
  auto it = directory_bookmarks.find(bookmark_name);
  if (it != directory_bookmarks.end()) {
    return it->second;
  }
  return "";
}

const std::unordered_map<std::string, std::string>&
Built_ins::get_directory_bookmarks() const {
  return directory_bookmarks;
}