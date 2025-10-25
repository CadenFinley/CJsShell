/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "tty.h"

#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#define isatty(fd) _isatty(fd)
#define read(fd, s, n) _read(fd, s, n)
#define STDIN_FILENO 0
#if (_WIN32_WINNT < 0x0600)
WINBASEAPI ULONGLONG WINAPI GetTickCount64(VOID);
#endif
#else
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#if !defined(FIONREAD)
#include <fcntl.h>
#endif
#endif

#define TTY_PUSH_MAX (32)

struct tty_s {
    int fd_in;
    bool raw_enabled;
    bool is_utf8;
    bool has_term_resize_event;
    bool term_resize_event;
    alloc_t* mem;
    code_t pushbuf[TTY_PUSH_MAX];
    ssize_t push_count;
    uint8_t cpushbuf[TTY_PUSH_MAX];
    ssize_t cpush_count;
    long esc_initial_timeout;

    long esc_timeout;
#if defined(_WIN32)
    HANDLE hcon;
    DWORD hcon_orig_mode;
#else
    struct termios orig_ios;
    struct termios raw_ios;
#endif
    bool paste_mode;
};

//-------------------------------------------------------------
// Forward declarations of platform dependent primitives below
//-------------------------------------------------------------

ic_private bool tty_readc_noblock(
    tty_t* tty, uint8_t* c,
    long timeout_ms);  // does not modify `c` when no input (false is returned)

//-------------------------------------------------------------
// Key code helpers
//-------------------------------------------------------------
ic_private bool tty_readc_noblock(tty_t* tty, uint8_t* c, long timeout_ms);

ic_private bool code_is_ascii_char(code_t c, char* chr) {
    if (c >= ' ' && c <= 0x7F) {
        if (chr != NULL)
            *chr = (char)c;
        return true;
    } else {
        if (chr != NULL)
            *chr = 0;
        return false;
    }
}

ic_private bool code_is_unicode(code_t c, unicode_t* uchr) {
    if (c <= KEY_UNICODE_MAX) {
        if (uchr != NULL)
            *uchr = c;
        return true;
    } else {
        if (uchr != NULL)
            *uchr = 0;
        return false;
    }
}

ic_private bool code_is_virt_key(code_t c) {
    return (KEY_NO_MODS(c) <= 0x20 || KEY_NO_MODS(c) >= KEY_VIRT);
}

static code_t modify_code(code_t code, bool in_paste_mode);

static code_t tty_read_utf8(tty_t* tty, uint8_t c0) {
    uint8_t buf[5];
    memset(buf, 0, 5);

    buf[0] = c0;
    ssize_t count = 1;
    if (c0 > 0x7F) {
        if (tty_readc_noblock(tty, buf + count, tty->esc_timeout)) {
            count++;
            if (c0 > 0xDF) {
                if (tty_readc_noblock(tty, buf + count, tty->esc_timeout)) {
                    count++;
                    if (c0 > 0xEF) {
                        if (tty_readc_noblock(tty, buf + count, tty->esc_timeout)) {
                            count++;
                        }
                    }
                }
            }
        }
    }

    buf[count] = 0;
    debug_msg("tty: read utf8: count: %zd: %02x,%02x,%02x,%02x", count, buf[0], buf[1], buf[2],
              buf[3]);

    ssize_t read = 0;
    code_t code = key_unicode(unicode_from_qutf8(buf, count, &read));

    while (count > read) {
        count--;
        if (count >= 0 && count <= 4) {
            tty_cpush_char(tty, buf[count]);
        }
    }
    return code;
}

static bool tty_code_pop(tty_t* tty, code_t* code);

