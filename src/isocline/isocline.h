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
#pragma once
#ifndef IC_ISOCLINE_H
#define IC_ISOCLINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "keybindings.h"
#include "keycodes.h"

/// Token returned from ic_readline* when Ctrl+C is pressed with an empty buffer.
#define IC_READLINE_TOKEN_CTRL_C "<CTRL+C>"
/// Token returned from ic_readline* when Ctrl+D is pressed with an empty buffer (EOF).
#define IC_READLINE_TOKEN_CTRL_D "<CTRL+D>"

#ifndef IC_HISTORY_EXIT_CODE_UNKNOWN
#define IC_HISTORY_EXIT_CODE_UNKNOWN (-1)
#endif

/*! \mainpage
Isocline C API reference.

Isocline is a pure C library that can be used as an alternative to the GNU
readline library.

See the [Github repository](https://github.com/cadenfinley/isocline#readme)
for general information and building the library.

Contents:
- \ref readline
- \ref bbcode
- \ref history
- \ref completion
- \ref highlight
- \ref options
- \ref helper
- \ref completex
- \ref term
- \ref async
- \ref alloc
*/

/// \defgroup readline Readline
/// The basic readline interface.
/// \{

/// Isocline version: 102 = 1.0.2.
#define IC_VERSION (104)

/// Read input from the user using rich editing abilities.
/// @param prompt_text   The prompt text, can be NULL for the default ("").
///   The displayed prompt becomes `prompt_text` followed by the `prompt_marker`
///   ("> ").
/// @param inline_right_text Optional text to display right-aligned on the same
///   line as the user input. Can be NULL for no right-aligned text. When user
///   input reaches this text, it will be overridden.
/// @param initial_input Optional initial buffer contents. Pass NULL to start
///   with an empty buffer.
/// @returns the heap allocated input on success, which should be `free`d by the
/// caller.
///   Returns NULL on error. When the user presses ctrl+d or ctrl+c on an empty
///   buffer the function returns the tokens `IC_READLINE_TOKEN_CTRL_D` and
///   `IC_READLINE_TOKEN_CTRL_C` respectively so callers can react accordingly.
///
/// If the standard input (`stdin`) has no editing capability
/// (like a dumb terminal (e.g. `TERM`=`dumb`), running in a debugger, a pipe or
/// redirected file, etc.) the input is read directly from the input stream up
/// to the next line without editing capability. See also \a
/// ic_set_prompt_marker(), \a ic_style_def()
///
/// @see ic_set_prompt_marker(), ic_style_def()
char* ic_readline(const char* prompt_text, const char* inline_right_text,
                  const char* initial_input);

/// Queue a single key event so it is processed before the next read.
/// Returns `false` if the readline environment is not yet initialized.
bool ic_push_key_event(ic_keycode_t key);

/// Callback function type for unhandled key events.
/// This callback is invoked when a key is pressed that is not bound to any
/// isocline action. The callback receives the keycode and can return true
/// to indicate the key was handled, or false to let isocline's default
/// behavior continue.
/// @param key The keycode that was not handled by isocline
/// @param arg User-provided argument passed when setting the callback
/// @returns true if the key was handled by the callback, false otherwise
typedef bool(ic_unhandled_key_fun_t)(ic_keycode_t key, void* arg);

/// Set a callback for unhandled key events.
/// This allows applications to handle custom keybindings that are not
/// directly tied to isocline's built-in actions. The callback will be
/// invoked for any key press that doesn't match an isocline keybinding.
/// @param callback The callback function to invoke for unhandled keys, or NULL to disable
/// @param arg User-provided argument that will be passed to the callback
void ic_set_unhandled_key_handler(ic_unhandled_key_fun_t* callback, void* arg);

/// Callback that produces a transient status message below the current input.
/// The callback runs before each key read while readline is active. The returned
/// string is copied immediately, so it does not need to remain valid after the
/// function returns. Return NULL or an empty string to hide the status message.
/// @param input_buffer The current readline buffer (UTF-8, null-terminated).
/// @param arg User-provided argument passed to ic_set_status_message_callback()
/// @returns Pointer to a UTF-8 string or NULL for no message.
typedef const char*(ic_status_message_fun_t)(const char* input_buffer, void* arg);

/// Set the status message callback that renders text below the current input.
/// @param callback The callback that produces the status message, or NULL to disable
/// @param arg User-provided argument forwarded to the callback
void ic_set_status_message_callback(ic_status_message_fun_t* callback, void* arg);

/// Callback invoked when isocline is about to submit the current buffer.
/// Return `true` to exit the readline loop and hand the buffer back to the caller.
/// Return `false` to keep editing; isocline inserts a newline (when multiline editing is enabled)
/// and continues reading input.
/// @param input_buffer Current UTF-8 buffer contents (never NULL; empty string when buffer is
/// empty)
/// @param arg User-provided argument passed to ic_set_check_for_continuation_or_return_callback()
typedef bool(ic_check_for_continuation_or_return_fun_t)(const char* input_buffer, void* arg);

/// Configure the continuation-or-return callback.
/// The callback runs whenever the user presses Enter, when ic_request_submit() fires, and when
/// the initial buffer ends with a newline. Pass NULL to restore the default behavior (always
/// submit immediately).
/// @param callback The callback that decides whether to exit the readline loop.
/// @param arg User-provided argument forwarded to the callback
void ic_set_check_for_continuation_or_return_callback(
    ic_check_for_continuation_or_return_fun_t* callback, void* arg);

