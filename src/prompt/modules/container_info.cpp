#include "container_info.h"

#include <cstdio>
#include <regex>

#include "cjsh_filesystem.h"
#include "command_utils.h"

using prompt_modules::detail::command_output_or;
using prompt_modules::detail::command_output_trimmed;

bool file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

std::string read_file_content(const std::string& path) {
    auto result = cjsh_filesystem::read_file_content(path);
    return result.is_ok() ? result.value() : "";
}

std::string get_container_name() {
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
        if (content.find("Microsoft") != std::string::npos ||
            content.find("WSL") != std::string::npos) {
            return "WSL";
        }
    }

    return "";
}

bool is_in_container() {
    return !get_container_name().empty();
}

std::string get_container_type() {
    return get_container_name();
}

bool is_in_docker() {
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

std::string get_docker_context() {
    return command_output_or("docker context show 2>/dev/null", "default");
}

std::string get_docker_image() {
    if (!is_in_docker()) {
        return "";
    }

    std::string hostname = command_output_trimmed("hostname");
    if (!hostname.empty()) {
        std::string image_cmd =
            "docker inspect " + hostname + " --format='{{.Config.Image}}' 2>/dev/null";
        std::string image = command_output_trimmed(image_cmd);
        if (!image.empty()) {
            return image;
        }
    }

    return "";
}

bool is_in_podman() {
    return file_exists("/run/.containerenv");
}

bool is_in_lxc() {
    if (file_exists("/proc/1/environ")) {
        std::string content = read_file_content("/proc/1/environ");
        return content.find("container=lxc") != std::string::npos;
    }
    return false;
}

bool is_in_openvz() {
    return file_exists("/proc/vz") && !file_exists("/proc/bc");
}

bool is_in_systemd_nspawn() {
    return file_exists("/run/systemd/container");
}

bool is_in_wsl() {
    if (file_exists("/proc/version")) {
        std::string content = read_file_content("/proc/version");
        return content.find("Microsoft") != std::string::npos ||
               content.find("WSL") != std::string::npos;
    }
    return false;
}