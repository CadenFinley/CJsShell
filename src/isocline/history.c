/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "isocline.h"
#include "stringbuf.h"

#define IC_DEFAULT_HISTORY (200)
#define IC_ABSOLUTE_MAX_HISTORY (5000)

struct history_s {
    ssize_t count;
    ssize_t len;
    const char** elems;
    const char* fname;
    alloc_t* mem;
    bool allow_duplicates;
};

static bool history_char_is_blank(char c) {
    return (c == ' ' || c == '\t');
}

static ssize_t history_find_first_content(const char* entry, ssize_t len) {
    bool line_has_content = false;
    ssize_t line_start = 0;

    for (ssize_t i = 0; i < len; i++) {
        char c = entry[i];
        if (c == '\n' || c == '\r') {
            if (line_has_content) {
                return line_start;
            }
            line_start = i + 1;
            line_has_content = false;
        } else if (!history_char_is_blank(c)) {
            line_has_content = true;
        }
    }

    if (line_has_content) {
        return line_start;
    }

    return len;
}

static ssize_t history_find_last_content(const char* entry, ssize_t len) {
    ssize_t last_non_empty_end = 0;
    bool line_has_content = false;

    for (ssize_t i = 0; i < len; i++) {
        char c = entry[i];
        if (c == '\n' || c == '\r') {
            if (line_has_content) {
                last_non_empty_end = i;
            }
            line_has_content = false;
        } else if (!history_char_is_blank(c)) {
            line_has_content = true;
        }
    }

    if (line_has_content) {
        last_non_empty_end = len;
    }

    return last_non_empty_end;
}

static char* history_entry_dup_trimmed(alloc_t* mem, const char* entry) {
    if (entry == NULL) {
        return NULL;
    }

    ssize_t len = ic_strlen(entry);
    ssize_t start = history_find_first_content(entry, len);

    if (start >= len) {
        return mem_strdup(mem, "");
    }

    ssize_t end = history_find_last_content(entry, len);

    if (start == 0 && end == len) {
        return mem_strdup(mem, entry);
    }

    return mem_strndup(mem, entry + start, end - start);
}

ic_private history_t* history_new(alloc_t* mem) {
    history_t* h = mem_zalloc_tp(mem, history_t);
    h->mem = mem;
    return h;
}

ic_private void history_free(history_t* h) {
    if (h == NULL)
        return;
    history_clear(h);
    if (h->len > 0) {
        mem_free(h->mem, h->elems);
        h->elems = NULL;
        h->len = 0;
    }
    mem_free(h->mem, h->fname);
    h->fname = NULL;
    mem_free(h->mem, h);
}

ic_private bool history_enable_duplicates(history_t* h, bool enable) {
    bool prev = h->allow_duplicates;
    h->allow_duplicates = enable;
    return prev;
}

ic_private ssize_t history_count(const history_t* h) {
    return h->count;
}

ic_private bool history_update(history_t* h, const char* entry) {
    if (entry == NULL)
        return false;
    history_remove_last(h);
    history_push(h, entry);

    return true;
}

static void history_delete_at(history_t* h, ssize_t idx) {
    if (idx < 0 || idx >= h->count)
        return;
    mem_free(h->mem, h->elems[idx]);
    for (ssize_t i = idx + 1; i < h->count; i++) {
        h->elems[i - 1] = h->elems[i];
    }
    h->count--;
}

ic_private bool history_push(history_t* h, const char* entry) {
    if (h->len <= 0 || entry == NULL)
        return false;
    char* normalized = history_entry_dup_trimmed(h->mem, entry);
    if (normalized == NULL)
        return false;

    if (!h->allow_duplicates) {
        for (int i = 0; i < h->count; i++) {
            if (strcmp(h->elems[i], normalized) == 0) {
                history_delete_at(h, i);
                i--;
            }
        }
    }

    if (h->count == h->len) {
        history_delete_at(h, 0);
    }
    assert(h->count < h->len);
    h->elems[h->count] = normalized;
    h->count++;
    return true;
}

static void history_remove_last_n(history_t* h, ssize_t n) {
    if (n <= 0)
        return;
    if (n > h->count)
        n = h->count;
    for (ssize_t i = h->count - n; i < h->count; i++) {
        mem_free(h->mem, h->elems[i]);
    }
    h->count -= n;
    assert(h->count >= 0);
}

ic_private void history_remove_last(history_t* h) {
    history_remove_last_n(h, 1);
}

ic_private void history_clear(history_t* h) {
    history_remove_last_n(h, h->count);
}

ic_private const char* history_get(const history_t* h, ssize_t n) {
    if (n < 0 || n >= h->count)
        return NULL;
    return h->elems[h->count - n - 1];
}

