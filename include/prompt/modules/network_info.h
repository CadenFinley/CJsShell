#pragma once

#include <string>

namespace network_info {

std::string get_ip_address(bool external = false);
bool is_vpn_active();
std::string get_active_network_interface();

}  // namespace network_info