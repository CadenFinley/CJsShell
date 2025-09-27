#pragma once

#include <string>

/**
 * Main interactive loop for the shell
 * Handles user input, command execution, and prompt display
 */
void main_process_loop();

/**
 * Update the terminal title with current shell information
 */
void update_terminal_title();

/**
 * Reprint the current prompt (used by plugins and signal handlers)
 */
void reprint_prompt();

/**
 * Notify all enabled plugins of an event
 * @param trigger The event trigger name
 * @param data Associated data for the event
 */
void notify_plugins(const std::string& trigger, const std::string& data);