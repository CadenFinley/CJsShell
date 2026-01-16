#pragma once

#include <string>
#include <string_view>

namespace typeahead {
std::string capture_available_input();
void initialize();
void cleanup();

void filter_escape_sequences_into(std::string_view input, std::string& output);

void normalize_line_edit_sequences_into(std::string_view input, std::string& output);
void ingest_typeahead_input(const std::string& raw_input);
void flush_pending_typeahead();

void clear_input_buffer();
const std::string& get_input_buffer();

}  // namespace typeahead
