#include "history_command.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "cjsh_filesystem.h"
#include "error_out.h"

int history_command(const std::vector<std::string>& args) {
    cjsh_filesystem::initialize_cjsh_directories();

    auto read_result = cjsh_filesystem::FileOperations::read_file_content(
        cjsh_filesystem::g_cjsh_history_path.string());

    std::string content;
    if (read_result.is_error()) {
        auto write_result = cjsh_filesystem::FileOperations::write_file_content(
            cjsh_filesystem::g_cjsh_history_path.string(), "");
        if (write_result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "history",
                         "could not create history file at " +
                             cjsh_filesystem::g_cjsh_history_path.string() +
                             ": " + write_result.error(),
                         {}});
            return 1;
        }
        content = "";
    } else {
        content = read_result.value();
    }

    std::stringstream content_stream(content);
    std::string line;
    int index = 0;

    if (args.size() > 1) {
        try {
            index = std::stoi(args[1]);
        } catch (const std::invalid_argument&) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "history",
                         "Invalid index: " + args[1],
                         {}});
            return 1;
        }
        for (int i = 0; i < index && std::getline(content_stream, line); ++i) {
            std::cout << std::setw(5) << i << "  " << line << std::endl;
        }
    } else {
        int i = 0;
        while (std::getline(content_stream, line)) {
            std::cout << std::setw(5) << i++ << "  " << line << std::endl;
        }
    }

    return 0;
}
