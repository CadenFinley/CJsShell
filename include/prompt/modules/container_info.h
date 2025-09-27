#pragma once

#include <string>

class ContainerInfo {
   private:
    bool file_exists(const std::string& path);
    std::string read_file_content(const std::string& path);
    std::string execute_command(const std::string& command);

   public:
    ContainerInfo();

    std::string get_container_name();
    bool is_in_container();
    std::string get_container_type();

    bool is_in_docker();
    std::string get_docker_context();
    std::string get_docker_image();

    bool is_in_podman();
    bool is_in_lxc();
    bool is_in_openvz();
    bool is_in_systemd_nspawn();
    bool is_in_wsl();
};