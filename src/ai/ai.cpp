#include "ai.h"

#include "cjsh.h"
#include "http_client.h"

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
  (void)save_directory;
  set_enabled(enabled);
  if (enabled) {
    initialize(api_key, assistant_type, initial_instruction, user_files);
    // set_save_directory(save_directory);
  }
}

Ai::Ai() {
}

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

std::string Ai::get_initial_instruction() const {
  return initial_instruction;
}

void Ai::set_assistant_name(const std::string& name) {
  assistant_name = name;
}

std::string Ai::get_assistant_name() {
  return assistant_name;
}

void Ai::set_assistant_type(const std::string& assistant_type) {
  this->assistant_type = assistant_type;
}

std::string Ai::get_assistant_type() const {
  return assistant_type;
}

void Ai::set_max_prompt_length(int max_prompt_length) {
  this->max_prompt_length = max_prompt_length;
}

int Ai::get_max_prompt_length() const {
  return max_prompt_length;
}

void Ai::set_cache_tokens(bool cache_tokens) {
  this->cache_tokens = cache_tokens;
}

bool Ai::get_cache_tokens() const {
  return cache_tokens;
}

void Ai::toggle_cache_tokens() {
  cache_tokens = !cache_tokens;
}

void Ai::clear_all_cached_tokens() {
  make_call_to_chat_gpt("Clear all cached tokens.");
}

std::vector<std::string> Ai::get_files() const {
  return files;
}

std::string Ai::get_file_contents() const {
  return file_contents;
}

std::vector<std::string> Ai::get_chat_cache() const {
  return chat_cache;
}

void Ai::set_chat_cache(const std::vector<std::string>& chat_cache) {
  this->chat_cache = chat_cache;
}

void Ai::clear_chat_cache() {
  chat_cache.clear();
}

void Ai::set_dynamic_prompt_length(bool dynamic_prompt_length) {
  this->dynamic_prompt_length = dynamic_prompt_length;
}

bool Ai::get_dynamic_prompt_length() const {
  return dynamic_prompt_length;
}

void Ai::toggle_dynamic_prompt_length() {
  dynamic_prompt_length = !dynamic_prompt_length;
}

void Ai::add_chat_to_cache(const std::string& chat) {
  chat_cache.push_back(chat);
}

std::string Ai::get_response_data(const std::string& key) const {
  auto it = response_data_map.find(key);
  if (it != response_data_map.end()) {
    return it->second.dump();
  }
  return "";
}

std::string Ai::get_last_prompt_used() const {
  return last_prompt_used;
}

void Ai::remove_file(const std::string& user_file) {
  files.erase(std::remove(files.begin(), files.end(), user_file), files.end());
}

void Ai::clear_files() {
  files.clear();
}

void Ai::refresh_files() {
  std::vector<std::string> active_files = get_files();
  clear_files();
  set_files(active_files);
}

std::string Ai::get_last_response_received() const {
  return last_response_received;
}

void Ai::set_max_prompt_precision(bool max_prompt_precision) {
  this->max_prompt_precision = max_prompt_precision;
}

bool Ai::get_max_prompt_precision() const {
  return max_prompt_precision;
}

void Ai::toggle_max_prompt_precision() {
  max_prompt_precision = !max_prompt_precision;
}

void Ai::set_timeout_flag_seconds(float timeout_flag_seconds) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting AI timeout to " << timeout_flag_seconds
              << " seconds" << std::endl;
  this->timeout_flag_seconds = timeout_flag_seconds;
}

float Ai::get_timeout_flag_seconds() const {
  return timeout_flag_seconds;
}

void Ai::set_model(const std::string& model) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting AI model to " << model << std::endl;
  current_model = model;
}

std::string Ai::get_model() const {
  return current_model;
}

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

std::string Ai::get_save_directory() const {
  return save_directory;
}

void Ai::set_enabled(bool enabled) {
  if (g_debug_mode)
    std::cerr << "DEBUG: " << (enabled ? "Enabling" : "Disabling")
              << " AI system" << std::endl;
  this->enabled = enabled;
}

bool Ai::is_enabled() const {
  return enabled;
}