/// Controls when the built-in status hint line (the underlined control hints) is displayed.
/// - `IC_STATUS_HINT_OFF`: never show the built-in hints.
/// - `IC_STATUS_HINT_NORMAL`: show only when both the input buffer and the status line are empty.
/// - `IC_STATUS_HINT_TRANSIENT`: show whenever the status line has no other content (default).
/// - `IC_STATUS_HINT_PERSISTENT`: always show and prepend the hints above other status messages.
typedef enum ic_status_hint_mode_e {
    IC_STATUS_HINT_OFF = 0,
    IC_STATUS_HINT_NORMAL,
    IC_STATUS_HINT_TRANSIENT,
    IC_STATUS_HINT_PERSISTENT,
} ic_status_hint_mode_t;

/// Set when the default status hint line should be rendered. Returns the previous mode.
ic_status_hint_mode_t ic_set_status_hint_mode(ic_status_hint_mode_t mode);

/// Get the current status hint rendering mode.
ic_status_hint_mode_t ic_get_status_hint_mode(void);

/// Queue multiple key events so they are processed before the next read.
/// Returns `false` if the readline environment is not yet initialized.
bool ic_push_key_sequence(const ic_keycode_t* keys, size_t count);

/// Queue raw terminal bytes (including escape sequences) for processing.
/// Returns `false` if the readline environment is not yet initialized.
bool ic_push_raw_input(const uint8_t* data, size_t length);

/// Read heredoc content with full editing capabilities.
/// Reads lines from the user until the specified delimiter is encountered on a line by itself.
/// Each line is read with full isocline editing capabilities (history, completion, etc.).
/// The delimiter line is NOT included in the returned content.
/// @param delimiter The heredoc delimiter string (e.g., "EOF").
/// @param strip_tabs If true, leading tabs are stripped from each line (for <<- syntax).
/// @returns A heap-allocated string containing all heredoc lines joined with newlines,
///          or NULL on error/cancellation. The caller must free() the result.
///          If ctrl-C or ctrl-D is pressed, returns NULL.
char* ic_read_heredoc(const char* delimiter, bool strip_tabs);

/// Set the current input buffer to the specified string.
/// This function can only be called while a readline operation is active.
/// @param buffer The string to set as the current input buffer. If NULL, clears the buffer.
/// @returns true if the buffer was successfully set, false if no readline operation is active.
bool ic_set_buffer(const char* buffer);

/// Get the current input buffer content.
/// This function can only be called while a readline operation is active.
/// The returned string is valid until the next edit operation or until readline returns.
/// @returns A pointer to the current buffer content, or NULL if no readline operation is active.
///          The returned string should NOT be freed by the caller.
const char* ic_get_buffer(void);

/// Get the current cursor position in the input buffer.
/// This function can only be called while a readline operation is active.
/// @param out_pos Pointer to receive the cursor position (byte offset from start of buffer).
/// @returns true if the position was successfully retrieved, false if no readline operation is
/// active.
bool ic_get_cursor_pos(size_t* out_pos);

/// Set the current cursor position in the input buffer.
/// This function can only be called while a readline operation is active.
/// @param pos The desired cursor position (byte offset from start of buffer).
/// @returns true if the position was successfully set, false if no readline operation is active.
bool ic_set_cursor_pos(size_t pos);

bool ic_request_submit(void);

bool ic_current_loop_reset(const char* new_buffer, const char* new_prompt,
                           const char* new_inline_right);

/// \}

//--------------------------------------------------------------
/// \defgroup bbcode Formatted Text
/// Formatted text using [bbcode
/// markup](https://github.com/daanx/isocline#bbcode-format).
/// \{

/// Print to the terminal while respection bbcode markup.
/// Any unclosed tags are closed automatically at the end of the print.
/// For example:
/// ```
/// ic_print("[b]bold, [i]bold and italic[/i], [red]red and bold[/][/b]
/// default."); ic_print("[b]bold[/], [i b]bold and italic[/], [yellow on
/// blue]yellow on blue background"); ic_style_add("em","i color=#888800");
/// ic_print("[em]emphasis");
/// ```
/// Properties that can be assigned are:
/// * `color=` _clr_, `bgcolor=` _clr_: where _clr_ is either a hex value
/// `#`RRGGBB or `#`RGB, a
///    standard HTML color name, or an ANSI palette name, like `ansi-maroon`,
///    `ansi-default`, etc.
/// * `bold`,`italic`,`reverse`,`underline`: can be `on` or `off`.
/// * everything else is a style; all HTML and ANSI color names are also a style
/// (so we can just use `red`
///   instead of `color=red`, or `on red` instead of `bgcolor=red`), and there
///   are the `b`, `i`, `u`, and `r` styles for bold, italic, underline, and
///   reverse.
///
/// See [here](https://github.com/daanx/isocline#bbcode-format) for a
/// description of the full bbcode format.
void ic_print(const char* s);

/// Print with bbcode markup ending with a newline.
/// @see ic_print()
void ic_println(const char* s);

/// Print formatted with bbcode markup.
/// @see ic_print()
void ic_printf(const char* fmt, ...);

/// Print formatted with bbcode markup.
/// @see ic_print
void ic_vprintf(const char* fmt, va_list args);

/// Define or redefine a style.
/// @param style_name The name of the style.
/// @param fmt        The `fmt` string is the content of a tag and can contain
///   other styles. This is very useful to theme the output of a program
///   by assigning standard styles like `em` or `warning` etc.
void ic_style_def(const char* style_name, const char* fmt);

/// Start a global style that is only reset when calling a matching
/// ic_style_close().
void ic_style_open(const char* fmt);

