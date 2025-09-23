#include "plugin.h"

#include "cjsh.h"
#include "error_out.h"
#include "pluginapi.h"

static thread_local std::string current_plugin_context;

extern "C" PLUGIN_API plugin_error_t plugin_register_prompt_variable(
    const char* name, plugin_get_prompt_variable_func func) {
  if (current_plugin_context.empty() || !g_plugin) {
    return PLUGIN_ERROR_GENERAL;
  }
  plugin_data* pd = g_plugin->get_plugin_data(current_plugin_context);
  if (!pd) {
    return PLUGIN_ERROR_GENERAL;
  }
  pd->prompt_variables[std::string(name)] = func;
  return PLUGIN_SUCCESS;
}

// Enhanced helper functions for plugin API
extern "C" PLUGIN_API char* plugin_safe_strdup(const char* src) {
  if (!src) return nullptr;
  size_t len = strlen(src);
  char* copy = static_cast<char*>(malloc(len + 1));
  if (copy) {
    memcpy(copy, src, len + 1);
  }
  return copy;
}

extern "C" PLUGIN_API plugin_string_t plugin_create_string(const char* src) {
  plugin_string_t result = {nullptr, 0, 0};
  if (!src) return result;
  
  size_t len = strlen(src);
  result.data = static_cast<char*>(malloc(len + 1));
  if (result.data) {
    memcpy(result.data, src, len + 1);
    result.length = static_cast<int>(len);
    result.capacity = static_cast<int>(len + 1);
  }
  return result;
}

extern "C" PLUGIN_API void plugin_free_plugin_string(plugin_string_t* str) {
  if (str && str->data) {
    free(str->data);
    str->data = nullptr;
    str->length = 0;
    str->capacity = 0;
  }
}

#include <sys/utsname.h>

#include <cstdio>
#include <cstring>

#include "cjsh.h"
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

Plugin::Plugin(const std::filesystem::path& plugins_dir, bool enabled,
               bool lazy_loading) {
  plugins_directory = plugins_dir;
  plugins_discovered = false;
  this->enabled = enabled;
  this->lazy_loading_enabled = lazy_loading;

  if (g_debug_mode) {
    std::cerr << "DEBUG: Plugin constructor - Directory: " << plugins_dir
              << ", Enabled: " << (enabled ? "true" : "false")
              << ", Lazy loading: " << (lazy_loading ? "true" : "false")
              << std::endl;
  }

  if (enabled) {
    if (lazy_loading_enabled) {
      // In lazy loading mode, only discover metadata
      cache_plugin_metadata();
      plugins_discovered = true;
    } else {
      // Traditional eager loading
      plugins_discovered = discover_plugins();
    }
  } else {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Plugin constructor - Plugins are disabled"
                << std::endl;
    }
  }
}

Plugin::~Plugin() {
  if (!enabled) {
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: Plugin destructor - Plugins disabled, skipping cleanup"
          << std::endl;
    }
    return;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Plugin destructor - Cleaning up loaded plugins"
              << std::endl;
  }

  std::unique_lock lock(plugins_mutex);
  for (auto& [name, data] : loaded_plugins) {
    if (data.enabled && data.shutdown) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: Plugin destructor - Shutting down plugin: " << name
                  << std::endl;
      }
      data.shutdown();
    }
    if (data.handle) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: Plugin destructor - Closing handle for plugin: "
                  << name << std::endl;
      }
      dlclose(data.handle);
    }
  }
  loaded_plugins.clear();

  {
    std::unique_lock events_lock(events_mutex);
    if (g_debug_mode) {
      std::cerr << "DEBUG: Plugin destructor - Clearing subscribed events"
                << std::endl;
    }
    subscribed_events.clear();
  }
}

bool Plugin::discover_plugins() {
  if (!enabled) {
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: discover_plugins - Plugins disabled, skipping discovery"
          << std::endl;
    }
    return false;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: discover_plugins - Starting plugin discovery"
              << std::endl;
  }

  std::unique_lock discovery_lock(discovery_mutex);

  {
    std::shared_lock plugins_lock(plugins_mutex);
    if (plugins_discovered && !loaded_plugins.empty()) {
      if (g_debug_mode) {
        std::cerr
            << "DEBUG: discover_plugins - Plugins already discovered, count: "
            << loaded_plugins.size() << std::endl;
      }
      return true;
    }
  }

  if (!std::filesystem::exists(plugins_directory)) {
    std::cerr
        << "cjsh: plugin: discover_plugins: Plugins directory does not exist: "
        << plugins_directory << std::endl;
    if (g_debug_mode) {
      std::cerr << "DEBUG: discover_plugins - Plugin directory not found"
                << std::endl;
    }
    return false;
  }

  {
    std::unique_lock plugins_lock(plugins_mutex);
    if (g_debug_mode && !loaded_plugins.empty()) {
      std::cerr << "DEBUG: discover_plugins - Cleaning up existing plugins "
                   "before rediscovery"
                << std::endl;
    }
    for (auto& [name, data] : loaded_plugins) {
      if (data.enabled && data.shutdown) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: discover_plugins - Shutting down plugin: "
                    << name << std::endl;
        }
        data.shutdown();
      }
      if (data.handle) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: discover_plugins - Closing handle for plugin: "
                    << name << std::endl;
        }
        dlclose(data.handle);
      }
    }
    loaded_plugins.clear();
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: discover_plugins - Scanning directory: "
              << plugins_directory << std::endl;
  }

  for (const auto& entry :
       std::filesystem::directory_iterator(plugins_directory)) {
    std::string file_name = entry.path().filename().string();
    if (entry.path().extension() == ".so" ||
        entry.path().extension() == ".dylib") {
      if (g_debug_mode) {
        std::cerr << "DEBUG: discover_plugins - Found plugin file: "
                  << file_name << std::endl;
      }
      load_plugin(entry.path());
    }
  }

  plugins_discovered = true;

  if (g_debug_mode) {
    std::shared_lock plugins_lock(plugins_mutex);
    std::cerr << "DEBUG: discover_plugins - Discovery complete, loaded "
              << loaded_plugins.size() << " plugins" << std::endl;
  }

  return true;
}

