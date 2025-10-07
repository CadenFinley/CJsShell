#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Callback function for isocline to check if multiline input should continue.
/// Returns true if the input has unclosed quotes, brackets, or here documents.
/// @param input The current input buffer
/// @param arg   User-defined argument (unused)
/// @returns true if input should continue, false otherwise
bool multiline_continuation_check(const char* input, void* arg);

#ifdef __cplusplus
}
#endif