/// End a global style.
void ic_style_close(void);

/// \}

//--------------------------------------------------------------
// History
//--------------------------------------------------------------
/// \defgroup history History
/// Readline input history.
/// \{

/// Enable history.
/// Use a \a NULL filename to not persist the history. Use -1 for max_entries to
/// get the default (200).
void ic_set_history(const char* fname, long max_entries);

/// Remove the last entry in the history.
/// The last returned input from ic_readline() is automatically added to the
/// history; this function removes it.
void ic_history_remove_last(void);

/// Clear the history.
void ic_history_clear(void);

/// Add an entry to the history with an explicit exit code. Use
/// IC_HISTORY_EXIT_CODE_UNKNOWN when the exit status is not available.
void ic_history_add_with_exit_code(const char* entry, int exit_code);

/// Add an entry to the history
void ic_history_add(const char* entry);
/// Force save in-memory history to the history file.
void ic_history_save(void);

/// \}

//--------------------------------------------------------------
// Basic Completion
//--------------------------------------------------------------

/// \defgroup completion Completion
/// Basic word completion.
/// \{

/// A completion environment
struct ic_completion_env_s;

/// A completion environment
typedef struct ic_completion_env_s ic_completion_env_t;

/// A completion callback that is called by isocline when tab is pressed.
/// It is passed a completion environment (containing the current input and the
/// current cursor position), the current input up-to the cursor (`prefix`) and
/// the user given argument when the callback was set. When using completion
/// transformers, like `ic_complete_quoted_word` the `prefix` contains the the
/// word to be completed without escape characters or quotes.
typedef void(ic_completer_fun_t)(ic_completion_env_t* cenv, const char* prefix);

/// Set the default completion handler.
/// @param completer  The completion function
/// @param arg        Argument passed to the \a completer.
/// There can only be one default completion function, setting it again disables
/// the previous one. The initial completer use `ic_complete_filename`.
void ic_set_default_completer(ic_completer_fun_t* completer, void* arg);

/// In a completion callback (usually from ic_complete_word()), use this
/// function to add a completion. (the completion string is copied by isocline
/// and do not need to be preserved or allocated).
///
/// Returns `true` if the callback should continue trying to find more possible
/// completions. If `false` is returned, the callback should try to return and
/// not add more completions (for improved latency).
bool ic_add_completion(ic_completion_env_t* cenv, const char* completion);

/// In a completion callback (usually from ic_complete_word()), use this
/// function to add a completion. The `display` is used to display the
/// completion in the completion menu, and `help` is displayed for hints for
/// example. Both can be `NULL` for the default. (all are copied by isocline and
/// do not need to be preserved or allocated).
///
/// Returns `true` if the callback should continue trying to find more possible
/// completions. If `false` is returned, the callback should try to return and
/// not add more completions (for improved latency).
bool ic_add_completion_ex(ic_completion_env_t* cenv, const char* completion, const char* display,
                          const char* help);

/// In a completion callback (usually from ic_complete_word()), use this
/// function to add a completion with source information. The `display` is used
/// to display the completion in the completion menu, `help` is displayed for
/// hints, and `source` indicates where the completion came from (e.g.,
/// "history", "file", "command"). All parameters can be `NULL` for the default.
/// (all are copied by isocline and do not need to be preserved or allocated).
///
/// Returns `true` if the callback should continue trying to find more possible
/// completions. If `false` is returned, the callback should try to return and
/// not add more completions (for improved latency).
bool ic_add_completion_ex_with_source(ic_completion_env_t* cenv, const char* completion,
                                      const char* display, const char* help, const char* source);

/// In a completion callback (usually from ic_complete_word()), use this
/// function to add completions. The `completions` array should be terminated
/// with a NULL element, and all elements are added as completions if they start
/// with `prefix`.
///
/// Returns `true` if the callback should continue trying to find more possible
/// completions. If `false` is returned, the callback should try to return and
/// not add more completions (for improved latency).
bool ic_add_completions(ic_completion_env_t* cenv, const char* prefix, const char** completions);

/// Complete a filename.
/// Complete a filename given a semi-colon separated list of root directories
/// `roots` and semi-colon separated list of possible extensions (excluding
/// directories). If `roots` is NULL, the current directory is the root (".").
/// If `extensions` is NULL, any extension will match.
/// Each root directory should _not_ end with a directory separator.
/// If a directory is completed, the `dir_separator` is added at the end if it
/// is not `0`. Usually the `dir_separator` is `/` but it can be set to `\\` on
/// Windows systems. For example:
/// ```
/// /ho         --> /home/
/// /home/.ba   --> /home/.bashrc
/// ```
/// (This already uses ic_complete_quoted_word() so do not call it from inside a
/// word handler).
void ic_complete_filename(ic_completion_env_t* cenv, const char* prefix, char dir_separator,
                          const char* roots, const char* extensions);

/// Function that returns whether a (utf8) character (of length `len`) is in a
/// certain character class
/// @see ic_char_is_separator() etc.
typedef bool(ic_is_char_class_fun_t)(const char* s, long len);

/// Complete a _word_ (i.e. _token_).
/// Calls the user provided function `fun` to complete on the
/// current _word_. Almost all user provided completers should use this
/// function. If `is_word_char` is NULL, the default `&ic_char_is_nonseparator`
/// is used. The `prefix` passed to `fun` is modified to only contain the
/// current word, and any results from `ic_add_completion` are automatically
/// adjusted to replace that part. For example, on the input "hello w", a the
/// user `fun` only gets `w` and can just complete with "world" resulting in
/// "hello world" without needing to consider `delete_before` etc.
/// @see ic_complete_qword() for completing quoted and escaped tokens.
void ic_complete_word(ic_completion_env_t* cenv, const char* prefix, ic_completer_fun_t* fun,
                      ic_is_char_class_fun_t* is_word_char);