bool Plugin::load_plugin(const std::filesystem::path& path) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Attempting to load plugin: "
              << path.filename().string() << std::endl;
  }

  if (!enabled) {
    print_error(
        {ErrorType::RUNTIME_ERROR, "plugin", "Plugin loading is disabled", {}});
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Plugin loading is disabled"
                << std::endl;
    }
    return false;
  }

  std::string file_arch = get_file_architecture(path);
  std::string current_arch = get_current_architecture();

  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Plugin architecture: " << file_arch
              << ", System architecture: " << current_arch << std::endl;
  }

  if (!is_architecture_compatible(file_arch, current_arch)) {
    print_error(
        {ErrorType::RUNTIME_ERROR,
         "plugin",
         "Architecture mismatch for plugin: " + path.filename().string() +
             " (plugin: " + file_arch + ", system: " + current_arch + ")",
         {}});
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: load_plugin - Architecture mismatch, cannot load plugin"
          << std::endl;
    }
    return false;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Loading dynamic library: " << path
              << std::endl;
  }

  void* handle = dlopen(path.c_str(), RTLD_LAZY);
  if (!handle) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "plugin",
                 "Failed to load plugin: " + path.string() + " - " +
                     std::string(dlerror()),
                 {}});
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - dlopen failed" << std::endl;
    }
    return false;
  }

  dlerror();

  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Looking up plugin_get_info symbol"
              << std::endl;
  }

  plugin_get_info_func get_info =
      reinterpret_cast<plugin_get_info_func>(dlsym(handle, "plugin_get_info"));
  const char* dlsym_error = dlerror();
  if (dlsym_error) {
    print_error(
        {ErrorType::RUNTIME_ERROR,
         "plugin",
         "Cannot load symbol 'plugin_get_info': " + std::string(dlsym_error),
         {}});
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Failed to find plugin_get_info symbol"
                << std::endl;
    }
    dlclose(handle);
    return false;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Calling plugin_get_info function"
              << std::endl;
  }

  plugin_info_t* info = get_info();
  if (!info) {
    print_error(
        {ErrorType::RUNTIME_ERROR, "plugin", "Failed to get plugin info", {}});
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - plugin_get_info returned NULL"
                << std::endl;
    }
    dlclose(handle);
    return false;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Plugin info retrieved - Name: "
              << info->name << ", Version: " << info->version
              << ", Interface version: " << info->interface_version
              << std::endl;
  }

  if (info->interface_version != PLUGIN_INTERFACE_VERSION) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "plugin",
                 "Plugin interface version mismatch for " +
                     std::string(info->name) +
                     ". Expected: " + std::to_string(PLUGIN_INTERFACE_VERSION) +
                     ", Got: " + std::to_string(info->interface_version),
                 {}});
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Interface version mismatch"
                << std::endl;
    }
    dlclose(handle);
    return false;
  }

  std::string name = info->name;

  {
    std::shared_lock plugins_lock(plugins_mutex);
    if (loaded_plugins.find(name) != loaded_plugins.end()) {
      print_error(
          {ErrorType::RUNTIME_ERROR,
           "plugin",
           "Plugin '" + name + "' is already loaded. Ignoring duplicate",
           {}});
      if (g_debug_mode) {
        std::cerr
            << "DEBUG: load_plugin - Plugin already loaded, ignoring duplicate"
            << std::endl;
      }
      dlclose(handle);
      return false;
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Setting up plugin data structure"
              << std::endl;
  }

  plugin_data data;
  data.handle = handle;
  data.info = info;
  data.enabled = false;

  data.get_info = get_info;
  data.initialize = reinterpret_cast<plugin_initialize_func>(
      dlsym(handle, "plugin_initialize"));
  data.shutdown =
      reinterpret_cast<plugin_shutdown_func>(dlsym(handle, "plugin_shutdown"));
  data.handle_command = reinterpret_cast<plugin_handle_command_func>(
      dlsym(handle, "plugin_handle_command"));
  data.get_commands = reinterpret_cast<plugin_get_commands_func>(
      dlsym(handle, "plugin_get_commands"));
  data.get_subscribed_events =
      reinterpret_cast<plugin_get_subscribed_events_func>(
          dlsym(handle, "plugin_get_subscribed_events"));
  data.get_default_settings =
      reinterpret_cast<plugin_get_default_settings_func>(
          dlsym(handle, "plugin_get_default_settings"));
  data.update_setting = reinterpret_cast<plugin_update_setting_func>(
      dlsym(handle, "plugin_update_setting"));
  data.free_memory = reinterpret_cast<plugin_free_memory_func>(
      dlsym(handle, "plugin_free_memory"));
  data.validate = reinterpret_cast<plugin_validate_func>(
      dlsym(handle, "plugin_validate"));  // Optional function

  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Checking for required functions"
              << std::endl;
    std::cerr << "DEBUG: load_plugin - initialize: "
              << (data.initialize ? "found" : "not found")
              << ", shutdown: " << (data.shutdown ? "found" : "not found")
              << ", handle_command: "
              << (data.handle_command ? "found" : "not found")
              << ", get_commands: "
              << (data.get_commands ? "found" : "not found")
              << ", free_memory: " << (data.free_memory ? "found" : "not found")
              << std::endl;
  }

  if (!data.initialize || !data.shutdown || !data.handle_command ||
      !data.get_commands || !data.free_memory) {
    std::cerr << "Plugin " << name << " is missing required functions"
              << std::endl;
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Plugin missing required functions"
                << std::endl;
    }
    dlclose(handle);
    return false;
  }

  if (data.get_default_settings) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Loading default settings" << std::endl;
    }
    int count = 0;
    plugin_setting_t* settings = data.get_default_settings(&count);
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Found " << count << " default settings"
                << std::endl;
    }
    
    // Store settings and free memory properly
    if (settings && count > 0) {
      for (int i = 0; i < count; i++) {
        if (settings[i].key && settings[i].value) {
          data.settings[settings[i].key] = settings[i].value;
          if (g_debug_mode) {
            std::cerr << "DEBUG: load_plugin - Setting " << settings[i].key << "="
                      << settings[i].value << std::endl;
          }
        }
      }

      // Free the memory allocated by the plugin using its own free function
      if (data.free_memory) {
        for (int i = 0; i < count; i++) {
          if (settings[i].key) {
            data.free_memory(settings[i].key);
          }
          if (settings[i].value) {
            data.free_memory(settings[i].value);
          }
        }
        data.free_memory(settings);
      }
    }
  }

  // Run plugin validation if available (enhanced API)
  if (data.validate) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Running plugin validation" << std::endl;
    }
    plugin_validation_t validation_result = data.validate();
    if (validation_result.status != PLUGIN_SUCCESS) {
      std::string error_msg = "Plugin validation failed";
      if (validation_result.error_message) {
        error_msg += ": " + std::string(validation_result.error_message);
        // Free the error message if it was allocated
        if (data.free_memory) {
          data.free_memory(validation_result.error_message);
        }
      }
      std::cerr << "Plugin loading failed: " << name << ": " << error_msg << std::endl;
      dlclose(handle);
      return false;
    }
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Plugin validation passed" << std::endl;
    }
  }

  {
    std::unique_lock plugins_lock(plugins_mutex);
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin - Adding plugin '" << name
                << "' to loaded_plugins map" << std::endl;
    }
    loaded_plugins[name] = std::move(data);
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin - Successfully loaded plugin: " << name
              << std::endl;
  }
  return true;
}

