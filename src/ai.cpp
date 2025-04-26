#include "ai.h"

/**
 * @brief Constructs an Ai instance with the specified API key, assistant type, and initial instruction.
 *
 * Initializes the AI with the provided configuration and no user files.
 *
 * @param apiKey The OpenAI API key.
 * @param assistantType The type of assistant to use (e.g., "chat", "file-search", "code-interpreter").
 * @param initialInstruction The initial instruction or system prompt for the assistant.
 */
Ai::Ai(const std::string& apiKey, const std::string& assistantType, const std::string& initialInstruction) {
    initialize(apiKey, assistantType, initialInstruction, {});
}

/**
 * @brief Constructs an Ai instance with API key, assistant type, initial instruction, and user files.
 *
 * Initializes the AI with the provided API key, assistant type, initial instruction, and a list of user files for context or code editing.
 */
Ai::Ai(const std::string& apiKey, const std::string& assistantType, const std::string& initialInstruction, const std::vector<std::string>& userFiles) {
    initialize(apiKey, assistantType, initialInstruction, userFiles);
}

/**
 * @brief Constructs an Ai instance with API key, assistant type, initial instruction, user files, and save directory.
 *
 * Initializes the AI with the provided configuration and sets the directory for saving files.
 *
 * @param apiKey OpenAI API key.
 * @param assistantType Type of assistant to use (e.g., "chat", "file-search", "code-interpreter").
 * @param initialInstruction Initial instruction or system prompt for the assistant.
 * @param userFiles List of user file paths to include in the session.
 * @param saveDirectory Directory path where files and outputs will be saved.
 */
Ai::Ai(const std::string& apiKey, const std::string& assistantType, const std::string& initialInstruction, const std::vector<std::string>& userFiles, const std::string& saveDirectory) {
    initialize(apiKey, assistantType, initialInstruction, userFiles);
    setSaveDirectory(saveDirectory);
}

/**
 * @brief Constructs an uninitialized Ai instance.
 *
 * The default constructor creates an Ai object without setting any configuration or API credentials. Initialization must be performed before use.
 */
Ai::Ai() {}

/**
 * @brief Sets the API key used for authenticating requests to the OpenAI API.
 *
 * Replaces the current API key with the provided value for subsequent API interactions.
 */
void Ai::setAPIKey(const std::string& apiKey) {
    USER_API_KEY = apiKey;
}

/**
 * @brief Returns the current API key used for OpenAI requests.
 *
 * @return The API key as a string.
 */
std::string Ai::getAPIKey() const {
    return USER_API_KEY;
}

/**
 * @brief Sets the initial instruction for the AI assistant.
 *
 * Updates the instruction that guides the assistant's behavior in subsequent interactions.
 */
void Ai::setInitialInstruction(const std::string& instruction) {
    initialInstruction = instruction;
}

/**
 * @brief Returns the initial instruction configured for the AI assistant.
 *
 * @return The initial instruction string used to guide the assistant's behavior.
 */
std::string Ai::getInitialInstruction() const {
    return initialInstruction;
}

/**
 * @brief Sets the assistant type for the AI instance.
 *
 * The assistant type determines the behavior and capabilities of the AI, such as "chat", "file-search", or "code-interpreter".
 */
void Ai::setAssistantType(const std::string& assistantType) {
    this->assistantType = assistantType;
}

/**
 * @brief Returns the current assistant type used by the AI instance.
 *
 * @return The assistant type as a string.
 */
std::string Ai::getAssistantType() const {
    return assistantType;
}

/**
 * @brief Sets the maximum allowed length for prompts sent to the AI model.
 *
 * Adjusts the prompt length limit, which can affect how much context or information is included in each API request.
 */
void Ai::setMaxPromptLength(int maxPromptLength) {
    this->maxPromptLength = maxPromptLength;
}

/**
 * @brief Returns the maximum allowed prompt length for API requests.
 *
 * @return The maximum number of characters permitted in a prompt.
 */
int Ai::getMaxPromptLength() const {
    return maxPromptLength;
}

/**
 * @brief Enables or disables token caching for API responses.
 *
 * @param cacheTokens If true, token usage data will be cached; if false, it will not be cached.
 */
void Ai::setCacheTokens(bool cacheTokens) {
    this->cacheTokens = cacheTokens;
}

