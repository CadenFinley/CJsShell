#include "ai.h"

#include "main.h"

Ai::Ai(const std::string& api_key, const std::string& assistant_type,
       const std::string& initial_instruction) {
  initialize(api_key, assistant_type, initial_instruction, {});
}

Ai::Ai(const std::string& api_key, const std::string& assistant_type,
       const std::string& initial_instruction,
       const std::vector<std::string>& user_files) {
  initialize(api_key, assistant_type, initial_instruction, user_files);
}

Ai::Ai(const std::string& api_key, const std::string& assistant_type,
       const std::string& initial_instruction,
       const std::vector<std::string>& user_files,
       const std::string& save_directory, bool enabled) {
  initialize(api_key, assistant_type, initial_instruction, user_files);
  set_save_directory(save_directory);
  set_enabled(enabled);
}

Ai::Ai() {}

void Ai::set_api_key(const std::string& api_key) {
  setenv("OPENAI_API_KEY", api_key.c_str(), 1);
  user_api_key = api_key;
}

std::string Ai::get_api_key() const {
  const char* env_key = getenv("OPENAI_API_KEY");
  if (env_key && env_key[0] != '\0') {
    return env_key;
  }
  return user_api_key;
}

void Ai::set_initial_instruction(const std::string& instruction) {
  initial_instruction = instruction;
}

std::string Ai::get_initial_instruction() const { return initial_instruction; }

void Ai::set_assistant_type(const std::string& assistant_type) {
  this->assistant_type = assistant_type;
}

std::string Ai::get_assistant_type() const { return assistant_type; }

void Ai::set_max_prompt_length(int max_prompt_length) {
  this->max_prompt_length = max_prompt_length;
}

int Ai::get_max_prompt_length() const { return max_prompt_length; }

void Ai::set_cache_tokens(bool cache_tokens) { this->cache_tokens = cache_tokens; }

bool Ai::get_cache_tokens() const { return cache_tokens; }

void Ai::toggle_cache_tokens() { cache_tokens = !cache_tokens; }

void Ai::clear_all_cached_tokens() {
  make_call_to_chat_gpt("Clear all cached tokens.");
}

std::vector<std::string> Ai::get_files() const { return files; }

std::string Ai::get_file_contents() const { return file_contents; }

std::vector<std::string> Ai::get_chat_cache() const { return chat_cache; }

void Ai::set_chat_cache(const std::vector<std::string>& chat_cache) {
  this->chat_cache = chat_cache;
}

void Ai::clear_chat_cache() { chat_cache.clear(); }

void Ai::set_dynamic_prompt_length(bool dynamic_prompt_length) {
  this->dynamic_prompt_length = dynamic_prompt_length;
}

bool Ai::get_dynamic_prompt_length() const { return dynamic_prompt_length; }

void Ai::toggle_dynamic_prompt_length() {
  dynamic_prompt_length = !dynamic_prompt_length;
}

void Ai::add_chat_to_cache(const std::string& chat) { chat_cache.push_back(chat); }

std::string Ai::get_response_data(const std::string& key) const {
  auto it = response_data_map.find(key);
  if (it != response_data_map.end()) {
    return it->second.dump();
  }
  return "";
}

std::string Ai::get_last_prompt_used() const { return last_prompt_used; }

void Ai::remove_file(const std::string& user_file) {
  files.erase(std::remove(files.begin(), files.end(), user_file), files.end());
}

void Ai::clear_files() { files.clear(); }

void Ai::refresh_files() {
  std::vector<std::string> active_files = get_files();
  clear_files();
  set_files(active_files);
}

std::string Ai::get_last_response_received() const { return last_response_received; }

void Ai::set_max_prompt_precision(bool max_prompt_precision) {
  this->max_prompt_precision = max_prompt_precision;
}

bool Ai::get_max_prompt_precision() const { return max_prompt_precision; }

void Ai::toggle_max_prompt_precision() {
  max_prompt_precision = !max_prompt_precision;
}