bool Plugin::extract_plugin_metadata(const std::filesystem::path& path,
                                     plugin_metadata& metadata) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: extract_plugin_metadata - Extracting metadata from: "
              << path.filename().string() << std::endl;
  }

  metadata.library_path = path;
  metadata.last_modified =
      std::filesystem::last_write_time(path).time_since_epoch().count();
  metadata.is_loaded = false;
  metadata.load_failed = false;

  // Temporarily open the library to extract basic info
  void* temp_handle = dlopen(path.c_str(), RTLD_LAZY);
  if (!temp_handle) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: extract_plugin_metadata - Failed to open library: "
                << dlerror() << std::endl;
    }
    metadata.load_failed = true;
    return false;
  }

  // Get plugin info
  plugin_get_info_func get_info = reinterpret_cast<plugin_get_info_func>(
      dlsym(temp_handle, "plugin_get_info"));
  if (!get_info) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: extract_plugin_metadata - Failed to find "
                   "plugin_get_info symbol"
                << std::endl;
    }
    dlclose(temp_handle);
    metadata.load_failed = true;
    return false;
  }

  plugin_info_t* info = get_info();
  if (!info) {
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: extract_plugin_metadata - plugin_get_info returned NULL"
          << std::endl;
    }
    dlclose(temp_handle);
    metadata.load_failed = true;
    return false;
  }

  metadata.name = info->name;
  metadata.version = info->version;
  metadata.description = info->description;
  metadata.author = info->author;

  // Try to get commands list
  plugin_get_commands_func get_commands =
      reinterpret_cast<plugin_get_commands_func>(
          dlsym(temp_handle, "plugin_get_commands"));
  if (get_commands) {
    int count = 0;
    char** commands = get_commands(&count);
    if (commands && count > 0) {
      metadata.commands.clear();
      for (int i = 0; i < count; i++) {
        if (commands[i]) {
          metadata.commands.push_back(commands[i]);
        }
      }

      // Free the commands memory using plugin's free function if available
      plugin_free_memory_func free_memory =
          reinterpret_cast<plugin_free_memory_func>(
              dlsym(temp_handle, "plugin_free_memory"));
      if (free_memory) {
        for (int i = 0; i < count; i++) {
          if (commands[i]) {
            free_memory(commands[i]);
          }
        }
        free_memory(commands);
      }
    }
  }

  // Try to get subscribed events
  plugin_get_subscribed_events_func get_events =
      reinterpret_cast<plugin_get_subscribed_events_func>(
          dlsym(temp_handle, "plugin_get_subscribed_events"));
  if (get_events) {
    int count = 0;
    char** events = get_events(&count);
    if (events && count > 0) {
      metadata.events.clear();
      for (int i = 0; i < count; i++) {
        if (events[i]) {
          metadata.events.push_back(events[i]);
        }
      }

      // Free the events memory using plugin's free function if available
      plugin_free_memory_func free_memory =
          reinterpret_cast<plugin_free_memory_func>(
              dlsym(temp_handle, "plugin_free_memory"));
      if (free_memory) {
        for (int i = 0; i < count; i++) {
          if (events[i]) {
            free_memory(events[i]);
          }
        }
        free_memory(events);
      }
    }
  }

  dlclose(temp_handle);

  if (g_debug_mode) {
    std::cerr << "DEBUG: extract_plugin_metadata - Successfully extracted "
                 "metadata for: "
              << metadata.name << " (v" << metadata.version << ")" << std::endl;
  }

  return true;
}

bool Plugin::load_plugin_on_demand(const std::string& name) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: load_plugin_on_demand - Loading plugin: " << name
              << std::endl;
  }

  // Check if already loaded
  {
    std::shared_lock plugins_lock(plugins_mutex);
    if (loaded_plugins.find(name) != loaded_plugins.end()) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: load_plugin_on_demand - Plugin already loaded"
                  << std::endl;
      }
      return true;
    }
  }

  // Get metadata to find the library path
  std::shared_lock metadata_lock(metadata_mutex);
  auto metadata_it = plugin_metadata_cache.find(name);
  if (metadata_it == plugin_metadata_cache.end()) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: load_plugin_on_demand - Plugin metadata not found"
                << std::endl;
    }
    return false;
  }

  std::filesystem::path library_path = metadata_it->second.library_path;
  metadata_lock.unlock();

  // Load the plugin fully
  bool result = load_plugin(library_path);

  if (result) {
    // Update metadata to mark as loaded
    std::unique_lock metadata_lock(metadata_mutex);
    auto& metadata = plugin_metadata_cache[name];
    metadata.is_loaded = true;
    metadata.load_failed = false;
  }

  return result;
}

bool Plugin::is_metadata_stale(const plugin_metadata& metadata) const {
  if (!std::filesystem::exists(metadata.library_path)) {
    return true;
  }

  auto current_mtime = std::filesystem::last_write_time(metadata.library_path)
                           .time_since_epoch()
                           .count();
  return current_mtime != metadata.last_modified;
}

