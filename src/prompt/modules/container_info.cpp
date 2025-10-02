#include "container_info.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include "utils/cjsh_filesystem.h"

ContainerInfo::ContainerInfo() {
}

bool ContainerInfo::file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

std::string ContainerInfo::read_file_content(const std::string& path) {
    auto result = cjsh_filesystem::FileOperations::read_file_content(path);
    return result.is_ok() ? result.value() : "";
}

std::string ContainerInfo::execute_command(const std::string& command) {
    auto result = cjsh_filesystem::FileOperations::read_command_output(command);
    if (result.is_error()) {
        return "";
    }

    std::string output = result.value();

    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }

    return output;
}

std::string ContainerInfo::get_container_name() {
    if (is_in_docker()) {
        return "Docker";
    }

    if (file_exists("/proc/vz") && !file_exists("/proc/bc")) {
        return "OpenVZ";
    }

    if (file_exists("/run/host/container-manager")) {
        return "OCI";
    }

    if (file_exists("/dev/incus/sock")) {
        return "Incus";
    }

    if (file_exists("/run/.containerenv")) {
        std::string content = read_file_content("/run/.containerenv");

        std::regex name_regex("name=\"([^\"]+)\"");
        std::smatch match;
        if (std::regex_search(content, match, name_regex)) {
            return match[1];
        }

        std::regex image_regex("image=\"([^\"]+)\"");
        if (std::regex_search(content, match, image_regex)) {
            std::string image = match[1];
            size_t last_slash = image.find_last_of('/');
            if (last_slash != std::string::npos) {
                image = image.substr(last_slash + 1);
            }
            size_t quote_pos = image.find('"');
            if (quote_pos != std::string::npos) {
                image = image.substr(0, quote_pos);
            }
            return image;
        }

        return "Podman";
    }

    if (file_exists("/proc/1/environ")) {
        std::string content = read_file_content("/proc/1/environ");
        if (content.find("container=lxc") != std::string::npos) {
            return "LXC";
        }
    }

    if (file_exists("/run/systemd/container")) {
        return "systemd-nspawn";
    }

    if (file_exists("/proc/version")) {
        std::string content = read_file_content("/proc/version");
        if (content.find("Microsoft") != std::string::npos || content.find("WSL") != std::string::npos) {
            return "WSL";
        }
    }

    return "";
}

bool ContainerInfo::is_in_container() {
    return !get_container_name().empty();
}

std::string ContainerInfo::get_container_type() {
    return get_container_name();
}

bool ContainerInfo::is_in_docker() {
    if (file_exists("/.dockerenv")) {
        return true;
    }

    if (file_exists("/proc/self/cgroup")) {
        std::string content = read_file_content("/proc/self/cgroup");
        if (content.find("docker") != std::string::npos) {
            return true;
        }
    }

    return false;
}

std::string ContainerInfo::get_docker_context() {
    std::string context = execute_command("docker context show 2>/dev/null");
    return context.empty() ? "default" : context;
}

std::string ContainerInfo::get_docker_image() {
    if (!is_in_docker()) {
        return "";
    }

    std::string hostname = execute_command("hostname");
    if (!hostname.empty()) {
        std::string image_cmd = "docker inspect " + hostname + " --format='{{.Config.Image}}' 2>/dev/null";
        std::string image = execute_command(image_cmd);
        if (!image.empty()) {
            return image;
        }
    }

    return "";
}

bool ContainerInfo::is_in_podman() {
    return file_exists("/run/.containerenv");
}

bool ContainerInfo::is_in_lxc() {
    if (file_exists("/proc/1/environ")) {
        std::string content = read_file_content("/proc/1/environ");
        return content.find("container=lxc") != std::string::npos;
    }
    return false;
}

bool ContainerInfo::is_in_openvz() {
    return file_exists("/proc/vz") && !file_exists("/proc/bc");
}

bool ContainerInfo::is_in_systemd_nspawn() {
    return file_exists("/run/systemd/container");
}

bool ContainerInfo::is_in_wsl() {
    if (file_exists("/proc/version")) {
        std::string content = read_file_content("/proc/version");
        return content.find("Microsoft") != std::string::npos || content.find("WSL") != std::string::npos;
    }
    return false;
}