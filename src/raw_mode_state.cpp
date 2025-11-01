#include "shell.h"

#include <unistd.h>

void raw_mode_state_init(RawModeState* state) {
    if (state == nullptr) {
        return;
    }
    raw_mode_state_init_with_fd(state, STDIN_FILENO);
}

void raw_mode_state_init_with_fd(RawModeState* state, int fd) {
    if (state == nullptr) {
        return;
    }

    state->entered = false;
    state->fd = fd;

    if (fd < 0 || (isatty(fd) == 0)) {
        return;
    }

    if (tcgetattr(fd, &state->saved_modes) == -1) {
        return;
    }

    struct termios raw_modes = state->saved_modes;
    raw_modes.c_lflag &= ~ICANON;
    raw_modes.c_cc[VMIN] = 0;
    raw_modes.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &raw_modes) == -1) {
        return;
    }

    state->entered = true;
}

void raw_mode_state_release(RawModeState* state) {
    if ((state == nullptr) || !state->entered) {
        return;
    }

    if (tcsetattr(state->fd, TCSANOW, &state->saved_modes) == -1) {
        // Don't do anything if saved mode is invalid; we likely inherited a broken state
    }

    state->entered = false;
}

bool raw_mode_state_entered(const RawModeState* state) {
    return (state != nullptr) && state->entered;
}