/**
 * @brief Returns whether token caching is enabled for the AI instance.
 *
 * @return True if token caching is enabled; otherwise, false.
 */
bool Ai::getCacheTokens() const {
    return cacheTokens;
}

/**
 * @brief Toggles the cache tokens setting on or off.
 *
 * Switches the internal flag that determines whether token usage is cached.
 */
void Ai::toggleCacheTokens() {
    cacheTokens = !cacheTokens;
}

/**
 * @brief Sends a request to the AI to clear all cached tokens.
 *
 * This method prompts the AI backend to clear any cached tokens it may be storing.
 */
void Ai::clearAllCachedTokens() {
    makeCallToChatGPT("Clear all cached tokens.");
}

/**
 * @brief Returns the list of user file paths currently managed by the AI instance.
 *
 * @return Vector of file path strings.
 */
std::vector<std::string> Ai::getFiles() const {
    return files;
}

/**
 * @brief Returns the concatenated contents of all user files managed by the AI instance.
 *
 * The returned string includes the contents of all files currently tracked, with file headers and formatting as processed by the class.
 * @return Concatenated file contents as a single string.
 */
std::string Ai::getFileContents() const {
    return fileContents;
}

/**
 * @brief Returns the current chat history cache.
 *
 * @return A vector containing the cached chat messages.
 */
std::vector<std::string> Ai::getChatCache() const {
    return chatCache;
}

/**
 * @brief Replaces the current chat history cache with the provided messages.
 *
 * @param chatCache Vector of chat messages to set as the new chat history.
 */
void Ai::setChatCache(const std::vector<std::string>& chatCache) {
    this->chatCache = chatCache;
}

/**
 * @brief Removes all messages from the chat history cache.
 */
void Ai::clearChatCache() {
    chatCache.clear();
}

/**
 * @brief Enables or disables dynamic scaling of the prompt length.
 *
 * When enabled, the prompt length is adjusted dynamically based on the context or scaling factor.
 */
void Ai::setDynamicPromptLength(bool dynamicPromptLength) {
    this->dynamicPromptLength = dynamicPromptLength;
}

/**
 * @brief Checks if dynamic prompt length adjustment is enabled.
 *
 * @return True if dynamic prompt length is enabled; otherwise, false.
 */
bool Ai::getDynamicPromptLength() const {
    return dynamicPromptLength;
}

/**
 * @brief Toggles the dynamic prompt length setting on or off.
 *
 * Switches the dynamic prompt length feature between enabled and disabled states.
 */
void Ai::toggleDynamicPromptLength() {
    dynamicPromptLength = !dynamicPromptLength;
}

/**
 * @brief Appends a chat message to the chat history cache.
 *
 * Adds the provided chat message to the end of the internal chat cache for maintaining conversation context.
 *
 * @param chat The chat message to add to the cache.
 */
void Ai::addChatToCache(const std::string& chat) {
    chatCache.push_back(chat);
}

/**
 * @brief Retrieves the stored JSON response data for a given key.
 *
 * @param key The key identifying the desired response data.
 * @return A string containing the JSON value associated with the key, or an empty string if the key is not found.
 */
std::string Ai::getResponseData(const std::string& key) const {
    auto it = responseDataMap.find(key);
    if (it != responseDataMap.end()) {
        return it->second.dump();
    }
    return "";
}

/**
 * @brief Returns the most recent prompt sent to the ChatGPT API.
 *
 * @return The last prompt string used in an API request.
 */
std::string Ai::getLastPromptUsed() const {
    return lastPromptUsed;
}

/**
 * @brief Removes a specified file from the list of managed user files.
 *
 * If the file is present in the list, it is removed; otherwise, no action is taken.
 *
 * @param userFile The name or path of the file to remove.
 */
void Ai::removeFile(const std::string& userFile) {
    files.erase(std::remove(files.begin(), files.end(), userFile), files.end());
}

/**
 * @brief Removes all user files from the current file list.
 *
 * After calling this method, the AI instance will have no associated user files.
 */
void Ai::clearFiles() {
    files.clear();
}

/**
 * @brief Reloads the current list of user files and updates their cached contents.
 *
 * This method reprocesses all files currently tracked by the AI instance, ensuring that any external changes to file contents are reflected in the internal cache.
 */
