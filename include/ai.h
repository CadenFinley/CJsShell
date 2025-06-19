#pragma once

#include <curl/curl.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

class Ai {
 public:
  Ai(const std::string& api_key, const std::string& assistant_type,
     const std::string& initial_instruction);
  Ai(const std::string& api_key, const std::string& assistant_type,
     const std::string& initial_instruction,
     const std::vector<std::string>& user_files);
  Ai(const std::string& api_key, const std::string& assistant_type,
     const std::string& initial_instruction,
     const std::vector<std::string>& user_files,
     const std::string& save_directory, bool enabled);
  Ai();

  void set_api_key(const std::string& api_key);
  std::string get_api_key() const;
  void set_initial_instruction(const std::string& instruction);
  std::string get_initial_instruction() const;
  void set_assistant_type(const std::string& assistant_type);
  std::string get_assistant_type() const;
  void set_max_prompt_length(int max_prompt_length);
  int get_max_prompt_length() const;
  void set_cache_tokens(bool cache_tokens);
  bool get_cache_tokens() const;
  void toggle_cache_tokens();
  void clear_all_cached_tokens();
  std::vector<std::string> get_files() const;
  std::string get_file_contents() const;
  std::vector<std::string> get_chat_cache() const;
  void set_chat_cache(const std::vector<std::string>& chat_cache);
  void clear_chat_cache();
  void set_dynamic_prompt_length(bool dynamic_prompt_length);
  bool get_dynamic_prompt_length() const;
  void toggle_dynamic_prompt_length();
  void add_chat_to_cache(const std::string& chat);
  std::string get_response_data(const std::string& key) const;
  std::string get_last_prompt_used() const;
  std::string get_last_response_received() const;
  void set_max_prompt_precision(bool max_prompt_precision);
  bool get_max_prompt_precision() const;
  void toggle_max_prompt_precision();
  void set_timeout_flag_seconds(float timeout_flag_seconds);
  float get_timeout_flag_seconds() const;
  void set_model(const std::string& model);
  std::string get_model() const;
  void set_dynamic_prompt_length_scale(float dynamic_prompt_length_scale);
  float get_dynamic_prompt_length_scale() const;
  void remove_file(const std::string& user_file);
  void clear_files();
  void set_save_directory(const std::string& directory);
  std::string get_save_directory() const;
  void set_enabled(bool enabled);
  bool is_enabled() const;

  std::string chat_gpt(const std::string& message, bool format);
  std::string force_direct_chat_gpt(const std::string& message, bool format);
  int set_files(const std::vector<std::string>& user_files);
  int set_file(const std::string& user_file);
  int add_file(const std::string& user_file);
  int add_files(const std::vector<std::string>& user_files);
  static bool test_api_key(const std::string& api_key);
  void reject_changes();
  void refresh_files();

 private:
  void initialize(const std::string& api_key, const std::string& assistant_type,
                  const std::string& initial_instruction,
                  const std::vector<std::string>& user_files);
  bool is_valid_configuration() const;
  std::string get_invalid_configuration_message() const;
  std::string build_prompt(const std::string& message);
  std::string make_call_to_chat_gpt(const std::string& message);
  static std::string filter_message(const std::string& message);
  std::map<std::string, nlohmann::json> parse_json_response(
      const std::string& json_response) const;
  std::string extract_content_from_json(const std::string& json_response) const;
  int process_file_contents();
  void process_text_file(const std::string& file, std::string& out);
  void process_other_file(const std::string& file, std::string& out);
  static std::vector<std::string> extract_code_snippet(
      const std::string& content);
  std::string process_code_blocks_for_code_interpreter(
      const std::string& message);
  static std::string format_markdown(const std::string& text);
  static size_t write_callback(void* contents, size_t size, size_t nmemb,
                               std::string* userp);
  std::vector<std::string> split_string(const std::string& str, char delimiter);
  std::string sanitize_file_name(const std::string& file_name);
  static void monitor_cancellation(std::atomic<bool>& loading,
                                   std::atomic<bool>& request_cancelled);
  static void handle_error_response(CURL* curl, long response_code,
                                    const std::string& error_body);

  std::string user_api_key;
  std::string initial_instruction;
  std::string assistant_type;
  std::vector<std::string> files;
  int max_prompt_length = -1;
  bool cache_tokens = false;
  bool max_prompt_precision = false;
  bool dynamic_prompt_length = false;
  float dynamic_prompt_length_scale = 5;
  float timeout_flag_seconds = 300;
  std::string current_model = "gpt-3.5-turbo";
  std::vector<std::string> last_used_files;
  std::string file_contents;
  std::vector<std::string> chat_cache;
  std::string last_prompt_used;
  std::string last_response_received;
  std::map<std::string, nlohmann::json> response_data_map;
  std::string save_directory;
  bool enabled = true;
  std::atomic<bool> request_in_progress{false};
};