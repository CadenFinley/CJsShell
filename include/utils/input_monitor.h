#pragma once

#include <string>

namespace input_monitor {

// Initialize or reset the input monitor state.
void initialize();

// Clear all buffered state (queued commands and partial input).
void clear();

// Collect any pending type-ahead input from the terminal.
void collect_typeahead();

// Returns true if there is at least one completed command queued.
bool has_queued_command();

// Pop the next queued command (without the trailing newline). Returns an empty
// string if no commands are queued.
std::string pop_queued_command();

// Take the current partial line that has been typed but not completed by a
// newline. Returns an empty string if no partial input is available.
std::string take_partial_input();

}  // namespace input_monitor
