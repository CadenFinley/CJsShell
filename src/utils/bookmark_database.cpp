#include "utils/bookmark_database.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include "error_out.h"

namespace bookmark_database {

BookmarkDatabase g_bookmark_db;

BookmarkDatabase::BookmarkDatabase() : dirty_(false) {
    database_path_ = (cjsh_filesystem::g_cjsh_cache_path / "directory_bookmarks.txt").string();
}

BookmarkDatabase::~BookmarkDatabase() {
    if (dirty_) {
        auto result = save();
        if (result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR, "bookmark", "Failed to save bookmark database: " + result.error(), {}});
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
        return cjsh_filesystem::Result<void>::error("Failed to create database directory: " + std::string(e.what()));
    }
}

cjsh_filesystem::Result<void> BookmarkDatabase::load() {
    auto dir_result = ensure_database_directory();
    if (dir_result.is_error()) {
        return dir_result;
    }

    if (!std::filesystem::exists(database_path_)) {
        bookmarks_.clear();
        dirty_ = false;
        return cjsh_filesystem::Result<void>::ok();
    }

    auto content_result = cjsh_filesystem::FileOperations::read_file_content(database_path_);
    if (content_result.is_error()) {
        return cjsh_filesystem::Result<void>::error("Failed to read bookmark database: " + content_result.error());
    }

    auto parse_result = from_text_format(content_result.value());
    if (parse_result.is_error()) {
        return parse_result;
    }

    dirty_ = false;
    return cjsh_filesystem::Result<void>::ok();
}

cjsh_filesystem::Result<void> BookmarkDatabase::save() {
    auto dir_result = ensure_database_directory();
    if (dir_result.is_error()) {
        return dir_result;
    }

    std::string text_content = to_text_format();

    auto write_result = cjsh_filesystem::FileOperations::write_file_content(database_path_, text_content);
    if (write_result.is_error()) {
        return cjsh_filesystem::Result<void>::error("Failed to write bookmark database: " + write_result.error());
    }

    dirty_ = false;
    return cjsh_filesystem::Result<void>::ok();
}

std::string BookmarkDatabase::to_text_format() const {
    std::stringstream ss;

    ss << "last_updated=" << time_to_iso_string(std::chrono::system_clock::now()) << "\n";
    ss << "# Format: name|path|access_count|added_time|last_accessed\n";

    for (const auto& [name, entry] : bookmarks_) {
        ss << name << "|" << entry.path << "|" << entry.access_count << "|" << time_to_iso_string(entry.added_time) << "|"
           << time_to_iso_string(entry.last_accessed) << "\n";
    }

    return ss.str();
}