ic_private bool tty_read_timeout(tty_t* tty, long timeout_ms, code_t* code) {
    if (tty_code_pop(tty, code)) {
        return code;
    }

    uint8_t c;
    if (!tty_readc_noblock(tty, &c, timeout_ms))
        return false;

    if (c == KEY_ESC) {
        *code = tty_read_esc(tty, tty->esc_initial_timeout, tty->esc_timeout);
    } else if (c <= 0x7F) {
        *code = key_unicode(c);
    } else if (tty->is_utf8) {
        *code = tty_read_utf8(tty, c);
    } else {
        *code = key_unicode(unicode_from_raw(c));
    }

    *code = modify_code(*code, tty->paste_mode);

    if (*code == IC_KEY_PASTE_START) {
        tty->paste_mode = true;
        debug_msg("tty: entering paste mode\n");
    } else if (*code == IC_KEY_PASTE_END) {
        tty->paste_mode = false;
        debug_msg("tty: exiting paste mode\n");
    }

    return true;
}

static code_t modify_code(code_t code, bool in_paste_mode) {
    code_t key = KEY_NO_MODS(code);
    code_t mods = KEY_MODS(code);
    debug_msg("tty: readc %s%s%s 0x%03x ('%c')\n", mods & KEY_MOD_SHIFT ? "shift+" : "",
              mods & KEY_MOD_CTRL ? "ctrl+" : "", mods & KEY_MOD_ALT ? "alt+" : "", key,
              (key >= ' ' && key <= '~' ? key : ' '));

    if (key == KEY_NONE && mods == 0 && !in_paste_mode) {
        code = WITH_CTRL(KEY_SPACE);
        key = KEY_SPACE;
        mods = KEY_MOD_CTRL;
    }

    if (key == KEY_RUBOUT) {
        code = KEY_BACKSP | mods;
    }

    else if (key == key_char('\x1F') && (mods & KEY_MOD_ALT) == 0) {
        key = '_';
        code = WITH_CTRL(key_char('_'));
    }

    else if (key == KEY_ENTER &&
             (mods == KEY_MOD_SHIFT || mods == KEY_MOD_ALT || mods == KEY_MOD_CTRL)) {
        code = KEY_LINEFEED;
    }

    else if (code == WITH_CTRL(KEY_TAB)) {
        code = KEY_SHIFT_TAB;
    }

    else if (code == WITH_ALT(KEY_DOWN) || code == WITH_ALT('>') || code == WITH_CTRL(KEY_END)) {
        code = KEY_PAGEDOWN;
    } else if (code == WITH_ALT(KEY_UP) || code == WITH_ALT('<') || code == WITH_CTRL(KEY_HOME)) {
        code = KEY_PAGEUP;
    }

    if (key < ' ' && (mods & KEY_MOD_CTRL) != 0) {
        code &= ~KEY_MOD_CTRL;
    }

    return code;
}

ic_private code_t tty_read(tty_t* tty) {
    code_t code;
    if (!tty_read_timeout(tty, -1, &code))
        return KEY_NONE;
    return code;
}

//-------------------------------------------------------------
// Read back an ANSI query response
//-------------------------------------------------------------

ic_private bool tty_read_esc_response(tty_t* tty, char esc_start, bool final_st, char* buf,
                                      ssize_t buflen) {
    buf[0] = 0;
    ssize_t len = 0;
    uint8_t c = 0;
    if (!tty_readc_noblock(tty, &c, 2 * tty->esc_initial_timeout) || c != '\x1B') {
        debug_msg("initial esc response failed: 0x%02x\n", c);
        return false;
    }
    if (!tty_readc_noblock(tty, &c, tty->esc_timeout) || (c != esc_start))
        return false;
    while (len < buflen) {
        if (!tty_readc_noblock(tty, &c, tty->esc_timeout))
            return false;
        if (final_st) {
            if (c == '\x07' || c == '\x02') {
                break;
            } else if (c == '\x1B') {
                uint8_t c1;
                if (!tty_readc_noblock(tty, &c1, tty->esc_timeout))
                    return false;
                if (c1 == '\\')
                    break;
                tty_cpush_char(tty, c1);
            }
        } else {
            if (c == '\x02') {
                break;
            } else if (!((c >= '0' && c <= '9') || strchr("<=>?;:", c) != NULL)) {
                buf[len++] = (char)c;
                break;
            }
        }
        buf[len++] = (char)c;
    }
    buf[len] = 0;
    debug_msg("tty: escape query response: %s\n", buf);
    return true;
}

