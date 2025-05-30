#include "ai.h"

#include "main.h"

Ai::Ai(const std::string& apiKey, const std::string& assistantType,
       const std::string& initialInstruction) {
  initialize(apiKey, assistantType, initialInstruction, {});
}

Ai::Ai(const std::string& apiKey, const std::string& assistantType,
       const std::string& initialInstruction,
       const std::vector<std::string>& userFiles) {
  initialize(apiKey, assistantType, initialInstruction, userFiles);
}

Ai::Ai(const std::string& apiKey, const std::string& assistantType,
       const std::string& initialInstruction,
       const std::vector<std::string>& userFiles,
       const std::string& saveDirectory, bool enabled) {
  initialize(apiKey, assistantType, initialInstruction, userFiles);
  setSaveDirectory(saveDirectory);
  setEnabled(enabled);
}

Ai::Ai() {}

void Ai::setAPIKey(const std::string& apiKey) {
  setenv("OPENAI_API_KEY", apiKey.c_str(), 1);
  USER_API_KEY = apiKey;
}

std::string Ai::getAPIKey() const {
  const char* env_key = getenv("OPENAI_API_KEY");
  if (env_key && env_key[0] != '\0') {
    return env_key;
  }
  return USER_API_KEY;
}

void Ai::setInitialInstruction(const std::string& instruction) {
  initialInstruction = instruction;
}

std::string Ai::getInitialInstruction() const { return initialInstruction; }

void Ai::setAssistantType(const std::string& assistantType) {
  this->assistantType = assistantType;
}

std::string Ai::getAssistantType() const { return assistantType; }

void Ai::setMaxPromptLength(int maxPromptLength) {
  this->maxPromptLength = maxPromptLength;
}

int Ai::getMaxPromptLength() const { return maxPromptLength; }

void Ai::setCacheTokens(bool cacheTokens) { this->cacheTokens = cacheTokens; }

bool Ai::getCacheTokens() const { return cacheTokens; }

void Ai::toggleCacheTokens() { cacheTokens = !cacheTokens; }

void Ai::clearAllCachedTokens() {
  makeCallToChatGPT("Clear all cached tokens.");
}

std::vector<std::string> Ai::getFiles() const { return files; }

std::string Ai::getFileContents() const { return fileContents; }

std::vector<std::string> Ai::getChatCache() const { return chatCache; }

void Ai::setChatCache(const std::vector<std::string>& chatCache) {
  this->chatCache = chatCache;
}

void Ai::clearChatCache() { chatCache.clear(); }

void Ai::setDynamicPromptLength(bool dynamicPromptLength) {
  this->dynamicPromptLength = dynamicPromptLength;
}

bool Ai::getDynamicPromptLength() const { return dynamicPromptLength; }

void Ai::toggleDynamicPromptLength() {
  dynamicPromptLength = !dynamicPromptLength;
}

void Ai::addChatToCache(const std::string& chat) { chatCache.push_back(chat); }

std::string Ai::getResponseData(const std::string& key) const {
  auto it = responseDataMap.find(key);
  if (it != responseDataMap.end()) {
    return it->second.dump();
  }
  return "";
}

std::string Ai::getLastPromptUsed() const { return lastPromptUsed; }

void Ai::removeFile(const std::string& userFile) {
  files.erase(std::remove(files.begin(), files.end(), userFile), files.end());
}

void Ai::clearFiles() { files.clear(); }

void Ai::refreshFiles() {
  std::vector<std::string> activeFiles = getFiles();
  clearFiles();
  setFiles(activeFiles);
}

std::string Ai::getLastResponseReceived() const { return lastResponseReceived; }

void Ai::setMaxPromptPrecision(bool maxPromptPrecision) {
  this->maxPromptPrecision = maxPromptPrecision;
}

bool Ai::getMaxPromptPrecision() const { return maxPromptPrecision; }

void Ai::toggleMaxPromptPrecision() {
  maxPromptPrecision = !maxPromptPrecision;
}

void Ai::setTimeoutFlagSeconds(float timeoutFlagSeconds) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting AI timeout to " << timeoutFlagSeconds
              << " seconds" << std::endl;
  this->timeoutFlagSeconds = timeoutFlagSeconds;
}

float Ai::getTimeoutFlagSeconds() const { return timeoutFlagSeconds; }

void Ai::setModel(const std::string& model) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting AI model to " << model << std::endl;
  currentModel = model;
}

std::string Ai::getModel() const { return currentModel; }

