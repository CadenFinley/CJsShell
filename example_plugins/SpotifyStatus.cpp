#include "plugininterface.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include "nlohmann/json.hpp"
#include <filesystem>
#include <cstdio>
#include <memory>
#include <string>
#include <array>
#include <curl/curl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using json = nlohmann::json;

class SpotifyStatusPlugin : public PluginInterface {
private:
    static constexpr const char* SPOTIFY_API_URL = "https://api.spotify.com/v1";
    static constexpr const char* SPOTIFY_AUTH_URL = "https://accounts.spotify.com/api/token";
    static constexpr const char* AUTH_ENDPOINT = "https://accounts.spotify.com/authorize";
    static constexpr const char* CLIENT_ID = "7a28101732584969a6fca575e220ad38";
    static constexpr const char* CLIENT_SECRET = "3288672aede9495ca1ef8a7b21114ae3";
    static constexpr const char* REDIRECT_URI = "http://localhost:8080/callback";
    
    std::string accessToken;
    std::chrono::system_clock::time_point tokenExpiry;
    CURL* curl;

    bool running = false;
    bool visible = true;
    std::thread update_thread;
    std::mutex mutex;
    std::string currentStatus;
    std::string refreshToken;
    int updateInterval = 1;
    std::string dataDirectory;
    
    std::string formatStatusLine(const std::string& title, const std::string& artist, 
                                 const std::string& remainingTime, bool isPlaying) {
        std::string status = "Spotify ";
        status += isPlaying ? "▶ " : "⏸ ";
        status += title;
        if(!artist.empty()){
            status += " - " + artist;
        }
        if(!remainingTime.empty()){
            status += " [" + remainingTime + "]";
        }
        return status;
    }
    
    void setTerminalTitle(const std::string& title) {
        // Set the terminal title using OSC escape sequence
        std::cout << "\033]0;" << title << "\007";
        std::cout.flush();
    }
    
    void resetTerminalTitle() {
        // Reset terminal title to default or empty
        std::cout << "\033]0;\007";
        std::cout.flush();
    }
    