void Ai::set_timeout_flag_seconds(float timeout_flag_seconds) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting AI timeout to " << timeout_flag_seconds
              << " seconds" << std::endl;
  this->timeout_flag_seconds = timeout_flag_seconds;
}

float Ai::get_timeout_flag_seconds() const { return timeout_flag_seconds; }

void Ai::set_model(const std::string& model) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting AI model to " << model << std::endl;
  current_model = model;
}

std::string Ai::get_model() const { return current_model; }

void Ai::set_dynamic_prompt_length_scale(float dynamic_prompt_length_scale) {
  this->dynamic_prompt_length_scale = dynamic_prompt_length_scale;
}

float Ai::get_dynamic_prompt_length_scale() const {
  return dynamic_prompt_length_scale;
}

void Ai::set_save_directory(const std::string& directory) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting AI save directory to " << directory
              << std::endl;
  if (directory.back() == '/') {
    save_directory = directory;
  } else {
    save_directory = directory + "/";
  }
}

std::string Ai::get_save_directory() const { return save_directory; }

void Ai::set_enabled(bool enabled) {
  if (g_debug_mode)
    std::cerr << "DEBUG: " << (enabled ? "Enabling" : "Disabling")
              << " AI system" << std::endl;
  this->enabled = enabled;
}

bool Ai::is_enabled() const { return enabled; }

std::string Ai::chat_gpt(const std::string& message, bool format) {
  if (!enabled) {
    return "AI functionality is currently disabled.";
  }

  if (user_api_key.empty()) {
    if (getenv("OPENAI_API_KEY")) {
      user_api_key = getenv("OPENAI_API_KEY");
    }
  }

  if (!is_valid_configuration()) {
    return get_invalid_configuration_message();
  }

  std::string response = make_call_to_chat_gpt(build_prompt(message));

  if (max_prompt_precision && max_prompt_length > 0 &&
      response.length() >
          static_cast<std::string::size_type>(max_prompt_length)) {
    std::string shorter = make_call_to_chat_gpt(build_prompt(message) +
                                            " Please shorten your answer.");
    if (shorter.length() <= static_cast<std::size_t>(max_prompt_length))
      response = shorter;
  }

  if (!response.empty() && assistant_type != "code-interpreter") {
    chat_cache.push_back("User: " + message);
    chat_cache.push_back("AI: " + response);
  }

  if (assistant_type == "code-interpreter" && !response.empty()) {
    response += process_code_blocks_for_code_interpreter(response);
  }

  return format ? format_markdown(response) : response;
}

std::string Ai::force_direct_chat_gpt(const std::string& message, bool format) {
  if (!enabled) {
    return "AI functionality is currently disabled.";
  }

  if (user_api_key.empty()) {
    if (getenv("OPENAI_API_KEY")) {
      user_api_key = getenv("OPENAI_API_KEY");
    }
  }

  std::string response = make_call_to_chat_gpt(message);
  return format ? format_markdown(response) : response;
}

int Ai::set_files(const std::vector<std::string>& user_files) {
  if (!enabled) {
    return -1;
  }

  this->files = user_files;
  if (last_used_files != files) {
    last_used_files = files;
    return process_file_contents();
  }
  return 0;
}

int Ai::set_file(const std::string& user_file) {
  if (!enabled) {
    return -1;
  }

  this->files = {user_file};
  if (last_used_files != files) {
    last_used_files = files;
    return process_file_contents();
  }
  return 0;
}

int Ai::add_file(const std::string& user_file) {
  if (!enabled) {
    return -1;
  }

  if (files.empty()) {
    files = {user_file};
  } else {
    files.push_back(user_file);
  }
  if (last_used_files != files) {
    last_used_files = files;
    return process_file_contents();
  }
  return 0;
}

