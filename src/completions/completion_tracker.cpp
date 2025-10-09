#include "completion_tracker.h"

#include <cstring>
#include <string>

#include "isocline.h"

namespace completion_tracker {

static const size_t MAX_COMPLETION_TRACKER_ENTRIES = 250;
static const size_t MAX_TOTAL_COMPLETIONS = 50;

thread_local static CompletionTracker* g_current_completion_tracker = nullptr;

SourcePriority get_source_priority(const char* source) {
    if (source == nullptr)
        return PRIORITY_UNKNOWN;

    if (strcmp(source, "history") == 0)
        return PRIORITY_HISTORY;
    if (strcmp(source, "bookmark") == 0)
        return PRIORITY_BOOKMARK;
    if (strcmp(source, "file") == 0)
        return PRIORITY_FILE;
    if (strcmp(source, "directory") == 0)
        return PRIORITY_DIRECTORY;
    if (strcmp(source, "function") == 0)
        return PRIORITY_FUNCTION;

    return PRIORITY_UNKNOWN;
}

CompletionTracker::CompletionTracker(ic_completion_env_t* env, const char* prefix)
    : cenv(env), original_prefix(prefix) {
    added_completions.reserve(128);
}

CompletionTracker::~CompletionTracker() {
    added_completions.clear();
}

bool CompletionTracker::has_reached_completion_limit() const {
    return total_completions_added >= MAX_TOTAL_COMPLETIONS;
}

std::string CompletionTracker::calculate_final_result(const char* completion_text,
                                                       long delete_before) const {
    std::string prefix_str = original_prefix;

    if (delete_before > 0 && delete_before <= static_cast<long>(prefix_str.length())) {
        prefix_str = prefix_str.substr(0, prefix_str.length() - delete_before);
    }

    return prefix_str + completion_text;
}

bool CompletionTracker::would_create_duplicate(const char* completion_text, const char* source,
                                                long delete_before) {
    if (added_completions.size() >= MAX_COMPLETION_TRACKER_ENTRIES) {
        return true;
    }

    std::string final_result = calculate_final_result(completion_text, delete_before);
    auto it = added_completions.find(final_result);

    if (it == added_completions.end()) {
        if ((source != nullptr) && strcmp(source, "bookmark") == 0) {
            std::string directory_result = final_result + "/";
            auto dir_it = added_completions.find(directory_result);
            if (dir_it != added_completions.end()) {
                return true;
            }
        } else if ((source != nullptr) && strcmp(source, "directory") == 0) {
            std::string bookmark_result = final_result;
            if (bookmark_result.back() == '/') {
                bookmark_result.pop_back();
                auto bookmark_it = added_completions.find(bookmark_result);
                if (bookmark_it != added_completions.end() &&
                    bookmark_it->second == PRIORITY_BOOKMARK) {
                    added_completions.erase(bookmark_it);
                }
            }
        }

        return false;
    }

    SourcePriority existing_priority = it->second;
    SourcePriority new_priority = get_source_priority(source);

    return new_priority <= existing_priority;
}

bool CompletionTracker::add_completion_if_unique(const char* completion_text) {
    const char* source = nullptr;
    if (has_reached_completion_limit()) {
        return true;
    }
    if (would_create_duplicate(completion_text, source, 0)) {
        return true;
    }

    std::string final_result = calculate_final_result(completion_text, 0);
    added_completions[final_result] = get_source_priority(source);
    total_completions_added++;
    return ic_add_completion_ex_with_source(cenv, completion_text, nullptr, nullptr, source);
}

bool CompletionTracker::add_completion_prim_if_unique(const char* completion_text,
                                                       const char* display, const char* help,
                                                       long delete_before, long delete_after) {
    const char* source = nullptr;
    if (has_reached_completion_limit()) {
        return true;
    }
    if (would_create_duplicate(completion_text, source, delete_before)) {
        return true;
    }

    std::string final_result = calculate_final_result(completion_text, delete_before);
    added_completions[final_result] = get_source_priority(source);
    total_completions_added++;
    return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source,
                                              delete_before, delete_after);
}

bool CompletionTracker::add_completion_prim_with_source_if_unique(const char* completion_text,
                                                                   const char* display,
                                                                   const char* help,
                                                                   const char* source,
                                                                   long delete_before,
                                                                   long delete_after) {
    if (has_reached_completion_limit()) {
        return true;
    }

    std::string final_result = calculate_final_result(completion_text, delete_before);
    auto it = added_completions.find(final_result);
    SourcePriority new_priority = get_source_priority(source);

    if (it == added_completions.end()) {
        if ((source != nullptr) && strcmp(source, "bookmark") == 0) {
            std::string directory_result = final_result + "/";
            auto dir_it = added_completions.find(directory_result);
            if (dir_it != added_completions.end()) {
                return true;
            }
        } else if ((source != nullptr) && strcmp(source, "directory") == 0) {
            std::string bookmark_result = final_result;
            if (bookmark_result.back() == '/') {
                bookmark_result.pop_back();
                auto bookmark_it = added_completions.find(bookmark_result);
                if (bookmark_it != added_completions.end() &&
                    bookmark_it->second == PRIORITY_BOOKMARK) {
                    added_completions.erase(bookmark_it);
                }
            }
        }

        total_completions_added++;
    } else {
        SourcePriority existing_priority = it->second;

        if (new_priority <= existing_priority) {
            return true;
        }
    }

    added_completions[final_result] = new_priority;
    return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source,
                                              delete_before, delete_after);
}

void completion_session_begin(ic_completion_env_t* cenv, const char* prefix) {
    delete g_current_completion_tracker;
    g_current_completion_tracker = new CompletionTracker(cenv, prefix);
}

void completion_session_end() {
    if (g_current_completion_tracker != nullptr) {
        delete g_current_completion_tracker;
        g_current_completion_tracker = nullptr;
    }
}

CompletionTracker* get_current_tracker() {
    return g_current_completion_tracker;
}

bool safe_add_completion_with_source(ic_completion_env_t* cenv, const char* completion_text,
                                      const char* source) {
    if ((g_current_completion_tracker != nullptr) &&
        g_current_completion_tracker->has_reached_completion_limit()) {
        return true;
    }
    return ic_add_completion_ex_with_source(cenv, completion_text, nullptr, nullptr, source);
}

bool safe_add_completion_prim_with_source(ic_completion_env_t* cenv, const char* completion_text,
                                           const char* display, const char* help, const char* source,
                                           long delete_before, long delete_after) {
    if (g_current_completion_tracker != nullptr) {
        return g_current_completion_tracker->add_completion_prim_with_source_if_unique(
            completion_text, display, help, source, delete_before, delete_after);
    }
    return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source,
                                              delete_before, delete_after);
}

bool completion_limit_hit() {
    return (g_current_completion_tracker != nullptr) &&
           g_current_completion_tracker->has_reached_completion_limit();
}

bool completion_limit_hit_with_log(const char* label) {
    (void)label;
    return completion_limit_hit();
}

}  // namespace completion_tracker