    void displayStatus() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!visible || currentStatus.empty()) return;
        
        // Set the terminal title to the current status
        setTerminalTitle(currentStatus);
    }
    
    std::string executeCommand(const std::string& cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            return "Error executing command";
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    bool refreshAccessToken() {
        if (refreshToken.empty()) return false;

        std::string auth_header = "Basic " + base64Encode(CLIENT_ID + std::string(":") + CLIENT_SECRET);
        std::string post_fields = "grant_type=refresh_token&refresh_token=" + refreshToken;

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + auth_header).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, SPOTIFY_AUTH_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        // Add timeout options to prevent hanging during network issues
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "Token refresh failed: " << curl_easy_strerror(res) << std::endl;
            return false;
        }

        try {
            json j = json::parse(response);
            accessToken = j["access_token"];
            int expires_in = j["expires_in"];
            tokenExpiry = std::chrono::system_clock::now() + std::chrono::seconds(expires_in - 60);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing token response: " << e.what() << std::endl;
            return false;
        }
    }

    json getCurrentPlayback() {
        if (std::chrono::system_clock::now() >= tokenExpiry) {
            if (!refreshAccessToken()) return json();
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, (std::string(SPOTIFY_API_URL) + "/me/player").c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        // Add timeout options to prevent hanging during network issues
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 second timeout
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); // 5 second connect timeout

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            // Mark for reconnection on network-related errors
            if (res == CURLE_OPERATION_TIMEDOUT || 
                res == CURLE_COULDNT_CONNECT || 
                res == CURLE_COULDNT_RESOLVE_HOST) {
                needsReconnection = true;
            }
            return json();
        }

        try {
            return json::parse(response);
        } catch (...) {
            return json();
        }
    }

    std::string formatDuration(int milliseconds) {
        int totalSeconds = milliseconds / 1000;
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        
        std::string formattedTime = std::to_string(minutes) + ":";
        if (seconds < 10) formattedTime += "0";
        formattedTime += std::to_string(seconds);
        
        return formattedTime;
    }

    void updateSpotifyStatus() {
        while (running) {
            try {
                if (!accessToken.empty()) {
                    // Check if we need to handle potential system sleep
                    auto now = std::chrono::system_clock::now();
                    auto timeSinceLastSuccess = std::chrono::duration_cast<std::chrono::seconds>(
                        now - lastSuccessfulConnection).count();
                    
                    // If it's been significantly longer than our update interval, system might have been asleep
                    if (lastSuccessfulConnection.time_since_epoch().count() > 0 && 
                        timeSinceLastSuccess > updateInterval * 3) {
                        std::cerr << "Potential system sleep detected, reconnecting..." << std::endl;
                        needsReconnection = true;
                        // Reset curl handle to ensure clean state
                        curl_easy_reset(curl);
                    }
                    
                    // Force refresh token if reconnection needed
                    if (needsReconnection) {
                        if (!refreshAccessToken()) {
                            std::cerr << "Failed to refresh token after sleep" << std::endl;
                            // Don't hammer the API if we're failing
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                            consecutiveFailures++;
                            if (consecutiveFailures > MAX_CONSECUTIVE_FAILURES) {
                                std::lock_guard<std::mutex> lock(mutex);
                                currentStatus = "Spotify: Connection error";
                                displayStatus();
                                // Back off longer
                                std::this_thread::sleep_for(std::chrono::seconds(30));
                                consecutiveFailures = 0; // Reset and try again
                            }
                            continue;
                        }
                        needsReconnection = false;
                    }
                    
                    json playbackState = getCurrentPlayback();
                    
                    if (!playbackState.empty()) {
                        // Reset failure counter on success
                        consecutiveFailures = 0;
                        // Update last successful connection timestamp
                        lastSuccessfulConnection = std::chrono::system_clock::now();
                        
                        bool isPlaying = playbackState["is_playing"];
                        std::string title = playbackState["item"]["name"];
                        std::string artist = playbackState["item"]["artists"][0]["name"];
                        
                        // Format as <time played>:<total time>
                        std::string timeInfo = "";
                        if (playbackState.contains("item") && 
                            playbackState["item"].contains("duration_ms") && 
                            playbackState.contains("progress_ms")) {
                            
                            int duration = playbackState["item"]["duration_ms"];
                            int progress = playbackState["progress_ms"];
                            
                            std::string playedTime = formatDuration(progress);
                            std::string totalTime = formatDuration(duration);
                            
                            timeInfo = playedTime + " : " + totalTime;
                        }

                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            currentStatus = formatStatusLine(title, artist, timeInfo, isPlaying);
                        }
                        
                        displayStatus();
                    } else {
                        consecutiveFailures++;
                        if (consecutiveFailures > MAX_CONSECUTIVE_FAILURES) {
                            // After multiple failures, assume connection issues
                            needsReconnection = true;
                            std::lock_guard<std::mutex> lock(mutex);
                            currentStatus = "Spotify: Connection error";
                            displayStatus();
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error in update thread: " << e.what() << std::endl;
                needsReconnection = true;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(updateInterval));
        }
    }
    
    void saveUserData() {
        std::filesystem::path userDataPath = dataDirectory + "/spotify";
        std::filesystem::create_directories(userDataPath);
        
        json userData;
        userData["refresh_token"] = refreshToken;
        userData["update_interval"] = updateInterval;
        userData["visible"] = visible;
        
        std::ofstream file(userDataPath / "user-data.json");
        if (file.is_open()) {
            file << userData.dump(4);
            file.close();
        }
    }
    
    void loadUserData() {
        std::filesystem::path userDataPath = dataDirectory + "/spotify/user-data.json";
        
        if (std::filesystem::exists(userDataPath)) {
            std::ifstream file(userDataPath);
            if (file.is_open()) {
                try {
                    json userData;
                    file >> userData;
                    
                    refreshToken = userData.value("refresh_token", "");
                    updateInterval = userData.value("update_interval", 10);
                    visible = userData.value("visible", true);
                    
                    file.close();
                } catch (const std::exception& e) {
                    std::cerr << "Error loading Spotify user data: " << e.what() << std::endl;
                }
            }
        }
    }

    std::string base64Encode(const std::string& input) {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        std::string encoded;
        int i = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        size_t in_len = input.length();
        const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(input.c_str());
        
        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for(i = 0; i < 4; i++)
                    encoded += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i) {
            for(size_t j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            for (size_t j = 0; j < i + 1; j++)
                encoded += base64_chars[char_array_4[j]];

            while(i++ < 3)
                encoded += '=';
        }

        return encoded;
    }

    // New callback server implementation
    int callbackServerPort = 8080;
    int callbackServerSocket = -1;
    std::string capturedAuthCode;
    bool authCodeReceived = false;
    std::thread serverThread;
    
    bool startAuthServer() {
        // Create a socket
        callbackServerSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (callbackServerSocket < 0) {
            std::cerr << "Failed to create socket for auth server" << std::endl;
            return false;
        }
        
        // Set socket options for reuse
        int opt = 1;
        if (setsockopt(callbackServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set socket options" << std::endl;
            close(callbackServerSocket);
            return false;
        }
        
        // Prepare the sockaddr_in structure
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(callbackServerPort);
        
        // Bind
        if (bind(callbackServerSocket, (struct sockaddr *)&server, sizeof(server)) < 0) {
            std::cerr << "Bind failed for auth server" << std::endl;
            close(callbackServerSocket);
            return false;
        }
        
        // Listen
        if (listen(callbackServerSocket, 3) < 0) {
            std::cerr << "Listen failed for auth server" << std::endl;
            close(callbackServerSocket);
            return false;
        }
        
        // Start a thread to handle the connection
        serverThread = std::thread(&SpotifyStatusPlugin::handleAuthServerConnections, this);
        return true;
    }
    
    void handleAuthServerConnections() {
        struct sockaddr_in client;
        int c = sizeof(struct sockaddr_in);
        
        while (running && !authCodeReceived) {
            // Accept connection
            int client_sock = accept(callbackServerSocket, (struct sockaddr *)&client, (socklen_t*)&c);
            if (client_sock < 0) {
                continue;
            }
            
            // Read request
            char buffer[4096] = {0};
            read(client_sock, buffer, 4095);
            std::string request(buffer);
            
            // Check for auth code in request
            std::string authCodeParam = "code=";
            size_t codePos = request.find(authCodeParam);
            if (codePos != std::string::npos) {
                size_t codeStart = codePos + authCodeParam.length();
                size_t codeEnd = request.find(" ", codeStart);
                if (codeEnd == std::string::npos) {
                    codeEnd = request.find("&", codeStart);
                }
                if (codeEnd == std::string::npos) {
                    codeEnd = request.find("\r\n", codeStart);
                }
                
                if (codeEnd != std::string::npos) {
                    capturedAuthCode = request.substr(codeStart, codeEnd - codeStart);
                    authCodeReceived = true;
                    
                    // Send success response to browser
                    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                        "<!DOCTYPE html><html><head><title>Spotify Auth Success</title>"
                        "<style>"
                        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
                        "margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; "
                        "min-height: 100vh; background: linear-gradient(135deg, #1DB954 0%, #1ed760 100%); color: white; }"
                        ".container { background: rgba(0,0,0,0.2); border-radius: 12px; padding: 2rem; "
                        "text-align: center; backdrop-filter: blur(10px); max-width: 500px; margin: 1rem; }"
                        "h1 { margin-bottom: 1rem; }"
                        ".logo { width: 64px; height: 64px; margin-bottom: 1rem; }"
                        ".checkmark { font-size: 64px; margin-bottom: 1rem; animation: pop 0.5s ease-out; }"
                        "@keyframes pop { 0% { transform: scale(0); } 100% { transform: scale(1); } }"
                        "p { line-height: 1.5; opacity: 0.9; }"
                        "</style></head>"
                        "<body><div class='container'>"
                        "<img class='logo' src='https://storage.googleapis.com/pr-newsroom-wp/1/2018/11/Spotify_Logo_RGB_White.png' "
                        "alt='Spotify Logo'>"
                        "<div class='checkmark'>✓</div>"
                        "<h1>Successfully Connected!</h1>"
                        "<p>You can now close this window and return to your terminal.</p>"
                        "<p>Your Spotify status will begin displaying shortly.</p>"
                        "</div></body></html>";
                    send(client_sock, response.c_str(), response.length(), 0);
                }
            } else {
                // Send a waiting/callback page
                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                    "<!DOCTYPE html><html><head><title>Spotify Auth</title>"
                    "<style>"
                    "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
                    "margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; "
                    "min-height: 100vh; background: linear-gradient(135deg, #1DB954 0%, #1ed760 100%); color: white; }"
                    ".container { background: rgba(0,0,0,0.2); border-radius: 12px; padding: 2rem; "
                    "text-align: center; backdrop-filter: blur(10px); max-width: 500px; margin: 1rem; }"
                    "h1 { margin-bottom: 1rem; }"
                    ".logo { width: 64px; height: 64px; margin-bottom: 1rem; }"
                    ".spinner { width: 40px; height: 40px; margin: 1rem auto; "
                    "border: 4px solid rgba(255,255,255,0.3); border-radius: 50%; "
                    "border-top-color: white; animation: spin 1s linear infinite; }"
                    "@keyframes spin { 0% { transform: rotate(0deg); } "
                    "100% { transform: rotate(360deg); } }"
                    "p { line-height: 1.5; opacity: 0.9; }"
                    "</style></head>"
                    "<body><div class='container'>"
                    "<img class='logo' src='https://storage.googleapis.com/pr-newsroom-wp/1/2018/11/Spotify_Logo_RGB_White.png' "
                    "alt='Spotify Logo'>"
                    "<h1>Connecting to Spotify</h1>"
                    "<div class='spinner'></div>"
                    "<p>Please wait while we complete the authorization process...</p>"
                    "<p>If you haven't authorized the app yet, you should be redirected shortly.</p>"
                    "</div></body></html>";
                send(client_sock, response.c_str(), response.length(), 0);
            }
            
            close(client_sock);
        }
    }
    
    void stopAuthServer() {
        if (callbackServerSocket >= 0) {
            ::shutdown(callbackServerSocket, SHUT_RDWR); // interrupt blocking accept()
            close(callbackServerSocket);
            callbackServerSocket = -1;
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }
    
    void startAuthFlow() {
        // Reset auth code status
        capturedAuthCode = "";
        authCodeReceived = false;
        
        // Generate random state string for security
        std::string state = std::to_string(std::rand());
        
        // Build authorization URL
        // Note: This uses the application's CLIENT_ID but will authenticate the user's personal Spotify account
        // Each user who runs this command will connect their own Spotify account
        std::string authUrl = std::string(AUTH_ENDPOINT) +
            "?client_id=" + CLIENT_ID +
            "&response_type=code" +
            "&redirect_uri=" + REDIRECT_URI +
            "&state=" + state +
            "&scope=user-read-playback-state";
            
        // Start local server to handle callback
        if (!startAuthServer()) {
            std::cout << "Failed to start authentication server. Please ensure port " 
                      << callbackServerPort << " is available." << std::endl;
            return;
        }
        
        // Open URL in default browser
        #ifdef __APPLE__
            system(("open '" + authUrl + "'").c_str());
        #elif defined(__linux__)
            system(("xdg-open '" + authUrl + "'").c_str());
        #endif
        
        std::cout << "Please authorize the application in your browser." << std::endl;
        std::cout << "Waiting for authorization..." << std::endl;
        
        // Wait for auth code to be received (with timeout)
        int timeout = 120; // 2 minutes
        while (!authCodeReceived && timeout > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            timeout--;
            
            // Show a spinner to indicate waiting
            if (timeout % 5 == 0) {
                std::cout << "." << std::flush;
            }
        }
        
        if (authCodeReceived) {
            std::cout << "\nAuthorization code received! Exchanging for access token..." << std::endl;
            if (exchangeCodeForToken(capturedAuthCode)) {
                std::cout << "✓ Successfully authenticated with Spotify!" << std::endl;
            } else {
                std::cout << "✗ Failed to exchange authorization code for tokens." << std::endl;
                std::cout << "This could be due to an expired authorization code or network issues." << std::endl;
                std::cout << "Try running 'spotify auth' again to get a fresh authorization code." << std::endl;
            }
        } else {
            std::cout << "\n✗ Timed out waiting for authorization." << std::endl;
            std::cout << "If you authorized the app, you can try manually using:" << std::endl;
            std::cout << "spotify auth <code>" << std::endl;
        }
        
        // Clean up server
        stopAuthServer();
    }
    
    bool exchangeCodeForToken(const std::string& code) {
        std::string auth_header = "Basic " + base64Encode(CLIENT_ID + std::string(":") + CLIENT_SECRET);
        std::string post_fields = "grant_type=authorization_code&code=" + code + 
                                "&redirect_uri=" + REDIRECT_URI;

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + auth_header).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, SPOTIFY_AUTH_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        // Add these settings for better error handling
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L); // Don't fail on HTTP errors
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects if any
        
        // Debug information
        std::cout << "Sending token exchange request to Spotify..." << std::endl;
        
        CURLcode res = curl_easy_perform(curl);
        
        // Get HTTP response code
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
            return false;
        }

        if (http_code != 200) {
            std::cerr << "HTTP error: " << http_code << std::endl;
            std::cerr << "Response: " << response << std::endl;
            return false;
        }

        try {
            json j = json::parse(response);
            
            // Check if there's an error in the JSON response
            if (j.contains("error")) {
                std::cerr << "Spotify API error: " << j["error"].dump() << std::endl;
                if (j.contains("error_description")) {
                    std::cerr << "Error description: " << j["error_description"] << std::endl;
                }
                return false;
            }
            
            // Check if required fields are present
            if (!j.contains("access_token")) {
                std::cerr << "Missing access_token in response" << std::endl;
                return false;
            }
            
            accessToken = j["access_token"];
            
            // Refresh token is sometimes not included in token refresh responses
            if (j.contains("refresh_token")) {
                refreshToken = j["refresh_token"];
            }
            
            int expires_in = j["expires_in"];
            tokenExpiry = std::chrono::system_clock::now() + std::chrono::seconds(expires_in - 60);
            
            // Save the refresh token
            saveUserData();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            return false;
        }
    }

    // Add variables to track sleep state and reconnection attempts
    std::chrono::system_clock::time_point lastSuccessfulConnection;
    bool needsReconnection = false;
    int consecutiveFailures = 0;
    const int MAX_CONSECUTIVE_FAILURES = 5;

public:
    SpotifyStatusPlugin() : dataDirectory(".DTT-Data") {
        curl = curl_easy_init();
        // Initialize lastSuccessfulConnection to epoch (0)
        lastSuccessfulConnection = std::chrono::system_clock::time_point();
    }

    ~SpotifyStatusPlugin() {
        if (curl) curl_easy_cleanup(curl);
        stopAuthServer();
    }
    
    std::string getName() const override {
        return "SpotifyStatus";
    }
    
    std::string getVersion() const override {
        return "1.0";
    }
    
    std::string getDescription() const override {
        return "Displays the current Spotify status at the top of the terminal";
    }
    
    std::string getAuthor() const override {
        return "Caden Finley";
    }
    
    bool initialize() override {
        loadUserData();
        if (!curl) return false;
        
        // Set up curl global settings for better reliability
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // Prevent SIGALRM during DNS timeout
        
        if (!refreshToken.empty()) {
            if (!refreshAccessToken()) {
                std::cerr << "Failed to refresh Spotify access token" << std::endl;
            }
        }

        running = true;
        update_thread = std::thread(&SpotifyStatusPlugin::updateSpotifyStatus, this);
        return true;
    }
    
    void shutdown() override {
        running = false;
        if (update_thread.joinable()) {
            update_thread.join();
        }
        stopAuthServer(); // Make sure to stop the auth server if running
        saveUserData();
        resetTerminalTitle(); // Reset the terminal title when shutting down
    }
    
    std::vector<std::string> getCommands() const override {
        return {"spotify"};
    }

    std::vector<std::string> getSubscribedEvents() const override {
        return {};
    }

    int getInterfaceVersion() const override {
        return 1;
    }
    
    bool handleCommand(std::queue<std::string>& args) override {
        if (args.empty()) return false;
        
        std::string command = args.front();
        args.pop();
        
        if (command == "spotify") {
            if (args.empty()) {
                std::cout << "Spotify Status Plugin Commands:" << std::endl;
                std::cout << "  spotify status - Show current status" << std::endl;
                std::cout << "  spotify interval [SECONDS] - Set update interval" << std::endl;
                std::cout << "  spotify show - Show status in terminal title" << std::endl;
                std::cout << "  spotify hide - Hide status from terminal title" << std::endl;
                std::cout << "  spotify auth - Start Spotify authorization flow (required first step)" << std::endl;
                std::cout << "  spotify logout - Clear authentication data" << std::endl;
                return true;
            }
            
            std::string subcommand = args.front();
            args.pop();
            
            if (subcommand == "status") {
                std::lock_guard<std::mutex> lock(mutex);
                std::cout << "Current Spotify status: " << (currentStatus.empty() ? "Not playing" : currentStatus) << std::endl;
                std::cout << "Update interval: " << updateInterval << " seconds" << std::endl;
                std::cout << "Status visible: " << (visible ? "Yes" : "No") << std::endl;
                std::cout << "Authenticated: " << (!refreshToken.empty() ? "Yes" : "No") << std::endl;
                if (refreshToken.empty()) {
                    std::cout << "\nTo use this plugin, you need to authenticate with Spotify first:" << std::endl;
                    std::cout << "  Run 'spotify auth' to start the authorization process" << std::endl;
                }
                return true;
            }
            else if (subcommand == "set") {
                if (args.empty()) return false;
                
                std::string setting = args.front();
                args.pop();
                
                if (setting == "refresh_token" && !args.empty()) {
                    refreshToken = args.front();
                    args.pop();
                    std::cout << "Spotify refresh token set" << std::endl;
                    if (!refreshAccessToken()) {
                        std::cout << "Warning: Failed to validate refresh token" << std::endl;
                    }
                    saveUserData();
                    return true;
                }
                return false;
            }
            else if (subcommand == "interval" && !args.empty()) {
                try {
                    updateInterval = std::stoi(args.front());
                    args.pop();
                    std::cout << "Update interval set to: " << updateInterval << " seconds" << std::endl;
                    saveUserData();
                    return true;
                } catch (const std::exception& e) {
                    std::cerr << "Invalid interval value" << std::endl;
                    return false;
                }
            }
            else if (subcommand == "show") {
                visible = true;
                displayStatus();
                std::cout << "Spotify status is now visible in terminal title" << std::endl;
                saveUserData();
                return true;
            }
            else if (subcommand == "hide") {
                visible = false;
                resetTerminalTitle();
                std::cout << "Spotify status is now hidden from terminal title" << std::endl;
                saveUserData();
                return true;
            }
            else if (subcommand == "auth") {
                if (args.empty()) {
                    std::cout << "Starting Spotify authorization process..." << std::endl;
                    std::cout << "This will connect YOUR Spotify account to this plugin." << std::endl;
                    std::cout << "Note: Each user can authorize their own Spotify account with this plugin." << std::endl;
                    startAuthFlow();
                    return true;
                } else {
                    std::string code = args.front();
                    args.pop();
                    if (exchangeCodeForToken(code)) {
                        std::cout << "Successfully authenticated with Spotify!" << std::endl;
                    } else {
                        std::cout << "Failed to authenticate with Spotify." << std::endl;
                    }
                    return true;
                }
            }
            else if (subcommand == "logout") {
                refreshToken = "";
                accessToken = "";
                std::cout << "Spotify authentication data cleared. Use 'spotify auth' to reconnect." << std::endl;
                saveUserData();
                return true;
            }
        }
        
        return false;
    }
    
    std::map<std::string, std::string> getDefaultSettings() const override {
        return {
            {"update_interval", std::to_string(updateInterval)},
            {"visible", visible ? "true" : "false"}
        };
    }
    
    void updateSetting(const std::string& key, const std::string& value) override {
        if (key == "update_interval") {
            try {
                updateInterval = std::stoi(value);
            } catch (...) {
                std::cerr << "Invalid update interval value: " << value << std::endl;
            }
        }
        else if (key == "visible") {
            visible = (value == "true" || value == "1");
            if (visible) {
                displayStatus();
            } else {
                resetTerminalTitle();
            }
        }
        
        saveUserData();
    }
};

// Plugin API implementation
IMPLEMENT_PLUGIN(SpotifyStatusPlugin)
