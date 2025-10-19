#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t unicode_codepoint_t;

bool unicode_decode_utf8(const uint8_t* data, ssize_t length, unicode_codepoint_t* codepoint,
                         ssize_t* bytes_read);
int unicode_encode_utf8(unicode_codepoint_t codepoint, uint8_t out[4]);
bool unicode_is_valid_codepoint(unicode_codepoint_t codepoint);
int unicode_codepoint_width(unicode_codepoint_t codepoint);
bool unicode_is_combining_codepoint(unicode_codepoint_t codepoint);
bool unicode_is_control_codepoint(unicode_codepoint_t codepoint);

#ifdef __cplusplus
}
#endif