void ltrim(std::string& s) {
  size_t start = 0;
  while (start < s.size() &&
         std::isspace(static_cast<unsigned char>(s[start]))) {
    ++start;
  }
  s.erase(0, start);
}

void rtrim(std::string& s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
}

void trim(std::string& s) {
  ltrim(s);
  rtrim(s);
}

std::string Ai::chat_gpt(const std::string& sys_prompt,
                         const std::string& message, bool format) {
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

  std::string response =
      make_call_to_chat_gpt(build_prompt(sys_prompt, message));

  if (max_prompt_precision && max_prompt_length > 0 &&
      response.length() >
          static_cast<std::string::size_type>(max_prompt_length)) {
    std::string shorter = make_call_to_chat_gpt(
        build_prompt(sys_prompt, message) + " Please shorten your answer.");
    if (shorter.length() <= static_cast<std::size_t>(max_prompt_length))
      response = shorter;
  }

  std::string clean_text;
  {
    bool in_code_block = false;
    std::istringstream ss(response);
    std::ostringstream oss;
    std::string ln;
    std::string next;
    if (!std::getline(ss, ln))
      ln = "";
    while (true) {
      if (!std::getline(ss, next)) {
        if (!in_code_block && !ln.empty() && ln.rfind("```", 0) != 0) {
          oss << ln << "\n";
        }
        break;
      }
      if (ln.rfind("```", 0) == 0) {
        in_code_block = !in_code_block;
        ln = next;
        continue;
      }
      if (!in_code_block && next.rfind("```", 0) == 0) {
        ln = next;
        continue;
      }
      if (!in_code_block && !ln.empty()) {
        oss << ln << "\n";
      }
      ln = next;
    }
    clean_text = oss.str();
  }
  clean_text = (format ? format_markdown(clean_text) : clean_text);
  clean_text.erase(std::remove(clean_text.begin(), clean_text.end(), '`'),
                   clean_text.end());
  trim(clean_text);

  if (assistant_type == "code-interpreter" && !response.empty()) {
    std::cout << process_code_blocks_for_code_interpreter(response)
              << std::endl;
  }

  if (voice_dictation_enabled && clean_text != "Request cancelled by user.") {
    process_voice_dictation(clean_text);
  }

  if (!clean_text.empty()) {
    chat_cache.push_back("User: " + message);
    chat_cache.push_back(assistant_name + ": " + clean_text);
  }

  return (assistant_type == "code-interpreter" ? clean_text : response);
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

void Ai::load_ai_config() {
  cjsh_filesystem::fs::path config_file_path;
  if (config_name == "default") {
    config_file_path = cjsh_filesystem::g_cjsh_ai_default_config_path;

    if (!cjsh_filesystem::fs::exists(config_file_path)) {
      config_file_path = cjsh_filesystem::g_cjsh_ai_config_file_path;
    }
  } else {
    config_file_path =
        cjsh_filesystem::g_cjsh_ai_config_path / (config_name + ".json");
  }

  if (!cjsh_filesystem::fs::exists(config_file_path)) {
    if (config_name == "default") {
      create_default_config_file();

      if (cjsh_filesystem::fs::exists(
              cjsh_filesystem::g_cjsh_ai_default_config_path)) {
        config_file_path = cjsh_filesystem::g_cjsh_ai_default_config_path;
      } else {
        std::cerr << "Error: Could not create default AI config file."
                  << std::endl;
        return;
      }
    } else {
      std::cerr << "Error: AI config file '" << config_name << "' not found."
                << std::endl;
      return;
    }
  }

  std::ifstream config_file(config_file_path);
  if (config_file.is_open()) {
    nlohmann::json config_json;
    try {
      config_file >> config_json;
      if (config_json.contains("assistant_name")) {
        assistant_name = config_json["assistant_name"].get<std::string>();
      }
      if (config_json.contains("initial_instruction")) {
        set_initial_instruction(
            config_json["initial_instruction"].get<std::string>());
      }
      if (config_json.contains("assistant_type")) {
        this->assistant_type = config_json["assistant_type"].get<std::string>();
      }
      if (config_json.contains("max_prompt_length")) {
        max_prompt_length = config_json["max_prompt_length"].get<int>();
      }
      if (config_json.contains("cache_tokens")) {
        cache_tokens = config_json["cache_tokens"].get<bool>();
      }
      if (config_json.contains("max_prompt_precision")) {
        max_prompt_precision = config_json["max_prompt_precision"].get<bool>();
      }
      if (config_json.contains("dynamic_prompt_length")) {
        dynamic_prompt_length =
            config_json["dynamic_prompt_length"].get<bool>();
      }
      if (config_json.contains("dynamic_prompt_length_scale")) {
        dynamic_prompt_length_scale =
            config_json["dynamic_prompt_length_scale"].get<float>();
      }
      if (config_json.contains("timeout_flag_seconds")) {
        timeout_flag_seconds = config_json["timeout_flag_seconds"].get<float>();
      }
      if (config_json.contains("model")) {
        current_model = config_json["model"].get<std::string>();
      }
      set_save_directory(cjsh_filesystem::g_cjsh_data_path);
      if (config_json.contains("enabled")) {
        enabled = config_json["enabled"].get<bool>();
      } else {
        enabled = true;
      }
      if (config_json.contains("voice_dictation_enabled")) {
        voice_dictation_enabled =
            config_json["voice_dictation_enabled"].get<bool>();
      } else {
        voice_dictation_enabled = true;
      }
      if (config_json.contains("voice_dictation_voice")) {
        voice_dictation_voice =
            config_json["voice_dictation_voice"].get<std::string>();
      } else {
        voice_dictation_voice = "onyx";
      }
      if (config_json.contains("voice_dictation_instructions")) {
        voice_dictation_instructions =
            config_json["voice_dictation_instructions"].get<std::string>();
      } else {
        voice_dictation_instructions =
            "Accent/Affect: Moderate British accent; sophisticated yet "
            "friendly, clearly understandable but lower voice tones. Tone: "
            "Warm, Calm. Pacing: Moderate.";
      }
    } catch (const std::exception& e) {
      std::cerr << "Error parsing AI config file: " << e.what() << std::endl;
    }
    config_file.close();
  } else {
    std::cerr << "Error opening AI config file." << std::endl;
  }
}

void Ai::save_ai_config() {
  cjsh_filesystem::fs::path config_file_path;
  if (config_name == "default") {
    config_file_path = cjsh_filesystem::g_cjsh_ai_default_config_path;
  } else {
    config_file_path =
        cjsh_filesystem::g_cjsh_ai_config_path / (config_name + ".json");
  }

  std::ofstream config_file(config_file_path);
  if (config_file.is_open()) {
    nlohmann::json config_json = {
        {"assistant_name", assistant_name},
        {"initial_instruction", initial_instruction},
        {"assistant_type", assistant_type},
        {"max_prompt_length", max_prompt_length},
        {"cache_tokens", cache_tokens},
        {"max_prompt_precision", max_prompt_precision},
        {"dynamic_prompt_length", dynamic_prompt_length},
        {"dynamic_prompt_length_scale", dynamic_prompt_length_scale},
        {"timeout_flag_seconds", timeout_flag_seconds},
        {"model", current_model},
        {"enabled", enabled},
        {"voice_dictation_enabled", voice_dictation_enabled},
        {"voice_dictation_voice", voice_dictation_voice},
        {"voice_dictation_instructions", voice_dictation_instructions}};
    config_file << config_json.dump(4);
    config_file.close();
  } else {
    std::cerr << "Error saving AI config file." << std::endl;
  }
}

void Ai::create_default_config_file() {
  std::ofstream config_file(cjsh_filesystem::g_cjsh_ai_default_config_path);
  if (config_file.is_open()) {
    nlohmann::json default_config = {
        {"assistant_name", "CJ's Shell Assistant"},
        {"initial_instruction",
         "You are a helpful AI assistant within the user's shell environment. "
         "Provide concise, accurate information and assist with shell tasks "
         "when requested."},
        {"assistant_type", "chat"},
        {"max_prompt_length", 1000},
        {"cache_tokens", false},
        {"max_prompt_precision", true},
        {"dynamic_prompt_length", true},
        {"dynamic_prompt_length_scale", 3},
        {"timeout_flag_seconds", 180},
        {"model", "gpt-3.5-turbo"},
        {"enabled", true},
        {"voice_dictation_enabled", false},
        {"voice_dictation_voice", "alloy"},
        {"voice_dictation_instructions",
         "Use a natural, conversational tone with clear pronunciation and "
         "moderate pacing."}};
    config_file << default_config.dump(4);
    config_file.close();
  } else {
    std::cerr << "Error creating default AI config file." << std::endl;
  }
}

void Ai::initialize(const std::string& api_key,
                    const std::string& assistant_type,
                    const std::string& initial_instruction,
                    const std::vector<std::string>& user_files) {
  user_api_key = api_key;
  this->assistant_type = assistant_type;
  this->initial_instruction = initial_instruction;
  this->files = user_files;

  cjsh_filesystem::fs::path ai_config_dir =
      cjsh_filesystem::g_cjsh_ai_config_path;
  if (!cjsh_filesystem::fs::exists(ai_config_dir)) {
    try {
      cjsh_filesystem::fs::create_directories(ai_config_dir);
    } catch (const cjsh_filesystem::fs::filesystem_error& e) {
      std::cerr << "Error creating AI config directory: " << e.what()
                << std::endl;
    }
  }

  bool has_default_config = cjsh_filesystem::fs::exists(
      cjsh_filesystem::g_cjsh_ai_default_config_path);
  bool has_legacy_config =
      cjsh_filesystem::fs::exists(cjsh_filesystem::g_cjsh_ai_config_file_path);

  if (!has_default_config && !has_legacy_config) {
    create_default_config_file();
  } else if (!has_default_config && has_legacy_config) {
    try {
      cjsh_filesystem::fs::copy_file(
          cjsh_filesystem::g_cjsh_ai_config_file_path,
          cjsh_filesystem::g_cjsh_ai_default_config_path);
    } catch (const cjsh_filesystem::fs::filesystem_error& e) {
      std::cerr << "Error copying legacy config to default: " << e.what()
                << std::endl;
    }
  }

  config_name = "default";
  load_ai_config();
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

std::string Ai::build_prompt(const std::string& sys_prompt,
                             const std::string& message) {
  std::stringstream prompt;
  process_file_contents();
  if (!assistant_name.empty()) {
    prompt << "You are named " << assistant_name
           << ". Please refer to yourself as such. ";
  }
  prompt << initial_instruction << "\n\n" << sys_prompt;
  if (assistant_type != "code-interpreter") {
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
  if (!chat_cache.empty()) {
    prompt << " This is the chat history between you and the user: [ ";
    for (const std::string& chat : chat_cache) {
      prompt << chat << " ";
    }
    prompt << "] This is the latest message from the user: [" << message
           << "] ";
  } else {
    prompt << " This is the first message from the user: [" << message << "] ";
  }
  if (assistant_type == "code-interpreter") {
    prompt << message
           << "Please only return code in your response if edits were made. "
              "Please only make the edits that I request.  Please use markdown "
              "syntax in your response for the code. Include only the exact "
              "file name and only the file name in the line above. "
              "Be sure to give a brief summary of the changes you made, but "
              "explain them in a professional conversation matter not in a "
              "list format."
              "Do not reference this prompt in any way.";
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

  nlohmann::json request_body = {
      {"model", current_model},
      {"messages", {{{"role", "user"}, {"content", filtered_message}}}}};

  std::string request_body_str = request_body.dump();

  std::atomic<bool> loading(true);
  std::atomic<bool> request_cancelled(false);
  request_in_progress = true;

  std::thread cancellation_thread([&loading, &request_cancelled]() {
    monitor_cancellation(loading, request_cancelled);
  });

  std::thread loading_thread([&loading]() {
    const char* loading_chars = "|/-\\";
    int i = 0;
    while (loading) {
      std::cout << "\rLoading " << loading_chars[i++ % 4] << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\r                    \r" << std::flush;
  });

  // Prepare headers
  std::map<std::string, std::string> headers;
  headers["Authorization"] = "Bearer " + user_api_key;
  headers["Content-Type"] = "application/json";

  // Make HTTP request
  HttpResponse response;
  if (!request_cancelled) {
    response = HttpClient::post(url, request_body_str, headers,
                                static_cast<int>(timeout_flag_seconds));
  }

  loading = false;
  request_in_progress = false;

  if (loading_thread.joinable()) {
    loading_thread.join();
  }

  if (cancellation_thread.joinable()) {
    cancellation_thread.join();
  }

  if (request_cancelled) {
    return "Request cancelled by user.";
  }

  if (!response.success) {
    if (response.status_code >= 400) {
      handle_error_response(response.status_code, response.body);
      return "Error: API request failed with status code " +
             std::to_string(response.status_code) +
             ". See console for details.";
    } else {
      std::cerr << "HTTP error: " << response.error_message << std::endl;
      return "Error: Failed to connect to API server. Please check your "
             "internet connection. " +
             response.error_message;
    }
  }

  try {
    auto json_response = nlohmann::json::parse(response.body);
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
    response_data_map["received_message_length"] =
        last_response_received.length();

    return last_response_received;
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "JSON parsing error: " << e.what() << std::endl;
    std::cerr << "Raw response: " << response.body << std::endl;
    return "Error: Failed to parse API response. The service might be "
           "experiencing issues.";
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

    if (!loading)
      break;
  }

  tcflush(stdin_fd, TCIFLUSH);
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

std::string Ai::extract_content_from_json(
    const std::string& json_response) const {
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
      if (!original_lines.empty() &&
          new_lines.size() == original_lines.size() &&
          std::equal(original_lines.begin(), original_lines.end(),
                     new_lines.begin())) {
        updated_lines = new_lines;
      } else {
        size_t start_index = std::string::npos;
        for (size_t idx = 0; idx < original_lines.size(); idx++) {
          if (original_lines[idx].find(new_lines.front()) !=
              std::string::npos) {
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
          changes_summary << "\033[1;31m- " << j + 1 << ": "
                          << original_lines[j] << "\033[0m\n";
          changes_summary << "\033[1;32m+ " << j + 1 << ": " << new_lines[j]
                          << "\033[0m\n";
        }
      }
      if (original_lines.size() > new_lines.size()) {
        for (size_t j = new_lines.size(); j < original_lines.size(); j++) {
          changes_summary << "\033[1;31m- " << j + 1 << ": "
                          << original_lines[j] << "\033[0m\n";
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

  std::map<std::string, std::string> headers;
  headers["Authorization"] = "Bearer " + api_key;
  headers["Content-Type"] = "application/json";

  HttpResponse response = HttpClient::head(url, headers, 30);

  return response.success && response.status_code == 200;
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

bool Ai::process_voice_dictation(const std::string& message) {
  std::string temp_file_name =
      cjsh_filesystem::g_cjsh_ai_conversations_path.string() + "/" +
      current_model + "_" + assistant_type + ".mp3";

  std::ofstream ofs(temp_file_name, std::ios::binary);
  if (!ofs.is_open())
    return false;

  json body = {{"model", "gpt-4o-mini-tts"},
               {"input", message},
               {"voice", voice_dictation_voice},
               {"instructions", voice_dictation_instructions}};
  std::string jsonData = body.dump();

  std::atomic<bool> loading(true);
  std::atomic<bool> request_cancelled(false);
  request_in_progress = true;

  std::thread cancellation_thread([&loading, &request_cancelled]() {
    monitor_cancellation(loading, request_cancelled);
  });

  std::thread loading_thread([&loading]() {
    const char* loading_chars = "|/-\\";
    int i = 0;
    while (loading) {
      std::cout << "\rGenerating audio " << loading_chars[i++ % 4]
                << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\r                         \r" << std::flush;
  });

  // Prepare headers
  std::map<std::string, std::string> headers;
  headers["Authorization"] = "Bearer " + user_api_key;
  headers["Content-Type"] = "application/json";

  // Make HTTP request
  HttpResponse response;
  if (!request_cancelled) {
    response =
        HttpClient::post("https://api.openai.com/v1/audio/speech", jsonData,
                         headers, static_cast<int>(timeout_flag_seconds));
  }

  loading = false;
  request_in_progress = false;

  if (loading_thread.joinable()) {
    loading_thread.join();
  }

  if (cancellation_thread.joinable()) {
    cancellation_thread.join();
  }

  if (request_cancelled) {
    ofs.close();
    if (std::filesystem::exists(temp_file_name)) {
      std::filesystem::remove(temp_file_name);
    }
    return false;
  }

  if (!response.success) {
    ofs.close();
    std::cerr << "HTTP error generating audio: " << response.error_message
              << std::endl;
    return false;
  }

  // Write the audio data to file
  ofs.write(response.body.c_str(), response.body.length());
  ofs.close();

  std::string command =
      "(afplay \"" + temp_file_name + "\" && rm \"" + temp_file_name + "\")";
  std::vector<std::string> args;
  args.push_back("__INTERNAL_SUBSHELL__");
  args.push_back(command);
  if (g_shell && g_shell->shell_exec) {
    g_shell->shell_exec->execute_command_async(args);
  }

  return response.success;
}

void Ai::set_voice_dictation_enabled(bool enabled) {
  voice_dictation_enabled = enabled;
}

bool Ai::get_voice_dictation_enabled() const {
  return voice_dictation_enabled;
}

void Ai::set_voice_dictation_voice(const std::string& voice) {
  voice_dictation_voice = voice;
}

std::string Ai::get_voice_dictation_voice() const {
  return voice_dictation_voice;
}

void Ai::set_voice_dictation_instructions(const std::string& instructions) {
  voice_dictation_instructions = instructions;
}

std::string Ai::get_voice_dictation_instructions() const {
  return voice_dictation_instructions;
}

void Ai::handle_error_response(int status_code, const std::string& error_body) {
  std::string error_message;

  switch (status_code) {
    case 400:
      error_message =
          "Bad Request: The server could not understand the request due to "
          "invalid syntax.";
      break;
    case 401:
      error_message =
          "Unauthorized: The API key is invalid or missing.\n"
          "Possible Causes:\n"
          "- Invalid Authentication: Ensure the correct API key and requesting "
          "organization are being used.\n"
          "- Incorrect API key provided: Verify the API key, clear your "
          "browser cache, or generate a new one.\n"
          "- You must be a member of an organization to use the API: Contact "
          "support to join an organization or ask your organization manager to "
          "invite you.";
      break;
    case 403:
      error_message =
          "Forbidden: You do not have permission to access this resource.\n"
          "Cause: You are accessing the API from an unsupported country, "
          "region, or territory.\n"
          "Solution: Please see the OpenAI documentation for supported "
          "regions.";
      break;
    case 404:
      error_message = "Not Found: The requested resource could not be found.";
      break;
    case 429:
      error_message =
          "Too Many Requests: You have exceeded the rate limit.\n"
          "Possible Causes:\n"
          "- Rate limit reached for requests: Pace your requests. Read the "
          "Rate limit guide.\n"
          "- You exceeded your current quota: Check your plan and billing "
          "details, or buy more credits.";
      break;
    case 500:
      error_message =
          "Internal Server Error: The server encountered an error and could "
          "not complete your request.\n"
          "Solution: Retry your request after a brief wait and contact support "
          "if the issue persists. Check the status page.";
      break;
    case 502:
      error_message =
          "Bad Gateway: The server received an invalid response from the "
          "upstream server.";
      break;
    case 503:
      error_message =
          "Service Unavailable: The server is not ready to handle the "
          "request.\n"
          "Possible Causes:\n"
          "- The engine is currently overloaded: Retry your requests after a "
          "brief wait.\n"
          "- Slow Down: Reduce your request rate to its original level, "
          "maintain a consistent rate for at least 15 minutes, and then "
          "gradually increase it.";
      break;
    case 504:
      error_message =
          "Gateway Timeout: The server did not receive a timely response from "
          "the upstream server.";
      break;
    default:
      error_message = "Unexpected Error: Received HTTP response code " +
                      std::to_string(status_code);
  }

  error_message += "\nDetails: " + error_body;
  std::cerr << error_message << std::endl;
}
