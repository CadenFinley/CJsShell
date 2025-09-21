#include "utils/bookmark_database.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include "error_out.h"

namespace bookmark_database {

// Global singleton instance
BookmarkDatabase g_bookmark_db;

BookmarkDatabase::BookmarkDatabase() : dirty_(false) {
  // Initialize database path using cjsh_filesystem paths
  database_path_ =
      (cjsh_filesystem::g_cjsh_cache_path / "directory_bookmarks.json")
          .string();
}

BookmarkDatabase::~BookmarkDatabase() {
  if (dirty_) {
    // Try to save on destruction if there are unsaved changes
    auto result = save();
    if (result.is_error()) {
      print_error({ErrorType::RUNTIME_ERROR,
                   "bookmark",
                   "Failed to save bookmark database: " + result.error(),
                   {}});
    }
  }
}

cjsh_filesystem::Result<void> BookmarkDatabase::ensure_database_directory() {
  try {
    std::filesystem::path db_path(database_path_);
    std::filesystem::path parent_dir = db_path.parent_path();

    if (!std::filesystem::exists(parent_dir)) {
      std::filesystem::create_directories(parent_dir);
    }

    return cjsh_filesystem::Result<void>::ok();
  } catch (const std::filesystem::filesystem_error& e) {
    return cjsh_filesystem::Result<void>::error(
        "Failed to create database directory: " + std::string(e.what()));
  }
}

cjsh_filesystem::Result<void> BookmarkDatabase::load() {
  // Ensure the directory exists
  auto dir_result = ensure_database_directory();
  if (dir_result.is_error()) {
    return dir_result;
  }

  // If file doesn't exist, start with empty database
  if (!std::filesystem::exists(database_path_)) {
    bookmarks_.clear();
    dirty_ = false;
    return cjsh_filesystem::Result<void>::ok();
  }

  // Read the file content
  auto content_result =
      cjsh_filesystem::FileOperations::read_file_content(database_path_);
  if (content_result.is_error()) {
    return cjsh_filesystem::Result<void>::error(
        "Failed to read bookmark database: " + content_result.error());
  }

  // Parse JSON
  auto json_result = from_json(content_result.value());
  if (json_result.is_error()) {
    return json_result;
  }

  dirty_ = false;
  return cjsh_filesystem::Result<void>::ok();
}

cjsh_filesystem::Result<void> BookmarkDatabase::save() {
  // Ensure the directory exists
  auto dir_result = ensure_database_directory();
  if (dir_result.is_error()) {
    return dir_result;
  }

  // Convert to JSON
  std::string json_content = to_json();

  // Write to file
  auto write_result = cjsh_filesystem::FileOperations::write_file_content(
      database_path_, json_content);
  if (write_result.is_error()) {
    return cjsh_filesystem::Result<void>::error(
        "Failed to write bookmark database: " + write_result.error());
  }

  dirty_ = false;
  return cjsh_filesystem::Result<void>::ok();
}

std::string BookmarkDatabase::to_json() const {
  using json = nlohmann::json;

  json root;
  root["version"] = "1.0";
  root["last_updated"] = time_to_iso_string(std::chrono::system_clock::now());

  json bookmarks_obj;
  for (const auto& [name, entry] : bookmarks_) {
    json bookmark_entry;
    bookmark_entry["path"] = entry.path;
    bookmark_entry["added_time"] = time_to_iso_string(entry.added_time);
    bookmark_entry["last_accessed"] = time_to_iso_string(entry.last_accessed);
    bookmark_entry["access_count"] = entry.access_count;

    bookmarks_obj[name] = bookmark_entry;
  }

  root["bookmarks"] = bookmarks_obj;

  return root.dump(2);  // Pretty print with 2-space indentation
}

cjsh_filesystem::Result<void> BookmarkDatabase::from_json(
    const std::string& json_str) {
  try {
    using json = nlohmann::json;

    json root = json::parse(json_str);

    // Validate version
    if (!root.contains("version")) {
      return cjsh_filesystem::Result<void>::error(
          "Invalid bookmark database: missing version");
    }

    std::string version = root["version"];
    if (version != "1.0") {
      return cjsh_filesystem::Result<void>::error(
          "Unsupported bookmark database version: " + version);
    }

    // Clear existing bookmarks
    bookmarks_.clear();

    // Load bookmarks
    if (root.contains("bookmarks") && root["bookmarks"].is_object()) {
      for (const auto& [name, bookmark_data] : root["bookmarks"].items()) {
        if (!bookmark_data.is_object()) {
          continue;  // Skip invalid entries
        }

        BookmarkEntry entry;

        if (bookmark_data.contains("path") &&
            bookmark_data["path"].is_string()) {
          entry.path = bookmark_data["path"];
        } else {
          continue;  // Skip entries without valid path
        }

        if (bookmark_data.contains("access_count") &&
            bookmark_data["access_count"].is_number()) {
          entry.access_count = bookmark_data["access_count"];
        } else {
          entry.access_count = 1;
        }

        if (bookmark_data.contains("added_time") &&
            bookmark_data["added_time"].is_string()) {
          entry.added_time = time_from_iso_string(bookmark_data["added_time"]);
        } else {
          entry.added_time = std::chrono::system_clock::now();
        }

        if (bookmark_data.contains("last_accessed") &&
            bookmark_data["last_accessed"].is_string()) {
          entry.last_accessed =
              time_from_iso_string(bookmark_data["last_accessed"]);
        } else {
          entry.last_accessed = entry.added_time;
        }

        bookmarks_[name] = entry;
      }
    }

    return cjsh_filesystem::Result<void>::ok();

  } catch (const nlohmann::json::parse_error& e) {
    return cjsh_filesystem::Result<void>::error(
        "Failed to parse bookmark database JSON: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return cjsh_filesystem::Result<void>::error(
        "Error loading bookmark database: " + std::string(e.what()));
  }
}

std::string BookmarkDatabase::time_to_iso_string(
    const std::chrono::system_clock::time_point& tp) const {
  auto time_t = std::chrono::system_clock::to_time_t(tp);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

std::chrono::system_clock::time_point BookmarkDatabase::time_from_iso_string(
    const std::string& iso_str) const {
  std::tm tm = {};
  std::stringstream ss(iso_str);
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

  if (ss.fail()) {
    // If parsing fails, return current time
    return std::chrono::system_clock::now();
  }

  auto time_t = std::mktime(&tm);
  return std::chrono::system_clock::from_time_t(time_t);
}

cjsh_filesystem::Result<void> BookmarkDatabase::add_bookmark(
    const std::string& name, const std::string& path) {
  // Validate the path exists and is a directory
  try {
    std::filesystem::path fs_path(path);
    if (!std::filesystem::exists(fs_path)) {
      return cjsh_filesystem::Result<void>::error("Path does not exist: " +
                                                  path);
    }

    if (!std::filesystem::is_directory(fs_path)) {
      return cjsh_filesystem::Result<void>::error("Path is not a directory: " +
                                                  path);
    }

    // Canonicalize the path
    std::string canonical_path = std::filesystem::canonical(fs_path).string();

    // Check if bookmark already exists
    if (bookmarks_.find(name) != bookmarks_.end()) {
      // Update existing bookmark
      bookmarks_[name].path = canonical_path;
      bookmarks_[name].last_accessed = std::chrono::system_clock::now();
      bookmarks_[name].access_count++;
    } else {
      // Create new bookmark
      BookmarkEntry entry(canonical_path);
      bookmarks_[name] = entry;
    }

    dirty_ = true;
    return cjsh_filesystem::Result<void>::ok();

  } catch (const std::filesystem::filesystem_error& e) {
    return cjsh_filesystem::Result<void>::error("Filesystem error: " +
                                                std::string(e.what()));
  }
}

cjsh_filesystem::Result<void> BookmarkDatabase::remove_bookmark(
    const std::string& name) {
  auto it = bookmarks_.find(name);
  if (it == bookmarks_.end()) {
    return cjsh_filesystem::Result<void>::error("Bookmark not found: " + name);
  }

  bookmarks_.erase(it);
  dirty_ = true;
  return cjsh_filesystem::Result<void>::ok();
}

std::optional<std::string> BookmarkDatabase::get_bookmark(
    const std::string& name) {
  auto it = bookmarks_.find(name);
  if (it == bookmarks_.end()) {
    return std::nullopt;
  }

  // Update access information
  it->second.last_accessed = std::chrono::system_clock::now();
  it->second.access_count++;
  dirty_ = true;

  return it->second.path;
}

std::unordered_map<std::string, std::string>
BookmarkDatabase::get_all_bookmarks() {
  std::unordered_map<std::string, std::string> result;
  for (const auto& [name, entry] : bookmarks_) {
    result[name] = entry.path;
  }
  return result;
}

std::vector<std::string> BookmarkDatabase::search_bookmarks(
    const std::string& pattern) {
  std::vector<std::string> matches;

  try {
    std::regex regex_pattern(pattern, std::regex_constants::icase);

    for (const auto& [name, entry] : bookmarks_) {
      if (std::regex_search(name, regex_pattern) ||
          std::regex_search(entry.path, regex_pattern)) {
        matches.push_back(name);
      }
    }
  } catch (const std::regex_error&) {
    // If regex fails, fall back to simple substring search
    std::string lower_pattern = pattern;
    std::transform(lower_pattern.begin(), lower_pattern.end(),
                   lower_pattern.begin(), ::tolower);

    for (const auto& [name, entry] : bookmarks_) {
      std::string lower_name = name;
      std::string lower_path = entry.path;
      std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                     ::tolower);
      std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(),
                     ::tolower);

      if (lower_name.find(lower_pattern) != std::string::npos ||
          lower_path.find(lower_pattern) != std::string::npos) {
        matches.push_back(name);
      }
    }
  }

