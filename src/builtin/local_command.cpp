#include "local_command.h"

#include <iostream>
#include "cjsh.h"
#include "error_out.h"

int local_command(const std::vector<std::string>& args, Shell* shell) {
  if (args.size() == 1) {
    // No arguments - just return success (POSIX behavior)
    return 0;
  }

  // Get the script interpreter to access variable scope functions
  auto script_interpreter = shell->get_shell_script_interpreter();
  if (!script_interpreter) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "local",
                 "not available outside of functions",
                 {}});
    return 1;
  }

  bool all_successful = true;

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    // Parse variable assignment (name=value)
    size_t eq_pos = arg.find('=');
    if (eq_pos != std::string::npos) {
      std::string name = arg.substr(0, eq_pos);
      std::string value = arg.substr(eq_pos + 1);

      if (name.empty()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "local",
                     "invalid variable name",
                     {}});
        all_successful = false;
        continue;
      }

      // Set as local variable
      script_interpreter->set_local_variable(name, value);

      if (g_debug_mode) {
        std::cerr << "DEBUG: Set local variable: " << name << "='" << value
                  << "'" << std::endl;
      }
    } else {
      // Just declare the variable as local without assigning a value
      std::string name = arg;

      if (name.empty()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "local",
                     "invalid variable name",
                     {}});
        all_successful = false;
        continue;
      }

      // Get current value if it exists, otherwise empty
      const char* current_value = getenv(name.c_str());
      std::string value = current_value ? current_value : "";

      script_interpreter->set_local_variable(name, value);

      if (g_debug_mode) {
        std::cerr << "DEBUG: Declared local variable: " << name << std::endl;
      }
    }
  }

  return all_successful ? 0 : 1;
}