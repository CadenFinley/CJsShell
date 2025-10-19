#pragma once

#include <string>
#include <vector>

namespace flags {

struct ParseResult {
    std::string script_file;
    std::vector<std::string> script_args;
    int exit_code = 0;
    bool should_exit = false;
};

ParseResult parse_arguments(int argc, char* argv[]);
void apply_profile_startup_flags();

}  