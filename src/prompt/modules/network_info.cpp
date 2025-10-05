#include "network_info.h"

#include "utils/cjsh_filesystem.h"

std::string get_ip_address(bool external) {
    std::string cmd;

    if (external) {
        cmd = R"(
      curl -s ipinfo.io/ip 2>/dev/null || 
      curl -s ifconfig.me 2>/dev/null || 
      curl -s icanhazip.com 2>/dev/null || 
      echo "N/A"
    )";
    } else {
#ifdef __APPLE__
        cmd = R"(
      ifconfig | grep 'inet ' | grep -v '127.0.0.1' | head -n 1 | awk '{print $2}'
    )";
#elif defined(__linux__)
        cmd = R"(
      hostname -I | awk '{print $1}' 2>/dev/null || 
      ip route get 1.1.1.1 | awk '{print $7}' 2>/dev/null ||
      ifconfig | grep 'inet ' | grep -v '127.0.0.1' | head -n 1 | awk '{print $2}' | sed 's/addr:
    )";
#else
        return "N/A";
#endif
    }

    auto result_data = cjsh_filesystem::read_command_output(cmd);
    if (result_data.is_error()) {
        return "N/A";
    }

    std::string result = result_data.value();
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result.empty() ? "N/A" : result;
}

bool is_vpn_active() {
#ifdef __APPLE__
    std::string cmd = R"(
    scutil --nwi | grep -q "utun\|tun\|ppp" && echo "1" || echo "0"
  )";
#elif defined(__linux__)
    std::string cmd = R"(
    ip link show | grep -q "tun\|tap\|ppp" && echo "1" || echo "0"
  )";
#else
    return false;
#endif

    auto result_data = cjsh_filesystem::read_command_output(cmd);
    if (result_data.is_error()) {
        return false;
    }

    return (result_data.value().length() > 0 && result_data.value()[0] == '1');
}

std::string get_active_network_interface() {
#ifdef __APPLE__
    std::string cmd = R"(
    route get default | grep interface | awk '{print $2}'
  )";
#elif defined(__linux__)
    std::string cmd = R"(
    ip route | grep default | awk '{print $5}' | head -n 1
  )";
#else
    return "N/A";
#endif

    auto result_data = cjsh_filesystem::read_command_output(cmd);
    if (result_data.is_error()) {
        return "N/A";
    }

    std::string result = result_data.value();
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result.empty() ? "N/A" : result;
}