#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cjsh_filesystem.h"

namespace bookmark_database {

struct BookmarkEntry {
    std::string path;
    std::chrono::system_clock::time_point added_time;
    std::chrono::system_clock::time_point last_accessed;
    int access_count;

    BookmarkEntry() : access_count(0) {
    }
    explicit BookmarkEntry(const std::string& p) : path(p), access_count(1) {
        auto now = std::chrono::system_clock::now();
        added_time = now;
        last_accessed = now;
    }
};

class BookmarkDatabase {
   public:
    BookmarkDatabase();
    ~BookmarkDatabase();

    cjsh_filesystem::Result<void> load();
    cjsh_filesystem::Result<void> save();

    void set_max_bookmarks(size_t max_bookmarks) {
        MAX_BOOKMARKS = max_bookmarks;
        enforce_bookmark_limit();
    }

    size_t get_max_bookmarks() const {
        return MAX_BOOKMARKS;
    }

    cjsh_filesystem::Result<void> add_bookmark(const std::string& name, const std::string& path);
    cjsh_filesystem::Result<void> remove_bookmark(const std::string& name);
    std::optional<std::string> get_bookmark(const std::string& name);
    bool has_bookmark(const std::string& name) const;

    std::unordered_map<std::string, std::string> get_all_bookmarks();
    std::vector<std::string> search_bookmarks(const std::string& pattern);
    std::vector<std::pair<std::string, std::string>> get_most_used_bookmarks(int limit = 10);

    void update_bookmark_access(const std::string& name);
    cjsh_filesystem::Result<void> cleanup_invalid_bookmarks();
    cjsh_filesystem::Result<int> cleanup_invalid_bookmarks_with_count();
    size_t size() const;
    bool empty() const;

    cjsh_filesystem::Result<void> import_from_map(
        const std::unordered_map<std::string, std::string>& old_bookmarks);

    
    cjsh_filesystem::Result<void> add_to_blacklist(const std::string& path);
    cjsh_filesystem::Result<void> remove_from_blacklist(const std::string& path);
    bool is_blacklisted(const std::string& path) const;
    std::vector<std::string> get_blacklist() const;
    cjsh_filesystem::Result<void> clear_blacklist();

   private:
    size_t MAX_BOOKMARKS = 10;
    std::unordered_map<std::string, BookmarkEntry> bookmarks_;
    std::unordered_set<std::string> blacklisted_paths_;
    std::string database_path_;
    bool dirty_;

    cjsh_filesystem::Result<void> ensure_database_directory();
    std::string to_text_format() const;
    cjsh_filesystem::Result<void> from_text_format(const std::string& text_content);
    std::string time_to_iso_string(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point time_from_iso_string(const std::string& iso_str) const;
    void enforce_bookmark_limit();
};

extern BookmarkDatabase g_bookmark_db;

inline cjsh_filesystem::Result<void> add_directory_bookmark(const std::string& name,
                                                            const std::string& path) {
    return g_bookmark_db.add_bookmark(name, path);
}

inline std::optional<std::string> find_directory_bookmark(const std::string& name) {
    return g_bookmark_db.get_bookmark(name);
}

inline std::unordered_map<std::string, std::string> get_directory_bookmarks() {
    return g_bookmark_db.get_all_bookmarks();
}

inline size_t get_max_bookmarks() {
    return g_bookmark_db.get_max_bookmarks();
}

inline void set_max_bookmarks(size_t max_bookmarks) {
    g_bookmark_db.set_max_bookmarks(max_bookmarks);
}

inline cjsh_filesystem::Result<void> add_path_to_blacklist(const std::string& path) {
    return g_bookmark_db.add_to_blacklist(path);
}

inline cjsh_filesystem::Result<void> remove_path_from_blacklist(const std::string& path) {
    return g_bookmark_db.remove_from_blacklist(path);
}

inline bool is_path_blacklisted(const std::string& path) {
    return g_bookmark_db.is_blacklisted(path);
}

inline std::vector<std::string> get_bookmark_blacklist() {
    return g_bookmark_db.get_blacklist();
}

inline cjsh_filesystem::Result<void> clear_bookmark_blacklist() {
    return g_bookmark_db.clear_blacklist();
}

}  