void Plugin::cache_plugin_metadata() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: cache_plugin_metadata - Starting metadata caching"
              << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: cache_plugin_metadata - Plugins disabled, skipping"
                << std::endl;
    }
    return;
  }

  if (!std::filesystem::exists(plugins_directory)) {
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: cache_plugin_metadata - Plugin directory does not exist: "
          << plugins_directory << std::endl;
    }
    return;
  }

  std::unique_lock metadata_lock(metadata_mutex);

  // Scan plugin directory for .so/.dylib files
  for (const auto& entry :
       std::filesystem::directory_iterator(plugins_directory)) {
    if (entry.path().extension() == ".so" ||
        entry.path().extension() == ".dylib") {
      std::string filename = entry.path().filename().string();

      // Check if we already have metadata and if it's current
      auto existing = plugin_metadata_cache.find(filename);
      if (existing != plugin_metadata_cache.end()) {
        if (!is_metadata_stale(existing->second)) {
          if (g_debug_mode) {
            std::cerr
                << "DEBUG: cache_plugin_metadata - Using cached metadata for: "
                << filename << std::endl;
          }
          continue;
        }
      }

      // Extract fresh metadata
      plugin_metadata metadata;
      if (extract_plugin_metadata(entry.path(), metadata)) {
        plugin_metadata_cache[metadata.name] = metadata;
        if (g_debug_mode) {
          std::cerr << "DEBUG: cache_plugin_metadata - Cached metadata for: "
                    << metadata.name << std::endl;
        }
      } else if (g_debug_mode) {
        std::cerr
            << "DEBUG: cache_plugin_metadata - Failed to extract metadata for: "
            << filename << std::endl;
      }
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: cache_plugin_metadata - Cached "
              << plugin_metadata_cache.size() << " plugin metadata entries"
              << std::endl;
  }
}

bool Plugin::uninstall_plugin(const std::string& name) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: uninstall_plugin - Attempting to uninstall plugin: "
              << name << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: uninstall_plugin - Plugins disabled, cannot uninstall"
          << std::endl;
    }
    return false;
  }

  std::shared_lock plugins_lock(plugins_mutex);
  auto it = loaded_plugins.find(name);
  if (it == loaded_plugins.end()) {
    print_error({ErrorType::COMMAND_NOT_FOUND,
                 "plugin",
                 "Plugin not found: " + name,
                 {}});
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: uninstall_plugin - Plugin not found in loaded_plugins"
          << std::endl;
    }
    return false;
  }

  if (it->second.enabled) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "plugin",
                 "Please disable the plugin before uninstalling: " + name,
                 {}});
    if (g_debug_mode) {
      std::cerr << "DEBUG: uninstall_plugin - Cannot uninstall enabled plugin, "
                   "disable first"
                << std::endl;
    }
    return false;
  }
  plugins_lock.unlock();

  if (g_debug_mode) {
    std::cerr << "DEBUG: uninstall_plugin - Searching for plugin file in "
                 "plugins directory"
              << std::endl;
  }

  std::filesystem::path plugin_path;
  for (const auto& entry :
       std::filesystem::directory_iterator(plugins_directory)) {
    if (entry.path().extension() != ".so" &&
        entry.path().extension() != ".dylib") {
      continue;
    }

    if (g_debug_mode) {
      std::cerr << "DEBUG: uninstall_plugin - Checking file: "
                << entry.path().string() << std::endl;
    }

    void* temp_handle = dlopen(entry.path().c_str(), RTLD_LAZY);
    if (!temp_handle) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: uninstall_plugin - Failed to open library: "
                  << dlerror() << std::endl;
      }
      continue;
    }

    plugin_get_info_func get_info = reinterpret_cast<plugin_get_info_func>(
        dlsym(temp_handle, "plugin_get_info"));
    if (!get_info) {
      if (g_debug_mode) {
        std::cerr
            << "DEBUG: uninstall_plugin - Failed to find plugin_get_info symbol"
            << std::endl;
      }
      dlclose(temp_handle);
      continue;
    }

    plugin_info_t* temp_info = get_info();
    if (temp_info && std::string(temp_info->name) == name) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: uninstall_plugin - Found matching plugin file: "
                  << entry.path().string() << std::endl;
      }
      plugin_path = entry.path();
      dlclose(temp_handle);
      break;
    }
    dlclose(temp_handle);
  }

  if (plugin_path.empty()) {
    std::cerr
        << "cjsh: plugin: uninstall_plugin: Could not find plugin file for: "
        << name << std::endl;
    if (g_debug_mode) {
      std::cerr << "DEBUG: uninstall_plugin - Could not find plugin file in "
                   "plugins directory"
                << std::endl;
    }
    return false;
  }

  try {
    if (g_debug_mode) {
      std::cerr << "DEBUG: uninstall_plugin - Unloading plugin from memory"
                << std::endl;
    }
    unload_plugin(name);

    if (g_debug_mode) {
      std::cerr << "DEBUG: uninstall_plugin - Removing plugin file: "
                << plugin_path.string() << std::endl;
    }
    std::filesystem::remove(plugin_path);
    std::cout << "Successfully uninstalled plugin: " << name << std::endl;
    if (g_debug_mode) {
      std::cerr << "DEBUG: uninstall_plugin - Successfully uninstalled plugin"
                << std::endl;
    }
    return true;
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr
        << "cjsh: plugin: uninstall_plugin: Failed to remove plugin file: "
        << e.what() << std::endl;
    if (g_debug_mode) {
      std::cerr << "DEBUG: uninstall_plugin - Filesystem error: " << e.what()
                << std::endl;
    }
    return false;
  }
}

void Plugin::unload_plugin(const std::string& name) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: unload_plugin - Attempting to unload plugin: " << name
              << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: unload_plugin - Plugins disabled, skipping unload"
                << std::endl;
    }
    return;
  }

  std::unique_lock plugins_lock(plugins_mutex);
  auto it = loaded_plugins.find(name);
  if (it != loaded_plugins.end()) {
    if (it->second.enabled && it->second.shutdown) {
      if (g_debug_mode) {
        std::cerr
            << "DEBUG: unload_plugin - Calling shutdown function for plugin: "
            << name << std::endl;
      }
      it->second.shutdown();
    }

    if (it->second.handle) {
      if (g_debug_mode) {
        std::cerr
            << "DEBUG: unload_plugin - Closing library handle for plugin: "
            << name << std::endl;
      }
      dlclose(it->second.handle);
    }

    if (g_debug_mode) {
      std::cerr
          << "DEBUG: unload_plugin - Removing plugin from loaded_plugins map"
          << std::endl;
    }
    loaded_plugins.erase(it);
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: unload_plugin - Plugin not found in loaded_plugins map"
              << std::endl;
  }
}

