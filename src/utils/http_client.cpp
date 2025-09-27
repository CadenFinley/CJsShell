#include "http_client.h"

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include "cjsh_filesystem.h"

HttpResponse HttpClient::post(const std::string& url, const std::string& data,
                              const std::map<std::string, std::string>& headers,
                              int timeout_seconds) {
    if (is_curl_available()) {
        return system_curl_post(url, data, headers, timeout_seconds);
    }

    HttpResponse response;
    response.success = false;
    response.error_message = "No HTTP client available. Please install curl.";
    return response;
}

HttpResponse HttpClient::head(const std::string& url,
                              const std::map<std::string, std::string>& headers,
                              int timeout_seconds) {
    if (is_curl_available()) {
        return system_curl_head(url, headers, timeout_seconds);
    }

    HttpResponse response;
    response.success = false;
    response.error_message = "No HTTP client available. Please install curl.";
    return response;
}

bool HttpClient::is_curl_available() {
    FILE* fp = popen("which curl 2>/dev/null", "r");
    if (fp == nullptr) {
        return false;
    }

    char buffer[256];
    bool found = (fgets(buffer, sizeof(buffer), fp) != nullptr);
    pclose(fp);
    return found;
}

std::string HttpClient::escape_for_shell(const std::string& input) {
    std::string escaped;
    for (char c : input) {
        if (c == '\'' || c == '"' || c == '\\' || c == '$' || c == '`') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

HttpResponse HttpClient::system_curl_post(
    const std::string& url, const std::string& data,
    const std::map<std::string, std::string>& headers, int timeout_seconds) {
    HttpResponse response;
    response.success = false;

    auto temp_data_result =
        cjsh_filesystem::FileOperations::create_temp_file("cjsh_http_data");
    auto temp_response_result =
        cjsh_filesystem::FileOperations::create_temp_file("cjsh_http_response");
    auto temp_headers_result =
        cjsh_filesystem::FileOperations::create_temp_file("cjsh_http_headers");

    if (temp_data_result.is_error() || temp_response_result.is_error() ||
        temp_headers_result.is_error()) {
        response.error_message = "Failed to create temporary files";
        return response;
    }

    std::string temp_data_file = temp_data_result.value();
    std::string temp_response_file = temp_response_result.value();
    std::string temp_headers_file = temp_headers_result.value();

    auto write_result =
        cjsh_filesystem::FileOperations::write_temp_file(temp_data_file, data);
    if (write_result.is_error()) {
        response.error_message = "Failed to write data to temporary file";
        cjsh_filesystem::FileOperations::cleanup_temp_file(temp_data_file);
        cjsh_filesystem::FileOperations::cleanup_temp_file(temp_response_file);
        cjsh_filesystem::FileOperations::cleanup_temp_file(temp_headers_file);
        return response;
    }

    std::ostringstream cmd;
    cmd << "curl -s -w \"%{http_code}\\n\" -m " << timeout_seconds;
    cmd << " -o \"" << temp_response_file << "\"";
    cmd << " -D \"" << temp_headers_file << "\"";
    cmd << " -X POST -d @\"" << temp_data_file << "\"";

    for (const auto& header : headers) {
        cmd << " -H \"" << escape_for_shell(header.first) << ": "
            << escape_for_shell(header.second) << "\"";
    }

    cmd << " \"" << escape_for_shell(url) << "\" 2>/dev/null";

    FILE* fp = popen(cmd.str().c_str(), "r");
    if (fp == nullptr) {
        unlink(temp_data_file.c_str());
        response.error_message = "Failed to execute curl command";
        return response;
    }

    char status_buffer[16];
    if (fgets(status_buffer, sizeof(status_buffer), fp) != nullptr) {
        response.status_code = std::atoi(status_buffer);
    }
    pclose(fp);

    std::ifstream response_file(temp_response_file);
    if (response_file) {
        std::ostringstream body_stream;
        body_stream << response_file.rdbuf();
        response.body = body_stream.str();
        response_file.close();
    }

    std::ifstream headers_file(temp_headers_file);
    if (headers_file) {
        std::string line;
        while (std::getline(headers_file, line)) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);

                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                if (!key.empty() && !value.empty()) {
                    response.headers[key] = value;
                }
            }
        }
        headers_file.close();
    }

    // Cleanup temporary files
    cjsh_filesystem::FileOperations::cleanup_temp_file(temp_data_file);
    cjsh_filesystem::FileOperations::cleanup_temp_file(temp_response_file);
    cjsh_filesystem::FileOperations::cleanup_temp_file(temp_headers_file);

    response.success =
        (response.status_code >= 200 && response.status_code < 400);
    if (!response.success && response.error_message.empty()) {
        response.error_message = "HTTP request failed with status code " +
                                 std::to_string(response.status_code);
    }

    return response;
}

HttpResponse HttpClient::system_curl_head(
    const std::string& url, const std::map<std::string, std::string>& headers,
    int timeout_seconds) {
    HttpResponse response;
    response.success = false;

    std::string temp_headers_file =
        "/tmp/cjsh_http_headers_" + std::to_string(getpid());

    std::ostringstream cmd;
    cmd << "curl -s -I -w \"%{http_code}\\n\" -m " << timeout_seconds;
    cmd << " -D \"" << temp_headers_file << "\"";

    for (const auto& header : headers) {
        cmd << " -H \"" << escape_for_shell(header.first) << ": "
            << escape_for_shell(header.second) << "\"";
    }

    cmd << " \"" << escape_for_shell(url) << "\" 2>/dev/null";

    FILE* fp = popen(cmd.str().c_str(), "r");
    if (fp == nullptr) {
        response.error_message = "Failed to execute curl command";
        return response;
    }

    char status_buffer[16];
    if (fgets(status_buffer, sizeof(status_buffer), fp) != nullptr) {
        response.status_code = std::atoi(status_buffer);
    }
    pclose(fp);

    std::ifstream headers_file(temp_headers_file);
    if (headers_file) {
        std::string line;
        while (std::getline(headers_file, line)) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);

                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                if (!key.empty() && !value.empty()) {
                    response.headers[key] = value;
                }
            }
        }
        headers_file.close();
    }

    unlink(temp_headers_file.c_str());

    response.success =
        (response.status_code >= 200 && response.status_code < 400);
    if (!response.success && response.error_message.empty()) {
        response.error_message = "HTTP request failed with status code " +
                                 std::to_string(response.status_code);
    }

    return response;
}
