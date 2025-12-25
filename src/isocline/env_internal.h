#pragma once
#ifndef IC_INTERNAL_ENV_H
#define IC_INTERNAL_ENV_H

#include "common.h"

struct ic_env_s;

typedef struct ic_env_s ic_env_t;

// Shared helper used by the environment initializer and public setters
// to update the prompt and continuation prompt markers consistently.
ic_private void ic_env_apply_prompt_markers(ic_env_t* env, const char* prompt_marker,
                                            const char* continuation_prompt_marker);

#endif  // IC_INTERNAL_ENV_H