//-------------------------------------------------------------
// High level code pushback
//-------------------------------------------------------------

static bool tty_code_pop(tty_t* tty, code_t* code) {
    if (tty->push_count <= 0)
        return false;
    tty->push_count--;
    *code = tty->pushbuf[tty->push_count];
    return true;
}

ic_private void tty_code_pushback(tty_t* tty, code_t c) {
    if (tty->push_count >= TTY_PUSH_MAX)
        return;
    tty->pushbuf[tty->push_count] = c;
    tty->push_count++;
}

//-------------------------------------------------------------
// low-level character pushback (for escape sequences and windows)
//-------------------------------------------------------------

ic_private bool tty_cpop(tty_t* tty, uint8_t* c) {
    if (tty->cpush_count <= 0) {
        return false;
    } else {
        tty->cpush_count--;
        *c = tty->cpushbuf[tty->cpush_count];
        return true;
    }
}

static void tty_cpush(tty_t* tty, const char* s) {
    ssize_t len = ic_strlen(s);
    if (tty->push_count + len > TTY_PUSH_MAX) {
        debug_msg("tty: cpush buffer full! (pushing %s)\n", s);
        assert(false);
        return;
    }
    for (ssize_t i = 0; i < len; i++) {
        tty->cpushbuf[tty->cpush_count + i] = (uint8_t)(s[len - i - 1]);
    }
    tty->cpush_count += len;
    return;
}

static void tty_cpushf(tty_t* tty, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[TTY_PUSH_MAX + 1];
    vsnprintf(buf, TTY_PUSH_MAX, fmt, args);
    buf[TTY_PUSH_MAX] = 0;
    tty_cpush(tty, buf);
    va_end(args);
    return;
}

ic_private void tty_cpush_char(tty_t* tty, uint8_t c) {
    uint8_t buf[2];
    buf[0] = c;
    buf[1] = 0;
    tty_cpush(tty, (const char*)buf);
}

//-------------------------------------------------------------
// Push escape codes (used on Windows to insert keys)
//-------------------------------------------------------------

static unsigned csi_mods(code_t mods) {
    unsigned m = 1;
    if (mods & KEY_MOD_SHIFT)
        m += 1;
    if (mods & KEY_MOD_ALT)
        m += 2;
    if (mods & KEY_MOD_CTRL)
        m += 4;
    return m;
}

static void tty_cpush_csi_vt(tty_t* tty, code_t mods, uint32_t vtcode) {
    tty_cpushf(tty, "\x1B[%u;%u~", vtcode, csi_mods(mods));
}

static void tty_cpush_csi_xterm(tty_t* tty, code_t mods, char xcode) {
    tty_cpushf(tty, "\x1B[1;%u%c", csi_mods(mods), xcode);
}

static void tty_cpush_csi_unicode(tty_t* tty, code_t mods, uint32_t unicode) {
    if ((unicode < 0x80 && mods == 0) ||
        (mods == KEY_MOD_CTRL && unicode < ' ' && unicode != KEY_TAB && unicode != KEY_ENTER &&
         unicode != KEY_LINEFEED && unicode != KEY_BACKSP) ||
        (mods == KEY_MOD_SHIFT && unicode >= ' ' && unicode <= KEY_RUBOUT)) {
        tty_cpush_char(tty, (uint8_t)unicode);
    } else {
        tty_cpushf(tty, "\x1B[%u;%uu", unicode, csi_mods(mods));
    }
}

//-------------------------------------------------------------
// Init
//-------------------------------------------------------------

static bool tty_init_raw(tty_t* tty);
static void tty_done_raw(tty_t* tty);

static bool tty_init_utf8(tty_t* tty) {
#ifdef _WIN32
    tty->is_utf8 = true;
#else
    const char* loc = setlocale(LC_ALL, "");
    tty->is_utf8 =
        (ic_icontains(loc, "UTF-8") || ic_icontains(loc, "utf8") || ic_stricmp(loc, "C") == 0);
    debug_msg("tty: utf8: %s (loc=%s)\n", tty->is_utf8 ? "true" : "false", loc);
#endif
    return true;
}

