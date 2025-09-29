#pragma once

#include <string>

namespace typeahead {
std::string capture_available_input();
void initialize();
void cleanup();

// Debug utilities
std::string to_debug_visible(const std::string& data);

// Typeahead input processing functions
std::string filter_escape_sequences(const std::string& input);
std::string normalize_line_edit_sequences(const std::string& input);
void enqueue_queued_command(const std::string& command);
void ingest_typeahead_input(const std::string& raw_input);
void flush_pending_typeahead();

// Command queue management
bool has_queued_commands();
std::string dequeue_command();
void clear_input_buffer();
void clear_command_queue();
std::string get_input_buffer();

}  // namespace typeahead