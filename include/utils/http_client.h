#pragma once

#include <map>
#include <string>
#include <vector>

struct HttpResponse {
  int status_code;
  std::string body;
  std::map<std::string, std::string> headers;
  bool success;
  std::string error_message;
};

class HttpClient {
 public:
  static HttpResponse post(
      const std::string& url, const std::string& data,
      const std::map<std::string, std::string>& headers = {},
      int timeout_seconds = 300);

  static HttpResponse head(
      const std::string& url,
      const std::map<std::string, std::string>& headers = {},
      int timeout_seconds = 30);

 private:
  // Try to use system curl command as fallback
  static HttpResponse system_curl_post(
      const std::string& url, const std::string& data,
      const std::map<std::string, std::string>& headers, int timeout_seconds);

  static HttpResponse system_curl_head(
      const std::string& url, const std::map<std::string, std::string>& headers,
      int timeout_seconds);

  static bool is_curl_available();
  static std::string escape_for_shell(const std::string& input);
};
