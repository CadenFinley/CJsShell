#pragma once
#ifndef IC_INTERNAL_KEYBINDING_H
#define IC_INTERNAL_KEYBINDING_H

#include "common.h"

struct ic_env_s;
struct ic_keybinding_profile_s;

typedef struct ic_env_s ic_env_t;

ic_private const struct ic_keybinding_profile_s* ic_keybinding_profile_default_ptr(void);

ic_private bool ic_keybinding_apply_profile(ic_env_t* env,
                                            const struct ic_keybinding_profile_s* profile);

#endif  // IC_INTERNAL_KEYBINDING_H