ic_private bool history_search(const history_t* h, ssize_t from, const char* search, bool backward,
                               ssize_t* hidx, ssize_t* hpos) {
    const char* p = NULL;
    ssize_t i;
    if (backward) {
        for (i = from; i < h->count; i++) {
            p = strstr(history_get(h, i), search);
            if (p != NULL)
                break;
        }
    } else {
        for (i = from; i >= 0; i--) {
            p = strstr(history_get(h, i), search);
            if (p != NULL)
                break;
        }
    }
    if (p == NULL)
        return false;
    if (hidx != NULL)
        *hidx = i;
    if (hpos != NULL)
        *hpos = (p - history_get(h, i));
    return true;
}

ic_private bool history_search_prefix(const history_t* h, ssize_t from, const char* prefix,
                                      bool backward, ssize_t* hidx) {
    if (prefix == NULL || h == NULL)
        return false;

    const size_t prefix_len = strlen(prefix);
    if (prefix_len == 0) {
        if (backward) {
            if (from < h->count) {
                if (hidx != NULL)
                    *hidx = from;
                return true;
            }
        } else {
            if (from >= 0) {
                if (hidx != NULL)
                    *hidx = from;
                return true;
            }
        }
        return false;
    }

    ssize_t i;
    if (backward) {
        for (i = from; i < h->count; i++) {
            const char* entry = history_get(h, i);
            if (entry != NULL && strncmp(entry, prefix, prefix_len) == 0) {
                if (hidx != NULL)
                    *hidx = i;
                return true;
            }
        }
    } else {
        for (i = from; i >= 0; i--) {
            const char* entry = history_get(h, i);
            if (entry != NULL && strncmp(entry, prefix, prefix_len) == 0) {
                if (hidx != NULL)
                    *hidx = i;
                return true;
            }
        }
    }
    return false;
}