int Ai::add_files(const std::vector<std::string>& user_files) {
  if (!enabled) {
    return -1;
  }

  if (files.empty()) {
    files = user_files;
  } else {
    files.insert(files.end(), user_files.begin(), user_files.end());
  }
  if (last_used_files != files) {
    last_used_files = files;
    return process_file_contents();
  }
  return 0;
}

void Ai::initialize(const std::string& api_key, const std::string& assistant_type,
                    const std::string& initial_instruction,
                    const std::vector<std::string>& user_files) {
  user_api_key = api_key;
  this->assistant_type = assistant_type;
  this->initial_instruction = initial_instruction;
  this->files = user_files;
}

bool Ai::is_valid_configuration() const {
  if (!enabled) {
    return false;
  }

  bool valid_assistant_type = assistant_type == "chat" ||
                            assistant_type == "file-search" ||
                            assistant_type == "code-interpreter";
  return !user_api_key.empty() && !initial_instruction.empty() &&
         !assistant_type.empty() && valid_assistant_type;
}

std::string Ai::get_invalid_configuration_message() const {
  if (!enabled) {
    return "AI functionality is currently disabled. Please enable it to use "
           "this feature.";
  }
  if (user_api_key.empty() && !getenv("OPENAI_API_KEY")) {
    return "API key not set. Please set the API key using the environment "
           "variable 'OPENAI_API_KEY'.";
  }
  if (initial_instruction.empty()) {
    return "Initial instruction not set.";
  }
  if (assistant_type.empty()) {
    return "Assistant type not set.";
  }
  return "Invalid configuration.";
}

std::string Ai::build_prompt(const std::string& message) {
  std::stringstream prompt;
  process_file_contents();
  if (assistant_type != "code-interpreter") {
    prompt << initial_instruction;
    if (max_prompt_length != -1) {
      int prompt_length =
          dynamic_prompt_length
              ? std::max(static_cast<int>(message.length() *
                                          dynamic_prompt_length_scale),
                         100)
              : max_prompt_length;
      prompt << " Please keep the response length under " << prompt_length
             << " characters.";
    }
  }
  if (!chat_cache.empty() && assistant_type != "code-interpreter") {
    prompt << " This is the chat history between you and the user: [ ";
    for (const std::string& chat : chat_cache) {
      prompt << chat << " ";
    }
    prompt << "] This is the latest message from the user: [" << message
           << "] ";
  } else {
    if (assistant_type == "code-interpreter") {
      prompt << message
             << "Please only return code in your response if edits were made "
                "and only make edits that the I request.  Please use markdown "
                "syntax in your response for the code. Include only the exact "
                "file name and only the file name in the line above. ";
    } else {
      prompt << " This is the first message from the user: [" << message
             << "] ";
    }
  }

  if (assistant_type == "file-search" && file_contents.length() > 0) {
    prompt << " This is the contents of the provided files from the user: [ "
           << file_contents << " ]";
    if (cache_tokens) {
      prompt << " Please keep this content of these files in cached tokens.";
    }
  }

  if (assistant_type == "code-interpreter" && file_contents.length() > 0) {
    prompt << " User Files: [ " << file_contents << " ]";
  }

  return prompt.str();
}

