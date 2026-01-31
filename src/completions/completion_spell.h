#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>

#include "completion_utils.h"
#include "isocline/isocline.h"

namespace completion_spell {

struct SpellCorrectionMatch {
    std::string candidate;
    int distance{};
    bool is_transposition{};
    size_t shared_prefix_len{};
};

bool is_adjacent_transposition(const std::string& a, const std::string& b);
int compute_edit_distance_with_limit(const std::string& source, const std::string& target,
                                     int max_distance);
bool should_consider_spell_correction(const std::string& normalized_prefix);

template <typename Container, typename Extractor>
void collect_spell_correction_candidates(
    const Container& container, Extractor extractor,
    const std::function<bool(const std::string&)>& filter, const std::string& normalized_prefix,
    std::unordered_map<std::string, SpellCorrectionMatch>& matches) {
    for (const auto& item : container) {
        std::string candidate = extractor(item);
        if (filter && !filter(candidate)) {
            continue;
        }

        std::string normalized_candidate = completion_utils::normalize_for_comparison(candidate);
        if (normalized_candidate == normalized_prefix) {
            continue;
        }

        size_t longer_length = std::max(normalized_candidate.length(), normalized_prefix.length());
        int max_distance = longer_length >= 6 ? 3 : 2;

        bool is_transposition_match =
            is_adjacent_transposition(normalized_candidate, normalized_prefix);
        int distance =
            compute_edit_distance_with_limit(normalized_candidate, normalized_prefix, max_distance);
        if (!is_transposition_match && distance > max_distance) {
            continue;
        }

        size_t shared_prefix_len = 0;
        size_t compare_len = std::min(normalized_candidate.length(), normalized_prefix.length());
        while (shared_prefix_len < compare_len &&
               normalized_candidate[shared_prefix_len] == normalized_prefix[shared_prefix_len]) {
            shared_prefix_len++;
        }

        int effective_distance = is_transposition_match ? 1 : distance;
        auto it = matches.find(candidate);

        bool replace = false;
        if (it == matches.end()) {
            replace = true;
        } else if (effective_distance < it->second.distance) {
            replace = true;
        } else if (effective_distance == it->second.distance) {
            if (is_transposition_match && !it->second.is_transposition) {
                replace = true;
            } else if (is_transposition_match == it->second.is_transposition &&
                       shared_prefix_len > it->second.shared_prefix_len) {
                replace = true;
            }
        }

        if (replace) {
            matches[candidate] = SpellCorrectionMatch{candidate, effective_distance,
                                                      is_transposition_match, shared_prefix_len};
        }
    }
}

void add_spell_correction_matches(
    ic_completion_env_t* cenv, const std::unordered_map<std::string, SpellCorrectionMatch>& matches,
    size_t prefix_length);

}  // namespace completion_spell