void Ai::refreshFiles() {
    std::vector<std::string> activeFiles = getFiles();
    clearFiles();
    setFiles(activeFiles);
}

/**
 * @brief Returns the most recent response received from the AI.
 *
 * @return The last response string received from the ChatGPT API.
 */
std::string Ai::getLastResponseReceived() const {
    return lastResponseReceived;
}

/**
 * @brief Sets whether to enable maximum prompt precision mode.
 *
 * When enabled, the AI will attempt to fit responses within the maximum prompt length by retrying with truncated chat history if necessary.
 *
 * @param maxPromptPrecision True to enable maximum prompt precision, false to disable.
 */
void Ai::setMaxPromptPrecision(bool maxPromptPrecision) {
    this->maxPromptPrecision = maxPromptPrecision;
}

/**
 * @brief Returns whether maximum prompt precision is enabled.
 *
 * When enabled, the prompt length is adjusted to maximize the use of available tokens for more precise responses.
 *
 * @return True if maximum prompt precision is enabled; otherwise, false.
 */
bool Ai::getMaxPromptPrecision() const {
    return maxPromptPrecision;
}

/**
 * @brief Toggles the maximum prompt precision setting.
 *
 * Switches the maxPromptPrecision flag between enabled and disabled states.
 */
void Ai::toggleMaxPromptPrecision() {
    maxPromptPrecision = !maxPromptPrecision;
}

/**
 * @brief Sets the timeout duration for API requests in seconds.
 *
 * Adjusts how long the AI will wait for a response from the API before timing out.
 *
 * @param timeoutFlagSeconds Timeout duration in seconds.
 */
void Ai::setTimeoutFlagSeconds(float timeoutFlagSeconds) {
    this->timeoutFlagSeconds = timeoutFlagSeconds;
}

/**
 * @brief Returns the timeout duration in seconds for API requests.
 *
 * @return Timeout value in seconds.
 */
float Ai::getTimeoutFlagSeconds() const {
    return timeoutFlagSeconds;
}

/**
 * @brief Sets the name of the model to be used for API requests.
 *
 * @param model The model identifier (e.g., "gpt-3.5-turbo").
 */
void Ai::setModel(const std::string& model) {
    currentModel = model;
}

/**
 * @brief Returns the name of the current OpenAI model in use.
 *
 * @return The model name as a string.
 */
std::string Ai::getModel() const {
    return currentModel;
}

/**
 * @brief Sets the scaling factor for dynamic prompt length adjustment.
 *
 * Adjusts how much the prompt length can scale dynamically when building prompts for the AI model.
 *
 * @param dynamicPromptLengthScale The scaling factor to apply for dynamic prompt length.
 */
void Ai::setDynamicPromptLengthScale(float dynamicPromptLengthScale) {
    this->dynamicPromptLengthScale = dynamicPromptLengthScale;
}

/**
 * @brief Returns the scaling factor used for dynamic prompt length adjustment.
 *
 * @return The current dynamic prompt length scale.
 */
float Ai::getDynamicPromptLengthScale() const {
    return dynamicPromptLengthScale;
}

/**
 * @brief Sets the directory path where files will be saved, ensuring it ends with a trailing slash.
 *
 * If the provided directory path does not end with '/', one is appended automatically.
 */
void Ai::setSaveDirectory(const std::string& directory) {
    if(directory.back() == '/') {
        saveDirectory = directory;
    } else {
        saveDirectory = directory + "/";
    }
}

/**
 * @brief Returns the current directory path used for saving files.
 *
 * @return The save directory path as a string.
 */
std::string Ai::getSaveDirectory() const {
    return saveDirectory;
}

/**
 * @brief Sends a message to the ChatGPT API and returns the AI's response.
 *
 * Constructs a prompt using the provided message and current configuration, sends it to the ChatGPT API, and returns the AI's reply. Maintains chat history for non-code-interpreter assistant types. For the "code-interpreter" assistant type, processes and applies code blocks from the response to user files. Optionally formats the response as markdown.
 *
 * @param message The user's message to send to the AI.
 * @param format If true, formats the AI's response as markdown.
 * @return The AI's response, optionally formatted.
 */
