#pragma once

#include <string>

/**
 * @brief POSIX-compliant prompt formatter for CJsShell
 *
 * Supports escape sequences similar to bash PS1 with extensions for:
 * - Multiline prompts (using \n)
 * - Right-aligned text (using \r prefix)
 * - Color codes and formatting
 *
 * Escape sequences:
 *   \a     ASCII bell (07)
 *   \d     Date in "Weekday Month Date" format
 *   \D{format} strftime format (e.g., \D{%Y-%m-%d})
 *   \e     ASCII escape (033)
 *   \h     Hostname up to first '.'
 *   \H     Full hostname
 *   \j     Number of jobs managed by shell
 *   \l     Basename of terminal device
 *   \n     Newline
 *   \r     Carriage return (for right-aligned prompts)
 *   \s     Name of shell
 *   \t     Time in 24-hour HH:MM:SS format
 *   \T     Time in 12-hour HH:MM:SS format
 *   \@     Time in 12-hour am/pm format
 *   \A     Time in 24-hour HH:MM format
 *   \u     Username
 *   \v     Version of shell
 *   \V     Release of shell (version + patch)
 *   \w     Current working directory with $HOME as ~
 *   \W     Basename of current working directory
 *   \!     History number of this command
 *   \#     Command number
 *   \$     Prompt character ($ for normal user, # for root)
 *   \nnn   Character with octal code nnn
 *   \\     Backslash
 *   \[     Begin non-printing characters (for terminal codes)
 *   \]     End non-printing characters
 */

class PromptFormatter {
   public:
    /**
     * @brief Format a prompt string with escape sequences
     * @param format The prompt format string (e.g., from $CJSH_PROMPT)
     * @return Formatted prompt ready for isocline
     */
    static std::string format_prompt(const std::string& format);

    /**
     * @brief Check if a prompt contains right-aligned content
     * @param format The prompt format string
     * @return true if the prompt has right-aligned portions
     */
    static bool has_right_aligned(const std::string& format);

    /**
     * @brief Split prompt into main and right-aligned parts
     * @param format The prompt format string
     * @return Pair of (main_prompt, right_aligned_prompt)
     */
    static std::pair<std::string, std::string> split_prompt(const std::string& format);

   private:
    /**
     * @brief Process escape sequences in a prompt string
     * @param format The input format string
     * @param for_isocline Whether to process for isocline (handle \[ \])
     * @return Processed string
     */
    static std::string process_escapes(const std::string& format, bool for_isocline = true);

    /**
     * @brief Get the current username
     */
    static std::string get_username();

    /**
     * @brief Get the hostname (short or full)
     * @param full Whether to return full hostname
     */
    static std::string get_hostname(bool full = false);

    /**
     * @brief Get current working directory (with optional ~ substitution)
     * @param basename_only Whether to return only basename
     */
    static std::string get_cwd(bool basename_only = false);

    /**
     * @brief Get shell version information
     * @param include_patch Whether to include patch level
     */
    static std::string get_version(bool include_patch = false);

    /**
     * @brief Format time according to strftime format
     * @param format The strftime format string
     */
    static std::string format_time(const std::string& format);

    /**
     * @brief Get terminal device name
     */
    static std::string get_tty_name();

    /**
     * @brief Get number of background jobs
     */
    static int get_job_count();

    /**
     * @brief Convert octal string to character
     * @param octal Octal string (e.g., "033")
     */
    static char octal_to_char(const std::string& octal);
};
