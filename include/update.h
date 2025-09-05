#pragma once

#include <ctime>
#include <string>

extern bool g_cached_update;
extern bool g_check_updates;
extern bool g_silent_update_check;
extern time_t g_last_update_check;
extern int g_update_check_interval;
extern std::string g_cached_version;
extern std::string g_last_updated;
extern bool g_first_boot;

bool check_for_update();
bool load_update_cache();
void save_update_cache(bool update_available,
                       const std::string& latest_version);
bool should_check_for_updates();
bool execute_update_if_available(bool update_available);
void display_changelog(const std::string& changelog_path);
void startup_update_process();
bool is_first_boot();
void mark_first_boot_complete();
bool is_tutorial_complete();
void mark_tutorial_complete();