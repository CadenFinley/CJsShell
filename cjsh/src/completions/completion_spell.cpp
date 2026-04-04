/*
  completion_spell.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "completion_spell.h"

#include <algorithm>
#include <string>
#include <unordered_map>

#include "completion_tracker.h"
#include "edit_distance_utils.h"
#include "isocline.h"

namespace completion_spell {

bool is_adjacent_transposition(const std::string& a, const std::string& b) {
    if (a.length() != b.length() || a.length() < 2) {
        return false;
    }

    size_t first_diff = std::string::npos;

    for (size_t i = 0; i < a.length(); ++i) {
        if (a[i] != b[i]) {
            if (first_diff == std::string::npos) {
                first_diff = i;
            } else {
                if (i == first_diff + 1 && a[first_diff] == b[i] && a[i] == b[first_diff]) {
                    for (size_t k = i + 1; k < a.length(); ++k) {
                        if (a[k] != b[k]) {
                            return false;
                        }
                    }
                    return true;
                }
                return false;
            }
        }
    }

    return false;
}

int compute_edit_distance_with_limit(const std::string& source, const std::string& target,
                                     int max_distance) {
    return edit_distance_utils::levenshtein_distance_with_limit(source, target, max_distance);
}

bool should_consider_spell_correction(const std::string& normalized_prefix) {
    return normalized_prefix.length() >= 2;
}

std::vector<SpellCorrectionMatch> order_spell_correction_matches(
    const std::unordered_map<std::string, SpellCorrectionMatch>& matches) {
    std::vector<SpellCorrectionMatch> ordered_matches;
    ordered_matches.reserve(matches.size());

    for (const auto& entry : matches) {
        ordered_matches.push_back(entry.second);
    }

    std::sort(ordered_matches.begin(), ordered_matches.end(),
              [](const SpellCorrectionMatch& a, const SpellCorrectionMatch& b) {
                  if (a.distance != b.distance) {
                      return a.distance < b.distance;
                  }
                  if (a.is_transposition != b.is_transposition) {
                      return a.is_transposition && !b.is_transposition;
                  }
                  if (a.shared_prefix_len != b.shared_prefix_len) {
                      return a.shared_prefix_len > b.shared_prefix_len;
                  }
                  return a.candidate < b.candidate;
              });

    return ordered_matches;
}

void add_spell_correction_matches(
    ic_completion_env_t* cenv, const std::unordered_map<std::string, SpellCorrectionMatch>& matches,
    size_t prefix_length) {
    auto ordered_matches = order_spell_correction_matches(matches);

    const size_t kMaxSpellMatches = 10;
    size_t added = 0;

    for (const auto& match : ordered_matches) {
        if (completion_tracker::completion_limit_hit()) {
            return;
        }
        long delete_before = static_cast<long>(prefix_length);
        if (!completion_tracker::safe_add_completion_prim_with_source(
                cenv, match.candidate.c_str(), nullptr, nullptr, "spell", delete_before, 0)) {
            return;
        }
        if (ic_stop_completing(cenv)) {
            return;
        }
        if (++added >= kMaxSpellMatches) {
            return;
        }
    }
}

}  // namespace completion_spell
