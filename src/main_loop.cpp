#include "main_loop.h"

#include <chrono>
#include <cstdio>
#include <deque>
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
#include "utils/typeahead.h"

namespace {
// Global input buffer for typeahead
std::string g_input_buffer;
std::deque<std::string> g_typeahead_queue;

constexpr std::size_t kMaxQueuedCommands = 32;

std::string normalize_line_edit_sequences(const std::string& input) {
  std::string normalized;
  normalized.reserve(input.size());

  for (unsigned char ch : input) {
    switch (ch) {
      case '\b':
      case 0x7F: {  // Backspace or DEL
        if (!normalized.empty()) {
          normalized.pop_back();
        }
        break;
      }
      case 0x15: {  // Ctrl+U clears to start of line
        while (!normalized.empty() && normalized.back() != '\n') {
          normalized.pop_back();
        }
        break;
      }
      case 0x17: {  // Ctrl+W deletes previous word
        while (!normalized.empty() &&
               (normalized.back() == ' ' || normalized.back() == '\t')) {
          normalized.pop_back();
        }
        while (!normalized.empty() && normalized.back() != ' ' &&
               normalized.back() != '\t' && normalized.back() != '\n') {
          normalized.pop_back();
        }
        break;
      }
      default:
        normalized.push_back(static_cast<char>(ch));
        break;
    }
  }

  return normalized;
}

void enqueue_queued_command(const std::string& command) {
  if (g_typeahead_queue.size() >= kMaxQueuedCommands) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Typeahead queue full, dropping oldest entry" << std::endl;
    }
    g_typeahead_queue.pop_front();
  }

  g_typeahead_queue.push_back(command);

  if (g_debug_mode) {
    std::cerr << "DEBUG: Queued typeahead command: '" << command << "'" << std::endl;
  }
}

void ingest_typeahead_input(const std::string& raw_input) {
  if (raw_input.empty()) {
    return;
  }

  std::string combined = g_input_buffer;
  g_input_buffer.clear();
  combined += raw_input;

  if (combined.find('\x1b') != std::string::npos) {
    // Contains escape sequences (arrow keys, etc.) we can't safely interpret
    g_input_buffer = combined;
    if (g_debug_mode) {
      std::cerr << "DEBUG: Stored raw typeahead containing escape sequences" << std::endl;
    }
    return;
  }

  std::string normalized = normalize_line_edit_sequences(combined);

  std::size_t start = 0;
  while (start < normalized.size()) {
    std::size_t newline_pos = normalized.find('\n', start);
    if (newline_pos == std::string::npos) {
      g_input_buffer += normalized.substr(start);
      break;
    }

    std::string line = normalized.substr(start, newline_pos - start);
    enqueue_queued_command(line);
    start = newline_pos + 1;

    if (start == normalized.size()) {
      g_input_buffer.clear();
    }
  }

  if (!normalized.empty() && normalized.back() == '\n') {
    g_input_buffer.clear();
  }

  if (g_debug_mode && !g_input_buffer.empty()) {
    std::cerr << "DEBUG: Buffered typeahead prefill: '" << g_input_buffer
              << "'" << std::endl;
  }
}

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

  // Capture any typeahead input that arrived during command execution
  std::string typeahead_input = typeahead::capture_available_input();
  if (!typeahead_input.empty()) {
    ingest_typeahead_input(typeahead_input);
  }

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
  typeahead::initialize();
  
  // Clear any existing input buffer
  g_input_buffer.clear();
  g_typeahead_queue.clear();

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



    if (g_debug_mode)
      std::cerr << "DEBUG: Calling update_terminal_title()" << std::endl;
    update_terminal_title();

    std::string command_to_run;
    bool command_available = false;

    if (!g_typeahead_queue.empty()) {
      command_to_run = g_typeahead_queue.front();
      g_typeahead_queue.pop_front();
      command_available = true;
      if (g_debug_mode) {
        std::cerr << "DEBUG: Dequeued queued command: '" << command_to_run
                  << "'" << std::endl;
      }
    } else {
      if (g_debug_mode)
        std::cerr << "DEBUG: Generating prompt" << std::endl;

      // Ensure the prompt always starts on a clean line
      std::printf(" \r");
      std::fflush(stdout);

      std::chrono::steady_clock::time_point render_time_start;
      if (g_debug_mode) {
        render_time_start = std::chrono::steady_clock::now();
      }

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

      inline_right_text = g_shell->get_inline_right_prompt();

      if (g_debug_mode) {
        auto render_time_end = std::chrono::steady_clock::now();
        auto render_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(
                render_time_end - render_time_start);
        std::cerr << "DEBUG: Prompt rendering took "
                  << render_duration.count() << "μs" << std::endl;
      }

      if (g_debug_mode) {
        std::cerr << "DEBUG: About to call ic_readline with prompt: '"
                  << prompt << "'" << std::endl;
        if (!inline_right_text.empty()) {
          std::cerr << "DEBUG: Inline right text: '" << inline_right_text
                    << "'" << std::endl;
        }
      }

      const char* initial_input =
          g_input_buffer.empty() ? nullptr : g_input_buffer.c_str();
      char* input = nullptr;
      if (!inline_right_text.empty()) {
        input = ic_readline_inline(prompt.c_str(), inline_right_text.c_str(),
                                   initial_input);
      } else {
        input = ic_readline(prompt.c_str(), initial_input);
      }
      g_input_buffer.clear();

      if (g_debug_mode) {
        std::cerr << "DEBUG: ic_readline returned" << std::endl;
      }

      if (input == nullptr) {
        notify_plugins("main_process_end", "");
        continue;
      }

      command_to_run.assign(input);
      ic_free(input);
      command_available = true;

      if (g_debug_mode) {
        std::cerr << "DEBUG: User input: " << command_to_run << std::endl;
      }
    }

    if (!command_available) {
      notify_plugins("main_process_end", "");
      continue;
    }

    bool exit_requested = process_command_line(command_to_run);
    notify_plugins("main_process_end", "");
    if (exit_requested || g_exit_flag) {
      if (g_exit_flag) {
        std::cerr << "Exiting main process loop..." << std::endl;
      }
      break;
    }
  }
  
  // Cleanup typeahead capture
  typeahead::cleanup();
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