ic_private tty_t* tty_new(alloc_t* mem, int fd_in) {
    tty_t* tty = mem_zalloc_tp(mem, tty_t);
    tty->mem = mem;
    tty->fd_in = (fd_in < 0 ? STDIN_FILENO : fd_in);
#if defined(__APPLE__)
    tty->esc_initial_timeout = 200;
#else
    tty->esc_initial_timeout = 100;
#endif
    tty->esc_timeout = 10;
    tty->paste_mode = false;
    if (!(isatty(tty->fd_in) && tty_init_raw(tty) && tty_init_utf8(tty))) {
        tty_free(tty);
        return NULL;
    }
    return tty;
}

ic_private void tty_free(tty_t* tty) {
    if (tty == NULL)
        return;
    tty_end_raw(tty);
    tty_done_raw(tty);
    mem_free(tty->mem, tty);
}

ic_private bool tty_is_utf8(const tty_t* tty) {
    if (tty == NULL)
        return true;
    return (tty->is_utf8);
}

ic_private bool tty_term_resize_event(tty_t* tty) {
    if (tty == NULL)
        return true;
    if (tty->has_term_resize_event) {
        if (!tty->term_resize_event)
            return false;
        tty->term_resize_event = false;
    }
    return true;
}

ic_private void tty_set_esc_delay(tty_t* tty, long initial_delay_ms, long followup_delay_ms) {
    tty->esc_initial_timeout =
        (initial_delay_ms < 0 ? 0 : (initial_delay_ms > 1000 ? 1000 : initial_delay_ms));
    tty->esc_timeout =
        (followup_delay_ms < 0 ? 0 : (followup_delay_ms > 1000 ? 1000 : followup_delay_ms));
}

//-------------------------------------------------------------
// Unix
//-------------------------------------------------------------
#if !defined(_WIN32)

static bool tty_readc_blocking(tty_t* tty, uint8_t* c) {
    if (tty_cpop(tty, c))
        return true;
    *c = 0;
    ssize_t nread = read(tty->fd_in, (char*)c, 1);
    if (nread < 0 && errno == EINTR) {
    }
    return (nread == 1);
}

ic_private bool tty_readc_noblock(tty_t* tty, uint8_t* c, long timeout_ms) {
    if (tty_cpop(tty, c))
        return true;

    if (timeout_ms < 0) {
        return tty_readc_blocking(tty, c);
    }

#if defined(FIONREAD)
    {
        int navail = 0;
        if (ioctl(0, FIONREAD, &navail) == 0) {
            if (navail >= 1) {
                return tty_readc_blocking(tty, c);
            } else if (timeout_ms == 0) {
                return false;
            }
        }
    }
#endif

#if defined(FD_SET)

    fd_set readset;
    struct timeval time;
    FD_ZERO(&readset);
    FD_SET(tty->fd_in, &readset);
    time.tv_sec = (timeout_ms > 0 ? timeout_ms / 1000 : 0);
    time.tv_usec = (timeout_ms > 0 ? 1000 * (timeout_ms % 1000) : 0);
    if (select(tty->fd_in + 1, &readset, NULL, NULL, &time) == 1) {
        return tty_readc_blocking(tty, c);
    }
#else

    do {

#if defined(FIONREAD)
        int navail = 0;
        if (ioctl(0, FIONREAD, &navail) == 0 && navail >= 1) {
            return tty_readc_blocking(tty, c);
        }
#elif defined(O_NONBLOCK)

        int fstatus = fcntl(tty->fd_in, F_GETFL, 0);
        if (fstatus != -1) {
            if (fcntl(tty->fd_in, F_SETFL, (fstatus | O_NONBLOCK)) != -1) {
                char buf[2] = {0, 0};
                ssize_t nread = read(tty->fd_in, buf, 1);
                fcntl(tty->fd_in, F_SETFL, fstatus);
                if (nread >= 1) {
                    *c = (uint8_t)buf[0];
                    return true;
                }
            }
        }
#else
#error "define an nonblocking read for this platform"
#endif

        if (timeout_ms > 0) {
            usleep(50 * 1000L);
            timeout_ms -= 100;
            if (timeout_ms < 0) {
                timeout_ms = 0;
            }
        }
    } while (timeout_ms > 0);
#endif
    return false;
}