std::vector<std::string> Plugin::get_available_plugins() const {
  if (g_debug_mode) {
    std::cerr
        << "DEBUG: get_available_plugins - Getting list of available plugins"
        << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_available_plugins - Plugins disabled, returning "
                   "empty list"
                << std::endl;
    }
    return {};
  }

  std::vector<std::string> plugins;

  if (lazy_loading_enabled) {
    // In lazy loading mode, use metadata cache
    std::shared_lock metadata_lock(metadata_mutex);
    for (const auto& [name, metadata] : plugin_metadata_cache) {
      if (!metadata.load_failed) {
        plugins.push_back(name);
      }
    }
  } else {
    // Traditional mode, use loaded plugins
    std::shared_lock plugins_lock(plugins_mutex);
    for (const auto& [name, _] : loaded_plugins) {
      plugins.push_back(name);
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: get_available_plugins - Found " << plugins.size()
              << " available plugins" << std::endl;
  }

  return plugins;
}

std::vector<std::string> Plugin::get_enabled_plugins() const {
  if (g_debug_mode) {
    std::cerr << "DEBUG: get_enabled_plugins - Getting list of enabled plugins"
              << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_enabled_plugins - Plugins disabled, returning "
                   "empty list"
                << std::endl;
    }
    return {};
  }

  std::shared_lock plugins_lock(plugins_mutex);
  std::vector<std::string> plugins;
  for (const auto& [name, data] : loaded_plugins) {
    if (data.enabled) {
      plugins.push_back(name);
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: get_enabled_plugins - Found " << plugins.size()
              << " enabled plugins" << std::endl;
  }

  return plugins;
}

bool Plugin::enable_plugin(const std::string& name) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: enable_plugin - Attempting to enable plugin: " << name
              << std::endl;
  }

  if (!enabled) {
    print_error(
        {ErrorType::RUNTIME_ERROR, "plugin", "Plugin system is disabled", {}});
    if (g_debug_mode) {
      std::cerr << "DEBUG: enable_plugin - Plugin system is disabled"
                << std::endl;
    }
    return false;
  }

  plugin_initialize_func init_func = nullptr;
  {
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end() && it->second.enabled) {
      std::cout << "Plugin already enabled: " << name << std::endl;
      if (g_debug_mode) {
        std::cerr << "DEBUG: enable_plugin - Plugin already enabled"
                  << std::endl;
      }
      return true;
    }

    // In lazy loading mode, load the plugin if not already loaded
    if (lazy_loading_enabled && it == loaded_plugins.end()) {
      plugins_lock.unlock();  // Release lock before loading
      if (!load_plugin_on_demand(name)) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "plugin",
                     "Failed to load plugin: " + name,
                     {}});
        if (g_debug_mode) {
          std::cerr << "DEBUG: enable_plugin - Failed to load plugin on demand"
                    << std::endl;
        }
        return false;
      }
      plugins_lock.lock();  // Re-acquire lock
      it = loaded_plugins.find(name);
    }

    if (it == loaded_plugins.end()) {
      print_error({ErrorType::COMMAND_NOT_FOUND,
                   "plugin",
                   "Plugin not found: " + name,
                   {}});
      if (g_debug_mode) {
        std::cerr << "DEBUG: enable_plugin - Plugin not found in loaded_plugins"
                  << std::endl;
      }
      return false;
    }

    init_func = it->second.initialize;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: enable_plugin - Initializing plugin: " << name
              << std::endl;
  }

  current_plugin_context = name;
  bool init_ok = (init_func && init_func() == PLUGIN_SUCCESS);
  current_plugin_context.clear();
  if (!init_ok) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "plugin",
                 "Failed to initialize plugin: " + name,
                 {}});
    if (g_debug_mode) {
      std::cerr << "DEBUG: enable_plugin - Plugin initialization failed"
                << std::endl;
    }
    return false;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: enable_plugin - Plugin initialized successfully"
              << std::endl;
  }

  int count = 0;
  char** events = nullptr;
  {
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it == loaded_plugins.end()) {
      std::cerr << "Plugin not found after initialization: " << name
                << std::endl;
      if (g_debug_mode) {
        std::cerr
            << "DEBUG: enable_plugin - Plugin not found after initialization"
            << std::endl;
      }
      return false;
    }
    it->second.enabled = true;
    if (!g_startup_active) {
      std::cout << "Enabled plugin: " << name << std::endl;
    }
    if (g_debug_mode) {
      std::cerr << "DEBUG: enable_plugin - Marked plugin as enabled"
                << std::endl;
    }

    if (it->second.get_subscribed_events) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: enable_plugin - Getting subscribed events"
                  << std::endl;
      }
      events = it->second.get_subscribed_events(&count);
      if (g_debug_mode) {
        std::cerr << "DEBUG: enable_plugin - Plugin subscribes to " << count
                  << " events" << std::endl;
      }
    } else if (g_debug_mode) {
      std::cerr << "DEBUG: enable_plugin - Plugin doesn't have "
                   "get_subscribed_events function"
                << std::endl;
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: enable_plugin - Triggering plugin_enabled event"
              << std::endl;
  }
  trigger_subscribed_global_event("plugin_enabled", name);

  if (events && count > 0) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: enable_plugin - Registering event subscriptions"
                << std::endl;
    }
    std::unique_lock events_lock(events_mutex);
    for (int i = 0; i < count; i++) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: enable_plugin - Subscribing to event: "
                  << events[i] << std::endl;
      }
      subscribed_events[events[i]].push_back(name);
    }
  }

  if (events && count > 0) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: enable_plugin - Freeing event memory" << std::endl;
    }
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end() && it->second.free_memory) {
      for (int i = 0; i < count; i++) {
        it->second.free_memory(events[i]);
      }
      it->second.free_memory(events);
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: enable_plugin - Plugin enabled successfully"
              << std::endl;
  }
  return true;
}

