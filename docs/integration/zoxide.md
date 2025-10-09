# Zoxide Integration

## Prerequisites

Before enabling the integration, make sure you have zoxide installed and on your `$PATH`:

```bash
brew install zoxide
```

Any other installation method supported by zoxide works too—cjsh only needs the executable.

## Enabling the integration in `~/.cjshrc`

Add the following block to your `~/.cjshrc`.

```bash
if command -v zoxide >/dev/null 2>&1; then
    if [ -z "${__CJSH_ZOXIDE_INITIALIZED:-}" ]; then
        __cjsh_zoxide_cd() {
            if builtin cd "$@"; then
                return 0
            fi
            cd "$@"
        }

        z() {
            if [ "$#" -eq 0 ]; then
                __cjsh_zoxide_cd ~ && __cjsh_zoxide_track_pwd
            elif [ "$#" -eq 1 ] && [ "$1" = '-' ]; then
                if [ -n "${OLDPWD:-}" ]; then
                    __cjsh_zoxide_cd "$OLDPWD" && __cjsh_zoxide_track_pwd
                else
                    printf 'zoxide: $OLDPWD is not set\n' >&2
                    return 1
                fi
            elif [ "$#" -eq 1 ] && [ -d "$1" ]; then
                __cjsh_zoxide_cd "$1" && __cjsh_zoxide_track_pwd
            else
                __CJSH_ZOXIDE_RESULT="$(command zoxide query --exclude "$(pwd)" -- "$@")"
                __CJSH_ZOXIDE_STATUS=$?
                if [ "${__CJSH_ZOXIDE_STATUS}" -eq 0 ] && [ -n "$__CJSH_ZOXIDE_RESULT" ]; then
                    __cjsh_zoxide_cd "$__CJSH_ZOXIDE_RESULT" && __cjsh_zoxide_track_pwd
                else
                    unset __CJSH_ZOXIDE_RESULT
                    return $__CJSH_ZOXIDE_STATUS
                fi
                unset __CJSH_ZOXIDE_RESULT
                unset __CJSH_ZOXIDE_STATUS
            fi
        }

        zi() {
            __CJSH_ZOXIDE_RESULT="$(command zoxide query --interactive -- "$@")"
            __CJSH_ZOXIDE_STATUS=$?
            if [ "${__CJSH_ZOXIDE_STATUS}" -eq 0 ] && [ -n "$__CJSH_ZOXIDE_RESULT" ]; then
                __cjsh_zoxide_cd "$__CJSH_ZOXIDE_RESULT" && __cjsh_zoxide_track_pwd
            else
                unset __CJSH_ZOXIDE_RESULT
                return $__CJSH_ZOXIDE_STATUS
            fi
            unset __CJSH_ZOXIDE_RESULT
            unset __CJSH_ZOXIDE_STATUS
        }

        __cjsh_zoxide_track_pwd() {
            command zoxide add -- "$(pwd)"
        }

        if command -v hook >/dev/null 2>&1; then
            hook add chpwd __cjsh_zoxide_track_pwd >/dev/null 2>&1
        fi
        __cjsh_zoxide_track_pwd

        __CJSH_ZOXIDE_INITIALIZED=1
    fi
fi
```