#if defined(TIOCSTI)
ic_private bool tty_async_stop(const tty_t* tty) {
    char c = KEY_CTRL_C;
    return (ioctl(tty->fd_in, TIOCSTI, &c) >= 0);
}
#else
ic_private bool tty_async_stop(const tty_t* tty) {
    return false;
}
#endif

#if defined(SIGWINCH) && defined(SA_RESTART)

static tty_t* sig_tty;

typedef struct signal_handler_s {
    int signum;
    union {
        int _avoid_warning;
        struct sigaction previous;
    } action;
} signal_handler_t;

static signal_handler_t sighandlers[] = {
    {SIGWINCH, {0}}, {SIGTERM, {0}}, {SIGINT, {0}},  {SIGQUIT, {0}}, {SIGHUP, {0}},  {SIGSEGV, {0}},
    {SIGTRAP, {0}},  {SIGBUS, {0}},  {SIGTSTP, {0}}, {SIGTTIN, {0}}, {SIGTTOU, {0}}, {0, {0}}};

static bool sigaction_is_valid(struct sigaction* sa) {
    return (sa->sa_sigaction != NULL && sa->sa_handler != SIG_DFL && sa->sa_handler != SIG_IGN);
}

static void sig_handler(int signum, siginfo_t* siginfo, void* uap) {
    if (signum == SIGWINCH) {
        if (sig_tty != NULL) {
            sig_tty->term_resize_event = true;
        }
    } else {
        if (sig_tty != NULL && sig_tty->raw_enabled) {
            tcsetattr(sig_tty->fd_in, TCSAFLUSH, &sig_tty->orig_ios);
            sig_tty->raw_enabled = false;
        }
    }

    signal_handler_t* sh = sighandlers;
    while (sh->signum != 0 && sh->signum != signum) {
        sh++;
    }
    if (sh->signum == signum) {
        if (sigaction_is_valid(&sh->action.previous)) {
            (sh->action.previous.sa_sigaction)(signum, siginfo, uap);
        }
    }
}

static void signals_install(tty_t* tty) {
    sig_tty = tty;

    struct sigaction handler;
    memset(&handler, 0, sizeof(handler));
    sigemptyset(&handler.sa_mask);
    handler.sa_sigaction = &sig_handler;
    handler.sa_flags = SA_RESTART;

    for (signal_handler_t* sh = sighandlers; sh->signum != 0; sh++) {
        if (sigaction(sh->signum, NULL, &sh->action.previous) == 0) {
            if (sh->action.previous.sa_handler != SIG_IGN) {
                if (sigaction(sh->signum, &handler, &sh->action.previous) < 0) {
                    sh->action.previous.sa_sigaction = NULL;
                } else if (sh->signum == SIGWINCH) {
                    sig_tty->has_term_resize_event = true;
                };
            }
        }
    }
}

static void signals_restore(void) {
    for (signal_handler_t* sh = sighandlers; sh->signum != 0; sh++) {
        if (sigaction_is_valid(&sh->action.previous)) {
            sigaction(sh->signum, &sh->action.previous, NULL);
        };
    }
    sig_tty = NULL;
}

#else
static void signals_install(tty_t* tty) {
    ic_unused(tty);
}
static void signals_restore(void) {
}

#endif

ic_private bool tty_start_raw(tty_t* tty) {
    if (tty == NULL)
        return false;
    if (tty->raw_enabled)
        return true;
    if (tcsetattr(tty->fd_in, TCSAFLUSH, &tty->raw_ios) < 0)
        return false;
    tty->raw_enabled = true;
    return true;
}

