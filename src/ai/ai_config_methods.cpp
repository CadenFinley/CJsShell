#include <filesystem>
#include <fstream>
#include <iostream>

#include "ai.h"
#include "cjsh_filesystem.h"

void Ai::set_config_name(const std::string& config_name) {
  if (config_name.empty()) {
    std::cerr << "cjsh: ai: Config name cannot be empty." << std::endl;
    return;
  }

  this->config_name = config_name;
}

std::string Ai::get_config_name() const {
  return config_name;
}

std::vector<std::string> Ai::list_configs() const {
  std::vector<std::string> configs;
  try {
    for (const auto& entry : cjsh_filesystem::fs::directory_iterator(
             cjsh_filesystem::g_cjsh_ai_config_path)) {
      if (cjsh_filesystem::fs::is_regular_file(entry.path()) &&
          entry.path().extension() == ".json") {
        std::string filename = entry.path().filename().string();

        std::string config_name = filename.substr(0, filename.size() - 5);
        configs.push_back(config_name);
      }
    }
  } catch (const cjsh_filesystem::fs::filesystem_error& e) {
    std::cerr << "cjsh: ai: Error listing AI config files: " << e.what() << std::endl;
  }
  return configs;
}

bool Ai::load_config(const std::string& config_name) {
  std::string old_config_name = this->config_name;

  set_config_name(config_name);

  try {
    load_ai_config();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "cjsh: ai: Error loading AI config '" << config_name << "': " << e.what()
              << std::endl;
    this->config_name = old_config_name;
    return false;
  }
}

bool Ai::save_config_as(const std::string& config_name) {
  std::string old_config_name = this->config_name;

  set_config_name(config_name);

  try {
    save_ai_config();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "cjsh: ai: Error saving AI config as '" << config_name
              << "': " << e.what() << std::endl;
    this->config_name = old_config_name;
    return false;
  }
}