/// Complete a quoted _word_.
/// Calls the user provided function `fun` to complete while taking
/// care of quotes and escape characters. Almost all user provided completers
/// should use this function. The `prefix` passed to `fun` is modified to be
/// unquoted and unescaped, and any results from `ic_add_completion` are
/// automatically quoted and escaped again. For example, completing `hello
/// world`, the `fun` always just completes `hel` or `hello w` to `hello world`,
/// but depending on user input, it will complete as:
/// ```
/// hel        -->  hello\ world
/// hello\ w   -->  hello\ world
/// hello w    -->                   # no completion, the word is just 'w'>
/// "hel       -->  "hello world"
/// "hello w   -->  "hello world"
/// ```
/// with proper quotes and escapes.
/// If `is_word_char` is NULL, the default `&ic_char_is_nonseparator` is used.
/// @see ic_complete_quoted_word() to customize the word boundary, quotes etc.
void ic_complete_qword(ic_completion_env_t* cenv, const char* prefix, ic_completer_fun_t* fun,
                       ic_is_char_class_fun_t* is_word_char);

/// Complete a _word_.
/// Calls the user provided function `fun` to complete while taking
/// care of quotes and escape characters. Almost all user provided completers
/// should use this function. The `is_word_char` is a set of characters that are
/// part of a "word". Use NULL for the default (`&ic_char_is_nonseparator`). The
/// `escape_char` is the escaping character, usually `\` but use 0 to not have
/// escape characters. The `quote_chars` define the quotes, use NULL for the
/// default `"\'\""` quotes.
/// @see ic_complete_word() which uses the default values for `non_word_chars`,
/// `quote_chars` and `\` for escape characters.
void ic_complete_qword_ex(ic_completion_env_t* cenv, const char* prefix, ic_completer_fun_t fun,
                          ic_is_char_class_fun_t* is_word_char, char escape_char,
                          const char* quote_chars);

/// \}

//--------------------------------------------------------------
/// \defgroup highlight Syntax Highlighting
/// Basic syntax highlighting.
/// \{

/// A syntax highlight environment
struct ic_highlight_env_s;
typedef struct ic_highlight_env_s ic_highlight_env_t;

/// A syntax highlighter callback that is called by readline to syntax highlight
/// user input.
typedef void(ic_highlight_fun_t)(ic_highlight_env_t* henv, const char* input, void* arg);

/// Set a syntax highlighter.
/// There can only be one highlight function, setting it again disables the
/// previous one.
void ic_set_default_highlighter(ic_highlight_fun_t* highlighter, void* arg);

/// Set the style of characters starting at position `pos`.
void ic_highlight(ic_highlight_env_t* henv, long pos, long count, const char* style);

/// Experimental: Convenience callback for a function that highlights `s` using
/// bbcode's. The returned string should be allocated and is free'd by the
/// caller.
typedef char*(ic_highlight_format_fun_t)(const char* s, void* arg);

/// Experimental: Convenience function for highlighting with bbcodes.
/// Can be called in a `ic_highlight_fun_t` callback to colorize the `input`
/// using the the provided `formatted` input that is the styled `input` with
/// bbcodes. The content of `formatted` without bbcode tags should match `input`
/// exactly.
void ic_highlight_formatted(ic_highlight_env_t* henv, const char* s, const char* fmt);

/// \}

//--------------------------------------------------------------
// Readline with a specific completer and highlighter
//--------------------------------------------------------------

/// \defgroup readline
/// \{

/// Read input from the user using rich editing abilities,
/// using a particular completion function and highlighter for this call only.
/// both can be NULL in which case the defaults are used.
/// @see ic_readline(), ic_set_prompt_marker(), ic_set_default_completer(),
/// ic_set_default_highlighter().
char* ic_readline_ex(const char* prompt_text, ic_completer_fun_t* completer, void* completer_arg,
                     ic_highlight_fun_t* highlighter, void* highlighter_arg);

/// \}

//--------------------------------------------------------------
// Options
//--------------------------------------------------------------

/// \defgroup options Options
/// \{

/// Set a prompt marker and a potential marker for extra lines with multiline
/// input. Pass \a NULL for the `prompt_marker` for the default marker (`"> "`).
/// Pass \a NULL for continuation prompt marker to make it equal to the
/// `prompt_marker`.
void ic_set_prompt_marker(const char* prompt_marker, const char* cprompt_marker);

/// Get the current prompt marker.
const char* ic_get_prompt_marker(void);

/// Get the current continuation prompt marker.
const char* ic_get_continuation_prompt_marker(void);

/// Disable or enable multi-line input (enabled by default).
/// Returns the previous setting.
bool ic_enable_multiline(bool enable);

/// Disable or enable sound (enabled by default).
/// A beep is used when tab cannot find any completion for example.
/// Returns the previous setting.
bool ic_enable_beep(bool enable);

/// Disable or enable color output (enabled by default).
/// Returns the previous setting.
bool ic_enable_color(bool enable);

/// Disable or enable duplicate entries in the history (disabled by default).
/// Returns the previous setting.
bool ic_enable_history_duplicates(bool enable);

/// Configure whether the fuzzy history search menu matches case-sensitively (default: enabled).
/// Returns the previous setting.
bool ic_enable_history_fuzzy_case_sensitive(bool enable);

