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
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "completion_tracker.h"
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
    const size_t source_length = source.length();
    const size_t target_length = target.length();

    if (std::abs(static_cast<int>(source_length) - static_cast<int>(target_length)) >
        max_distance) {
        return max_distance + 1;
    }

    std::vector<int> previous_row(target_length + 1);
    std::vector<int> current_row(target_length + 1);

    for (size_t j = 0; j <= target_length; ++j) {
        previous_row[j] = static_cast<int>(j);
    }

    for (size_t i = 1; i <= source_length; ++i) {
        current_row[0] = static_cast<int>(i);
        int row_min = current_row[0];

        for (size_t j = 1; j <= target_length; ++j) {
            int cost = (source[i - 1] == target[j - 1]) ? 0 : 1;
            current_row[j] =
                std::min({previous_row[j] + 1, current_row[j - 1] + 1, previous_row[j - 1] + cost});
            row_min = std::min(row_min, current_row[j]);
        }

        if (row_min > max_distance) {
            return max_distance + 1;
        }

        std::swap(previous_row, current_row);
    }

    return previous_row[target_length];
}

bool should_consider_spell_correction(const std::string& normalized_prefix) {
    return normalized_prefix.length() >= 2;
}

void add_spell_correction_matches(
    ic_completion_env_t* cenv, const std::unordered_map<std::string, SpellCorrectionMatch>& matches,
    size_t prefix_length) {
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

    const size_t kMaxSpellMatches = 10;
    size_t added = 0;

    for (const auto& match : ordered_matches) {
        if (completion_tracker::completion_limit_hit_with_log("spell correction")) {
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
