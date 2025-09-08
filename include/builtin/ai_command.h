#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Built_ins;

int ai_command(const std::vector<std::string>& args, Built_ins* built_ins);
int ai_chat_commands(const std::vector<std::string>& args, int cmd_index);
int handle_ai_file_commands(const std::vector<std::string>& args, int cmd_index,
                            const std::string& current_directory);
int do_ai_request(const std::string& prompt);
std::string build_system_prompt();
