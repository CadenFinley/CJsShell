#include "widget_command.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "error_out.h"
#include "isocline.h"

int widget_builtin(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "cjsh-widget",
                     "Missing subcommand",
                     {"Usage: cjsh-widget <subcommand> [args...]", "",
                      "Subcommands:", "  get-buffer        Get current input buffer content",
                      "  set-buffer <text> Set input buffer to specified text",
                      "  get-cursor        Get current cursor position",
                      "  set-cursor <pos>  Set cursor position (byte offset)",
                      "  insert <text>     Insert text at cursor position",
                      "  append <text>     Append text to end of buffer",
                      "  clear             Clear the input buffer",
                      "  accept            Accept/execute the current line (like pressing Enter)",
                      "", "Environment variables (available in keybindings):",
                      "  CJSH_LINE         Current buffer content (like READLINE_LINE/BUFFER)",
                      "  CJSH_POINT        Current cursor position (like READLINE_POINT/CURSOR)"}});
        return 1;
    }

    const std::string& subcommand = args[1];

    if (subcommand == "get-buffer") {
        const char* buffer = ic_get_buffer();
        if (buffer == nullptr) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "no active readline session",
                         {"This widget requires an active interactive cjsh session."}});
            return 1;
        }
        std::cout << buffer;
        return 0;
    }

    if (subcommand == "set-buffer") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "cjsh-widget",
                         "set-buffer requires text argument",
                         {"Usage: cjsh-widget set-buffer <text>"}});
            return 1;
        }

        std::string text;
        for (size_t i = 2; i < args.size(); ++i) {
            if (i > 2)
                text += " ";
            text += args[i];
        }

        if (!ic_set_buffer(text.c_str())) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "no active readline session",
                         {"This widget requires an active interactive cjsh session."}});
            return 1;
        }
        return 0;
    }

    if (subcommand == "get-cursor") {
        size_t pos = 0;
        if (!ic_get_cursor_pos(&pos)) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "no active readline session",
                         {"This widget requires an active interactive cjsh session."}});
            return 1;
        }
        std::cout << pos;
        return 0;
    }

    if (subcommand == "set-cursor") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "cjsh-widget",
                         "set-cursor requires position argument",
                         {"Usage: cjsh-widget set-cursor <position>"}});
            return 1;
        }

        char* endptr;
        long pos = strtol(args[2].c_str(), &endptr, 10);
        if (*endptr != '\0' || pos < 0) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "cjsh-widget",
                         "Invalid position: " + args[2],
                         {"Position must be a non-negative integer"}});
            return 1;
        }

        if (!ic_set_cursor_pos((size_t)pos)) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "no active readline session",
                         {"This widget requires an active interactive cjsh session."}});
            return 1;
        }
        return 0;
    }

    if (subcommand == "insert") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "cjsh-widget",
                         "insert requires text argument",
                         {"Usage: cjsh-widget insert <text>"}});
            return 1;
        }

        const char* buffer = ic_get_buffer();
        size_t cursor_pos = 0;

        if (!buffer || !ic_get_cursor_pos(&cursor_pos)) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "no active readline session",
                         {"This widget requires an active interactive cjsh session."}});
            return 1;
        }

        std::string text;
        for (size_t i = 2; i < args.size(); ++i) {
            if (i > 2)
                text += " ";
            text += args[i];
        }

        std::string buf_str(buffer);
        std::string new_buffer = buf_str.substr(0, cursor_pos) + text + buf_str.substr(cursor_pos);

        if (!ic_set_buffer(new_buffer.c_str())) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "failed to update buffer",
                         {"Verify the session is interactive and try again."}});
            return 1;
        }

        ic_set_cursor_pos(cursor_pos + text.length());
        return 0;
    }

    if (subcommand == "append") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "cjsh-widget",
                         "append requires text argument",
                         {"Usage: cjsh-widget append <text>"}});
            return 1;
        }

        const char* buffer = ic_get_buffer();
        if (!buffer) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "no active readline session",
                         {"This widget requires an active interactive cjsh session."}});
            return 1;
        }

        std::string text;
        for (size_t i = 2; i < args.size(); ++i) {
            if (i > 2)
                text += " ";
            text += args[i];
        }

        std::string new_buffer = std::string(buffer) + text;

        if (!ic_set_buffer(new_buffer.c_str())) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "failed to update buffer",
                         {"Verify the session is interactive and try again."}});
            return 1;
        }
        return 0;
    }

    if (subcommand == "clear") {
        if (!ic_set_buffer("")) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "no active readline session",
                         {"This widget requires an active interactive cjsh session."}});
            return 1;
        }
        return 0;
    }

    if (subcommand == "accept") {
        if (!ic_request_submit()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::ERROR,
                         "cjsh-widget",
                         "no active readline session",
                         {"This widget requires an active interactive cjsh session."}});
            return 1;
        }
        ic_push_key_event(0);
        return 0;
    }

    print_error({ErrorType::INVALID_ARGUMENT,
                 "cjsh-widget",
                 "Unknown subcommand: " + subcommand,
                 {"Use 'cjsh-widget' without arguments to see available subcommands"}});
    return 1;
}