ic_private void tty_end_raw(tty_t* tty) {
    if (tty == NULL)
        return;
    if (!tty->raw_enabled)
        return;
    tty->cpush_count = 0;
    if (tcsetattr(tty->fd_in, TCSAFLUSH, &tty->orig_ios) < 0)
        return;
    tty->raw_enabled = false;
}

static bool tty_init_raw(tty_t* tty) {
    if (tcgetattr(tty->fd_in, &tty->orig_ios) == -1)
        return false;
    tty->raw_ios = tty->orig_ios;

    tty->raw_ios.c_iflag &= ~(unsigned long)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    tty->raw_ios.c_cflag |= CS8;

    tty->raw_ios.c_lflag &= ~(unsigned long)(ECHO | ICANON | IEXTEN | ISIG);

    tty->raw_ios.c_cc[VTIME] = 0;
    tty->raw_ios.c_cc[VMIN] = 1;

    signals_install(tty);

    return true;
}

static void tty_done_raw(tty_t* tty) {
    ic_unused(tty);
    signals_restore();
}

#else

//-------------------------------------------------------------
// Windows
// For best portability we push CSI escape sequences directly
// to the character stream (instead of returning key codes).
//-------------------------------------------------------------

static void tty_waitc_console(tty_t* tty, long timeout_ms);

ic_private bool tty_readc_noblock(tty_t* tty, uint8_t* c, long timeout_ms) {
    if (tty_cpop(tty, c))
        return true;

    tty_waitc_console(tty, timeout_ms);
    return tty_cpop(tty, c);
}

static void tty_waitc_console(tty_t* tty, long timeout_ms) {
    INPUT_RECORD inp;
    DWORD count;
    uint32_t surrogate_hi = 0;
    while (true) {
        if (timeout_ms >= 0) {
            if (!GetNumberOfConsoleInputEvents(tty->hcon, &count))
                return;
            if (count == 0) {
                if (timeout_ms == 0) {
                    return;
                } else {
                    ULONGLONG start_ms = GetTickCount64();
                    DWORD res = WaitForSingleObject(tty->hcon, (DWORD)timeout_ms);
                    switch (res) {
                        case WAIT_OBJECT_0: {
                            ULONGLONG waited_ms = (GetTickCount64() - start_ms);
                            timeout_ms -= (long)waited_ms;
                            if (timeout_ms < 0) {
                                timeout_ms = 0;
                            }
                            break;
                        }
                        case WAIT_TIMEOUT:
                        case WAIT_ABANDONED:
                        case WAIT_FAILED:
                        default:
                            return;
                    }
                }
            }
        }

        if (!ReadConsoleInputW(tty->hcon, &inp, 1, &count))
            return;
        if (count != 1)
            return;

        if (inp.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            tty->term_resize_event = true;
            continue;
        }

        if (inp.EventType != KEY_EVENT)
            continue;

        DWORD modstate = inp.Event.KeyEvent.dwControlKeyState;

        if (!inp.Event.KeyEvent.bKeyDown && inp.Event.KeyEvent.wVirtualKeyCode == VK_SHIFT) {
            modstate &= (DWORD)~SHIFT_PRESSED;
        }

        DWORD altgr = LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED;
        if ((modstate & altgr) == altgr) {
            modstate &= ~altgr;
        }

        code_t mods = 0;
        if ((modstate & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) != 0)
            mods |= KEY_MOD_CTRL;
        if ((modstate & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) != 0)
            mods |= KEY_MOD_ALT;
        if ((modstate & SHIFT_PRESSED) != 0)
            mods |= KEY_MOD_SHIFT;

        uint32_t chr = (uint32_t)inp.Event.KeyEvent.uChar.UnicodeChar;
        WORD virt = inp.Event.KeyEvent.wVirtualKeyCode;
        debug_msg("tty: console %s: %s%s%s virt 0x%04x, chr 0x%04x ('%c')\n",
                  inp.Event.KeyEvent.bKeyDown ? "down" : "up", mods & KEY_MOD_CTRL ? "ctrl-" : "",
                  mods & KEY_MOD_ALT ? "alt-" : "", mods & KEY_MOD_SHIFT ? "shift-" : "", virt, chr,
                  chr);

        if (!inp.Event.KeyEvent.bKeyDown && virt != VK_MENU) {
            continue;
        }

        if (chr == 0) {
            switch (virt) {
                case VK_UP:
                    tty_cpush_csi_xterm(tty, mods, 'A');
                    return;
                case VK_DOWN:
                    tty_cpush_csi_xterm(tty, mods, 'B');
                    return;
                case VK_RIGHT:
                    tty_cpush_csi_xterm(tty, mods, 'C');
                    return;
                case VK_LEFT:
                    tty_cpush_csi_xterm(tty, mods, 'D');
                    return;
                case VK_END:
                    tty_cpush_csi_xterm(tty, mods, 'F');
                    return;
                case VK_HOME:
                    tty_cpush_csi_xterm(tty, mods, 'H');
                    return;
                case VK_DELETE:
                    tty_cpush_csi_vt(tty, mods, 3);
                    return;
                case VK_PRIOR:
                    tty_cpush_csi_vt(tty, mods, 5);
                    return;
                case VK_NEXT:
                    tty_cpush_csi_vt(tty, mods, 6);
                    return;
                case VK_TAB:
                    tty_cpush_csi_unicode(tty, mods, 9);
                    return;
                case VK_RETURN:
                    tty_cpush_csi_unicode(tty, mods, 13);
                    return;
                default: {
                    uint32_t vtcode = 0;
                    if (virt >= VK_F1 && virt <= VK_F5) {
                        vtcode = 10 + (virt - VK_F1);
                    } else if (virt >= VK_F6 && virt <= VK_F10) {
                        vtcode = 17 + (virt - VK_F6);
                    } else if (virt >= VK_F11 && virt <= VK_F12) {
                        vtcode = 13 + (virt - VK_F11);
                    }
                    if (vtcode > 0) {
                        tty_cpush_csi_vt(tty, mods, vtcode);
                        return;
                    }
                }
            }

        }

        else if (chr >= 0xD800 && chr <= 0xDBFF) {
            surrogate_hi = (chr - 0xD800);
        }

        else if (chr >= 0xDC00 && chr <= 0xDFFF) {
            chr = ((surrogate_hi << 10) + (chr - 0xDC00) + 0x10000);
            tty_cpush_csi_unicode(tty, mods, chr);
            surrogate_hi = 0;
            return;
        }

        else {
            tty_cpush_csi_unicode(tty, mods, chr);
            return;
        }
    }
}

