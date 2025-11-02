#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "completion_utils.h"
#include "isocline.h"

namespace completion_spell {

struct SpellCorrectionMatch {
    std::string candidate;
    int distance{};
    bool is_transposition{};
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

        bool is_transposition_match =
            is_adjacent_transposition(normalized_candidate, normalized_prefix);
        int distance = compute_edit_distance_with_limit(normalized_candidate, normalized_prefix, 2);
        if (!is_transposition_match && distance > 2) {
            continue;
        }

        int effective_distance = is_transposition_match ? 1 : distance;
        auto it = matches.find(candidate);

        if (it == matches.end() || effective_distance < it->second.distance) {
            matches[candidate] =
                SpellCorrectionMatch{candidate, effective_distance, is_transposition_match};
        }
    }
}

void add_spell_correction_matches(
    ic_completion_env_t* cenv, const std::unordered_map<std::string, SpellCorrectionMatch>& matches,
    size_t prefix_length);

}  // namespace completion_spell
