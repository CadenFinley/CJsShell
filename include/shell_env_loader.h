#ifndef SHELL_ENV_LOADER_H
#define SHELL_ENV_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Load the user’s interactive shell environment into this process
void load_shell_env(void);

#ifdef __cplusplus
}
#endif

#endif // SHELL_ENV_LOADER_H
