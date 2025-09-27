#include "main_loop.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "cjsh.h"
#include "cjsh_completions.h"
#include "isocline/isocline.h"
#include "job_control.h"
#include "utils/threaded_input_monitor.h"

namespace {
bool process_command_line(const std::string& command) {
  if (command.empty()) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Received empty command" << std::endl;
    }
    g_shell->reset_command_timing();
    return g_exit_flag;
  }

  notify_plugins("main_process_command_processed", command);

  g_shell->start_command_timing();
  int exit_code = g_shell->execute(command);
  g_shell->end_command_timing(exit_code);

  std::string status_str = std::to_string(exit_code);

  if (g_debug_mode) {
    std::cerr << "DEBUG: Command exit status: " << status_str << std::endl;
  }

  ic_history_add(command.c_str());
  setenv("?", status_str.c_str(), 1);

#ifdef __APPLE__
  malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__linux__)
  malloc_trim(0);
#else
  g_shell->execute("echo '' > /dev/null");
#endif

  // Input monitoring is now handled by the threaded input monitor
  // No need for synchronous collect_typeahead calls

  return g_exit_flag;
}
}  // namespace

void update_terminal_title() {
  if (g_debug_mode) {
    std::cout << "\033]0;" << "<<<DEBUG MODE ENABLED>>>" << "\007";
    std::cout.flush();
  }
  std::cout << "\033]0;" << g_shell->get_title_prompt() << "\007";
  std::cout.flush();
}

void reprint_prompt() {
  // unused function for current implementation, useful for plugins
  if (g_debug_mode) {
    std::cerr << "DEBUG: Reprinting prompt" << std::endl;
  }

  update_terminal_title();

  std::string prompt;
  if (g_shell->get_menu_active()) {
    prompt = g_shell->get_prompt();
  } else {
    prompt = g_shell->get_ai_prompt();
  }

  if (g_theme && g_theme->uses_newline()) {
    prompt += "\n";
    prompt += g_shell->get_newline_prompt();
  }
  ic_print_prompt(prompt.c_str(), false);
}