void Ai::setDynamicPromptLengthScale(float dynamicPromptLengthScale) {
  this->dynamicPromptLengthScale = dynamicPromptLengthScale;
}

float Ai::getDynamicPromptLengthScale() const {
  return dynamicPromptLengthScale;
}

void Ai::setSaveDirectory(const std::string& directory) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting AI save directory to " << directory
              << std::endl;
  if (directory.back() == '/') {
    saveDirectory = directory;
  } else {
    saveDirectory = directory + "/";
  }
}

std::string Ai::getSaveDirectory() const { return saveDirectory; }

void Ai::setEnabled(bool enabled) {
  if (g_debug_mode)
    std::cerr << "DEBUG: " << (enabled ? "Enabling" : "Disabling")
              << " AI system" << std::endl;
  this->enabled = enabled;
}

bool Ai::isEnabled() const { return enabled; }

std::string Ai::chatGPT(const std::string& message, bool format) {
  if (!enabled) {
    return "AI functionality is currently disabled.";
  }

  if (!isValidConfiguration()) {
    return getInvalidConfigurationMessage();
  }

  std::string response = makeCallToChatGPT(buildPrompt(message));

  if (maxPromptPrecision && maxPromptLength > 0 &&
      response.length() >
          static_cast<std::string::size_type>(maxPromptLength)) {
    std::string shorter = makeCallToChatGPT(buildPrompt(message) +
                                            " Please shorten your answer.");
    if (shorter.length() <= static_cast<std::size_t>(maxPromptLength))
      response = shorter;
  }

  if (!response.empty() && assistantType != "code-interpreter") {
    chatCache.push_back("User: " + message);
    chatCache.push_back("AI: " + response);
  }

  if (assistantType == "code-interpreter" && !response.empty()) {
    response += processCodeBlocksForCodeInterpreter(response);
  }

  return format ? formatMarkdown(response) : response;
}

std::string Ai::forceDirectChatGPT(const std::string& message, bool format) {
  if (!enabled) {
    return "AI functionality is currently disabled.";
  }

  std::string response = makeCallToChatGPT(message);
  return format ? formatMarkdown(response) : response;
}

int Ai::setFiles(const std::vector<std::string>& userFiles) {
  if (!enabled) {
    return -1;
  }

  this->files = userFiles;
  if (lastUsedFiles != files) {
    lastUsedFiles = files;
    return processFileContents();
  }
  return 0;
}

int Ai::setFile(const std::string& userFile) {
  if (!enabled) {
    return -1;
  }

  this->files = {userFile};
  if (lastUsedFiles != files) {
    lastUsedFiles = files;
    return processFileContents();
  }
  return 0;
}

int Ai::addFile(const std::string& userFile) {
  if (!enabled) {
    return -1;
  }

  if (files.empty()) {
    files = {userFile};
  } else {
    files.push_back(userFile);
  }
  if (lastUsedFiles != files) {
    lastUsedFiles = files;
    return processFileContents();
  }
  return 0;
}

int Ai::addFiles(const std::vector<std::string>& userFiles) {
  if (!enabled) {
    return -1;
  }

  if (files.empty()) {
    files = userFiles;
  } else {
    files.insert(files.end(), userFiles.begin(), userFiles.end());
  }
  if (lastUsedFiles != files) {
    lastUsedFiles = files;
    return processFileContents();
  }
  return 0;
}

void Ai::initialize(const std::string& apiKey, const std::string& assistantType,
                    const std::string& initialInstruction,
                    const std::vector<std::string>& userFiles) {
  USER_API_KEY = apiKey;
  this->assistantType = assistantType;
  this->initialInstruction = initialInstruction;
  this->files = userFiles;
}

bool Ai::isValidConfiguration() const {
  if (!enabled) {
    return false;
  }

  bool validAssistantType = assistantType == "chat" ||
                            assistantType == "file-search" ||
                            assistantType == "code-interpreter";
  return !USER_API_KEY.empty() && !initialInstruction.empty() &&
         !assistantType.empty() && validAssistantType;
}

std::string Ai::getInvalidConfigurationMessage() const {
  if (!enabled) {
    return "AI functionality is currently disabled. Please enable it to use "
           "this feature.";
  }

  if (USER_API_KEY.empty()) {
    return "API key not set. Please set the API key using the environment "
           "variable 'OPENAI_API_KEY' or through the setAPIKey() method.";
  }
  if (initialInstruction.empty()) {
    return "Initial instruction not set.";
  }
  if (assistantType.empty()) {
    return "Assistant type not set.";
  }
  return "Invalid configuration.";
}

