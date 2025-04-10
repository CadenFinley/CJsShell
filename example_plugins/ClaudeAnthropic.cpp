#include "plugininterface.h"
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <curl/curl.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <queue>
#include "nlohmann/json.hpp"

// clang++ -dynamiclib -fPIC -O2 -std=c++11 -o ClaudeAnthropic.dylib ClaudeAnthropic.cpp -lcurl

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class ClaudeAnthropic : public PluginInterface {
private:
    std::string SETTINGS_DIRECTORY;
    std::string USER_DATA;
    CURL* curl;
    std::string apiKey;
    int maxTokens;

    void writeUserDataFile(){
        std::ofstream file(USER_DATA);
        if (file.is_open()) {
            nlohmann::json settings;
            settings["api_key"] = apiKey;
            settings["max_tokens"] = maxTokens;
            file << settings.dump(4);
            file.close();
        } else {
            std::cerr << "Error: Unable to write to the user data file at " << USER_DATA << std::endl;
        }
    }

    void readUserDataFile(){
        std::ifstream file(USER_DATA);
        if (file.is_open()) {
            nlohmann::json settings;
            file >> settings;
            file.close();
            apiKey = settings["api_key"].get<std::string>();
            maxTokens = settings["max_tokens"].get<int>();
        } else {
            std::cerr << "Error: Unable to read the user data file at " << USER_DATA << std::endl;
        }
    }

public:
    ClaudeAnthropic() : curl(nullptr), maxTokens(1000) {
        SETTINGS_DIRECTORY = getPluginDirectory();
        USER_DATA = (std::filesystem::path(SETTINGS_DIRECTORY) / "claude-anthropic-settings.json").string();
    }
    ~ClaudeAnthropic() throw() {
        if (curl) curl_easy_cleanup(curl);
    }
    
    std::string getName() const { 
        return "Claude Anthropic"; 
    }
    
    std::string getVersion() const { 
        return "1.0"; 
    }
    
    std::string getDescription() const { 
        return "A plugin for Anthropic - Claude AI use."; 
    }
    
    std::string getAuthor() const { 
        return "Caden Finley"; 
    }
    
    bool initialize() { 
        if (!std::filesystem::exists(SETTINGS_DIRECTORY)) {
            std::filesystem::create_directories(SETTINGS_DIRECTORY);
            
            std::ifstream checkFile(USER_DATA);
            if (!checkFile.good()) {
                std::ofstream file(USER_DATA);
                if (file.is_open()) {
                    file << "{\n\"api_key\": \"\",\n\"max_tokens\": 1000\n}";
                    file.close();
                } else {
                    std::cerr << "Error: Unable to create the user data file at " << USER_DATA << std::endl;
                }
            }
        }

        readUserDataFile();
        
        curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return false;
        }
        return true;
    }
    
    void shutdown() {
        if (curl) {
            curl_easy_cleanup(curl);
            curl = nullptr;
        }
        writeUserDataFile();
    }
    
    bool handleCommand(std::queue<std::string>& args) {
        std::string cmd = args.front();
        args.pop();
        
        if (cmd == "chat") {
            if (args.empty()) {
                std::cout << "Usage: claude chat <message>" << std::endl;
                return true;
            }
            
            std::string message;
            while (!args.empty()) {
                message += args.front() + " ";
                args.pop();
            }
            
            std::string payload = "{\"model\":\"claude-3-7-sonnet-20250219\",\"max_tokens\":" + 
                                std::to_string(maxTokens) + 
                                ",\"messages\":[{\"role\":\"user\",\"content\":\"" + message + "\"}]}";
            
            std::string response;
            if (curl) {
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, ("x-api-key: " + apiKey).c_str());
                headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
                headers = curl_slist_append(headers, "content-type: application/json");
                
                curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                
                CURLcode res = curl_easy_perform(curl);
                curl_slist_free_all(headers);
                
                if (res != CURLE_OK) {
                    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
                } else {
                    // Parse the JSON response
                    try {
                        auto jsonResponse = nlohmann::json::parse(response);
                        const auto& content = jsonResponse["content"];
                        if (content.is_array() && !content.empty()) {
                            std::cout << "Claude: " << content[0]["text"].get<std::string>() << std::endl;
                        } else {
                            std::cerr << "Invalid response format" << std::endl;
                        }
                    } catch (const nlohmann::json::exception& e) {
                        std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
                    }
                }
            }
            return true;
        }
        return false;
    }
    
    std::vector<std::string> getCommands() const { 
        std::vector<std::string> cmds;
        cmds.push_back("chat");
        return cmds;
    }

    std::vector<std::string> getSubscribedEvents() const { 
        return {};
    }

    int getInterfaceVersion() const{
        return 1;
    }
    
    std::map<std::string, std::string> getDefaultSettings() const { 
        std::map<std::string, std::string> settings;
        settings.insert(std::make_pair("api_key", ""));
        settings.insert(std::make_pair("max_tokens", "1000"));
        return settings;
    }
    
    void updateSetting(const std::string& key, const std::string& value) { 
        if (key == "api_key") {
            apiKey = value;
        }
        else if (key == "max_tokens") {
            try {
                maxTokens = std::stoi(value);
            } catch (const std::exception& e) {
                std::cerr << "Invalid max_tokens value: " << value << std::endl;
            }
        }
        writeUserDataFile();
    }
};

extern "C" {
    PLUGIN_API PluginInterface* createPlugin() { 
        return static_cast<PluginInterface*>(new ClaudeAnthropic()); 
    }
    PLUGIN_API void destroyPlugin(PluginInterface* plugin) { delete plugin; }
}