bool Plugin::disable_plugin(const std::string& name) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: disable_plugin - Attempting to disable plugin: "
              << name << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: disable_plugin - Plugin system is disabled"
                << std::endl;
    }
    return false;
  }

  std::vector<std::string> events_to_unsubscribe;

  {
    std::unique_lock plugins_lock(plugins_mutex);
    auto it = loaded_plugins.find(name);
    if (it != loaded_plugins.end() && it->second.enabled) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: disable_plugin - Calling shutdown function"
                  << std::endl;
      }
      it->second.shutdown();
      it->second.enabled = false;
      std::cout << "Disabled plugin: " << name << std::endl;
      if (g_debug_mode) {
        std::cerr << "DEBUG: disable_plugin - Plugin marked as disabled"
                  << std::endl;
      }

      int count = 0;
      char** events = nullptr;
      if (it->second.get_subscribed_events) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: disable_plugin - Getting subscribed events to "
                       "unsubscribe"
                    << std::endl;
        }
        events = it->second.get_subscribed_events(&count);
      }

      if (events && count > 0) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: disable_plugin - Collected " << count
                    << " events to unsubscribe" << std::endl;
        }
        for (int i = 0; i < count; i++) {
          events_to_unsubscribe.push_back(events[i]);
          if (g_debug_mode) {
            std::cerr << "DEBUG: disable_plugin - Will unsubscribe from event: "
                      << events[i] << std::endl;
          }
        }
        if (it->second.free_memory) {
          if (g_debug_mode) {
            std::cerr << "DEBUG: disable_plugin - Freeing event memory"
                      << std::endl;
          }
          for (int i = 0; i < count; i++) {
            it->second.free_memory(events[i]);
          }
          it->second.free_memory(events);
        }
      }
    } else {
      if (g_debug_mode) {
        std::cerr
            << "DEBUG: disable_plugin - Plugin not found or already disabled"
            << std::endl;
      }
      return false;
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: disable_plugin - Triggering plugin_disabled event"
              << std::endl;
  }
  trigger_subscribed_global_event("plugin_disabled", name);

  {
    std::unique_lock events_lock(events_mutex);
    if (g_debug_mode && !events_to_unsubscribe.empty()) {
      std::cerr
          << "DEBUG: disable_plugin - Removing plugin from event subscriptions"
          << std::endl;
    }
    for (const auto& event : events_to_unsubscribe) {
      auto eventIt = subscribed_events.find(event);
      if (eventIt != subscribed_events.end()) {
        if (g_debug_mode) {
          std::cerr << "DEBUG: disable_plugin - Removing plugin from event: "
                    << event << std::endl;
        }
        eventIt->second.erase(
            std::remove(eventIt->second.begin(), eventIt->second.end(), name),
            eventIt->second.end());
      }
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: disable_plugin - Plugin disabled successfully"
              << std::endl;
  }
  return true;
}

bool Plugin::handle_plugin_command(const std::string& targeted_plugin,
                                   std::vector<std::string>& args) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: handle_plugin_command - Handling command for plugin: "
              << targeted_plugin << ", args: ";
    for (size_t i = 0; i < args.size(); ++i) {
      std::cerr << "'" << args[i] << "'";
      if (i < args.size() - 1)
        std::cerr << ", ";
    }
    std::cerr << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: handle_plugin_command - Plugin system is disabled"
                << std::endl;
    }
    return false;
  }

  std::shared_lock plugins_lock(plugins_mutex);
  auto it = loaded_plugins.find(targeted_plugin);

  // If plugin not loaded but lazy loading is enabled, try to load it
  if (lazy_loading_enabled && it == loaded_plugins.end()) {
    plugins_lock.unlock();
    if (load_plugin_on_demand(targeted_plugin)) {
      plugins_lock.lock();
      it = loaded_plugins.find(targeted_plugin);
    } else {
      if (g_debug_mode) {
        std::cerr << "DEBUG: handle_plugin_command - Failed to load plugin on "
                     "demand: "
                  << targeted_plugin << std::endl;
      }
      return false;
    }
  }

  if (it != loaded_plugins.end() && it->second.enabled) {
    plugin_args_t args_struct;
    args_struct.count = args.size();
    args_struct.args = new char*[args.size()];
    args_struct.position = 0;

    if (g_debug_mode) {
      std::cerr
          << "DEBUG: handle_plugin_command - Preparing arguments for plugin"
          << std::endl;
    }

    for (size_t i = 0; i < args.size(); i++) {
      args_struct.args[i] = strdup(args[i].c_str());
    }

    if (g_debug_mode) {
      std::cerr << "DEBUG: handle_plugin_command - Calling plugin's "
                   "handle_command function"
                << std::endl;
    }
    int result = it->second.handle_command(&args_struct);
    if (g_debug_mode) {
      std::cerr << "DEBUG: handle_plugin_command - Result: "
                << (result == PLUGIN_SUCCESS ? "success" : "failure")
                << std::endl;
    }

    for (int i = 0; i < args_struct.count; i++) {
      free(args_struct.args[i]);
    }
    delete[] args_struct.args;

    return result == PLUGIN_SUCCESS;
  }

  if (g_debug_mode) {
    std::cerr
        << "DEBUG: handle_plugin_command - Plugin not found or not enabled"
        << std::endl;
  }
  return false;
}

std::vector<std::string> Plugin::get_plugin_commands(
    const std::string& name) const {
  if (g_debug_mode) {
    std::cerr << "DEBUG: get_plugin_commands - Getting commands for plugin: "
              << name << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_plugin_commands - Plugin system is disabled"
                << std::endl;
    }
    return {};
  }

  // In lazy loading mode, try to get commands from metadata cache first
  if (lazy_loading_enabled) {
    std::shared_lock metadata_lock(metadata_mutex);
    auto metadata_it = plugin_metadata_cache.find(name);
    if (metadata_it != plugin_metadata_cache.end()) {
      if (g_debug_mode) {
        std::cerr
            << "DEBUG: get_plugin_commands - Returning cached commands, count: "
            << metadata_it->second.commands.size() << std::endl;
      }
      return metadata_it->second.commands;
    }
  }

  std::shared_lock plugins_lock(plugins_mutex);
  auto it = loaded_plugins.find(name);
  if (it != loaded_plugins.end() && it->second.get_commands) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_plugin_commands - Calling plugin's get_commands "
                   "function"
                << std::endl;
    }
    int count = 0;
    char** commands = it->second.get_commands(&count);
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_plugin_commands - Found " << count << " commands"
                << std::endl;
    }

    std::vector<std::string> result;
    for (int i = 0; i < count; i++) {
      result.push_back(commands[i]);
      if (g_debug_mode) {
        std::cerr << "DEBUG: get_plugin_commands - Command " << i << ": "
                  << commands[i] << std::endl;
      }
    }

    if (it->second.free_memory) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: get_plugin_commands - Freeing command memory"
                  << std::endl;
      }
      for (int i = 0; i < count; i++) {
        it->second.free_memory(commands[i]);
      }
      it->second.free_memory(commands);
    }

    return result;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: get_plugin_commands - Plugin not found or missing "
                 "get_commands function"
              << std::endl;
  }
  return {};
}