/// Report whether the fuzzy history search menu currently matches case-sensitively.
bool ic_history_fuzzy_search_is_case_sensitive(void);

/// Disable or enable automatic tab completion after a completion
/// to expand as far as possible if the completions are unique. (disabled by
/// default). Returns the previous setting.
bool ic_enable_auto_tab(bool enable);

/// Disable or enable preview of a completion selection (enabled by default)
/// Returns the previous setting.
bool ic_enable_completion_preview(bool enable);

/// Disable or enable automatic identation of continuation lines in multiline
/// input so it aligns with the initial prompt. (enabled by default)
/// Returns the previous setting.
bool ic_enable_multiline_indent(bool enable);

/// Configure how many lines the editor should preallocate when multiline editing is enabled.
/// The default is 1, which means the cursor starts on the first prompt line. Larger values
/// insert blank newline-separated rows before the cursor so that editing begins on the last
/// preallocated line (for example, 2 renders the "2|" line immediately). Values below 1 are
/// clamped to 1.
/// Returns the previous configured line count.
size_t ic_set_multiline_start_line_count(size_t line_count);

/// Get the current number of preallocated lines for multiline editing.
size_t ic_get_multiline_start_line_count(void);

/// Enable or disable line numbers in multiline input mode. (enabled by default)
/// When enabled, each line will be prefixed with a line number (e.g., "2| ", "3| ", etc.).
/// The first line continues to use the regular prompt.
/// Returns the previous setting.
bool ic_enable_line_numbers(bool enable);

/// Enable or disable relative numbering for multiline line numbers.
/// Relative numbering displays the distance from the active cursor line instead of the absolute
/// line index (the current line shows "0"). Enabling relative numbering automatically enables
/// line numbers.
/// Returns the previous relative-numbering state.
bool ic_enable_relative_line_numbers(bool enable);

/// Returns whether line numbers are currently enabled (either absolute or relative).
bool ic_line_numbers_are_enabled(void);

/// Returns whether relative line numbers are currently active.
bool ic_line_numbers_are_relative(void);

/// Allow line numbers to remain visible even when a continuation prompt marker is configured.
/// When disabled (default), setting a continuation prompt hides multiline line numbers.
/// Returns the previous state.
bool ic_enable_line_numbers_with_continuation_prompt(bool enable);

/// Returns whether line numbers stay visible when a continuation prompt is set.
bool ic_line_numbers_with_continuation_prompt_are_enabled(void);

/// Replace the final line of a multi-line prompt with the line-number column.
/// When enabled, the last PS1 line will be dropped whenever line numbers are
/// displayed (e.g., when line numbers are active and either no PS2 is set or
/// continuation line numbers are forced). This makes the `1|` gutter appear in
/// place of the final prompt line so that every editable row shares a common
/// prefix.
bool ic_enable_line_number_prompt_replacement(bool enable);

/// Returns whether the last line of a multi-line prompt is replaced by the
/// line-number column.
bool ic_line_number_prompt_replacement_is_enabled(void);

/// Enable or disable highlighting of the current line number.
/// When enabled (default), the line number for the line containing the cursor
/// is displayed in a different color ("ic-linenumber-current" style) compared to
/// other line numbers ("ic-linenumbers" style).
/// Returns the previous setting.
bool ic_enable_current_line_number_highlight(bool enable);

/// Returns whether current line number highlighting is enabled.
bool ic_current_line_number_highlight_is_enabled(void);

/// Enable or disable visualization of plain space characters inside the buffer (disabled by
/// default). When enabled, every space is rendered using the whitespace marker returned by
/// `ic_get_whitespace_marker()`. Returns the previous state.
bool ic_enable_visible_whitespace(bool enable);

/// Set the marker string used when visualizing spaces. The string is copied so the caller retains
/// ownership. Passing NULL or an empty string restores the default middle-dot marker.
void ic_set_whitespace_marker(const char* marker);

/// Get the marker string currently used for visible whitespace (defaults to a middle dot).
const char* ic_get_whitespace_marker(void);

/// Disable or enable display of short help messages for history search etc. (enabled by default)
/// (full help is always dispayed when pressing F1 regardless of this setting)
/// @returns the previous setting.
bool ic_enable_inline_help(bool enable);

/// Enable or disable prompt cleanup after accepting input.
/// When enabled, accepting a line removes the prompt and rewrites just the
/// trailing prompt segment next to the submitted input. The optional
/// \p extra_lines parameter specifies how many additional terminal lines
/// beyond the prompt should be cleared while rewriting (defaults to 0).
/// Returns the previous setting.
bool ic_enable_prompt_cleanup(bool enable, size_t extra_lines
#ifdef __cplusplus
                                           = 0
#endif
);
bool ic_prompt_cleanup_is_enabled(void);
size_t ic_prompt_cleanup_extra_lines(void);
bool ic_enable_prompt_cleanup_newline(bool enable);
bool ic_prompt_cleanup_newline_is_enabled(void);

/// Enable or disable inserting an empty line after prompt cleanup output.
/// Returns the previous setting.
bool ic_enable_prompt_cleanup_empty_line(bool enable);
bool ic_prompt_cleanup_empty_line_is_enabled(void);

/// Enable or disable multiline truncation during prompt cleanup.
/// When enabled, multiline submissions are collapsed to the first line followed by an ellipsis.
/// Returns the previous setting.
bool ic_enable_prompt_cleanup_truncate_multiline(bool enable);
bool ic_prompt_cleanup_truncate_multiline_is_enabled(void);