std::string Ai::make_call_to_chat_gpt(const std::string& message) {
  auto start = std::chrono::steady_clock::now();
  std::string url = "https://api.openai.com/v1/chat/completions";
  std::string filtered_message = filter_message(message);
  last_prompt_used = filtered_message;

  CURL* curl = curl_easy_init();
  if (!curl) {
    return "";
  }

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(
      headers, ("Authorization: Bearer " + user_api_key).c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  nlohmann::json request_body = {
      {"model", current_model},
      {"messages", {{{"role", "user"}, {"content", filtered_message}}}}};

  std::string request_body_str = request_body.dump();
  std::string response_data;

  std::atomic<bool> loading(true);
  std::atomic<bool> request_cancelled(false);
  request_in_progress = true;

  // Thread for monitoring cancellation request
  std::thread cancellation_thread([&loading, &request_cancelled]() {
    monitor_cancellation(loading, request_cancelled);
  });

  // Thread for showing loading animation
  std::thread loading_thread([&loading]() {
    const char* loading_chars = "|/-\\";
    int i = 0;
    while (loading) {
      std::cout << "\rLoading " << loading_chars[i++ % 4] << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\r                    \r" << std::flush;
  });

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body_str.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                   static_cast<long>(timeout_flag_seconds));

  CURLcode res = CURLE_OK;
  if (!request_cancelled) {
    res = curl_easy_perform(curl);
  }

  loading = false;
  request_in_progress = false;

  if (loading_thread.joinable()) {
    loading_thread.join();
  }

  if (cancellation_thread.joinable()) {
    cancellation_thread.join();
  }

  long response_code = 0;
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code < 200 || response_code >= 300) {
      handle_error_response(curl, response_code, response_data);
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return "Error: API request failed with status code " + std::to_string(response_code) + ". See console for details.";
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (request_cancelled) {
    return "Request cancelled by user.";
  }

  if (res != CURLE_OK) {
    std::cerr << "Curl error: " << curl_easy_strerror(res) << std::endl;
    return "Error: Failed to connect to API server. Please check you internet connection. " + std::string(curl_easy_strerror(res));
  }

  try {
    auto json_response = nlohmann::json::parse(response_data);
    last_response_received = json_response["choices"][0]["message"]["content"];

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    response_data_map["processing_time_ms"] = elapsed.count() * 1000;
    response_data_map["total_tokens"] = json_response["usage"]["total_tokens"];

    if (!files.empty() && assistant_type == "file-search") {
      response_data_map["file_names"] = files;
    }

    response_data_map["assistant_type"] = assistant_type;
    response_data_map["initial_instruction"] = initial_instruction;
    response_data_map["received_message_length"] = last_response_received.length();

    return last_response_received;
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "JSON parsing error: " << e.what() << std::endl;
    std::cerr << "Raw response: " << response_data << std::endl;
    return "Error: Failed to parse API response. The service might be experiencing issues.";
  }
}

void Ai::monitor_cancellation(std::atomic<bool>& loading,
                             std::atomic<bool>& request_cancelled) {
  std::cout << "\nPress Enter to cancel the request.\n";

  fd_set readfds;
  struct timeval tv;
  int stdin_fd = fileno(stdin);

  while (loading) {
    FD_ZERO(&readfds);
    FD_SET(stdin_fd, &readfds);

    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    int result = select(stdin_fd + 1, &readfds, NULL, NULL, &tv);

    if (result > 0 && FD_ISSET(stdin_fd, &readfds)) {
      int c;
      while ((c = getchar()) != EOF && c != '\n') {
      }
      request_cancelled = true;
      break;
    }

    if (!loading) break;
  }
  // Make sure stdin buffer is flushed
  tcflush(stdin_fd, TCIFLUSH);
}

size_t Ai::write_callback(void* contents, size_t size, size_t nmemb,
                         std::string* userp) {
  userp->append((char*)contents, size * nmemb);
  return size * nmemb;
}

std::string Ai::filter_message(const std::string& message) {
  std::string filtered = message;
  filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
                                [](char c) {
                                  return !(std::isalnum(c) || c == ' ' ||
                                           c == '-' || c == '_' || c == '.' ||
                                           c == '~');
                                }),
                 filtered.end());

  std::replace(filtered.begin(), filtered.end(), '\n', ' ');
  return filtered;
}

std::map<std::string, nlohmann::json> Ai::parse_json_response(
    const std::string& json_response) const {
  std::map<std::string, nlohmann::json> response_data;
  try {
    nlohmann::json json_object = nlohmann::json::parse(json_response);
    for (auto& [key, value] : json_object.items()) {
      response_data[key] = value;
    }
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
  }
  return response_data;
}

