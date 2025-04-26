#include "exec.h"
#include <vector>

Exec::Exec(){
  last_terminal_output_error = "";
}

Exec::~Exec() {

}

void Exec::execute_command_sync(const std::vector<std::string>& args) {

  if (args.empty()) {
    last_terminal_output_error = "cjsh: Failed to parse command";
    std::cerr << last_terminal_output_error << std::endl;
    return;
  }
  
  pid_t pid = fork();
  
  if (pid == -1) {
    last_terminal_output_error = "cjsh: Failed to fork process: " + std::string(strerror(errno));
    std::cerr << last_terminal_output_error << std::endl;
    return;
  }
  
  if (pid == 0) {
    // Child process
    
    // Prepare arguments for execvp
    std::vector<char*> c_args;
    for (auto& arg : args) {
      c_args.push_back(const_cast<char*>(arg.data()));
    }
    c_args.push_back(nullptr);
    
    // Execute the command
    execvp(args[0].c_str(), c_args.data());
    
    // If we get here, execvp failed
    std::string error_msg = "cjsh: Failed to execute command: " + std::string(strerror(errno));
    std::cerr << error_msg << std::endl;
    exit(EXIT_FAILURE);
  }
  
  // Wait for the child process to complete with improved error handling
  int status;
  pid_t wait_result;
  
  do {
    wait_result = waitpid(pid, &status, 0);
  } while (wait_result == -1 && errno == EINTR);
}

void Exec::execute_command_async(const std::vector<std::string>& args) {

  if (args.empty()) {
    last_terminal_output_error = "cjsh: Failed to parse command";
    std::cerr << last_terminal_output_error << std::endl;
    return;
  }
  
  pid_t pid = fork();
  
  if (pid == -1) {
    last_terminal_output_error = "cjsh: Failed to fork process: " + std::string(strerror(errno));
    std::cerr << last_terminal_output_error << std::endl;
    return;
  }
  
  if (pid == 0) {
    // Child process
    
    // Create a new session to detach from terminal
    if (setsid() == -1) {
      std::perror("cjsh (async): setsid");
      _exit(EXIT_FAILURE);
    }
    
    // Redirect standard file descriptors to /dev/null
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null != -1) {
      if (dup2(dev_null, STDIN_FILENO)  == -1 ||
          dup2(dev_null, STDOUT_FILENO) == -1 ||
          dup2(dev_null, STDERR_FILENO) == -1) {
          std::perror("cjsh (async): dup2");
          _exit(EXIT_FAILURE);
      }
      if (dev_null > 2) {
        close(dev_null);
      }
    }
    
    // Prepare arguments for execvp
    std::vector<char*> c_args;
    for (auto& arg : args) {
      c_args.push_back(const_cast<char*>(arg.data()));
    }
    c_args.push_back(nullptr);
    
    // Execute the command
    execvp(args[0].c_str(), c_args.data());
    
    // If we get here, execvp failed
    exit(EXIT_FAILURE);
  }
  else {
    // Parent process
    last_terminal_output_error = "no error encountered";
  }
}