cjsh_filesystem::Result<void> BookmarkDatabase::from_text_format(const std::string& text_content) {
    try {
        std::stringstream ss(text_content);
        std::string line;

        bookmarks_.clear();

        while (std::getline(ss, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            if (line.substr(0, 8) == "version=") {
                continue;
            }

            if (line.substr(0, 13) == "last_updated=") {
                continue;
            }

            std::vector<std::string> parts;
            std::stringstream line_ss(line);
            std::string part;

            while (std::getline(line_ss, part, '|')) {
                parts.push_back(part);
            }

            if (parts.size() >= 2) {
                BookmarkEntry entry;
                std::string name = parts[0];
                entry.path = parts[1];

                if (parts.size() > 2 && !parts[2].empty()) {
                    try {
                        entry.access_count = std::stoi(parts[2]);
                    } catch (...) {
                        entry.access_count = 1;
                    }
                } else {
                    entry.access_count = 1;
                }

                if (parts.size() > 3 && !parts[3].empty()) {
                    entry.added_time = time_from_iso_string(parts[3]);
                } else {
                    entry.added_time = std::chrono::system_clock::now();
                }

                if (parts.size() > 4 && !parts[4].empty()) {
                    entry.last_accessed = time_from_iso_string(parts[4]);
                } else {
                    entry.last_accessed = entry.added_time;
                }

                bookmarks_[name] = entry;
            }
        }

        return cjsh_filesystem::Result<void>::ok();

    } catch (const std::exception& e) {
        return cjsh_filesystem::Result<void>::error("Failed to parse bookmark database: " + std::string(e.what()));
    }
}

std::string BookmarkDatabase::time_to_iso_string(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point BookmarkDatabase::time_from_iso_string(const std::string& iso_str) const {
    std::tm tm = {};
    std::stringstream ss(iso_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    auto time_t = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t);
}

cjsh_filesystem::Result<void> BookmarkDatabase::add_bookmark(const std::string& name, const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        if (!std::filesystem::exists(fs_path)) {
            return cjsh_filesystem::Result<void>::error("Path does not exist: " + path);
        }

        if (!std::filesystem::is_directory(fs_path)) {
            return cjsh_filesystem::Result<void>::error("Path is not a directory: " + path);
        }

        std::string canonical_path = std::filesystem::canonical(fs_path).string();

        if (bookmarks_.find(name) != bookmarks_.end()) {
            bookmarks_[name].path = canonical_path;
            bookmarks_[name].last_accessed = std::chrono::system_clock::now();
            bookmarks_[name].access_count++;
        } else {
            BookmarkEntry entry(canonical_path);
            bookmarks_[name] = entry;
        }

        dirty_ = true;
        return cjsh_filesystem::Result<void>::ok();

    } catch (const std::filesystem::filesystem_error& e) {
        return cjsh_filesystem::Result<void>::error("Filesystem error: " + std::string(e.what()));
    }
}

cjsh_filesystem::Result<void> BookmarkDatabase::remove_bookmark(const std::string& name) {
    auto it = bookmarks_.find(name);
    if (it == bookmarks_.end()) {
        return cjsh_filesystem::Result<void>::error("Bookmark not found: " + name);
    }

    bookmarks_.erase(it);
    dirty_ = true;
    return cjsh_filesystem::Result<void>::ok();
}

std::optional<std::string> BookmarkDatabase::get_bookmark(const std::string& name) {
    auto it = bookmarks_.find(name);
    if (it == bookmarks_.end()) {
        return std::nullopt;
    }

    it->second.last_accessed = std::chrono::system_clock::now();
    it->second.access_count++;
    dirty_ = true;

    return it->second.path;
}

std::unordered_map<std::string, std::string> BookmarkDatabase::get_all_bookmarks() {
    std::unordered_map<std::string, std::string> result;
    for (const auto& [name, entry] : bookmarks_) {
        result[name] = entry.path;
    }
    return result;
}

std::vector<std::string> BookmarkDatabase::search_bookmarks(const std::string& pattern) {
    std::vector<std::string> matches;

    try {
        std::regex regex_pattern(pattern, std::regex_constants::icase);

        for (const auto& [name, entry] : bookmarks_) {
            if (std::regex_search(name, regex_pattern) || std::regex_search(entry.path, regex_pattern)) {
                matches.push_back(name);
            }
        }
    } catch (const std::regex_error&) {
        std::string lower_pattern = pattern;
        std::transform(lower_pattern.begin(), lower_pattern.end(), lower_pattern.begin(), ::tolower);

        for (const auto& [name, entry] : bookmarks_) {
            std::string lower_name = name;
            std::string lower_path = entry.path;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::tolower);

            if (lower_name.find(lower_pattern) != std::string::npos || lower_path.find(lower_pattern) != std::string::npos) {
                matches.push_back(name);
            }
        }
    }

    return matches;
}

std::vector<std::pair<std::string, std::string>> BookmarkDatabase::get_most_used_bookmarks(int limit) {
    std::vector<std::pair<std::string, std::string>> bookmarks_with_count;

    for (const auto& [name, entry] : bookmarks_) {
        bookmarks_with_count.emplace_back(name, entry.path);
    }

    std::sort(bookmarks_with_count.begin(), bookmarks_with_count.end(), [this](const auto& a, const auto& b) {
        auto it_a = bookmarks_.find(a.first);
        auto it_b = bookmarks_.find(b.first);
        return it_a->second.access_count > it_b->second.access_count;
    });

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
        if (!std::filesystem::exists(entry.path) || !std::filesystem::is_directory(entry.path)) {
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

cjsh_filesystem::Result<int> BookmarkDatabase::cleanup_invalid_bookmarks_with_count() {
    std::vector<std::string> invalid_bookmarks;

    for (const auto& [name, entry] : bookmarks_) {
        if (!std::filesystem::exists(entry.path) || !std::filesystem::is_directory(entry.path)) {
            invalid_bookmarks.push_back(name);
        }
    }

    int removed_count = static_cast<int>(invalid_bookmarks.size());

    for (const auto& name : invalid_bookmarks) {
        bookmarks_.erase(name);
    }

    if (!invalid_bookmarks.empty()) {
        dirty_ = true;
    }

    return cjsh_filesystem::Result<int>::ok(removed_count);
}

size_t BookmarkDatabase::size() const {
    return bookmarks_.size();
}

bool BookmarkDatabase::empty() const {
    return bookmarks_.empty();
}

cjsh_filesystem::Result<void> BookmarkDatabase::import_from_map(const std::unordered_map<std::string, std::string>& old_bookmarks) {
    for (const auto& [name, path] : old_bookmarks) {
        auto result = add_bookmark(name, path);
        if (result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR, "bookmark", "Failed to import bookmark '" + name + "': " + result.error(), {}});
        }
    }

    return cjsh_filesystem::Result<void>::ok();
}

}  // namespace bookmark_database