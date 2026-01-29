/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License.

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
-----------------------------------------------------------------------------*/
#include "history.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "common.h"
#include "stringbuf.h"

#define IC_DEFAULT_HISTORY (200)
#define IC_ABSOLUTE_MAX_HISTORY (5000)

static bool g_default_single_io_mode = true;

typedef struct history_list_s {
    history_entry_t* entries;
    ssize_t count;
    ssize_t capacity;
} history_list_t;

struct history_s {
    const char* fname;
    alloc_t* mem;
    bool allow_duplicates;
    ssize_t max_entries;
    char* scratch;
    ssize_t scratch_cap;
    history_list_t cache;
    bool cache_initialized;
    bool cache_dirty;
    bool single_io_mode;
    ssize_t cache_disk_count_at_load;
    time_t cache_disk_max_timestamp;
};

static bool history_is_disabled(const history_t* h) {
    return (h == NULL || h->max_entries == 0);
}

static void history_list_init(history_list_t* list) {
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void history_list_free(history_t* h, history_list_t* list) {
    if (list->entries == NULL)
        return;
    for (ssize_t i = 0; i < list->count; i++) {
        if (list->entries[i].command != NULL) {
            mem_free(h->mem, list->entries[i].command);
            list->entries[i].command = NULL;
        }
    }
    mem_free(h->mem, list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

static bool history_list_reserve(history_t* h, history_list_t* list, ssize_t needed) {
    if (needed <= list->capacity)
        return true;
    ssize_t new_capacity = (list->capacity == 0) ? 16 : list->capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    history_entry_t* new_entries =
        mem_realloc_tp(h->mem, history_entry_t, list->entries, new_capacity);
    if (new_entries == NULL)
        return false;
    list->entries = new_entries;
    list->capacity = new_capacity;
    return true;
}

static bool history_list_append(history_t* h, history_list_t* list, history_entry_t entry) {
    if (entry.command == NULL)
        return true;
    if (!history_list_reserve(h, list, list->count + 1)) {
        mem_free(h->mem, entry.command);
        return false;
    }
    list->entries[list->count] = entry;
    list->count++;
    return true;
}

static bool history_list_insert(history_t* h, history_list_t* list, ssize_t idx,
                                history_entry_t entry) {
    if (entry.command == NULL)
        return true;
    if (idx < 0)
        idx = 0;
    if (idx > list->count)
        idx = list->count;
    if (!history_list_reserve(h, list, list->count + 1)) {
        mem_free(h->mem, entry.command);
        return false;
    }
    if (idx < list->count) {
        memmove(&list->entries[idx + 1], &list->entries[idx],
                (size_t)(list->count - idx) * sizeof(history_entry_t));
    }
    list->entries[idx] = entry;
    list->count++;
    return true;
}

static void history_list_remove_at(history_t* h, history_list_t* list, ssize_t idx) {
    if (idx < 0 || idx >= list->count)
        return;
    if (list->entries[idx].command != NULL) {
        mem_free(h->mem, list->entries[idx].command);
        list->entries[idx].command = NULL;
    }
    if (idx < list->count - 1) {
        memmove(&list->entries[idx], &list->entries[idx + 1],
                (size_t)(list->count - idx - 1) * sizeof(history_entry_t));
    }
    list->count--;
}

static bool history_write_successful(int result) {
    if (result < 0) {
        debug_msg("history: stream write failed\n");
        return false;
    }
    return true;
}

static bool history_close_stream(FILE* f) {
    if (f == NULL)
        return true;
    if (fclose(f) != 0) {
        debug_msg("history: fclose failed\n");
        return false;
    }
    return true;
}

static void history_list_prune_to_max(history_t* h, history_list_t* list) {
    if (h->max_entries < 0)
        return;
    if (h->max_entries == 0) {
        while (list->count > 0) {
            history_list_remove_at(h, list, 0);
        }
        return;
    }
    while (list->count > h->max_entries) {
        history_list_remove_at(h, list, 0);
    }
}

static void history_list_remove_duplicates(history_t* h, history_list_t* list) {
    if (h->allow_duplicates)
        return;
    for (ssize_t i = list->count - 1; i >= 0; i--) {
        const char* current = list->entries[i].command;
        if (current == NULL)
            continue;
        for (ssize_t j = i - 1; j >= 0; j--) {
            if (list->entries[j].command != NULL &&
                strcmp(current, list->entries[j].command) == 0) {
                history_list_remove_at(h, list, j);
            }
        }
    }
}

static bool history_collect_entries(history_t* h, history_list_t* list, bool dedup);
static bool history_write_all(const history_t* h, const history_list_t* list);
static bool history_append_entry(const history_t* h, const history_entry_t* entry);

static bool history_entries_equal(const history_entry_t* a, const history_entry_t* b) {
    if (a == NULL || b == NULL)
        return false;
    if (a->timestamp != b->timestamp)
        return false;
    if (a->exit_code != b->exit_code)
        return false;
    if (a->command == NULL || b->command == NULL)
        return a->command == b->command;
    return strcmp(a->command, b->command) == 0;
}

static bool history_list_contains_entry(const history_list_t* list, const history_entry_t* entry) {
    if (list == NULL || entry == NULL)
        return false;
    for (ssize_t i = 0; i < list->count; i++) {
        if (history_entries_equal(&list->entries[i], entry))
            return true;
    }
    return false;
}

static bool history_list_remove_value(history_t* h, history_list_t* list, const char* value) {
    if (list == NULL || value == NULL)
        return false;
    bool removed = false;
    for (ssize_t i = list->count - 1; i >= 0; i--) {
        if (list->entries[i].command != NULL && strcmp(list->entries[i].command, value) == 0) {
            history_list_remove_at(h, list, i);
            removed = true;
        }
    }
    return removed;
}

static void history_cache_reset(history_t* h) {
    if (h == NULL)
        return;
    if (h->cache_initialized) {
        history_list_free(h, &h->cache);
    }
    history_list_init(&h->cache);
    h->cache_initialized = false;
    h->cache_dirty = false;
    h->cache_disk_count_at_load = 0;
    h->cache_disk_max_timestamp = 0;
}

static bool history_cache_load(history_t* h, bool dedup) {
    if (h == NULL)
        return false;
    if (!h->single_io_mode)
        return false;
    if (!h->cache_initialized) {
        history_list_init(&h->cache);
        if (!history_collect_entries(h, &h->cache, dedup)) {
            history_list_free(h, &h->cache);
            h->cache_initialized = false;
            h->cache_dirty = false;
            return false;
        }
        h->cache_initialized = true;
        h->cache_dirty = false;
        h->cache_disk_count_at_load = h->cache.count;
        if (h->cache.count > 0) {
            h->cache_disk_max_timestamp = h->cache.entries[h->cache.count - 1].timestamp;
        } else {
            h->cache_disk_max_timestamp = 0;
        }
    } else {
        history_list_prune_to_max(h, &h->cache);
        if (dedup || !h->allow_duplicates) {
            history_list_remove_duplicates(h, &h->cache);
        }
    }
    return true;
}

static bool history_acquire_entries(history_t* h, history_list_t* scratch,
                                    history_list_t** out_list, bool dedup, bool* using_cache) {
    if (h == NULL || out_list == NULL)
        return false;
    if (using_cache)
        *using_cache = false;
    if (h->single_io_mode) {
        if (!history_cache_load(h, dedup))
            return false;
        *out_list = &h->cache;
        if (using_cache)
            *using_cache = true;
        return true;
    }
    history_list_init(scratch);
    if (!history_collect_entries(h, scratch, dedup)) {
        history_list_free(h, scratch);
        return false;
    }
    *out_list = scratch;
    return true;
}

static void history_release_entries(history_t* h, history_list_t* scratch, bool using_cache) {
    ic_unused(h);
    if (!using_cache && scratch != NULL) {
        history_list_free(h, scratch);
    }
}

static void history_mark_dirty(history_t* h) {
    if (h != NULL && h->single_io_mode) {
        h->cache_dirty = true;
    }
}

static bool history_cache_merge_remote_entries(history_t* h) {
    if (h == NULL || !h->single_io_mode)
        return true;
    if (!h->cache_initialized)
        return true;
    history_list_t disk_entries;
    history_list_init(&disk_entries);
    if (!history_collect_entries(h, &disk_entries, false)) {
        history_list_free(h, &disk_entries);
        return false;
    }

    const ssize_t baseline_count =
        (h->cache_disk_count_at_load < 0) ? h->cache.count : h->cache_disk_count_at_load;
    const time_t baseline_ts = h->cache_disk_max_timestamp;

    bool changed = false;
    for (ssize_t i = 0; i < disk_entries.count; i++) {
        history_entry_t* entry = &disk_entries.entries[i];
        bool beyond_baseline = (baseline_count >= 0 && i >= baseline_count);
        bool timestamp_new =
            (baseline_ts == 0) ? (entry->timestamp != 0) : (entry->timestamp > baseline_ts);
        if (!beyond_baseline && !timestamp_new) {
            continue;
        }

        if (!h->allow_duplicates && history_list_contains_entry(&h->cache, entry)) {
            continue;
        }

        char* dup_command = NULL;
        if (entry->command != NULL) {
            dup_command = mem_strdup(h->mem, entry->command);
            if (dup_command == NULL) {
                history_list_free(h, &disk_entries);
                return false;
            }
        }

        history_entry_t copy = *entry;
        copy.command = dup_command;

        ssize_t insert_pos = h->cache.count;
        while (insert_pos > 0 && h->cache.entries[insert_pos - 1].timestamp > copy.timestamp) {
            insert_pos--;
        }

        if (!history_list_insert(h, &h->cache, insert_pos, copy)) {
            if (dup_command != NULL)
                mem_free(h->mem, dup_command);
            history_list_free(h, &disk_entries);
            return false;
        }
        changed = true;
    }

    history_list_free(h, &disk_entries);

    if (changed) {
        history_list_remove_duplicates(h, &h->cache);
        history_list_prune_to_max(h, &h->cache);
        h->cache_disk_count_at_load = h->cache.count;
        if (h->cache.count > 0) {
            h->cache_disk_max_timestamp = h->cache.entries[h->cache.count - 1].timestamp;
        } else {
            h->cache_disk_max_timestamp = 0;
        }
    }

    return true;
}

ic_private bool history_snapshot_load(history_t* h, history_snapshot_t* snap, bool dedup) {
    if (snap == NULL)
        return false;
    if (h == NULL)
        return false;
    history_snapshot_free(h, snap);
    if (history_is_disabled(h)) {
        snap->entries = NULL;
        snap->count = 0;
        snap->capacity = 0;
        return true;
    }

    history_list_t scratch;
    history_list_t* list = NULL;
    bool using_cache = false;
    if (!history_acquire_entries(h, &scratch, &list, dedup, &using_cache))
        return false;

    if (list->count == 0) {
        snap->entries = NULL;
        snap->count = 0;
        snap->capacity = 0;
        history_release_entries(h, &scratch, using_cache);
        return true;
    }

    if (using_cache) {
        history_entry_t* entries = mem_zalloc_tp_n(h->mem, history_entry_t, list->count);
        if (entries == NULL) {
            history_release_entries(h, &scratch, using_cache);
            return false;
        }
        bool success = true;
        for (ssize_t i = 0; i < list->count; i++) {
            entries[i] = list->entries[i];
            if (entries[i].command != NULL) {
                entries[i].command = mem_strdup(h->mem, list->entries[i].command);
                if (entries[i].command == NULL) {
                    success = false;
                    break;
                }
            }
        }
        if (!success) {
            for (ssize_t i = 0; i < list->count; i++) {
                if (entries[i].command != NULL) {
                    mem_free(h->mem, entries[i].command);
                    entries[i].command = NULL;
                }
            }
            mem_free(h->mem, entries);
            history_release_entries(h, &scratch, using_cache);
            return false;
        }
        snap->entries = entries;
        snap->count = list->count;
        snap->capacity = list->count;
        history_release_entries(h, &scratch, using_cache);
        return true;
    }

    snap->entries = list->entries;
    snap->count = list->count;
    snap->capacity = list->capacity;
    // Prevent release from freeing transferred ownership.
    scratch.entries = NULL;
    scratch.count = 0;
    scratch.capacity = 0;
    // No release needed since ownership moved to snapshot.
    return true;
}

ic_private void history_snapshot_free(history_t* h, history_snapshot_t* snap) {
    if (snap == NULL)
        return;
    if (h == NULL) {
        snap->entries = NULL;
        snap->count = 0;
        snap->capacity = 0;
        return;
    }
    if (snap->entries == NULL) {
        snap->count = 0;
        snap->capacity = 0;
        return;
    }
    for (ssize_t i = 0; i < snap->count; i++) {
        if (snap->entries[i].command != NULL) {
            mem_free(h->mem, snap->entries[i].command);
            snap->entries[i].command = NULL;
        }
    }
    mem_free(h->mem, snap->entries);
    snap->entries = NULL;
    snap->count = 0;
    snap->capacity = 0;
}

ic_private const history_entry_t* history_snapshot_get(const history_snapshot_t* snap, ssize_t n) {
    if (snap == NULL || snap->entries == NULL)
        return NULL;
    if (n < 0 || n >= snap->count)
        return NULL;
    ssize_t idx = snap->count - n - 1;
    if (idx < 0 || idx >= snap->count)
        return NULL;
    return &snap->entries[idx];
}

ic_private ssize_t history_snapshot_count(const history_snapshot_t* snap) {
    if (snap == NULL)
        return 0;
    return snap->count;
}

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

static void history_entry_normalize_metadata(history_entry_t* entry) {
    if (entry == NULL)
        return;
    if (entry->timestamp == 0) {
        entry->timestamp = time(NULL);
    }
}

ic_private history_t* history_new(alloc_t* mem) {
    history_t* h = mem_zalloc_tp(mem, history_t);
    if (h == NULL)
        return NULL;
    h->mem = mem;
    h->allow_duplicates = false;
    h->max_entries = IC_DEFAULT_HISTORY;
    h->scratch = NULL;
    h->scratch_cap = 0;
    history_list_init(&h->cache);
    h->cache_initialized = false;
    h->cache_dirty = false;
    h->single_io_mode = g_default_single_io_mode;
    h->cache_disk_count_at_load = 0;
    h->cache_disk_max_timestamp = 0;
    return h;
}

ic_private void history_free(history_t* h) {
    if (h == NULL)
        return;
    if (h->scratch != NULL) {
        mem_free(h->mem, h->scratch);
        h->scratch = NULL;
        h->scratch_cap = 0;
    }
    if (h->cache_initialized) {
        history_list_free(h, &h->cache);
        h->cache_initialized = false;
        h->cache_dirty = false;
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

ic_private void history_set_single_io_mode(history_t* h, bool enable) {
    if (h == NULL)
        return;
    if (h->single_io_mode == enable)
        return;

    if (!enable) {
        if (h->cache_dirty && h->cache_initialized && !history_is_disabled(h)) {
            history_write_all(h, &h->cache);
        }
        history_cache_reset(h);
        h->single_io_mode = false;
        return;
    }

    h->single_io_mode = true;
    history_cache_reset(h);
    if (!history_is_disabled(h)) {
        history_cache_load(h, true);
    }
}

ic_private void history_set_single_io_default(bool enable) {
    g_default_single_io_mode = enable;
}

ic_private bool history_single_io_mode_enabled(const history_t* h) {
    if (h != NULL)
        return h->single_io_mode;
    return g_default_single_io_mode;
}

static const char* history_set_scratch(history_t* h, const char* entry) {
    if (entry == NULL)
        return NULL;
    ssize_t needed = ic_strlen(entry) + 1;
    if (needed > h->scratch_cap) {
        char* newscratch = mem_realloc_tp(h->mem, char, h->scratch, needed);
        if (newscratch == NULL)
            return NULL;
        h->scratch = newscratch;
        h->scratch_cap = needed;
    }
    ic_strncpy(h->scratch, h->scratch_cap, entry, needed - 1);
    return h->scratch;
}

ic_private ssize_t history_count(const history_t* h) {
    if (history_is_disabled(h))
        return 0;
    history_t* mutable_h = (history_t*)h;
    history_list_t list;
    history_list_t* active = NULL;
    bool using_cache = false;
    if (!history_acquire_entries(mutable_h, &list, &active, true, &using_cache))
        return 0;
    ssize_t count = active->count;
    history_release_entries(mutable_h, &list, using_cache);
    return count;
}

ic_private const char* history_get(const history_t* h, ssize_t n) {
    if (history_is_disabled(h))
        return NULL;
    history_t* mutable_h = (history_t*)h;
    history_list_t scratch;
    history_list_t* active = NULL;
    bool using_cache = false;
    if (!history_acquire_entries(mutable_h, &scratch, &active, true, &using_cache))
        return NULL;
    const char* result = NULL;
    if (n >= 0 && n < active->count) {
        ssize_t idx = active->count - n - 1;
        if (idx >= 0 && idx < active->count) {
            result = history_set_scratch(mutable_h, active->entries[idx].command);
        }
    }
    history_release_entries(mutable_h, &scratch, using_cache);
    return result;
}

static bool history_update_file(history_t* h, history_list_t* list) {
    if (!history_write_all(h, list))
        return false;
    return true;
}

ic_private bool history_update(history_t* h, const char* entry) {
    if (h == NULL || entry == NULL || history_is_disabled(h))
        return false;

    history_list_t scratch;
    history_list_t* list = NULL;
    bool using_cache = false;
    if (!history_acquire_entries(h, &scratch, &list, false, &using_cache))
        return false;

    if (list->count == 0) {
        history_release_entries(h, &scratch, using_cache);
        return history_push(h, entry);
    }

    char* normalized = history_entry_dup_trimmed(h->mem, entry);
    if (normalized == NULL) {
        history_release_entries(h, &scratch, using_cache);
        return false;
    }

    history_entry_t* last = &list->entries[list->count - 1];
    mem_free(h->mem, last->command);
    last->command = normalized;

    history_list_remove_duplicates(h, list);
    history_list_prune_to_max(h, list);

    bool ok = true;
    if (using_cache) {
        history_mark_dirty(h);
    } else {
        ok = history_update_file(h, list);
    }
    history_release_entries(h, &scratch, using_cache);
    return ok;
}

ic_private bool history_push(history_t* h, const char* entry) {
    return history_push_with_exit_code(h, entry, IC_HISTORY_EXIT_CODE_UNKNOWN);
}

ic_private bool history_push_with_exit_code(history_t* h, const char* entry, int exit_code) {
    if (h == NULL || entry == NULL || history_is_disabled(h))
        return false;

    char* normalized = history_entry_dup_trimmed(h->mem, entry);
    if (normalized == NULL)
        return false;

    history_list_t scratch;
    history_list_t* list = NULL;
    bool using_cache = false;
    if (!history_acquire_entries(h, &scratch, &list, false, &using_cache)) {
        mem_free(h->mem, normalized);
        return false;
    }

    bool removed_existing = false;
    if (!h->allow_duplicates) {
        removed_existing = history_list_remove_value(h, list, normalized);
    }

    history_entry_t new_entry = {
        .command = normalized,
        .exit_code = exit_code,
        .timestamp = time(NULL),
    };

    if (!history_list_append(h, list, new_entry)) {
        history_release_entries(h, &scratch, using_cache);
        return false;
    }

    ssize_t before_prune = list->count;
    history_list_prune_to_max(h, list);
    bool pruned = (list->count != before_prune);

    bool ok = true;
    if (using_cache) {
        history_mark_dirty(h);
    } else {
        const bool needs_full_rewrite = removed_existing || pruned;
        if (needs_full_rewrite) {
            ok = history_update_file(h, list);
        } else {
            history_entry_t* latest = (list->count > 0) ? &list->entries[list->count - 1] : NULL;
            ok = history_append_entry(h, latest);
            if (!ok && latest != NULL) {
                ok = history_update_file(h, list);
            }
        }
    }

    history_release_entries(h, &scratch, using_cache);
    return ok;
}

ic_private void history_remove_last(history_t* h) {
    if (history_is_disabled(h))
        return;
    history_list_t scratch;
    history_list_t* list = NULL;
    bool using_cache = false;
    if (!history_acquire_entries(h, &scratch, &list, false, &using_cache))
        return;
    if (list->count > 0) {
        history_list_remove_at(h, list, list->count - 1);
        if (using_cache) {
            history_mark_dirty(h);
        } else {
            history_update_file(h, list);
        }
    }
    history_release_entries(h, &scratch, using_cache);
}

ic_private void history_clear(history_t* h) {
    if (h == NULL)
        return;
    if (h->scratch != NULL) {
        mem_free(h->mem, h->scratch);
        h->scratch = NULL;
        h->scratch_cap = 0;
    }
    if (h->fname == NULL || history_is_disabled(h))
        return;
    if (h->single_io_mode) {
        history_cache_reset(h);
        h->cache_initialized = true;
        history_mark_dirty(h);
        return;
    }
    FILE* f = fopen(h->fname, "w");
    if (f != NULL) {
#ifndef _WIN32
        chmod(h->fname, S_IRUSR | S_IWUSR);
#endif
        history_close_stream(f);
    }
}

ic_private bool history_search(const history_t* h, ssize_t from, const char* search, bool backward,
                               ssize_t* hidx, ssize_t* hpos) {
    if (h == NULL || search == NULL || history_is_disabled(h))
        return false;
    history_list_t scratch;
    history_list_t* list = NULL;
    history_t* mutable_h = (history_t*)h;
    bool using_cache = false;
    if (!history_acquire_entries(mutable_h, &scratch, &list, true, &using_cache))
        return false;

    const char* p = NULL;
    ssize_t found = -1;

    if (backward) {
        for (ssize_t i = from; i < list->count; i++) {
            ssize_t idx = list->count - i - 1;
            const char* cmd = list->entries[idx].command;
            if (cmd == NULL)
                continue;
            p = strstr(cmd, search);
            if (p != NULL) {
                found = i;
                break;
            }
        }
    } else {
        for (ssize_t i = from; i >= 0; i--) {
            ssize_t idx = list->count - i - 1;
            if (idx < 0 || idx >= list->count)
                continue;
            const char* cmd = list->entries[idx].command;
            if (cmd == NULL)
                continue;
            p = strstr(cmd, search);
            if (p != NULL) {
                found = i;
                break;
            }
        }
    }

    if (found >= 0 && p != NULL) {
        if (hidx != NULL)
            *hidx = found;
        if (hpos != NULL && list->entries[list->count - found - 1].command != NULL)
            *hpos = (ssize_t)(p - list->entries[list->count - found - 1].command);
        history_release_entries(mutable_h, &scratch, using_cache);
        return true;
    }

    history_release_entries(mutable_h, &scratch, using_cache);
    return false;
}

ic_private bool history_search_prefix(const history_t* h, ssize_t from, const char* prefix,
                                      bool backward, ssize_t* hidx) {
    if (prefix == NULL || h == NULL || history_is_disabled(h))
        return false;

    history_t* mutable_h = (history_t*)h;
    history_list_t scratch;
    history_list_t* list = NULL;
    bool using_cache = false;
    if (!history_acquire_entries(mutable_h, &scratch, &list, true, &using_cache))
        return false;

    const size_t prefix_len = strlen(prefix);
    if (prefix_len == 0) {
        bool result = false;
        if (backward) {
            if (from < list->count) {
                if (hidx != NULL)
                    *hidx = from;
                result = true;
            }
        } else {
            if (from >= 0) {
                if (hidx != NULL)
                    *hidx = from;
                result = true;
            }
        }
        history_release_entries(mutable_h, &scratch, using_cache);
        return result;
    }

    if (backward) {
        for (ssize_t i = from; i < list->count; i++) {
            ssize_t idx = list->count - i - 1;
            if (idx < 0 || idx >= list->count)
                continue;
            const char* entry = list->entries[idx].command;
            if (entry != NULL && strncmp(entry, prefix, prefix_len) == 0) {
                if (hidx != NULL)
                    *hidx = i;
                history_release_entries(mutable_h, &scratch, using_cache);
                return true;
            }
        }
    } else {
        for (ssize_t i = from; i >= 0; i--) {
            ssize_t idx = list->count - i - 1;
            if (idx < 0 || idx >= list->count)
                continue;
            const char* entry = list->entries[idx].command;
            if (entry != NULL && strncmp(entry, prefix, prefix_len) == 0) {
                if (hidx != NULL)
                    *hidx = i;
                history_release_entries(mutable_h, &scratch, using_cache);
                return true;
            }
        }
    }

    history_release_entries(mutable_h, &scratch, using_cache);
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

static bool history_parse_exit_code_token(const char* token, size_t len, int* exit_code_out) {
    if (token == NULL || exit_code_out == NULL || len == 0)
        return false;

    const char* value_ptr = NULL;
    if (len > 1 && (token[0] == ':' || token[0] == '!')) {
        value_ptr = token + 1;
    } else {
        static const char* prefixes[] = {"exit:", "status:", "code:"};
        static const size_t prefix_lengths[] = {5U, 7U, 5U};
        for (size_t i = 0; i < (sizeof(prefixes) / sizeof(prefixes[0])); i++) {
            size_t prefix_len = prefix_lengths[i];
            if (len <= prefix_len)
                continue;
            if (ic_strnicmp(token, prefixes[i], (ssize_t)prefix_len) == 0) {
                value_ptr = token + prefix_len;
                break;
            }
        }
    }

    if (value_ptr == NULL || value_ptr >= token + len)
        return false;

    size_t value_len = (size_t)((token + len) - value_ptr);
    if (value_len == 0 || value_len >= 64)
        return false;

    char buffer[64];
    ic_memcpy(buffer, value_ptr, (ssize_t)value_len);
    buffer[value_len] = '\0';

    char* endptr = NULL;
    long parsed = strtol(buffer, &endptr, 10);
    if (endptr == buffer || *endptr != '\0')
        return false;
    if (parsed < INT_MIN || parsed > INT_MAX)
        return false;

    *exit_code_out = (int)parsed;
    return true;
}

ic_private bool history_fuzzy_search(const history_t* h, const char* query,
                                     history_match_t* matches, ssize_t max_matches,
                                     ssize_t* match_count, bool* exit_filter_applied,
                                     int* exit_filter_value) {
    if (exit_filter_applied)
        *exit_filter_applied = false;
    if (exit_filter_value)
        *exit_filter_value = IC_HISTORY_EXIT_CODE_UNKNOWN;

    if (h == NULL || query == NULL || matches == NULL || max_matches <= 0) {
        if (match_count)
            *match_count = 0;
        return false;
    }

    if (history_is_disabled(h)) {
        if (match_count)
            *match_count = 0;
        return false;
    }

    history_list_t list;
    history_list_init(&list);
    history_t* mutable_h = (history_t*)h;
    if (!history_collect_entries(mutable_h, &list, true)) {
        history_list_free(mutable_h, &list);
        if (match_count)
            *match_count = 0;
        return false;
    }

    bool filter_by_exit_code = false;
    int exit_code_filter = 0;
    char* sanitized_query = NULL;
    bool sanitized_available = false;
    size_t sanitized_len = 0;

    ssize_t original_query_len = ic_strlen(query);
    if (original_query_len < 0)
        original_query_len = 0;
    sanitized_query = mem_malloc_tp_n(mutable_h->mem, char, (size_t)original_query_len + 1);
    if (sanitized_query != NULL) {
        sanitized_query[0] = '\0';
        sanitized_available = true;
    }

    const char* cursor = query;
    while (*cursor != '\0') {
        while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
            cursor++;
        }

        const char* token_start = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
            cursor++;
        }

        size_t token_len = (size_t)(cursor - token_start);
        if (token_len == 0)
            break;

        int parsed_exit_code = 0;
        if (history_parse_exit_code_token(token_start, token_len, &parsed_exit_code)) {
            filter_by_exit_code = true;
            exit_code_filter = parsed_exit_code;
            continue;
        }

        if (sanitized_available) {
            if (sanitized_len > 0) {
                sanitized_query[sanitized_len++] = ' ';
            }
            ic_memcpy(sanitized_query + sanitized_len, token_start, (ssize_t)token_len);
            sanitized_len += token_len;
            sanitized_query[sanitized_len] = '\0';
        }
    }

    if (!sanitized_available && filter_by_exit_code) {
        sanitized_query = mem_malloc_tp_n(mutable_h->mem, char, 1);
        if (sanitized_query != NULL) {
            sanitized_query[0] = '\0';
            sanitized_available = true;
            sanitized_len = 0;
        }
    }

    const char* effective_query = query;
    if (filter_by_exit_code) {
        if (sanitized_available && sanitized_query != NULL) {
            sanitized_query[sanitized_len] = '\0';
            effective_query = sanitized_query;
        } else {
            effective_query = "";
        }
    }

    if (effective_query == NULL)
        effective_query = "";

    if (filter_by_exit_code) {
        if (exit_filter_applied)
            *exit_filter_applied = true;
        if (exit_filter_value)
            *exit_filter_value = exit_code_filter;
    }

    ssize_t count = 0;

    if (effective_query[0] == '\0') {
        for (ssize_t offset = 0; offset < list.count && count < max_matches; offset++) {
            ssize_t idx = list.count - offset - 1;
            const history_entry_t* entry = &list.entries[idx];
            if (entry->command == NULL)
                continue;
            if (filter_by_exit_code && entry->exit_code != exit_code_filter)
                continue;

            matches[count].hidx = offset;
            matches[count].score = (int)(100 - offset);
            matches[count].match_pos = 0;
            matches[count].match_len = 0;
            count++;
        }
    } else {
        for (ssize_t offset = 0; offset < list.count; offset++) {
            ssize_t idx = list.count - offset - 1;
            const history_entry_t* entry = &list.entries[idx];
            if (entry->command == NULL)
                continue;
            if (filter_by_exit_code && entry->exit_code != exit_code_filter)
                continue;

            ssize_t mpos = 0;
            ssize_t mlen = 0;
            int score = fuzzy_match_score(entry->command, effective_query, &mpos, &mlen);

            if (score >= 0) {
                score += (int)(offset / 10);

                if (count < max_matches) {
                    matches[count].hidx = offset;
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
                        matches[worst_idx].hidx = offset;
                        matches[worst_idx].score = score;
                        matches[worst_idx].match_pos = mpos;
                        matches[worst_idx].match_len = mlen;
                    }
                }
            }
        }
    }

    if (count > 1) {
        qsort(matches, count, sizeof(history_match_t), compare_matches);
    }

    if (match_count)
        *match_count = count;

    if (sanitized_query != NULL)
        mem_free(mutable_h->mem, sanitized_query);
    history_list_free(mutable_h, &list);

    return count > 0;
}

ic_private void history_load_from(history_t* h, const char* fname, long max_entries) {
    if (h == NULL)
        return;

    if (h->fname != NULL) {
        mem_free(h->mem, h->fname);
        h->fname = NULL;
    }

    if (fname != NULL)
        h->fname = mem_strdup(h->mem, fname);

    if (max_entries == 0) {
        h->max_entries = 0;
    } else if (max_entries < 0) {
        h->max_entries = IC_DEFAULT_HISTORY;
    } else if (max_entries > IC_ABSOLUTE_MAX_HISTORY) {
        h->max_entries = IC_ABSOLUTE_MAX_HISTORY;
    } else {
        h->max_entries = max_entries;
    }

    history_cache_reset(h);
    if (!history_is_disabled(h)) {
        if (h->single_io_mode) {
            history_cache_load(h, true);
        } else {
            history_load(h);
        }
    }
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

static char* history_read_entry(history_t* h, FILE* f, stringbuf_t* sbuf) {
    sbuf_clear(sbuf);
    while (true) {
        int c = fgetc(f);
        if (c == EOF) {
            if (ferror(f)) {
                return NULL;
            }
            break;
        }
        if (c == '\n')
            break;
        if (c == '\\') {
            int esc = fgetc(f);
            if (esc == EOF)
                return NULL;
            if (esc == 'n') {
                sbuf_append(sbuf, "\n");
            } else if (esc == 'r') {
                continue;
            } else if (esc == 't') {
                sbuf_append(sbuf, "\t");
            } else if (esc == '\\') {
                sbuf_append(sbuf, "\\");
            } else if (esc == 'x') {
                int c1 = fgetc(f);
                if (c1 == EOF)
                    return NULL;
                int c2 = fgetc(f);
                if (c2 == EOF)
                    return NULL;
                if (ic_isxdigit(c1) && ic_isxdigit(c2)) {
                    char chr = from_xdigit(c1) * 16 + from_xdigit(c2);
                    sbuf_append_char(sbuf, chr);
                } else {
                    return NULL;
                }
            } else {
                return NULL;
            }
        } else {
            sbuf_append_char(sbuf, (char)c);
        }
    }
    if (sbuf_len(sbuf) == 0)
        return mem_strdup(h->mem, "");
    if (sbuf_string(sbuf)[0] == '#')
        return NULL;
    return history_entry_dup_trimmed(h->mem, sbuf_string(sbuf));
}

static bool history_write_entry(const char* entry, FILE* f, stringbuf_t* sbuf) {
    sbuf_clear(sbuf);

    if (entry == NULL)
        return true;

    if (*entry == '\0')
        return history_write_successful(fputc('\n', f));

    while (*entry != 0) {
        char c = *entry++;
        if (c == '\\') {
            sbuf_append(sbuf, "\\\\");
        } else if (c == '\n') {
            sbuf_append(sbuf, "\\n");
        } else if (c == '\r') {
            continue;
        } else if (c == '\t') {
            sbuf_append(sbuf, "\\t");
        } else if (c < ' ' || c > '~' || c == '#') {
            char c1 = to_xdigit((uint8_t)c / 16);
            char c2 = to_xdigit((uint8_t)c % 16);
            sbuf_append(sbuf, "\\x");
            sbuf_append_char(sbuf, c1);
            sbuf_append_char(sbuf, c2);
        } else {
            sbuf_append_char(sbuf, c);
        }
    }

    if (sbuf_len(sbuf) > 0) {
        sbuf_append(sbuf, "\n");
        if (!history_write_successful(fputs(sbuf_string(sbuf), f)))
            return false;
    }
    return true;
}

static bool history_write_record(const history_entry_t* entry, FILE* f, stringbuf_t* sbuf) {
    if (entry == NULL || entry->command == NULL)
        return true;

    history_entry_t temp = *entry;
    history_entry_normalize_metadata(&temp);

    char header[64];
    int written =
        snprintf(header, sizeof(header), "# %lld %d\n", (long long)temp.timestamp, temp.exit_code);
    if (written < 0 || written >= (int)sizeof(header))
        return false;

    size_t to_write = (size_t)written;
    if (fwrite(header, 1, to_write, f) != to_write)
        return false;

    return history_write_entry(temp.command, f, sbuf);
}

static bool history_collect_entries(history_t* h, history_list_t* list, bool dedup) {
    history_list_init(list);
    if (h == NULL)
        return false;
    if (history_is_disabled(h))
        return true;
    if (h->fname == NULL)
        return true;

    FILE* f = fopen(h->fname, "r");
    if (f == NULL)
        return true;

    stringbuf_t* sbuf = sbuf_new(h->mem);
    if (sbuf == NULL) {
        history_close_stream(f);
        return false;
    }

    bool success = true;
    char header_buf[512];
    while (success) {
        if (feof(f)) {
            clearerr(f);
            break;
        }

        int c = fgetc(f);
        if (c == EOF) {
            if (ferror(f))
                success = false;
            else
                clearerr(f);
            break;
        }
        if (c == '\n' || c == '\r')
            continue;

        history_entry_t entry = {
            .command = NULL,
            .exit_code = IC_HISTORY_EXIT_CODE_UNKNOWN,
            .timestamp = 0,
        };

        if (c == '#') {
            if (ungetc(c, f) == EOF) {
                success = false;
                break;
            }
            errno = 0;
            if (fgets(header_buf, sizeof(header_buf), f) == NULL) {
                if (ferror(f))
                    success = false;
                break;
            }

            const char* cursor = header_buf + 1;
            while (*cursor == ' ' || *cursor == '\t')
                cursor++;

            if (*cursor == '\0') {
                // Comment line without metadata, skip.
                continue;
            }

            char* endptr = NULL;
            long long ts = strtoll(cursor, &endptr, 10);
            if (endptr == cursor) {
                // Metadata could not be parsed; treat as comment.
                continue;
            }

            entry.timestamp = (time_t)ts;
            cursor = endptr;

            while (*cursor == ' ' || *cursor == '\t')
                cursor++;

            if (*cursor != '\0' && *cursor != '\n' && *cursor != '\r') {
                long exit_ll = strtol(cursor, &endptr, 10);
                if (endptr != cursor) {
                    entry.exit_code = (int)exit_ll;
                }
            }

            long pos_after_header = ftell(f);
            if (pos_after_header < 0) {
                success = false;
                break;
            }

            int next_char = fgetc(f);
            if (next_char == EOF) {
                if (ferror(f))
                    success = false;
                break;
            }
            if (next_char == '#') {
                if (fseek(f, pos_after_header, SEEK_SET) != 0) {
                    success = false;
                    break;
                }
                clearerr(f);
                continue;
            }
            if (ungetc(next_char, f) == EOF) {
                clearerr(f);
                success = false;
                break;
            }
        } else {
            if (ungetc(c, f) == EOF) {
                clearerr(f);
                success = false;
                break;
            }
        }

        clearerr(f);

        char* command = history_read_entry(h, f, sbuf);
        if (command == NULL) {
            if (ferror(f)) {
                success = false;
                break;
            }
            if (feof(f))
                break;
            clearerr(f);
            continue;
        }

        entry.command = command;
        history_entry_normalize_metadata(&entry);

        if (!history_list_append(h, list, entry)) {
            history_list_free(h, list);
            sbuf_free(sbuf);
            history_close_stream(f);
            return false;
        }
    }

    sbuf_free(sbuf);
    bool close_ok = history_close_stream(f);

    if (!success) {
        history_list_free(h, list);
        return false;
    }

    history_list_prune_to_max(h, list);
    if (dedup)
        history_list_remove_duplicates(h, list);
    else if (!h->allow_duplicates)
        history_list_remove_duplicates(h, list);

    return close_ok;
}

static bool history_write_all(const history_t* h, const history_list_t* list) {
    if (h == NULL || h->fname == NULL)
        return false;

    FILE* f = fopen(h->fname, "w");
    if (f == NULL)
        return false;
#ifndef _WIN32
    chmod(h->fname, S_IRUSR | S_IWUSR);
#endif

    stringbuf_t* sbuf = sbuf_new(h->mem);
    if (sbuf == NULL) {
        history_close_stream(f);
        return false;
    }

    for (ssize_t i = 0; i < list->count; i++) {
        if (!history_write_record(&list->entries[i], f, sbuf)) {
            sbuf_free(sbuf);
            history_close_stream(f);
            return false;
        }
    }

    sbuf_free(sbuf);
    return history_close_stream(f);
}

static bool history_append_entry(const history_t* h, const history_entry_t* entry) {
    if (h == NULL || h->fname == NULL || entry == NULL)
        return false;

    FILE* f = fopen(h->fname, "a");
    if (f == NULL)
        return false;
#ifndef _WIN32
    chmod(h->fname, S_IRUSR | S_IWUSR);
#endif

    stringbuf_t* sbuf = sbuf_new(h->mem);
    if (sbuf == NULL) {
        history_close_stream(f);
        return false;
    }

    bool ok = history_write_record(entry, f, sbuf);

    sbuf_free(sbuf);
    if (!history_close_stream(f))
        ok = false;
    return ok;
}

ic_private void history_load(history_t* h) {
    if (h == NULL)
        return;
    if (history_is_disabled(h))
        return;
    history_list_t list;
    history_list_init(&list);
    if (!history_collect_entries(h, &list, true)) {
        history_list_free(h, &list);
        return;
    }
    history_write_all(h, &list);
    history_list_free(h, &list);
}

ic_private void history_save(const history_t* h) {
    if (h == NULL)
        return;
    history_t* mutable_h = (history_t*)h;
    if (!mutable_h->single_io_mode)
        return;
    if (!mutable_h->cache_initialized || !mutable_h->cache_dirty)
        return;
    if (history_is_disabled(mutable_h))
        return;
    if (!history_cache_merge_remote_entries(mutable_h))
        return;
    if (history_write_all(mutable_h, &mutable_h->cache)) {
        mutable_h->cache_dirty = false;
        mutable_h->cache_disk_count_at_load = mutable_h->cache.count;
        if (mutable_h->cache.count > 0) {
            mutable_h->cache_disk_max_timestamp =
                mutable_h->cache.entries[mutable_h->cache.count - 1].timestamp;
        } else {
            mutable_h->cache_disk_max_timestamp = 0;
        }
    }
}
