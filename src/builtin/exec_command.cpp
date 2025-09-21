#include "exec_command.h"

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>

#include <iostream>
#include "error_out.h"
#include "utils/cjsh_filesystem.h"

int exec_command(const std::vector<std::string>& args, Shell* shell,
                 std::string& last_terminal_output_error) {
  if (args.size() <= 1) {
    return 0;  // nothing to do
  }

  std::vector<std::string> exec_args;
  bool has_fd_operations = false;

  auto record_error = [&](const ErrorInfo& info) {
    last_terminal_output_error = info.message;
    print_error(info);
  };

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg.size() > 1 && std::isdigit(static_cast<unsigned char>(arg[0]))) {
      size_t fd_end = 0;
      while (fd_end < arg.size() &&
             std::isdigit(static_cast<unsigned char>(arg[fd_end]))) {
        fd_end++;
      }

      if (fd_end > 0 && fd_end < arg.size()) {
        int fd_num = std::stoi(arg.substr(0, fd_end));
        std::string op = arg.substr(fd_end);

        if (op == "<" && i + 1 < args.size()) {
          std::string filename = args[i + 1];
          auto redirect_result = cjsh_filesystem::FileOperations::redirect_fd(
              filename, fd_num, O_RDONLY);
          if (redirect_result.is_error()) {
            record_error({ErrorType::RUNTIME_ERROR,
                          "exec",
                          redirect_result.error(),
                          {}});
            return 1;
          }
          has_fd_operations = true;
          i++;
          continue;
        } else if (op == ">" && i + 1 < args.size()) {
          std::string filename = args[i + 1];
          auto redirect_result = cjsh_filesystem::FileOperations::redirect_fd(
              filename, fd_num, O_WRONLY | O_CREAT | O_TRUNC);
          if (redirect_result.is_error()) {
            record_error({ErrorType::RUNTIME_ERROR,
                          "exec",
                          redirect_result.error(),
                          {}});
            return 1;
          }
          has_fd_operations = true;
          i++;
          continue;
        } else if (op.find("<&") == 0 && op.size() > 2) {
          try {
            int src_fd = std::stoi(op.substr(2));
            auto dup_result =
                cjsh_filesystem::FileOperations::safe_dup2(src_fd, fd_num);
            if (dup_result.is_error()) {
              record_error(
                  {ErrorType::RUNTIME_ERROR, "exec", dup_result.error(), {}});
              return 1;
            }
            has_fd_operations = true;
            continue;
          } catch (const std::exception&) {
            // fallthrough to treat as normal arg
          }
        } else if (op.find(">&") == 0 && op.size() > 2) {
          try {
            int src_fd = std::stoi(op.substr(2));
            auto dup_result =
                cjsh_filesystem::FileOperations::safe_dup2(src_fd, fd_num);
            if (dup_result.is_error()) {
              record_error(
                  {ErrorType::RUNTIME_ERROR, "exec", dup_result.error(), {}});
              return 1;
            }
            has_fd_operations = true;
            continue;
          } catch (const std::exception&) {
            // fallthrough
          }
        }
      }
    }

    exec_args.push_back(arg);
  }

  if (has_fd_operations && exec_args.empty()) {
    return 0;  // only fd ops
  }

  if (!exec_args.empty() && shell) {
    return shell->execute_command(exec_args, false);
  }

  return 0;
}
