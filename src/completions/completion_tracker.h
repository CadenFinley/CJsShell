#pragma once

#include <string>
#include <unordered_set>

#include "isocline/isocline.h"

namespace completion_tracker {

struct CompletionTracker {
    std::unordered_set<std::string> added_completions;
    ic_completion_env_t* cenv;
    std::string original_prefix;
    size_t total_completions_added{};

    CompletionTracker(ic_completion_env_t* env, const char* prefix);
    ~CompletionTracker();

    bool has_reached_completion_limit() const;
    std::string calculate_final_result(const char* completion_text, long delete_before = 0) const;
    bool would_create_duplicate(const char* completion_text, long delete_before = 0);
    bool add_completion_prim_with_source_if_unique(const char* completion_text, const char* display,
                                                   const char* help, const char* source,
                                                   long delete_before, long delete_after);
};

void completion_session_begin(ic_completion_env_t* cenv, const char* prefix);
void completion_session_end();

bool safe_add_completion_with_source(ic_completion_env_t* cenv, const char* completion_text,
                                     const char* source);
bool safe_add_completion_prim_with_source(ic_completion_env_t* cenv, const char* completion_text,
                                          const char* display, const char* help, const char* source,
                                          long delete_before, long delete_after);

bool completion_limit_hit();
bool completion_limit_hit_with_log(const char* label);

bool set_completion_max_results(long max_results, std::string* error_message = nullptr);
long get_completion_max_results();
long get_completion_default_max_results();
long get_completion_min_allowed_results();

}  // namespace completion_tracker