/// Enable or disable cursor-tracking for the inline right prompt (RPS1).
/// When enabled, the right-aligned prompt is re-rendered on the same terminal row as the cursor
/// instead of remaining pinned to the first editable row. Returns the previous setting.
bool ic_enable_inline_right_prompt_cursor_follow(bool enable);

/// Returns whether the right-aligned prompt follows the cursor height.
bool ic_inline_right_prompt_follows_cursor(void);

/// Disable or enable hinting (enabled by default)
/// Shows a hint inline when there is a single possible completion.
/// @returns the previous setting.
bool ic_enable_hint(bool enable);

/// Disable or enable spell correction in completion (enabled by default).
/// When enabled and no completion matches, tab will try to correct the
/// current token to the closest available completion.
/// @returns the previous setting.
bool ic_enable_spell_correct(bool enable);

/// Set millisecond delay before a hint is displayed. Can be zero. (0ms by
/// default).
long ic_set_hint_delay(long delay_ms);

/// Disable or enable syntax highlighting (enabled by default).
/// This applies regardless whether a syntax highlighter callback was set
/// (`ic_set_highlighter`) Returns the previous setting.
bool ic_enable_highlight(bool enable);

/// Set millisecond delay for reading escape sequences in order to distinguish
/// a lone ESC from the start of a escape sequence. The defaults are 100ms and
/// 10ms, but it may be increased if working with very slow terminals.
void ic_set_tty_esc_delay(long initial_delay_ms, long followup_delay_ms);

/// Enable highlighting of matching braces (and error highlight unmatched
/// braces).`
bool ic_enable_brace_matching(bool enable);

/// Set matching brace pairs.
/// Pass \a NULL for the default `"()[]{}"`.
void ic_set_matching_braces(const char* brace_pairs);

/// Enable automatic brace insertion (enabled by default).
bool ic_enable_brace_insertion(bool enable);

/// Set matching brace pairs for automatic insertion.
/// Pass \a NULL for the default `()[]{}\"\"''`
void ic_set_insertion_braces(const char* brace_pairs);

/// Register or update a fish-style abbreviation.
/// Abbreviations expand when the trigger is followed by a space, newline, or when
/// submitting the line. The trigger must not contain whitespace.
/// Returns `true` on success.
bool ic_add_abbreviation(const char* trigger, const char* expansion);

/// Remove a previously registered abbreviation.
/// Returns `true` if the abbreviation existed and was removed.
bool ic_remove_abbreviation(const char* trigger);

/// Remove all registered abbreviations.
void ic_clear_abbreviations(void);

/// \}

//--------------------------------------------------------------
// Key bindings
//--------------------------------------------------------------
/// \defgroup keybinding Key binding
/// Manage interactive key bindings.
/// \{

/// Bind a key to an action.
/// @param key The key code to bind.
/// @param action The action to perform when the key is pressed.
/// @returns `true` if the binding was successful, `false` otherwise.
bool ic_bind_key(ic_keycode_t key, ic_key_action_t action);

/// Clear a key binding.
/// @param key The key code whose binding should be removed.
/// @returns `true` if the binding was cleared, `false` if no binding existed.
bool ic_clear_key_binding(ic_keycode_t key);

/// Reset all key bindings to their default values.
void ic_reset_key_bindings(void);

/// Get the action bound to a specific key.
/// @param key The key code to query.
/// @param out_action Pointer to receive the action bound to the key.
/// @returns `true` if a binding exists, `false` otherwise.
bool ic_get_key_binding(ic_keycode_t key, ic_key_action_t* out_action);

/// List all current key bindings.
/// @param buffer Buffer to receive the key binding entries.
/// @param capacity Maximum number of entries the buffer can hold.
/// @returns The number of entries written to the buffer.
size_t ic_list_key_bindings(ic_key_binding_entry_t* buffer, size_t capacity);

/// Get a key action from its name.
/// @param name The name of the action.
/// @returns The corresponding key action, or a default value if not found.
ic_key_action_t ic_key_action_from_name(const char* name);

/// Get the name of a key action.
/// @param action The key action.
/// @returns The name of the action, or NULL if invalid.
const char* ic_key_action_name(ic_key_action_t action);

/// Parse a key specification string into a keycode.
/// @param spec The key specification string (e.g., "ctrl-c", "alt-x").
/// @param out_key Pointer to receive the parsed keycode.
/// @returns `true` if parsing was successful, `false` otherwise.
bool ic_parse_key_spec(const char* spec, ic_keycode_t* out_key);

/// Bind a key to an action using string names.
/// @param key_spec The key specification string (e.g., "ctrl-c").
/// @param action_name The name of the action to bind.
/// @returns `true` if the binding was successful, `false` otherwise.
bool ic_bind_key_named(const char* key_spec, const char* action_name);

/// Format a keycode into a human-readable specification string.
/// @param key The keycode to format.
/// @param buffer Buffer to receive the formatted string.
/// @param buflen Length of the buffer.
/// @returns `true` if formatting was successful, `false` if the buffer was too small.
bool ic_format_key_spec(ic_keycode_t key, char* buffer, size_t buflen);

/// Set the active key binding profile.
/// @param name The name of the profile to activate (e.g., "emacs", "vi").
/// @returns `true` if the profile was set successfully, `false` otherwise.
bool ic_set_key_binding_profile(const char* name);

/// Get the name of the current key binding profile.
/// @returns The name of the active profile.
const char* ic_get_key_binding_profile(void);