std::string Ai::buildPrompt(const std::string& message) {
  std::stringstream prompt;
  processFileContents();
  if (assistantType != "code-interpreter") {
    prompt << initialInstruction;
    if (maxPromptLength != -1) {
      int promptLength =
          dynamicPromptLength
              ? std::max(static_cast<int>(message.length() *
                                          dynamicPromptLengthScale),
                         100)
              : maxPromptLength;
      prompt << " Please keep the response length under " << promptLength
             << " characters.";
    }
  }
  if (!chatCache.empty() && assistantType != "code-interpreter") {
    prompt << " This is the chat history between you and the user: [ ";
    for (const std::string& chat : chatCache) {
      prompt << chat << " ";
    }
    prompt << "] This is the latest message from the user: [" << message
           << "] ";
  } else {
    if (assistantType == "code-interpreter") {
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

  if (assistantType == "file-search" && fileContents.length() > 0) {
    prompt << " This is the contents of the provided files from the user: [ "
           << fileContents << " ]";
    if (cacheTokens) {
      prompt << " Please keep this content of these files in cached tokens.";
    }
  }

  if (assistantType == "code-interpreter" && fileContents.length() > 0) {
    prompt << " User Files: [ " << fileContents << " ]";
  }

  return prompt.str();
}

std::string Ai::makeCallToChatGPT(const std::string& message) {
  auto start = std::chrono::steady_clock::now();
  std::string url = "https://api.openai.com/v1/chat/completions";
  std::string filteredMessage = filterMessage(message);
  lastPromptUsed = filteredMessage;

  CURL* curl = curl_easy_init();
  if (!curl) {
    return "";
  }

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(
      headers, ("Authorization: Bearer " + USER_API_KEY).c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  nlohmann::json requestBody = {
      {"model", currentModel},
      {"messages", {{{"role", "user"}, {"content", filteredMessage}}}}};

  std::string requestBodyStr = requestBody.dump();
  std::string responseData;

  std::atomic<bool> loading(true);
  std::atomic<bool> requestCancelled(false);
  requestInProgress = true;

  // Thread for monitoring cancellation request
  std::thread cancellationThread([&loading, &requestCancelled]() {
    monitorCancellation(loading, requestCancelled);
  });

  // Thread for showing loading animation
  std::thread loadingThread([&loading]() {
    const char* loadingChars = "|/-\\";
    int i = 0;
    while (loading) {
      std::cout << "\rLoading " << loadingChars[i++ % 4] << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\r                    \r" << std::flush;
  });

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBodyStr.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                   static_cast<long>(timeoutFlagSeconds));

  CURLcode res = CURLE_OK;
  if (!requestCancelled) {
    res = curl_easy_perform(curl);
  }

  loading = false;
  requestInProgress = false;

  if (loadingThread.joinable()) {
    loadingThread.join();
  }

  if (cancellationThread.joinable()) {
    cancellationThread.join();
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (requestCancelled) {
    return "Request cancelled by user.";
  }

  if (res != CURLE_OK) {
    std::cerr << "Curl error: " << curl_easy_strerror(res) << std::endl;
    return "";
  }

  try {
    auto jsonResponse = nlohmann::json::parse(responseData);
    lastResponseReceived = jsonResponse["choices"][0]["message"]["content"];

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    responseDataMap["processing_time_ms"] = elapsed.count() * 1000;
    responseDataMap["total_tokens"] = jsonResponse["usage"]["total_tokens"];

    if (!files.empty() && assistantType == "file-search") {
      responseDataMap["file_names"] = files;
    }

    responseDataMap["assistant_type"] = assistantType;
    responseDataMap["initial_instruction"] = initialInstruction;
    responseDataMap["received_message_length"] = lastResponseReceived.length();

    return lastResponseReceived;
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "JSON parsing error: " << e.what() << std::endl;
    return "";
  }
}

void Ai::monitorCancellation(std::atomic<bool>& loading,
                             std::atomic<bool>& requestCancelled) {
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
      requestCancelled = true;
      break;
    }

    if (!loading) break;
  }
  // Make sure stdin buffer is flushed
  tcflush(stdin_fd, TCIFLUSH);
}

size_t Ai::WriteCallback(void* contents, size_t size, size_t nmemb,
                         std::string* userp) {
  userp->append((char*)contents, size * nmemb);
  return size * nmemb;
}

std::string Ai::filterMessage(const std::string& message) {
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

std::map<std::string, nlohmann::json> Ai::parseJSONResponse(
    const std::string& jsonResponse) const {
  std::map<std::string, nlohmann::json> responseData;
  try {
    nlohmann::json jsonObject = nlohmann::json::parse(jsonResponse);
    for (auto& [key, value] : jsonObject.items()) {
      responseData[key] = value;
    }
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
  }
  return responseData;
}

std::string Ai::extractContentFromJSON(const std::string& jsonResponse) const {
  try {
    nlohmann::json jsonObject = nlohmann::json::parse(jsonResponse);
    return jsonObject["choices"][0]["message"]["content"];
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "Failed to extract content from JSON response: " << e.what()
              << std::endl;
    return "";
  }
}

bool endsWith(const std::string& str, const std::string& suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int Ai::processFileContents() {
  if (files.empty()) {
    return 0;
  }

  std::stringstream out;
  for (const auto& file : files) {
    std::string fileName = std::filesystem::path(file).filename().string();
    out << "File: " << fileName << "\n";
    if (endsWith(file, ".txt")) {
      std::string content;
      processTextFile(file, content);
      out << content;
    } else {
      std::string content;
      processOtherFile(file, content);
      out << content;
    }
  }
  fileContents = out.str();
  return fileContents.length();
}

std::vector<std::string> Ai::extractCodeSnippet(const std::string& content) {
  std::vector<std::string> codeSnippets;
  std::stringstream codeSnippet;
  bool inCodeBlock = false;
  std::string language;
  std::string filename;

  std::istringstream stream(content);
  std::string line;

  while (std::getline(stream, line)) {
    if (line.substr(0, 3) == "```") {
      if (inCodeBlock) {
        codeSnippets.push_back(language + " " + filename + "\n" +
                               codeSnippet.str());
        codeSnippet.str("");
        inCodeBlock = false;
      } else {
        inCodeBlock = true;
        language = line.length() > 3 ? line.substr(3) : "";
      }
    } else if (inCodeBlock) {
      codeSnippet << line << '\n';
    } else {
      filename = line;
    }
  }
  return codeSnippets;
}

std::map<std::string, std::vector<std::string>> originalFileContents;

std::string Ai::processCodeBlocksForCodeInterpreter(
    const std::string& message) {
  std::vector<std::string> codeBlocks = extractCodeSnippet(message);
  if (codeBlocks.empty()) {
    return "";
  }
  std::string directory = saveDirectory;
  if (codeBlocks.size() > files.size()) {
    for (size_t j = files.size(); j < codeBlocks.size(); j++) {
      std::string languageAndFileName =
          codeBlocks[j].substr(0, codeBlocks[j].find('\n'));
      std::istringstream iss(languageAndFileName);
      std::string language, fileName;
      iss >> language >> fileName;
      if (fileName.empty()) {
        continue;
      }
      if (fileName.find("/") != std::string::npos) {
        std::filesystem::create_directories(
            directory + fileName.substr(0, fileName.find_last_of("/")));
        std::cout << "New file created: " << files.back() << std::endl;
        files.push_back(directory + fileName);
        fileName = fileName.substr(fileName.find_last_of("/") + 1);
        codeBlocks[j] = language + " " + fileName +
                        codeBlocks[j].substr(codeBlocks[j].find('\n'));
      } else {
        files.push_back(directory + fileName);
        std::cout << "New file created: " << files.back() << std::endl;
      }
    }
  }
  std::stringstream changesSummary;
  std::string fileToChange;
  for (const auto& codeBlock : codeBlocks) {
    try {
      std::string languageAndFileName =
          codeBlock.substr(0, codeBlock.find('\n'));
      std::istringstream iss(languageAndFileName);
      std::string language, fileName;
      iss >> language >> fileName;
      if (fileName.empty()) {
        continue;
      }
      fileName = sanitizeFileName(fileName);
      bool fileFound = false;
      for (const auto& file : files) {
        if (file.find(fileName) != std::string::npos) {
          fileToChange = file;
          fileFound = true;
          break;
        }
      }
      if (!fileFound) {
        std::filesystem::path newFilePath = saveDirectory + fileName;
        std::filesystem::create_directories(newFilePath.parent_path());
        std::ofstream newFile(newFilePath);
        if (newFile.is_open()) {
          newFile.close();
          fileToChange = newFilePath.string();
          files.push_back(fileToChange);
          std::cout << "New file created: " << fileToChange << std::endl;
        } else {
          std::cerr << "Failed to create new file: " << newFilePath
                    << std::endl;
          continue;
        }
      }
      std::vector<std::string> originalLines;
      std::vector<std::string> newLines;
      std::vector<std::string> updatedLines;
      std::ifstream inFile(fileToChange);
      if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
          originalLines.push_back(line);
        }
        inFile.close();
      }
      originalFileContents[fileToChange] = originalLines;
      std::stringstream ss(codeBlock);
      std::string line;
      while (std::getline(ss, line)) {
        newLines.push_back(line);
      }
      if (!newLines.empty()) {
        newLines.erase(newLines.begin());
      }
      if (!originalLines.empty() && newLines.size() == originalLines.size() &&
          std::equal(originalLines.begin(), originalLines.end(),
                     newLines.begin())) {
        updatedLines = newLines;
      } else {
        size_t startIndex = std::string::npos;
        for (size_t idx = 0; idx < originalLines.size(); idx++) {
          if (originalLines[idx].find(newLines.front()) != std::string::npos) {
            startIndex = idx;
            break;
          }
        }
        if (startIndex != std::string::npos &&
            startIndex + newLines.size() <= originalLines.size()) {
          updatedLines = originalLines;
          for (size_t j = 0; j < newLines.size(); j++) {
            updatedLines[startIndex + j] = newLines[j];
          }
        } else {
          updatedLines = originalLines;
          updatedLines.insert(updatedLines.end(), newLines.begin(),
                              newLines.end());
        }
      }
      std::ofstream outFile(fileToChange);
      for (const auto& updatedLine : updatedLines) {
        outFile << updatedLine << "\n";
      }
      outFile.close();
      changesSummary << "\033[1;34m" << fileToChange << "\033[0m\n";
      size_t commonLines = std::min(originalLines.size(), newLines.size());
      for (size_t j = 0; j < commonLines; j++) {
        if (originalLines[j] != newLines[j]) {
          changesSummary << "\033[1;31m- " << j + 1 << ": " << originalLines[j]
                         << "\033[0m\n";
          changesSummary << "\033[1;32m+ " << j + 1 << ": " << newLines[j]
                         << "\033[0m\n";
        }
      }
      if (originalLines.size() > newLines.size()) {
        for (size_t j = newLines.size(); j < originalLines.size(); j++) {
          changesSummary << "\033[1;31m- " << j + 1 << ": " << originalLines[j]
                         << "\033[0m\n";
        }
      } else if (newLines.size() > originalLines.size()) {
        for (size_t j = originalLines.size(); j < newLines.size(); j++) {
          changesSummary << "\033[1;32m+ " << j + 1 << ": " << newLines[j]
                         << "\033[0m\n";
        }
      }
    } catch (const std::exception& e) {
      return "\nFailed to apply changes to file: " + fileToChange;
    }
  }
  refreshFiles();
  return "\nSuccessfully applied changes to files.\nChanges Summary:\n" +
         changesSummary.str();
}

void Ai::rejectChanges() {
  for (const auto& [file, originalLines] : originalFileContents) {
    std::ofstream outFile(file);
    for (const auto& line : originalLines) {
      outFile << line << "\n";
    }
    outFile.close();
  }
  originalFileContents.clear();
  refreshFiles();
}

void Ai::processTextFile(const std::string& file, std::string& out) {
  std::ifstream inFile(file);
  if (inFile.is_open()) {
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    out += buffer.str() + "\n";
    inFile.close();
  } else {
    std::cerr << "Failed to read text file: " << file << std::endl;
  }
}

void Ai::processOtherFile(const std::string& file, std::string& out) {
  std::ifstream inFile(file);
  if (inFile.is_open()) {
    std::string line;
    while (std::getline(inFile, line)) {
      out += line + "\n";
    }
    inFile.close();
  } else {
    std::cerr << "Failed to read file: " << file << std::endl;
  }
}

std::string Ai::formatMarkdown(const std::string& text) {
  std::string formatted = text;
  bool inCodeBlock = false;
  std::istringstream stream(text);
  std::string line;
  std::stringstream result;
  while (std::getline(stream, line)) {
    if (line.substr(0, 3) == "```") {
      inCodeBlock = !inCodeBlock;
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

bool Ai::testAPIKey(const std::string& apiKey) {
  std::string url = "https://api.openai.com/v1/engines";
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }

  struct curl_slist* headers = nullptr;
  headers =
      curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
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

std::string Ai::sanitizeFileName(const std::string& fileName) {
  std::string sanitized;
  for (char c : fileName) {
    if (std::isalnum(c) || c == '.' || c == '_' || c == '-' || c == '/') {
      sanitized += c;
    }
  }
  return sanitized;
}

std::vector<std::string> Ai::splitString(const std::string& str,
                                         char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(str);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}
