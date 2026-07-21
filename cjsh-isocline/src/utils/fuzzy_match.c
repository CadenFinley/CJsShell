/*
  fuzzy_match.c

  Shared fuzzy matching helpers for isocline menus.

  MIT License
*/

#include "fuzzy_match.h"

#include <ctype.h>

ic_private bool ic_fuzzy_char_equals(char left, char right, bool case_sensitive) {
    if (case_sensitive) {
        return (left == right);
    }
    return (ic_tolower(left) == ic_tolower(right));
}

ic_private bool ic_fuzzy_find_substring(const char* haystack, const char* needle,
                                        bool case_sensitive, ssize_t* pos_out, ssize_t* len_out) {
    if (pos_out != NULL) {
        *pos_out = -1;
    }
    if (len_out != NULL) {
        *len_out = 0;
    }
    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        return false;
    }

    ssize_t hlen = ic_strlen(haystack);
    ssize_t nlen = ic_strlen(needle);
    if (hlen <= 0 || nlen <= 0 || nlen > hlen) {
        return false;
    }

    for (ssize_t i = 0; i + nlen <= hlen; ++i) {
        bool matched = true;
        for (ssize_t j = 0; j < nlen; ++j) {
            if (!ic_fuzzy_char_equals(haystack[i + j], needle[j], case_sensitive)) {
                matched = false;
                break;
            }
        }
        if (matched) {
            if (pos_out != NULL) {
                *pos_out = i;
            }
            if (len_out != NULL) {
                *len_out = nlen;
            }
            return true;
        }
    }
    return false;
}

ic_private int ic_fuzzy_match_score(const char* entry, const char* query, ssize_t* match_pos,
                                    ssize_t* match_len, bool case_sensitive) {
    if (entry == NULL || query == NULL || query[0] == '\0') {
        return -1;
    }

    const char* e = entry;
    const char* q = query;
    ssize_t first_match = -1;
    ssize_t last_match = -1;
    ssize_t consecutive = 0;
    ssize_t max_consecutive = 0;
    int score = 0;
    bool in_match = false;

    while (*e && *q) {
        char e_compare = case_sensitive ? *e : ic_tolower(*e);
        char q_compare = case_sensitive ? *q : ic_tolower(*q);

        if (e_compare == q_compare) {
            if (first_match == -1) {
                first_match = e - entry;
            }
            last_match = e - entry;

            if (in_match) {
                consecutive++;
                score += 5;
            } else {
                consecutive = 1;
                in_match = true;
            }

            if (e == entry || *(e - 1) == ' ' || *(e - 1) == '/' || *(e - 1) == '-' ||
                *(e - 1) == '_') {
                score += 10;
            }

            if (case_sensitive) {
                if (*e == *q) {
                    score += 2;
                }
            } else {
                score += 2;
            }

            q++;
            score += 1;
        } else {
            if (consecutive > max_consecutive) {
                max_consecutive = consecutive;
            }
            consecutive = 0;
            in_match = false;
        }
        e++;
    }

    if (consecutive > max_consecutive) {
        max_consecutive = consecutive;
    }

    if (*q != '\0') {
        return -1;
    }

    score += max_consecutive * 3;

    if (first_match >= 0 && last_match >= 0) {
        ssize_t span = last_match - first_match + 1;
        score -= (int)(span / 2);
    }

    score -= (int)(ic_strlen(entry) / 10);

    if (match_pos != NULL) {
        *match_pos = first_match;
    }
    if (match_len != NULL) {
        *match_len = (first_match >= 0 && last_match >= 0) ? (last_match - first_match + 1) : 0;
    }

    return score;
}

ic_private bool ic_fuzzy_next_token(const char** cursor, const char** token_start,
                                    size_t* token_len) {
    if (cursor == NULL || *cursor == NULL || token_start == NULL || token_len == NULL) {
        return false;
    }

    const char* p = *cursor;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0') {
        *cursor = p;
        *token_start = p;
        *token_len = 0;
        return false;
    }

    const char* start = p;
    while (*p != '\0' && *p != ' ' && *p != '\t') {
        p++;
    }

    *cursor = p;
    *token_start = start;
    *token_len = (size_t)(p - start);
    return (*token_len > 0);
}

ic_private bool ic_fuzzy_trim_token(const char** token_start, size_t* token_len) {
    if (token_start == NULL || *token_start == NULL || token_len == NULL) {
        return false;
    }

    const char* start = *token_start;
    size_t len = *token_len;

    while (len > 0 && !isalnum((unsigned char)start[len - 1]) && start[len - 1] != '_' &&
           start[len - 1] != '-') {
        len--;
    }
    while (len > 0 && !isalnum((unsigned char)start[0]) && start[0] != '_' && start[0] != '-') {
        start++;
        len--;
    }

    *token_start = start;
    *token_len = len;
    return (len > 0);
}
