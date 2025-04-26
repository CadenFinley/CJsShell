#include "exec.h"
#include "shell.h" // Include shell.h here to avoid circular dependency

Exec::Exec(Shell* shell_instance) : parser(), shell(shell_instance) {
  // Initialize current directory to the current working directory
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    current_directory = cwd;
  } else {
    current_directory = "/";
  }
}

Exec::~Exec() {
  // Destructor
}

void Exec::execute_command_sync(const std::string& command) {
  if (command.empty()) return;
  
  std::vector<std::string> args = parser.parse_command(command);
  if (args.empty()) {
    std::cerr << "cjsh: Failed to parse command" << std::endl;
    return;
  }

  // check if command is a built-in command
  if (builtin_command(args)) {
    return;
  }
  
  pid_t pid = fork();
  
  if (pid == -1) {
    std::cerr << "cjsh: Failed to fork process: " << strerror(errno) << std::endl;
    return;
  }
  
  if (pid == 0) {
    // Child process
    
    // Prepare arguments for execvp
    std::vector<char*> c_args;
    for (auto& arg : args) {
      c_args.push_back(arg.data());
    }
    c_args.push_back(nullptr);
    
    // Execute the command
    execvp(args[0].c_str(), c_args.data());
    
    // If we get here, execvp failed
    std::cerr << "cjsh: Failed to execute command: " << strerror(errno) << std::endl;
    exit(EXIT_FAILURE);
  }
  
  // Wait for the child process to complete with improved error handling
  int status;
  pid_t wait_result;
  
  do {
    wait_result = waitpid(pid, &status, 0);
  } while (wait_result == -1 && errno == EINTR);
}

void Exec::execute_command_async(const std::string& command) {
  if (command.empty()) return;
  
  std::vector<std::string> args = parser.parse_command(command);
  if (args.empty()) {
    std::cerr << "cjsh: Failed to parse command" << std::endl;
    return;
  }
  
  pid_t pid = fork();
  
  if (pid == -1) {
    std::cerr << "cjsh: Failed to fork process: " << strerror(errno) << std::endl;
    return;
  }
  
  if (pid == 0) {
    // Child process
    
    // Create a new session to detach from terminal
    setsid();
    
    // Redirect standard file descriptors to /dev/null
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null != -1) {
      dup2(dev_null, STDIN_FILENO);
      dup2(dev_null, STDOUT_FILENO);
      dup2(dev_null, STDERR_FILENO);
      if (dev_null > 2) {
        close(dev_null);
      }
    }
    
    // Prepare arguments for execvp
    std::vector<char*> c_args;
    for (auto& arg : args) {
      c_args.push_back(arg.data());
    }
    c_args.push_back(nullptr);
    
    // Execute the command
    execvp(args[0].c_str(), c_args.data());
    
    // If we get here, execvp failed
    exit(EXIT_FAILURE);
  }
}

bool Exec::builtin_command(const std::vector<std::string>& args) {
  switch (hash(args[0].c_str())) {
    case hash("exit"):
      shell->set_exit_flag(true);
      return true;
    case hash("cd"): {
      std::string result;
      std::string dir = "";
      
      // Extract directory argument if provided
      if (args.size() > 1) {
        dir = args[1];
      }
      
      if (!change_directory(dir, result) && !result.empty()) {
        std::cerr << result << std::endl;
      }
      return true;
    }
    case hash("alias"):
      // Handle alias command
      return true;
    case hash("export"):
      // Handle export command
      return true;
    case hash("unset"):
      // Handle unset command
      return true;
    case hash("source"):
      // Handle source command
      return true;
    case hash("unalias"):
      // Handle unalias command
      return true;
    default:
      return false;
  }
}

bool Exec::change_directory(const std::string& dir, std::string& result) {
  std::string target_dir = dir;
  
  // Handle empty input or just "~"
  if (target_dir.empty()) {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      result = "cjsh: HOME environment variable is not set";
      return false;
    }
    target_dir = home_dir;
  }
  
  // Expand tilde at the beginning of the path
  if (target_dir[0] == '~') {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      result = "cjsh: Cannot expand '~' - HOME environment variable is not set";
      return false;
    }
    target_dir.replace(0, 1, home_dir);
  }
  
  // Create a path object from the target directory
  std::filesystem::path dir_path;
  
  try {
    // If it's an absolute path, use it directly; otherwise, make it relative to current directory
    if (std::filesystem::path(target_dir).is_absolute()) {
      dir_path = target_dir;
    } else {
      dir_path = std::filesystem::path(current_directory) / target_dir;
    }

    // Check if the directory exists
    if (!std::filesystem::exists(dir_path)) {
      result = "cd: " + target_dir + ": No such file or directory";
      return false;
    }
    
    // Check if it's a directory
    if (!std::filesystem::is_directory(dir_path)) {
      result = "cd: " + target_dir + ": Not a directory";
      return false;
    }
    
    // Get the canonical (absolute, normalized) path
    std::filesystem::path canonical_path = std::filesystem::canonical(dir_path);
    current_directory = canonical_path.string();
    
    // Actually change the working directory
    if (chdir(current_directory.c_str()) != 0) {
      result = "cd: " + std::string(strerror(errno));
      return false;
    }
    
    // Update PWD environment variable
    setenv("PWD", current_directory.c_str(), 1);
    
    return true;
  }
  catch (const std::filesystem::filesystem_error& e) {
    result = "cd: " + std::string(e.what());
    return false;
  }
  catch (const std::exception& e) {
    result = "cd: Unexpected error: " + std::string(e.what());
    return false;
  }
}