std::string Ai::chatGPT(const std::string& message, bool format) {
    if (!isValidConfiguration()) {
        return getInvalidConfigurationMessage();
    }

    std::string response = makeCallToChatGPT(buildPrompt(message));
    
    if (maxPromptPrecision && maxPromptLength > 0 && response.length() > static_cast<std::string::size_type>(maxPromptLength)) {
        return chatGPT(message, format);
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

/**
 * @brief Sends a message directly to the ChatGPT API and returns the response.
 *
 * Bypasses prompt construction and chat history, sending the provided message as-is to the API. Optionally formats the response as markdown.
 *
 * @param message The message to send to ChatGPT.
 * @param format If true, formats the response as markdown.
 * @return The raw or formatted response from ChatGPT.
 */
std::string Ai::forceDirectChatGPT(const std::string& message, bool format) {
    std::string response = makeCallToChatGPT(message);
    return format ? formatMarkdown(response) : response;
}

/**
 * @brief Sets the list of user files and processes their contents if changed.
 *
 * Updates the internal file list and, if the new list differs from the previous one, processes the contents of all files. Returns the result of the file processing operation.
 *
 * @param userFiles Vector of file paths to set as the current file list.
 * @return int Result code from processing file contents; 0 if the file list is unchanged.
 */
int Ai::setFiles(const std::vector<std::string>& userFiles) {
    this->files = userFiles;
    if (lastUsedFiles != files) {
        lastUsedFiles = files;
        return processFileContents();
    }
    return 0;
}

/**
 * @brief Sets a single user file for the AI instance and processes its contents.
 *
 * Replaces the current file list with the specified file. If the file differs from the previously used file, its contents are processed and cached.
 *
 * @param userFile Path to the user file to set.
 * @return int Returns 0 if the file is unchanged, or the result of processing the file contents if changed.
 */
int Ai::setFile(const std::string& userFile) {
    this->files = {userFile};
    if (lastUsedFiles != files) {
        lastUsedFiles = files;
        return processFileContents();
    }
    return 0;
}

/**
 * @brief Adds a user file to the list of managed files and processes its contents if the file list has changed.
 *
 * If the file list was previously empty, the new file replaces the list; otherwise, it is appended. If this results in a change to the file list, the contents of all files are reprocessed.
 *
 * @param userFile Path to the user file to add.
 * @return int Status code from processing file contents, or 0 if no changes were made.
 */
int Ai::addFile(const std::string& userFile) {
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

/**
 * @brief Adds multiple user files to the current file list and processes their contents if the list changes.
 *
 * If the file list is initially empty, it is replaced with the provided files. Otherwise, the new files are appended.
 * If the resulting file list differs from the previous state, file contents are reprocessed.
 *
 * @param userFiles Vector of file paths to add.
 * @return int Result of processing file contents if the file list changed, otherwise 0.
 */
int Ai::addFiles(const std::vector<std::string>& userFiles) {
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

/**
 * @brief Initializes the AI instance with API key, assistant type, initial instruction, and user files.
 *
 * Sets the core configuration parameters required for the AI to interact with the ChatGPT API and manage user files.
 */
void Ai::initialize(const std::string& apiKey, const std::string& assistantType, const std::string& initialInstruction, const std::vector<std::string>& userFiles) {
    USER_API_KEY = apiKey;
    this->assistantType = assistantType;
    this->initialInstruction = initialInstruction;
    this->files = userFiles;
}

/**
 * @brief Checks if the AI configuration is valid for making API requests.
 *
 * The configuration is considered valid if the API key, initial instruction, and assistant type are all set and the assistant type is one of "chat", "file-search", or "code-interpreter".
 *
 * @return true if the configuration is valid; false otherwise.
 */
bool Ai::isValidConfiguration() const {
    bool validAssistantType = assistantType == "chat" || assistantType == "file-search" || assistantType == "code-interpreter";
    return !USER_API_KEY.empty() && !initialInstruction.empty() && !assistantType.empty() && validAssistantType;
}

/**
 * @brief Returns a message describing the first missing required configuration for the AI instance.
 *
 * Checks if the API key, initial instruction, or assistant type are unset and returns a corresponding error message. If all are set, returns a generic invalid configuration message.
 *
 * @return std::string Description of the missing configuration or a generic error.
 */
std::string Ai::getInvalidConfigurationMessage() const {
    if (USER_API_KEY.empty()) {
        return "API key not set.";
    }
    if (initialInstruction.empty()) {
        return "Initial instruction not set.";
    }
    if (assistantType.empty()) {
        return "Assistant type not set.";
    }
    return "Invalid configuration.";
}

/**
 * @brief Constructs a prompt string for the ChatGPT API based on the assistant type, user message, chat history, and file contents.
 *
 * Builds a contextually appropriate prompt by incorporating the initial instruction, chat history, user message, and relevant file contents. The prompt adapts its structure and length constraints according to the assistant type ("chat", "file-search", or "code-interpreter") and configuration settings such as dynamic prompt length and token caching.
 *
 * @param message The latest user message to include in the prompt.
 * @return std::string The fully constructed prompt string to be sent to the ChatGPT API.
 */
std::string Ai::buildPrompt(const std::string& message) {
    std::stringstream prompt;
    processFileContents();
    if(assistantType != "code-interpreter"){
        prompt << initialInstruction;
        if (maxPromptLength != -1) {
            int promptLength = dynamicPromptLength ? std::max(static_cast<int>(message.length() * dynamicPromptLengthScale), 100) : maxPromptLength;
            prompt << " Please keep the response length under " << promptLength << " characters.";
        }
    }
    if (!chatCache.empty() && assistantType != "code-interpreter") {
        prompt << " This is the chat history between you and the user: [ ";
        for (const std::string& chat : chatCache) {
            prompt << chat << " ";
        }
        prompt << "] This is the latest message from the user: [" << message << "] ";
    } else {
        if (assistantType == "code-interpreter") {
            prompt << message << "Please only return code in your response if edits were made and only make edits that the I request.  Please use markdown syntax in your response for the code. Include only the exact file name and only the file name in the line above. " ;
        } else {
            prompt << " This is the first message from the user: [" << message << "] ";
        }
    }

    if (assistantType == "file-search" && fileContents.length() > 0) {
        prompt << " This is the contents of the provided files from the user: [ " << fileContents << " ]";
        if (cacheTokens) {
            prompt << " Please keep this content of these files in cached tokens.";
        }
    }

    if (assistantType == "code-interpreter" && fileContents.length() > 0) {
        prompt << " User Files: [ " << fileContents << " ]";
    }

    return prompt.str();
}

/**
 * @brief Sends a prompt to the OpenAI ChatGPT API and returns the AI's response.
 *
 * Constructs and sends a POST request to the OpenAI Chat Completions API using the provided message, then parses and returns the AI-generated reply. Also updates internal metadata such as processing time, token usage, and response details. Returns an empty string if the request fails or the response cannot be parsed.
 *
 * @param message The user prompt to send to ChatGPT.
 * @return The AI-generated response as a string, or an empty string on error.
 */
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
    headers = curl_slist_append(headers, ("Authorization: Bearer " + USER_API_KEY).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    nlohmann::json requestBody = {
        {"model", currentModel},
        {"messages", {{
            {"role", "user"},
            {"content", filteredMessage}
        }}}
    };

    std::string requestBodyStr = requestBody.dump();
    std::string responseData;

    std::atomic<bool> loading(true);
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutFlagSeconds));

    CURLcode res = curl_easy_perform(curl);
    
    loading = false;
    if (loadingThread.joinable()) {
        loadingThread.join();
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

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

/**
 * @brief Callback function for libcurl to write received data into a string.
 *
 * Appends the data received from a curl request to the provided string buffer.
 *
 * @param contents Pointer to the received data.
 * @param size Size of each data element.
 * @param nmemb Number of data elements.
 * @param userp Pointer to the string buffer to append data to.
 * @return The total number of bytes processed.
 */
size_t Ai::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

/**
 * @brief Removes disallowed characters from a message, allowing only alphanumeric characters, spaces, hyphens, underscores, periods, and tildes.
 *
 * Newline characters are replaced with spaces in the filtered message.
 *
 * @param message The input string to be filtered.
 * @return The filtered string containing only allowed characters.
 */
std::string Ai::filterMessage(const std::string& message) {
    std::string filtered = message;
    filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
        [](char c) { 
            return !(std::isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.' || c == '~');
        }), filtered.end());
    
    std::replace(filtered.begin(), filtered.end(), '\n', ' ');
    return filtered;
}

/**
 * @brief Parses a JSON response string into a map of keys to JSON values.
 *
 * Attempts to parse the input JSON string and returns a map where each key corresponds to a top-level field in the JSON object. If parsing fails, an error is logged and an empty map is returned.
 *
 * @param jsonResponse The JSON response as a string.
 * @return std::map<std::string, nlohmann::json> Map of top-level keys to their JSON values.
 */
std::map<std::string, nlohmann::json> Ai::parseJSONResponse(const std::string& jsonResponse) const {
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

/**
 * @brief Extracts the message content from the first choice in a ChatGPT JSON response.
 *
 * Parses the provided JSON string and retrieves the value of `content` from the first element in the `choices` array. Returns an empty string if parsing fails or the expected fields are missing.
 *
 * @param jsonResponse The JSON response string from the ChatGPT API.
 * @return The extracted message content, or an empty string on error.
 */
std::string Ai::extractContentFromJSON(const std::string& jsonResponse) const {
    try {
        nlohmann::json jsonObject = nlohmann::json::parse(jsonResponse);
        return jsonObject["choices"][0]["message"]["content"];
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Failed to extract content from JSON response: " << e.what() << std::endl;
        return "";
    }
}

/**
 * @brief Checks if a string ends with the specified suffix.
 *
 * @param str The string to check.
 * @param suffix The suffix to look for.
 * @return true if str ends with suffix, false otherwise.
 */
bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/**
 * @brief Reads and concatenates the contents of all tracked files, prefixing each with its filename.
 *
 * For each file in the current file list, reads its contents (using text or line-by-line processing depending on file type), prefixes the content with a "File: <filename>" header, and appends it to the internal file contents buffer.
 *
 * @return The total length of the concatenated file contents string.
 */
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

/**
 * @brief Extracts code snippets from a string containing code blocks.
 *
 * Parses the input content to find code blocks delimited by triple backticks (```), optionally capturing the language and filename. Each extracted code snippet is returned as a string prefixed with its language and filename.
 *
 * @param content The input string potentially containing code blocks.
 * @return std::vector<std::string> A vector of extracted code snippets, each with language and filename headers.
 */
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
                codeSnippets.push_back(language + " " + filename + "\n" + codeSnippet.str());
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

/**
 * @brief Applies code blocks from an AI response to corresponding files, updating or creating files as needed.
 *
 * Parses code blocks from the provided message, matches them to existing files or creates new ones, and updates file contents accordingly. Tracks original file contents for potential rollback and summarizes changes with line-by-line diffs. Returns a summary of the changes applied or an error message if a failure occurs.
 *
 * @param message The AI response containing code blocks to apply.
 * @return std::string Summary of changes applied to files, or an error message on failure.
 */
std::string Ai::processCodeBlocksForCodeInterpreter(const std::string& message) {
    std::vector<std::string> codeBlocks = extractCodeSnippet(message);
    if (codeBlocks.empty()) {
        return "";
    }
    std::string directory = saveDirectory;
    if (codeBlocks.size() > files.size()) {
        for (size_t j = files.size(); j < codeBlocks.size(); j++) {
            std::string languageAndFileName = codeBlocks[j].substr(0, codeBlocks[j].find('\n'));
            std::istringstream iss(languageAndFileName);
            std::string language, fileName;
            iss >> language >> fileName;
            if (fileName.empty()) {
                continue;
            }
            if (fileName.find("/") != std::string::npos) {
                std::filesystem::create_directories(directory + fileName.substr(0, fileName.find_last_of("/")));
                std::cout << "New file created: " << files.back() << std::endl;
                files.push_back(directory + fileName);
                fileName = fileName.substr(fileName.find_last_of("/") + 1);
                codeBlocks[j] = language + " " + fileName + codeBlocks[j].substr(codeBlocks[j].find('\n'));
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
            std::string languageAndFileName = codeBlock.substr(0, codeBlock.find('\n'));
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
                    std::cerr << "Failed to create new file: " << newFilePath << std::endl;
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
                std::equal(originalLines.begin(), originalLines.end(), newLines.begin())) {
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
                    updatedLines.insert(updatedLines.end(), newLines.begin(), newLines.end());
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
                    changesSummary << "\033[1;31m- " << j + 1 << ": " << originalLines[j] << "\033[0m\n";
                    changesSummary << "\033[1;32m+ " << j + 1 << ": " << newLines[j] << "\033[0m\n";
                }
            }
            if (originalLines.size() > newLines.size()) {
                for (size_t j = newLines.size(); j < originalLines.size(); j++) {
                    changesSummary << "\033[1;31m- " << j + 1 << ": " << originalLines[j] << "\033[0m\n";
                }
            } else if (newLines.size() > originalLines.size()) {
                for (size_t j = originalLines.size(); j < newLines.size(); j++) {
                    changesSummary << "\033[1;32m+ " << j + 1 << ": " << newLines[j] << "\033[0m\n";
                }
            }
        } catch (const std::exception& e) {
            return "\nFailed to apply changes to file: " + fileToChange;
        }
    }
    refreshFiles();
    return "\nSuccessfully applied changes to files.\nChanges Summary:\n" + changesSummary.str();
}

/**
 * @brief Reverts all modified files to their original contents before the last code change.
 *
 * Restores each file tracked in the original file contents cache to its previous state, clears the cache, and refreshes the file list.
 */
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

/**
 * @brief Reads the entire contents of a text file and appends it to the output string.
 *
 * If the file cannot be opened, an error message is printed to standard error.
 *
 * @param file Path to the text file to read.
 * @param out String to which the file contents will be appended.
 */
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

/**
 * @brief Reads the contents of a non-text file line by line and appends them to the output string.
 *
 * Each line from the specified file is appended to the provided output string, separated by newlines.
 * If the file cannot be opened, an error message is printed to standard error.
 *
 * @param file Path to the file to be read.
 * @param out String to which the file's contents will be appended.
 */
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

/**
 * @brief Formats markdown text by removing certain markdown syntax outside code blocks.
 *
 * Strips bold, italic, and header formatting from lines that are not within code blocks, preserving code block content and structure.
 *
 * @param text The markdown-formatted input string.
 * @return std::string The formatted string with selected markdown syntax removed.
 */
std::string Ai::formatMarkdown(const std::string& text) {
    std::string formatted = text;
    bool inCodeBlock = false;
    std::istringstream stream(text);
    std::string line;
    std::stringstream result;
    while (std::getline(stream, line)) {
        if (line.substr(0, 3) == "```") {
            inCodeBlock = !inCodeBlock;
        } else if (line.substr(0, 2) == "**" && line.substr(line.length() - 2) == "**") {
            result << line.substr(2, line.length() - 4) << "\n";
        } else if (line.substr(0, 1) == "*" && line.substr(line.length() - 1) == "*") {
            result << line.substr(1, line.length() - 2) << "\n";
        } else if (line.substr(0, 2) == "# ") {
            result << line.substr(2) << "\n";
        } else {
            result << line << "\n";
        }
    }

    return result.str();
}

/**
 * @brief Verifies the validity of an OpenAI API key.
 *
 * Sends a HEAD request to the OpenAI API and returns true if the API key is valid and authorized.
 *
 * @param apiKey The OpenAI API key to test.
 * @return true if the API key is valid and accepted by the OpenAI API, false otherwise.
 */
bool Ai::testAPIKey(const std::string& apiKey) {
    std::string url = "https://api.openai.com/v1/engines";
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
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

/**
 * @brief Removes invalid characters from a file name.
 *
 * Returns a sanitized version of the input file name, containing only alphanumeric characters, dots, underscores, hyphens, and slashes.
 *
 * @param fileName The original file name to sanitize.
 * @return std::string The sanitized file name.
 */
std::string Ai::sanitizeFileName(const std::string& fileName) {
    std::string sanitized;
    for (char c : fileName) {
        if (std::isalnum(c) || c == '.' || c == '_' || c == '-' || c == '/') {
            sanitized += c;
        }
    }
    return sanitized;
}

/**
 * @brief Splits a string into a vector of substrings using the specified delimiter.
 *
 * @param str The input string to split.
 * @param delimiter The character used to separate substrings.
 * @return std::vector<std::string> A vector containing the split substrings.
 */
std::vector<std::string> Ai::splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}