void main_process_loop() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Entering main process loop" << std::endl;
  notify_plugins("main_process_pre_run", "");

  initialize_completion_system();
  
  // Initialize and start threaded input monitor (replaces old input_monitor)
  threaded_input_monitor::initialize();
  if (threaded_input_monitor::start_monitoring()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Threaded input monitor started successfully" << std::endl;
  } else {
    if (g_debug_mode)
      std::cerr << "DEBUG: Failed to start threaded input monitor" << std::endl;
  }
  
  std::string input_buffer = "testingbuffer";

  while (true) {
    if (g_debug_mode) {
      std::cerr << "---------------------------------------" << std::endl;
      std::cerr << "DEBUG: Starting new command input cycle" << std::endl;
    }
    notify_plugins("main_process_start", "");

    // Check and handle any pending signals before prompting for input
    g_shell->process_pending_signals();

    // Update job status and clean up finished jobs
    if (g_debug_mode)
      std::cerr << "DEBUG: Calling JobManager::update_job_status()"
                << std::endl;
    JobManager::instance().update_job_status();

    if (g_debug_mode)
      std::cerr << "DEBUG: Calling JobManager::cleanup_finished_jobs()"
                << std::endl;
    JobManager::instance().cleanup_finished_jobs();

    // Process queued commands from threaded input monitor
    if (threaded_input_monitor::has_queued_commands()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Processing queued commands from threaded input monitor"
                  << std::endl;

      while (threaded_input_monitor::has_queued_commands()) {
        auto parsed_command = threaded_input_monitor::get_next_command(std::chrono::milliseconds(0));
        if (parsed_command && parsed_command->is_complete) {
          if (g_debug_mode) {
            std::cerr << "DEBUG: Threaded queued command: " << parsed_command->command
                      << std::endl;
          }
          if (process_command_line(parsed_command->command)) {
            break;
          }
        }
      }

      std::string pending_partial = threaded_input_monitor::get_partial_input();
      if (!pending_partial.empty()) {
        input_buffer = pending_partial;
        if (g_debug_mode) {
          std::cerr << "DEBUG: Got partial input from threaded monitor: " << pending_partial << std::endl;
        }
      }

      notify_plugins("main_process_end", "");
      if (g_exit_flag) {
        std::cerr << "Exiting main process loop..." << std::endl;
        break;
      }
      continue;
    }

    if (g_debug_mode)
      std::cerr << "DEBUG: Calling update_terminal_title()" << std::endl;
    update_terminal_title();

    // Check for any partial input from the threaded monitor
    std::string pending_partial = threaded_input_monitor::get_partial_input();
    if (!pending_partial.empty()) {
      input_buffer = pending_partial;
      if (g_debug_mode) {
        std::cerr << "DEBUG: Using partial input as buffer: " << pending_partial << std::endl;
      }
    }

    if (g_debug_mode)
      std::cerr << "DEBUG: Generating prompt" << std::endl;

    // Ensure the prompt always starts on a clean line
    // We print a space, then carriage return to detect if we're at column 0
    std::printf(" \r");
    std::fflush(stdout);

    std::chrono::steady_clock::time_point render_time_start;
    if (g_debug_mode) {
      render_time_start = std::chrono::steady_clock::now();
    }

    // gather and create the prompt
    std::string prompt;
    std::string inline_right_text;
    if (g_shell->get_menu_active()) {
      prompt = g_shell->get_prompt();
    } else {
      prompt = g_shell->get_ai_prompt();
    }
    if (g_theme && g_theme->uses_newline()) {
      prompt += "\n";
      prompt += g_shell->get_newline_prompt();
    }

    // Get inline right-aligned text
    inline_right_text = g_shell->get_inline_right_prompt();

    if (g_debug_mode) {
      auto render_time_end = std::chrono::steady_clock::now();
      auto render_duration =
          std::chrono::duration_cast<std::chrono::microseconds>(
              render_time_end - render_time_start);
      std::cerr << "DEBUG: Prompt rendering took " << render_duration.count()
                << "μs" << std::endl;
    }

    if (g_debug_mode) {
      std::cerr << "DEBUG: About to call ic_readline with prompt: '" << prompt
                << "'" << std::endl;
      if (!inline_right_text.empty()) {
        std::cerr << "DEBUG: Inline right text: '" << inline_right_text << "'"
                  << std::endl;
      }
    }

    char* input;
    const char* initial_input =
        input_buffer.empty() ? nullptr : input_buffer.c_str();
    if (!inline_right_text.empty()) {
      input = ic_readline_inline(prompt.c_str(), inline_right_text.c_str(),
                                 initial_input);
    } else {
      input = ic_readline(prompt.c_str(), initial_input);
    }
    input_buffer = "";  // Clear input buffer after using it
    if (g_debug_mode)
      std::cerr << "DEBUG: ic_readline returned" << std::endl;
    if (input != nullptr) {
      std::string command(input);
      if (g_debug_mode)
        std::cerr << "DEBUG: User input: " << command << std::endl;
      ic_free(input);
      if (process_command_line(command)) {
        break;
      }
      if (g_exit_flag) {
        break;
      }
    } else {
      continue;
    }
    notify_plugins("main_process_end", "");
    if (g_exit_flag) {
      std::cerr << "Exiting main process loop..." << std::endl;
      break;
    }
  }
  
  // Cleanup threaded input monitor
  if (g_debug_mode)
    std::cerr << "DEBUG: Shutting down threaded input monitor" << std::endl;
  threaded_input_monitor::stop_monitoring();
  threaded_input_monitor::shutdown();
}

void notify_plugins(const std::string& trigger, const std::string& data) {
  // notify all enabled plugins of the event with data
  if (g_plugin == nullptr) {
    if (g_debug_mode)
      std::cerr << "DEBUG: notify_plugins: plugin manager is nullptr"
                << std::endl;
    return;
  }
  if (g_plugin->get_enabled_plugins().empty()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: notify_plugins: no enabled plugins" << std::endl;
    return;
  }
  if (g_debug_mode) {
    std::cerr << "DEBUG: Notifying plugins of trigger: " << trigger
              << " with data: " << data << std::endl;
  }
  g_plugin->trigger_subscribed_global_event(trigger, data);
}