std::string Plugin::get_plugin_info(const std::string& name) const {
  if (g_debug_mode) {
    std::cerr << "DEBUG: get_plugin_info - Getting info for plugin: " << name
              << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_plugin_info - Plugin system is disabled"
                << std::endl;
    }
    return "Plugin system is disabled";
  }

  std::shared_lock plugins_lock(plugins_mutex);
  auto it = loaded_plugins.find(name);
  if (it != loaded_plugins.end()) {
    const auto& data = it->second;
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_plugin_info - Found plugin, returning info"
                << std::endl;
    }
    return "Name: " + std::string(data.info->name) + "\n" +
           "Version: " + std::string(data.info->version) + "\n" +
           "Author: " + std::string(data.info->author) + "\n" +
           "Description: " + std::string(data.info->description) + "\n" +
           "Status: " + (data.enabled ? "Enabled" : "Disabled");
  }
  return "Plugin not found: " + name;
}

bool Plugin::update_plugin_setting(const std::string& plugin_name,
                                   const std::string& key,
                                   const std::string& value) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: update_plugin_setting - Updating setting for plugin: "
              << plugin_name << ", key: " << key << ", value: " << value
              << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: update_plugin_setting - Plugin system is disabled"
                << std::endl;
    }
    return false;
  }

  std::unique_lock plugins_lock(plugins_mutex);
  auto it = loaded_plugins.find(plugin_name);
  if (it != loaded_plugins.end()) {
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: update_plugin_setting - Found plugin, updating setting"
          << std::endl;
    }
    it->second.settings[key] = value;
    if (it->second.update_setting) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: update_plugin_setting - Calling plugin's "
                     "update_setting function"
                  << std::endl;
      }
      bool result = it->second.update_setting(key.c_str(), value.c_str()) ==
                    PLUGIN_SUCCESS;
      if (g_debug_mode) {
        std::cerr << "DEBUG: update_plugin_setting - Result: "
                  << (result ? "success" : "failure") << std::endl;
      }
      return result;
    }
    return true;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: update_plugin_setting - Plugin not found" << std::endl;
  }
  return false;
}

std::map<std::string, std::map<std::string, std::string>>
Plugin::get_all_plugin_settings() const {
  if (g_debug_mode) {
    std::cerr
        << "DEBUG: get_all_plugin_settings - Getting settings for all plugins"
        << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_all_plugin_settings - Plugin system is disabled"
                << std::endl;
    }
    return {};
  }

  std::shared_lock plugins_lock(plugins_mutex);
  std::map<std::string, std::map<std::string, std::string>> allSettings;
  for (const auto& [name, data] : loaded_plugins) {
    allSettings[name] = data.settings;
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_all_plugin_settings - Found "
                << data.settings.size() << " settings for plugin: " << name
                << std::endl;
    }
  }
  return allSettings;
}

void Plugin::trigger_subscribed_global_event(const std::string& event,
                                             const std::string& event_data) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: trigger_subscribed_global_event - Triggering event: "
              << event << " with data: " << event_data << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: trigger_subscribed_global_event - Plugin system is "
                   "disabled"
                << std::endl;
    }
    return;
  }

  std::vector<std::string> subscribedPlugins;

  {
    std::shared_lock events_lock(events_mutex);
    auto it = subscribed_events.find(event);
    if (it == subscribed_events.end() || it->second.empty()) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: trigger_subscribed_global_event - No plugins "
                     "subscribed to this event"
                  << std::endl;
      }
      return;
    }
    subscribedPlugins = it->second;
    if (g_debug_mode) {
      std::cerr << "DEBUG: trigger_subscribed_global_event - Found "
                << subscribedPlugins.size() << " plugins subscribed to event"
                << std::endl;
    }
  }

  plugin_args_t args;
  args.count = 3;
  args.args = new char*[3];
  args.position = 0;

  args.args[0] = strdup("event");
  args.args[1] = strdup(event.c_str());
  args.args[2] = strdup(event_data.c_str());

  if (g_debug_mode) {
    std::cerr << "DEBUG: trigger_subscribed_global_event - Created event args: "
              << "args[0]=" << args.args[0] << ", "
              << "args[1]=" << args.args[1] << ", "
              << "args[2]=" << args.args[2] << std::endl;
  }

  for (const auto& plugin_name : subscribedPlugins) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: trigger_subscribed_global_event - Notifying plugin: "
                << plugin_name << std::endl;
    }
    std::shared_lock plugins_lock(plugins_mutex);
    auto plugin_it = loaded_plugins.find(plugin_name);
    if (plugin_it != loaded_plugins.end() && plugin_it->second.enabled) {
      plugins_lock.unlock();

      if (g_debug_mode) {
        std::cerr << "DEBUG: trigger_subscribed_global_event - Calling "
                     "handle_command for plugin"
                  << std::endl;
      }
      current_plugin_context = plugin_name;
      plugin_it->second.handle_command(&args);
      current_plugin_context.clear();
    } else if (g_debug_mode) {
      std::cerr << "DEBUG: trigger_subscribed_global_event - Plugin not found "
                   "or not enabled"
                << std::endl;
    }
  }

  if (g_debug_mode) {
    std::cerr
        << "DEBUG: trigger_subscribed_global_event - Cleaning up event args"
        << std::endl;
  }
  for (int i = 0; i < args.count; i++) {
    free(args.args[i]);
  }
  delete[] args.args;
}

plugin_data* Plugin::get_plugin_data(const std::string& name) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: get_plugin_data - Getting data for plugin: " << name
              << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_plugin_data - Plugin system is disabled"
                << std::endl;
    }
    return nullptr;
  }

  std::shared_lock plugins_lock(plugins_mutex);
  auto it = loaded_plugins.find(name);
  if (it != loaded_plugins.end()) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_plugin_data - Found plugin, returning data"
                << std::endl;
    }
    return &it->second;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: get_plugin_data - Plugin not found" << std::endl;
  }
  return nullptr;
}