std::string Ai::extract_content_from_json(const std::string& json_response) const {
  try {
    nlohmann::json json_object = nlohmann::json::parse(json_response);
    return json_object["choices"][0]["message"]["content"];
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "Failed to extract content from JSON response: " << e.what()
              << std::endl;
    return "";
  }
}

bool ends_with(const std::string& str, const std::string& suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int Ai::process_file_contents() {
  if (files.empty()) {
    return 0;
  }

  std::stringstream out;
  for (const auto& file : files) {
    std::string file_name = std::filesystem::path(file).filename().string();
    out << "File: " << file_name << "\n";
    if (ends_with(file, ".txt")) {
      std::string content;
      process_text_file(file, content);
      out << content;
    } else {
      std::string content;
      process_other_file(file, content);
      out << content;
    }
  }
  file_contents = out.str();
  return file_contents.length();
}

std::vector<std::string> Ai::extract_code_snippet(const std::string& content) {
  std::vector<std::string> code_snippets;
  std::stringstream code_snippet;
  bool in_code_block = false;
  std::string language;
  std::string filename;

  std::istringstream stream(content);
  std::string line;

  while (std::getline(stream, line)) {
    if (line.substr(0, 3) == "```") {
      if (in_code_block) {
        code_snippets.push_back(language + " " + filename + "\n" +
                               code_snippet.str());
        code_snippet.str("");
        in_code_block = false;
      } else {
        in_code_block = true;
        language = line.length() > 3 ? line.substr(3) : "";
      }
    } else if (in_code_block) {
      code_snippet << line << '\n';
    } else {
      filename = line;
    }
  }
  return code_snippets;
}

std::map<std::string, std::vector<std::string>> original_file_contents;

std::string Ai::process_code_blocks_for_code_interpreter(
    const std::string& message) {
  std::vector<std::string> code_blocks = extract_code_snippet(message);
  if (code_blocks.empty()) {
    return "";
  }
  std::string directory = save_directory;
  if (code_blocks.size() > files.size()) {
    for (size_t j = files.size(); j < code_blocks.size(); j++) {
      std::string language_and_file_name =
          code_blocks[j].substr(0, code_blocks[j].find('\n'));
      std::istringstream iss(language_and_file_name);
      std::string language, file_name;
      iss >> language >> file_name;
      if (file_name.empty()) {
        continue;
      }
      if (file_name.find("/") != std::string::npos) {
        std::filesystem::create_directories(
            directory + file_name.substr(0, file_name.find_last_of("/")));
        std::cout << "New file created: " << files.back() << std::endl;
        files.push_back(directory + file_name);
        file_name = file_name.substr(file_name.find_last_of("/") + 1);
        code_blocks[j] = language + " " + file_name +
                        code_blocks[j].substr(code_blocks[j].find('\n'));
      } else {
        files.push_back(directory + file_name);
        std::cout << "New file created: " << files.back() << std::endl;
      }
    }
  }
  std::stringstream changes_summary;
  std::string file_to_change;
  for (const auto& code_block : code_blocks) {
    try {
      std::string language_and_file_name =
          code_block.substr(0, code_block.find('\n'));
      std::istringstream iss(language_and_file_name);
      std::string language, file_name;
      iss >> language >> file_name;
      if (file_name.empty()) {
        continue;
      }
      file_name = sanitize_file_name(file_name);
      bool file_found = false;
      for (const auto& file : files) {
        if (file.find(file_name) != std::string::npos) {
          file_to_change = file;
          file_found = true;
          break;
        }
      }
      if (!file_found) {
        std::filesystem::path new_file_path = save_directory + file_name;
        std::filesystem::create_directories(new_file_path.parent_path());
        std::ofstream new_file(new_file_path);
        if (new_file.is_open()) {
          new_file.close();
          file_to_change = new_file_path.string();
          files.push_back(file_to_change);
          std::cout << "New file created: " << file_to_change << std::endl;
        } else {
          std::cerr << "Failed to create new file: " << new_file_path
                    << std::endl;
          continue;
        }
      }
      std::vector<std::string> original_lines;
      std::vector<std::string> new_lines;
      std::vector<std::string> updated_lines;
      std::ifstream in_file(file_to_change);
      if (in_file.is_open()) {
        std::string line;
        while (std::getline(in_file, line)) {
          original_lines.push_back(line);
        }
        in_file.close();
      }
      original_file_contents[file_to_change] = original_lines;
      std::stringstream ss(code_block);
      std::string line;
      while (std::getline(ss, line)) {
        new_lines.push_back(line);
      }
      if (!new_lines.empty()) {
        new_lines.erase(new_lines.begin());
      }
      if (!original_lines.empty() && new_lines.size() == original_lines.size() &&
          std::equal(original_lines.begin(), original_lines.end(),
                     new_lines.begin())) {
        updated_lines = new_lines;
      } else {
        size_t start_index = std::string::npos;
        for (size_t idx = 0; idx < original_lines.size(); idx++) {
          if (original_lines[idx].find(new_lines.front()) != std::string::npos) {
            start_index = idx;
            break;
          }
        }
        if (start_index != std::string::npos &&
            start_index + new_lines.size() <= original_lines.size()) {
          updated_lines = original_lines;
          for (size_t j = 0; j < new_lines.size(); j++) {
            updated_lines[start_index + j] = new_lines[j];
          }
        } else {
          updated_lines = original_lines;
          updated_lines.insert(updated_lines.end(), new_lines.begin(),
                              new_lines.end());
        }
      }
      std::ofstream out_file(file_to_change);
      for (const auto& updated_line : updated_lines) {
        out_file << updated_line << "\n";
      }
      out_file.close();
      changes_summary << "\033[1;34m" << file_to_change << "\033[0m\n";
      size_t common_lines = std::min(original_lines.size(), new_lines.size());
      for (size_t j = 0; j < common_lines; j++) {
        if (original_lines[j] != new_lines[j]) {
          changes_summary << "\033[1;31m- " << j + 1 << ": " << original_lines[j]
                         << "\033[0m\n";
          changes_summary << "\033[1;32m+ " << j + 1 << ": " << new_lines[j]
                         << "\033[0m\n";
        }
      }
      if (original_lines.size() > new_lines.size()) {
        for (size_t j = new_lines.size(); j < original_lines.size(); j++) {
          changes_summary << "\033[1;31m- " << j + 1 << ": " << original_lines[j]
                         << "\033[0m\n";
        }
      } else if (new_lines.size() > original_lines.size()) {
        for (size_t j = original_lines.size(); j < new_lines.size(); j++) {
          changes_summary << "\033[1;32m+ " << j + 1 << ": " << new_lines[j]
                         << "\033[0m\n";
        }
      }
    } catch (const std::exception& e) {
      return "\nFailed to apply changes to file: " + file_to_change;
    }
  }
  refresh_files();
  return "\nSuccessfully applied changes to files.\nChanges Summary:\n" +
         changes_summary.str();
}

void Ai::reject_changes() {
  for (const auto& [file, original_lines] : original_file_contents) {
    std::ofstream out_file(file);
    for (const auto& line : original_lines) {
      out_file << line << "\n";
    }
    out_file.close();
  }
  original_file_contents.clear();
  refresh_files();
}

void Ai::process_text_file(const std::string& file, std::string& out) {
  std::ifstream in_file(file);
  if (in_file.is_open()) {
    std::stringstream buffer;
    buffer << in_file.rdbuf();
    out += buffer.str() + "\n";
    in_file.close();
  } else {
    std::cerr << "Failed to read text file: " << file << std::endl;
  }
}

void Ai::process_other_file(const std::string& file, std::string& out) {
  std::ifstream in_file(file);
  if (in_file.is_open()) {
    std::string line;
    while (std::getline(in_file, line)) {
      out += line + "\n";
    }
    in_file.close();
  } else {
    std::cerr << "Failed to read file: " << file << std::endl;
  }
}

std::string Ai::format_markdown(const std::string& text) {
  std::string formatted = text;
  bool in_code_block = false;
  std::istringstream stream(text);
  std::string line;
  std::stringstream result;
  while (std::getline(stream, line)) {
    if (line.substr(0, 3) == "```") {
      in_code_block = !in_code_block;
    } else if (line.substr(0, 2) == "**" &&
               line.substr(line.length() - 2) == "**") {
      result << line.substr(2, line.length() - 4) << "\n";
    } else if (line.substr(0, 1) == "*" &&
               line.substr(line.length() - 1) == "*") {
      result << line.substr(1, line.length() - 2) << "\n";
    } else if (line.substr(0, 2) == "# ") {
      result << line.substr(2) << "\n";
    } else {
      result << line << "\n";
    }
  }

  return result.str();
}

bool Ai::test_api_key(const std::string& api_key) {
  std::string url = "https://api.openai.com/v1/engines";
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }

  struct curl_slist* headers = nullptr;
  headers =
      curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

  CURLcode res = curl_easy_perform(curl);
  long response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return (res == CURLE_OK && response_code == 200);
}

