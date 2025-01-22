#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <future>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

class OpenAIProcess {
private:
    std::string USER_API_KEY;
    std::string lastPromptUsed;
    std::string lastResponseReceived;
    std::vector<std::string> chatCache;
    std::map<std::string, nlohmann::json> responseDataMap;

    std::string chatGPT(const std::string& passedMessage, bool usingChatCache);
    static std::string filterMessage(const std::string& message);
    std::map<std::string, nlohmann::json> parseJSONResponse(const std::string& jsonResponse);
    std::string extractContentFromJSON(const std::string& jsonResponse);

public:
    OpenAIProcess(const std::string& apiKey);
    OpenAIProcess();

    std::string buildPromptAndReturnResponse(const std::string& message, bool usingChatCache);
    void buildPromptAndReturnNoResponse(const std::string& message, bool usingChatCache);
    bool testAPIKey(const std::string& apiKey);
    void setAPIKey(const std::string& apiKey);
    std::string getAPIKey() const;
    std::string getLastPromptUsed() const;
    std::string getLastResponseReceived() const;
    std::vector<std::string> getChatCache() const;
    void clearChatCache();
    void setChatCache(const std::vector<std::string>& chatCache);
    std::string getResponseData(const std::string& key) const;
};

OpenAIProcess::OpenAIProcess(const std::string& apiKey) : USER_API_KEY(apiKey) {}

OpenAIProcess::OpenAIProcess() {}

std::string OpenAIProcess::buildPromptAndReturnResponse(const std::string& message, bool usingChatCache) {
    if (USER_API_KEY.empty()) {
        return "API key not set.";
    }
    if (message.empty()) {
        return "User's message is empty.";
    }
    std::string response = chatGPT(message, usingChatCache);
    if (usingChatCache) {
        chatCache.push_back("User: " + message);
        if (!response.empty()) {
            chatCache.push_back("ChatGPT: " + response);
        }
    }
    return response;
}

void OpenAIProcess::buildPromptAndReturnNoResponse(const std::string& message, bool usingChatCache) {
    if (USER_API_KEY.empty()) {
        std::cout << "OpenAI: " << std::chrono::system_clock::now().time_since_epoch().count() << " API key not set." << std::endl;
        return;
    }
    std::string response = chatGPT(message, usingChatCache);
    if (usingChatCache) {
        chatCache.push_back("User: " + message);
        if (!response.empty()) {
            chatCache.push_back("ChatGPT: " + response);
        }
    }
}

std::string OpenAIProcess::chatGPT(const std::string& passedMessage, bool usingChatCache) {
    std::string url = "https://api.openai.com/v1/chat/completions";
    std::string apiKey = USER_API_KEY;
    std::string model = "gpt-3.5-turbo";
    std::string sentMessage;

    if (usingChatCache && !lastPromptUsed.empty()) {
        sentMessage = "These are the previous messages from this conversation: '" + std::to_string(chatCache.size()) + "' This is the users response based on the previous conversation: '" + passedMessage + "'";
    } else {
        sentMessage = passedMessage;
    }
    sentMessage = filterMessage(sentMessage);
    lastPromptUsed = sentMessage;

    CURL* curl;
    CURLcode res;
    std::string response;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        nlohmann::json root;
        root["model"] = model;
        nlohmann::json message;
        message["role"] = "user";
        message["content"] = sentMessage;
        root["messages"].push_back(message);

        std::string body = root.dump();

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

        auto writeCallback = [](char* ptr, size_t size, size_t nmemb, std::string* data) {
            data->append(ptr, size * nmemb);
            return size * nmemb;
        };

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    responseDataMap = parseJSONResponse(response);
    lastResponseReceived = extractContentFromJSON(response);
    return lastResponseReceived;
}

std::string OpenAIProcess::filterMessage(const std::string& message) {
    std::string filteredMessage;
    for (char c : message) {
        if (isalnum(c) || isspace(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            filteredMessage += c;
        }
    }
    return filteredMessage;
}

std::map<std::string, nlohmann::json> OpenAIProcess::parseJSONResponse(const std::string& jsonResponse) {
    std::map<std::string, nlohmann::json> responseData;
    auto jsonObject = nlohmann::json::parse(jsonResponse);
    for (auto& [key, value] : jsonObject.items()) {
        responseData[key] = value;
    }
    return responseData;
}

std::string OpenAIProcess::extractContentFromJSON(const std::string& jsonResponse) {
    auto jsonObject = nlohmann::json::parse(jsonResponse);
    return jsonObject["choices"][0]["message"]["content"];
}

bool OpenAIProcess::testAPIKey(const std::string& apiKey) {
    std::string url = "https://api.openai.com/v1/engines";
    CURL* curl;
    CURLcode res;
    long response_code = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    return response_code == 200;
}

void OpenAIProcess::setAPIKey(const std::string& apiKey) {
    USER_API_KEY = apiKey;
}

std::string OpenAIProcess::getAPIKey() const {
    return USER_API_KEY;
}

std::string OpenAIProcess::getLastPromptUsed() const {
    return lastPromptUsed;
}

std::string OpenAIProcess::getLastResponseReceived() const {
    return lastResponseReceived;
}

std::vector<std::string> OpenAIProcess::getChatCache() const {
    return chatCache;
}

void OpenAIProcess::clearChatCache() {
    chatCache.clear();
}

void OpenAIProcess::setChatCache(const std::vector<std::string>& chatCache) {
    this->chatCache = chatCache;
}

std::string OpenAIProcess::getResponseData(const std::string& key) const {
    if (key == "all") {
        if (responseDataMap.empty()) {
            return "No data available.";
        }
        nlohmann::json jsonData(responseDataMap); // Convert map to JSON
        return jsonData.dump(); // Call dump on JSON object
    }
    auto it = responseDataMap.find(key);
    if (it == responseDataMap.end()) {
        return "No data available.";
    }
    return it->second.dump();
}

// ...existing code...