static int fuzzy_match_score(const char* entry, const char* query, ssize_t* match_pos,
                             ssize_t* match_len) {
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
        char e_lower = (*e >= 'A' && *e <= 'Z') ? (*e + 32) : *e;
        char q_lower = (*q >= 'A' && *q <= 'Z') ? (*q + 32) : *q;

        if (e_lower == q_lower) {
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

            if (*e == *q) {
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

    score -= (int)(strlen(entry) / 10);

    if (match_pos)
        *match_pos = first_match;
    if (match_len)
        *match_len = (first_match >= 0 && last_match >= 0) ? (last_match - first_match + 1) : 0;

    return score;
}

static int compare_matches(const void* a, const void* b) {
    const history_match_t* ma = (const history_match_t*)a;
    const history_match_t* mb = (const history_match_t*)b;

    if (mb->score != ma->score) {
        return mb->score - ma->score;
    }

    return (int)(mb->hidx - ma->hidx);
}

ic_private bool history_fuzzy_search(const history_t* h, const char* query,
                                     history_match_t* matches, ssize_t max_matches,
                                     ssize_t* match_count) {
    if (h == NULL || query == NULL || matches == NULL || max_matches <= 0) {
        if (match_count)
            *match_count = 0;
        return false;
    }

    if (query[0] == '\0') {
        ssize_t count = 0;
        for (ssize_t i = 0; i < h->count && count < max_matches; i++) {
            matches[count].hidx = i;
            matches[count].score = (int)(100 - i);
            matches[count].match_pos = 0;
            matches[count].match_len = 0;
            count++;
        }
        if (match_count)
            *match_count = count;
        return count > 0;
    }

    ssize_t count = 0;
    for (ssize_t i = 0; i < h->count; i++) {
        const char* entry = history_get(h, i);
        if (entry == NULL)
            continue;

        ssize_t mpos = 0, mlen = 0;
        int score = fuzzy_match_score(entry, query, &mpos, &mlen);

        if (score >= 0) {
            score += (int)(i / 10);

            if (count < max_matches) {
                matches[count].hidx = i;
                matches[count].score = score;
                matches[count].match_pos = mpos;
                matches[count].match_len = mlen;
                count++;
            } else {
                ssize_t worst_idx = 0;
                int worst_score = matches[0].score;
                for (ssize_t j = 1; j < max_matches; j++) {
                    if (matches[j].score < worst_score) {
                        worst_score = matches[j].score;
                        worst_idx = j;
                    }
                }

                if (score > worst_score) {
                    matches[worst_idx].hidx = i;
                    matches[worst_idx].score = score;
                    matches[worst_idx].match_pos = mpos;
                    matches[worst_idx].match_len = mlen;
                }
            }
        }
    }

    if (count > 1) {
        qsort(matches, count, sizeof(history_match_t), compare_matches);
    }

    if (match_count)
        *match_count = count;
    return count > 0;
}

ic_private void history_load_from(history_t* h, const char* fname, long max_entries) {
    history_clear(h);
    h->fname = mem_strdup(h->mem, fname);
    if (max_entries == 0) {
        assert(h->elems == NULL);
        return;
    }
    if (max_entries < 0)
        max_entries = IC_DEFAULT_HISTORY;
    else if (max_entries > IC_ABSOLUTE_MAX_HISTORY)
        max_entries = IC_ABSOLUTE_MAX_HISTORY;
    h->elems = (const char**)mem_zalloc_tp_n(h->mem, char*, max_entries);
    if (h->elems == NULL)
        return;
    h->len = max_entries;
    history_load(h);
}

static char from_xdigit(int c) {
    if (c >= '0' && c <= '9')
        return (char)(c - '0');
    if (c >= 'A' && c <= 'F')
        return (char)(10 + (c - 'A'));
    if (c >= 'a' && c <= 'f')
        return (char)(10 + (c - 'a'));
    return 0;
}

static char to_xdigit(uint8_t c) {
    if (c <= 9)
        return ((char)c + '0');
    if (c >= 10 && c <= 15)
        return ((char)c - 10 + 'A');
    return '0';
}

static bool ic_isxdigit(int c) {
    return ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9'));
}

static bool history_read_entry(history_t* h, FILE* f, stringbuf_t* sbuf) {
    sbuf_clear(sbuf);
    while (!feof(f)) {
        int c = fgetc(f);
        if (c == EOF || c == '\n')
            break;
        if (c == '\\') {
            c = fgetc(f);
            if (c == 'n') {
                sbuf_append(sbuf, "\n");
            } else if (c == 'r') {
            } else if (c == 't') {
                sbuf_append(sbuf, "\t");
            } else if (c == '\\') {
                sbuf_append(sbuf, "\\");
            } else if (c == 'x') {
                int c1 = fgetc(f);
                int c2 = fgetc(f);
                if (ic_isxdigit(c1) && ic_isxdigit(c2)) {
                    char chr = from_xdigit(c1) * 16 + from_xdigit(c2);
                    sbuf_append_char(sbuf, chr);
                } else
                    return false;
            } else
                return false;
        } else
            sbuf_append_char(sbuf, (char)c);
    }
    if (sbuf_len(sbuf) == 0 || sbuf_string(sbuf)[0] == '#')
        return true;
    return history_push(h, sbuf_string(sbuf));
}

static bool history_write_entry(const char* entry, FILE* f, stringbuf_t* sbuf) {
    sbuf_clear(sbuf);

    while (entry != NULL && *entry != 0) {
        char c = *entry++;
        if (c == '\\') {
            sbuf_append(sbuf, "\\\\");
        } else if (c == '\n') {
            sbuf_append(sbuf, "\\n");
        } else if (c == '\r') {
        } else if (c == '\t') {
            sbuf_append(sbuf, "\\t");
        } else if (c < ' ' || c > '~' || c == '#') {
            char c1 = to_xdigit((uint8_t)c / 16);
            char c2 = to_xdigit((uint8_t)c % 16);
            sbuf_append(sbuf, "\\x");
            sbuf_append_char(sbuf, c1);
            sbuf_append_char(sbuf, c2);
        } else
            sbuf_append_char(sbuf, c);
    }

    if (sbuf_len(sbuf) > 0) {
        sbuf_append(sbuf, "\n");
        fputs(sbuf_string(sbuf), f);
    }
    return true;
}

ic_private void history_load(history_t* h) {
    if (h->fname == NULL)
        return;
    FILE* f = fopen(h->fname, "r");
    if (f == NULL)
        return;
    stringbuf_t* sbuf = sbuf_new(h->mem);
    if (sbuf != NULL) {
        while (!feof(f)) {
            if (!history_read_entry(h, f, sbuf))
                break;
        }
        sbuf_free(sbuf);
    }
    fclose(f);
}

#include <time.h>
ic_private void history_save(const history_t* h) {
    if (h->fname == NULL)
        return;
    if (h->count <= 0)
        return;

    FILE* f = fopen(h->fname, "a");
    if (f == NULL)
        return;
#ifndef _WIN32
    chmod(h->fname, S_IRUSR | S_IWUSR);
#endif

    time_t t = time(NULL);
    fprintf(f, "# %lld\n", (long long)t);

    stringbuf_t* sbuf = sbuf_new(h->mem);
    if (sbuf != NULL) {
        history_write_entry(h->elems[h->count - 1], f, sbuf);
        sbuf_free(sbuf);
    }
    fclose(f);
}