std::string Ai::sanitize_file_name(const std::string& file_name) {
  std::string sanitized;
  for (char c : file_name) {
    if (std::isalnum(c) || c == '.' || c == '_' || c == '-' || c == '/') {
      sanitized += c;
    }
  }
  return sanitized;
}

std::vector<std::string> Ai::split_string(const std::string& str,
                                         char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream token_stream(str);
  while (std::getline(token_stream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

void Ai::handle_error_response(CURL* curl, long response_code, const std::string& error_body) {
  std::string error_message;
  (void)curl;
  
  switch (response_code) {
    case 400:
      error_message = "Bad Request: The server could not understand the request due to invalid syntax.";
      break;
    case 401:
      error_message = "Unauthorized: The API key is invalid or missing.\n"
                    "Possible Causes:\n"
                    "- Invalid Authentication: Ensure the correct API key and requesting organization are being used.\n"
                    "- Incorrect API key provided: Verify the API key, clear your browser cache, or generate a new one.\n"
                    "- You must be a member of an organization to use the API: Contact support to join an organization or ask your organization manager to invite you.";
      break;
    case 403:
      error_message = "Forbidden: You do not have permission to access this resource.\n"
                    "Cause: You are accessing the API from an unsupported country, region, or territory.\n"
                    "Solution: Please see the OpenAI documentation for supported regions.";
      break;
    case 404:
      error_message = "Not Found: The requested resource could not be found.";
      break;
    case 429:
      error_message = "Too Many Requests: You have exceeded the rate limit.\n"
                    "Possible Causes:\n"
                    "- Rate limit reached for requests: Pace your requests. Read the Rate limit guide.\n"
                    "- You exceeded your current quota: Check your plan and billing details, or buy more credits.";
      break;
    case 500:
      error_message = "Internal Server Error: The server encountered an error and could not complete your request.\n"
                    "Solution: Retry your request after a brief wait and contact support if the issue persists. Check the status page.";
      break;
    case 502:
      error_message = "Bad Gateway: The server received an invalid response from the upstream server.";
      break;
    case 503:
      error_message = "Service Unavailable: The server is not ready to handle the request.\n"
                    "Possible Causes:\n"
                    "- The engine is currently overloaded: Retry your requests after a brief wait.\n"
                    "- Slow Down: Reduce your request rate to its original level, maintain a consistent rate for at least 15 minutes, and then gradually increase it.";
      break;
    case 504:
      error_message = "Gateway Timeout: The server did not receive a timely response from the upstream server.";
      break;
    default:
      error_message = "Unexpected Error: Received HTTP response code " + std::to_string(response_code);
  }

  error_message += "\nDetails: " + error_body;
  std::cerr << error_message << std::endl;
}