/// List all available key binding profiles.
/// @param buffer Buffer to receive the profile information.
/// @param capacity Maximum number of profiles the buffer can hold.
/// @returns The number of profiles written to the buffer.
size_t ic_list_key_binding_profiles(ic_key_binding_profile_info_t* buffer, size_t capacity);

/// Get the default key specification strings for a given action.
/// @param action The key action to query.
/// @returns A string containing the default key specs, or NULL if none exist.
const char* ic_key_binding_profile_default_specs(ic_key_action_t action);

/// \}

//--------------------------------------------------------------
// Advanced Completion
//--------------------------------------------------------------

/// \defgroup completex Advanced Completion
/// \{

/// Get the raw current input (and cursor position if `cursor` != NULL) for the
/// completion. Usually completer functions should look at their `prefix` though
/// as transformers like `ic_complete_word` may modify the prefix (for example,
/// unescape it).
const char* ic_completion_input(ic_completion_env_t* cenv, long* cursor);

/// Get the completion argument passed to `ic_set_completer`.
void* ic_completion_arg(const ic_completion_env_t* cenv);

/// Do we have already some completions?
bool ic_has_completions(const ic_completion_env_t* cenv);

/// Do we already have enough completions and should we return if possible? (for
/// improved latency)
bool ic_stop_completing(const ic_completion_env_t* cenv);

/// Primitive completion, cannot be used with most transformers (like
/// `ic_complete_word` and `ic_complete_qword`). When completed, `delete_before`
/// _bytes_ are deleted before the cursor position, `delete_after` _bytes_ are
/// deleted after the cursor, and finally `completion` is inserted. The
/// `display` is used to display the completion in the completion menu, and
/// `help` is displayed with hinting. Both `display` and `help` can be NULL.
/// (all are copied by isocline and do not need to be preserved or allocated).
///
/// Returns `true` if the callback should continue trying to find more possible
/// completions. If `false` is returned, the callback should try to return and
/// not add more completions (for improved latency).
bool ic_add_completion_prim(ic_completion_env_t* cenv, const char* completion, const char* display,
                            const char* help, long delete_before, long delete_after);

/// Primitive completion with source information. When completed,
/// `delete_before` _bytes_ are deleted before the cursor position,
/// `delete_after` _bytes_ are deleted after the cursor, and finally
/// `completion` is inserted. The `display` is used to display the completion in
/// the completion menu, `help` is displayed with hinting, and `source`
/// indicates where the completion came from. All of `display`, `help`, and
/// `source` can be NULL. (all are copied by isocline and do not need to be
/// preserved or allocated).
///
/// Returns `true` if the callback should continue trying to find more possible
/// completions. If `false` is returned, the callback should try to return and
/// not add more completions (for improved latency).
bool ic_add_completion_prim_with_source(ic_completion_env_t* cenv, const char* completion,
                                        const char* display, const char* help, const char* source,
                                        long delete_before, long delete_after);

/// \}

//--------------------------------------------------------------
/// \defgroup helper Character Classes.
/// Convenience functions for character classes, highlighting and completion.
/// \{

/// Convenience: return the position of a previous code point in a UTF-8 string
/// `s` from postion `pos`. Returns `-1` if `pos <= 0` or `pos > strlen(s)` (or
/// other errors).
long ic_prev_char(const char* s, long pos);

/// Convenience: return the position of the next code point in a UTF-8 string
/// `s` from postion `pos`. Returns `-1` if `pos < 0` or `pos >= strlen(s)` (or
/// other errors).
long ic_next_char(const char* s, long pos);

/// Convenience: does a string `s` starts with a given `prefix` ?
bool ic_starts_with(const char* s, const char* prefix);

/// Convenience: does a string `s` starts with a given `prefix` ignoring (ascii)
/// case?
bool ic_istarts_with(const char* s, const char* prefix);

/// Convenience: character class for whitespace `[ \t\r\n]`.
bool ic_char_is_white(const char* s, long len);

/// Convenience: character class for non-whitespace `[^ \t\r\n]`.
bool ic_char_is_nonwhite(const char* s, long len);

/// Convenience: character class for separators.
/// (``[ \t\r\n,.;:/\\(){}\[\]]``.)
/// This is used for word boundaries in isocline.
bool ic_char_is_separator(const char* s, long len);

/// Convenience: character class for non-separators.
bool ic_char_is_nonseparator(const char* s, long len);

/// Convenience: character class for letters (`[A-Za-z]` and any unicode >
/// 0x80).
bool ic_char_is_letter(const char* s, long len);

/// Convenience: character class for digits (`[0-9]`).
bool ic_char_is_digit(const char* s, long len);

/// Convenience: character class for hexadecimal digits (`[A-Fa-f0-9]`).
bool ic_char_is_hexdigit(const char* s, long len);

/// Convenience: character class for identifier letters (`[A-Za-z0-9_-]` and any
/// unicode > 0x80).
bool ic_char_is_idletter(const char* s, long len);

/// Convenience: character class for filename letters (_not in_ "
/// \t\r\n`@$><=;|&\{\}\(\)\[\]]").
bool ic_char_is_filename_letter(const char* s, long len);

/// Convenience: If this is a token start, return the length. Otherwise return
/// 0.
long ic_is_token(const char* s, long pos, ic_is_char_class_fun_t* is_token_char);

/// Convenience: Does this match the specified token?
/// Ensures not to match prefixes or suffixes, and returns the length of the
/// match (in bytes). E.g.
/// `ic_match_token("function",0,&ic_char_is_letter,"fun")` returns 0. while
/// `ic_match_token("fun x",0,&ic_char_is_letter,"fun"})` returns 3.
long ic_match_token(const char* s, long pos, ic_is_char_class_fun_t* is_token_char,
                    const char* token);

