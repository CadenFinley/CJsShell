#pragma once

#include <string>
#include <vector>

namespace cjsh {

class CommandLineParser {
   public:
    struct ParseResult {
        std::string script_file;
        std::vector<std::string> script_args;
        int exit_code = 0;  // 0 = continue, non-zero = exit with this code
        bool should_exit = false;  // true if should exit immediately
    };

    static ParseResult parse_arguments(int argc, char* argv[]);
    static void apply_profile_startup_flags();

   private:
    static void detect_login_mode(char* argv[]);
    static void apply_minimal_mode();
    static void print_debug_info(const std::string& message);
};

}  // namespace cjsh