ic_private bool tty_async_stop(const tty_t* tty) {
    INPUT_RECORD events[2];
    memset(events, 0, 2 * sizeof(INPUT_RECORD));
    events[0].EventType = KEY_EVENT;
    events[0].Event.KeyEvent.bKeyDown = TRUE;
    events[0].Event.KeyEvent.uChar.AsciiChar = KEY_CTRL_C;
    events[1] = events[0];
    events[1].Event.KeyEvent.bKeyDown = FALSE;
    DWORD nwritten = 0;
    WriteConsoleInput(tty->hcon, events, 2, &nwritten);
    return (nwritten == 2);
}

ic_private bool tty_start_raw(tty_t* tty) {
    if (tty->raw_enabled)
        return true;
    GetConsoleMode(tty->hcon, &tty->hcon_orig_mode);
    DWORD mode = ENABLE_QUICK_EDIT_MODE | ENABLE_WINDOW_INPUT

        ;
    SetConsoleMode(tty->hcon, mode);
    tty->raw_enabled = true;
    return true;
}

ic_private void tty_end_raw(tty_t* tty) {
    if (!tty->raw_enabled)
        return;
    SetConsoleMode(tty->hcon, tty->hcon_orig_mode);
    tty->raw_enabled = false;
}

static bool tty_init_raw(tty_t* tty) {
    tty->hcon = GetStdHandle(STD_INPUT_HANDLE);
    tty->has_term_resize_event = true;
    return true;
}

static void tty_done_raw(tty_t* tty) {
    ic_unused(tty);
}

#endif
