#pragma once

#include <string>

class NetworkInfo {
 public:
  NetworkInfo() = default;
  ~NetworkInfo() = default;
  
  std::string get_ip_address(bool external = false);
  bool is_vpn_active();
  std::string get_active_network_interface();
};