  return matches;
}

std::vector<std::pair<std::string, std::string>>
BookmarkDatabase::get_most_used_bookmarks(int limit) {
  std::vector<std::pair<std::string, std::string>> bookmarks_with_count;

  for (const auto& [name, entry] : bookmarks_) {
    bookmarks_with_count.emplace_back(name, entry.path);
  }

  // Sort by access count (descending)
  std::sort(bookmarks_with_count.begin(), bookmarks_with_count.end(),
            [this](const auto& a, const auto& b) {
              auto it_a = bookmarks_.find(a.first);
              auto it_b = bookmarks_.find(b.first);
              return it_a->second.access_count > it_b->second.access_count;
            });

  // Limit results
  if (limit > 0 && bookmarks_with_count.size() > static_cast<size_t>(limit)) {
    bookmarks_with_count.resize(limit);
  }

  return bookmarks_with_count;
}

void BookmarkDatabase::update_bookmark_access(const std::string& name) {
  auto it = bookmarks_.find(name);
  if (it != bookmarks_.end()) {
    it->second.last_accessed = std::chrono::system_clock::now();
    it->second.access_count++;
    dirty_ = true;
  }
}

cjsh_filesystem::Result<void> BookmarkDatabase::cleanup_invalid_bookmarks() {
  std::vector<std::string> invalid_bookmarks;

  for (const auto& [name, entry] : bookmarks_) {
    if (!std::filesystem::exists(entry.path) ||
        !std::filesystem::is_directory(entry.path)) {
      invalid_bookmarks.push_back(name);
    }
  }

  for (const auto& name : invalid_bookmarks) {
    bookmarks_.erase(name);
  }

  if (!invalid_bookmarks.empty()) {
    dirty_ = true;
  }

  return cjsh_filesystem::Result<void>::ok();
}

size_t BookmarkDatabase::size() const {
  return bookmarks_.size();
}

bool BookmarkDatabase::empty() const {
  return bookmarks_.empty();
}

cjsh_filesystem::Result<void> BookmarkDatabase::import_from_map(
    const std::unordered_map<std::string, std::string>& old_bookmarks) {
  for (const auto& [name, path] : old_bookmarks) {
    auto result = add_bookmark(name, path);
    if (result.is_error()) {
      // Log error but continue with other bookmarks
      print_error(
          {ErrorType::RUNTIME_ERROR,
           "bookmark",
           "Failed to import bookmark '" + name + "': " + result.error(),
           {}});
    }
  }

  return cjsh_filesystem::Result<void>::ok();
}

}  // namespace bookmark_database