/// Convenience: Do any of the specified tokens match?
/// Ensures not to match prefixes or suffixes, and returns the length of the
/// match (in bytes). E.g.
/// `ic_match_any_token("function",0,&ic_char_is_letter,{"fun","func",NULL})`
/// returns 0. while `ic_match_any_token("func
/// x",0,&ic_char_is_letter,{"fun","func",NULL})` returns 4.
long ic_match_any_token(const char* s, long pos, ic_is_char_class_fun_t* is_token_char,
                        const char** tokens);

/// \}

//--------------------------------------------------------------
/// \defgroup term Terminal
///
/// Experimental: Low level terminal output.
/// Ensures basic ANSI SGR escape sequences are processed
/// in a portable way (e.g. on Windows)
/// \{

/// Initialize for terminal output.
/// Call this before using the terminal write functions (`ic_term_write`)
/// Does nothing on most platforms but on Windows it sets the console to UTF8
/// output and possible enables virtual terminal processing.
void ic_term_init(void);

/// Call this when done with the terminal functions.
void ic_term_done(void);

/// Flush the terminal output.
/// (happens automatically on newline characters ('\n') as well).
void ic_term_flush(void);

/// Write a string to the console (and process CSI escape sequences).
void ic_term_write(const char* s);

/// Write a string to the console and end with a newline
/// (and process CSI escape sequences).
void ic_term_writeln(const char* s);

/// Print prompt to the console with the proper styling.
/// @param prompt_text   The prompt text, can be NULL for the default ("").
/// @param continuation_line   If true, uses the continuation prompt marker
/// instead of the primary one.
void ic_print_prompt(const char* prompt_text, bool continuation_line);

/// Write a formatted string to the console.
/// (and process CSI escape sequences)
void ic_term_writef(const char* fmt, ...);

/// Write a formatted string to the console.
void ic_term_vwritef(const char* fmt, va_list args);

/// Set text attributes from a style.
void ic_term_style(const char* style);

/// Set text attribute to bold.
void ic_term_bold(bool enable);

/// Set text attribute to underline.
void ic_term_underline(bool enable);

/// Set text attribute to italic.
void ic_term_italic(bool enable);

/// Set text attribute to reverse video.
void ic_term_reverse(bool enable);

/// Set text attribute to ansi color palette index between 0 and 255 (or 256 for
/// the ANSI "default" color). (auto matched to smaller palette if not
/// supported)
void ic_term_color_ansi(bool foreground, int color);

/// Set text attribute to 24-bit RGB color (between `0x000000` and `0xFFFFFF`).
/// (auto matched to smaller palette if not supported)
void ic_term_color_rgb(bool foreground, uint32_t color);

/// Set underline color using an ANSI palette index between 0 and 255
/// (or 256 for the ANSI "default" color). (auto matched to smaller palette if
/// not supported)
void ic_term_underline_color_ansi(int color);

/// Set underline color using a 24-bit RGB color (between `0x000000` and
/// `0xFFFFFF`). (auto matched to smaller palette if not supported)
void ic_term_underline_color_rgb(uint32_t color);

/// Reset the text attributes.
void ic_term_reset(void);

/// Get the palette used by the terminal:
/// This is usually initialized from the COLORTERM environment variable. The
/// possible values of COLORTERM for each palette are given in parenthesis.
///
/// - 1: monochrome (`monochrome`)
/// - 3: old ANSI terminal with 8 colors, using bold for bright
/// (`8color`/`3bit`)
/// - 4: regular ANSI terminal with 16 colors.     (`16color`/`4bit`)
/// - 8: terminal with ANSI 256 color palette.     (`256color`/`8bit`)
/// - 24: true-color terminal with full RGB colors.
/// (`truecolor`/`24bit`/`direct`)
int ic_term_get_color_bits(void);

/// \}

//--------------------------------------------------------------
/// \defgroup async ASync
/// Async support
/// \{

/// Thread-safe way to asynchronously unblock a readline.
/// Behaves as if the user pressed the `ctrl-C` character
/// (resulting in returning NULL from `ic_readline`).
/// Returns `true` if the event was successfully delivered.
/// (This may not be supported on all platforms, but it is
/// functional on Linux, macOS and Windows).
bool ic_async_stop(void);

/// Thread-safe way to asynchronously unblock a getline operation.
/// Can be used to interrupt getline from another thread when reading from
/// stdin. Returns `true` if the event was successfully delivered.
bool ic_async_interrupt_getline(void);

/// \}

//--------------------------------------------------------------
/// \defgroup alloc Custom Allocation
/// Register allocation functions for custom allocators
/// \{

typedef void*(ic_malloc_fun_t)(size_t size);
typedef void*(ic_realloc_fun_t)(void* p, size_t newsize);
typedef void(ic_free_fun_t)(void* p);

/// Initialize with custom allocation functions.
/// This must be called as the first function in a program!
void ic_init_custom_alloc(ic_malloc_fun_t* custom_malloc, ic_realloc_fun_t* custom_realloc,
                          ic_free_fun_t* custom_free);

/// Free a potentially custom alloc'd pointer (in particular, the result
/// returned from `ic_readline`)
void ic_free(void* p);

/// Allocate using the current memory allocator.
void* ic_malloc(size_t sz);

/// Duplicate a string using the current memory allocator.
const char* ic_strdup(const char* s);

/// \}

#ifdef __cplusplus
}
#endif

#endif  /// IC_ISOCLINE_H