void Plugin::clear_plugin_cache() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: clear_plugin_cache - Clearing plugin discovery cache"
              << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: clear_plugin_cache - Plugin system is disabled"
                << std::endl;
    }
    return;
  }

  std::unique_lock discovery_lock(discovery_mutex);
  plugins_discovered = false;

  {
    std::unique_lock events_lock(events_mutex);
    if (g_debug_mode) {
      std::cerr << "DEBUG: clear_plugin_cache - Clearing subscribed events"
                << std::endl;
    }
    subscribed_events.clear();
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: clear_plugin_cache - Cache cleared" << std::endl;
  }
}

bool Plugin::is_plugin_loaded(const std::string& name) const {
  if (g_debug_mode) {
    std::cerr << "DEBUG: is_plugin_loaded - Checking if plugin is loaded: "
              << name << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: is_plugin_loaded - Plugin system is disabled"
                << std::endl;
    }
    return false;
  }

  std::shared_lock plugins_lock(plugins_mutex);
  bool result = loaded_plugins.find(name) != loaded_plugins.end();
  if (g_debug_mode) {
    std::cerr << "DEBUG: is_plugin_loaded - Result: "
              << (result ? "found" : "not found") << std::endl;
  }
  return result;
}

std::string Plugin::get_current_architecture() const {
  if (g_debug_mode) {
    std::cerr << "DEBUG: get_current_architecture - Getting current system "
                 "architecture"
              << std::endl;
  }

  struct utsname system_info;
  uname(&system_info);

  std::string arch = system_info.machine;

  if (g_debug_mode) {
    std::cerr << "DEBUG: get_current_architecture - Raw architecture: " << arch
              << std::endl;
  }

  if (arch == "x86_64" || arch == "amd64") {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_current_architecture - Normalized to: x86_64"
                << std::endl;
    }
    return "x86_64";
  } else if (arch == "arm64" || arch == "aarch64") {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_current_architecture - Normalized to: arm64"
                << std::endl;
    }
    return "arm64";
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: get_current_architecture - Using non-normalized "
                 "architecture: "
              << arch << std::endl;
  }
  return arch;
}

std::string Plugin::get_file_architecture(
    const std::filesystem::path& path) const {
  if (g_debug_mode) {
    std::cerr
        << "DEBUG: get_file_architecture - Getting architecture for file: "
        << path.string() << std::endl;
  }

  std::string result = "unknown";

  std::string cmd = "file -b " + path.string();
  if (g_debug_mode) {
    std::cerr << "DEBUG: get_file_architecture - Running command: " << cmd
              << std::endl;
  }

  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: get_file_architecture - Failed to execute file command"
          << std::endl;
    }
    return result;
  }

  char buffer[128];
  std::string output = "";
  while (!feof(pipe)) {
    if (fgets(buffer, 128, pipe) != NULL) {
      output += buffer;
    }
  }
  pclose(pipe);

  if (g_debug_mode) {
    std::cerr << "DEBUG: get_file_architecture - File command output: "
              << output << std::endl;
  }

  if (output.find("x86_64") != std::string::npos) {
    result = "x86_64";
  } else if (output.find("arm64") != std::string::npos ||
             output.find("ARM64") != std::string::npos ||
             output.find("aarch64") != std::string::npos) {
    result = "arm64";
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: get_file_architecture - Detected architecture: "
              << result << std::endl;
  }
  return result;
}

bool Plugin::is_architecture_compatible(const std::string& file_arch,
                                        const std::string& current_arch) const {
  if (g_debug_mode) {
    std::cerr << "DEBUG: is_architecture_compatible - Checking compatibility "
                 "between file: "
              << file_arch << " and system: " << current_arch << std::endl;
  }

  if (file_arch == current_arch) {
    if (g_debug_mode) {
      std::cerr
          << "DEBUG: is_architecture_compatible - Architectures match exactly"
          << std::endl;
    }
    return true;
  }

#ifdef __APPLE__
  if (current_arch == "arm64" && file_arch == "x86_64") {
    bool rosetta = is_rosetta_translated();
    if (g_debug_mode) {
      std::cerr << "DEBUG: is_architecture_compatible - Apple ARM64 system "
                   "with x86_64 binary, "
                << "Rosetta available: " << (rosetta ? "yes" : "no")
                << std::endl;
    }
    return rosetta;
  }
#endif

  if (g_debug_mode) {
    std::cerr
        << "DEBUG: is_architecture_compatible - Architectures are incompatible"
        << std::endl;
  }
  return false;
}

bool Plugin::is_rosetta_translated() const {
  if (g_debug_mode) {
    std::cerr << "DEBUG: is_rosetta_translated - Checking if Rosetta is active"
              << std::endl;
  }

#ifdef __APPLE__
  int ret = 0;
  size_t size = sizeof(ret);
  if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) != -1) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: is_rosetta_translated - Rosetta status: "
                << (ret == 1 ? "active" : "inactive") << std::endl;
    }
    return ret == 1;
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: is_rosetta_translated - Failed to query Rosetta status"
              << std::endl;
  }
#else
  if (g_debug_mode) {
    std::cerr << "DEBUG: is_rosetta_translated - Not on Apple platform, "
                 "Rosetta not available"
              << std::endl;
  }
#endif
  return false;
}

std::vector<std::string> Plugin::get_available_commands(
    const std::string& plugin_name) const {
  if (g_debug_mode) {
    std::cerr << "DEBUG: get_available_commands - Getting commands for plugin: "
              << plugin_name << std::endl;
  }

  if (!enabled) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: get_available_commands - Plugin system is disabled"
                << std::endl;
    }
    return {};
  }

  // First try metadata cache (for lazy loading)
  if (lazy_loading_enabled) {
    std::shared_lock metadata_lock(metadata_mutex);
    auto metadata_it = plugin_metadata_cache.find(plugin_name);
    if (metadata_it != plugin_metadata_cache.end()) {
      return metadata_it->second.commands;
    }
  }

  // Fallback to loaded plugins
  return get_plugin_commands(plugin_name);
}

size_t Plugin::get_loaded_plugin_count() const {
  std::shared_lock plugins_lock(plugins_mutex);
  return loaded_plugins.size();
}

size_t Plugin::get_metadata_cache_size() const {
  std::shared_lock metadata_lock(metadata_mutex);
  return plugin_metadata_cache.size();
}
