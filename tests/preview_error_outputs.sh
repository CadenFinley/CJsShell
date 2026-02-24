#!/usr/bin/env sh

set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

if [ -n "${1-}" ]; then
    CJSH_CANDIDATE="$1"
    shift
else
    CJSH_CANDIDATE="${CJSH:-$REPO_ROOT/build/cjsh}"
fi

if [ "${CJSH_CANDIDATE#/}" = "$CJSH_CANDIDATE" ]; then
    CJSH_CANDIDATE="$(pwd)/$CJSH_CANDIDATE"
fi

CJSH_DIR=$(cd "$(dirname "$CJSH_CANDIDATE")" && pwd)
CJSH_BASENAME=$(basename "$CJSH_CANDIDATE")
CJSH="$CJSH_DIR/$CJSH_BASENAME"

if [ ! -x "$CJSH" ]; then
    printf "Error: cjsh binary not found at %s\n" "$CJSH" >&2
    printf "Build it first: cmake -S . -B build && cmake --build build\n" >&2
    exit 1
fi

TMPDIR_BASE=${TMPDIR:-/tmp}
WORK_DIR=$(mktemp -d "$TMPDIR_BASE/cjsh-error-preview.XXXXXX")
trap 'chmod -R u+rw "$WORK_DIR" >/dev/null 2>&1; rm -rf "$WORK_DIR"' EXIT

NOEXEC_FILE="$WORK_DIR/noexec.sh"
BADBIN_FILE="$WORK_DIR/badbin"
NOPERM_DIR="$WORK_DIR/noperm"
NOCLOBBER_FILE="$WORK_DIR/noclobber.txt"
NOPERM_FILE="$WORK_DIR/noperm_file"

printf "#!/bin/sh\n" > "$NOEXEC_FILE"
printf "echo should-not-run\n" >> "$NOEXEC_FILE"
chmod 644 "$NOEXEC_FILE"

printf '\x7fELF\x01\x01\x01\x00\x00\x00\x00' > "$BADBIN_FILE"
chmod 755 "$BADBIN_FILE"

mkdir -p "$NOPERM_DIR"
chmod 000 "$NOPERM_DIR"

printf "existing\n" > "$NOCLOBBER_FILE"
printf "secret\n" > "$NOPERM_FILE"
chmod 000 "$NOPERM_FILE"

run_case() {
    name="$1"
    cmd="$2"
    printf "\n=== %s ===\n" "$name"
    printf '$ %s -c %s\n' "$(basename "$CJSH")" "$cmd"
    output=$($CJSH -c "$cmd" 2>&1)
    status=$?
    if [ -n "$output" ]; then
        printf "%s\n" "$output"
    else
        printf "(no output)\n"
    fi
    printf "exit status: %s\n" "$status"
}

run_case "command not found" "does_not_exist"
run_case "syntax error" "if then; fi"
run_case "unterminated quote" "echo 'unterminated"
run_case "bad parameter expansion" "unset FOO; echo \${FOO?missing}"
run_case "bad arithmetic expansion" "echo \$((1/0))"
run_case "bad test bracket" "[ 1 -eq 1"
run_case "bad double-bracket" "[[ 1 -eq ]]"
run_case "file not found (source)" ". $WORK_DIR/missing.sh"
run_case "permission denied (source)" ". $NOPERM_FILE"
run_case "permission denied (no exec bit)" "$NOEXEC_FILE"
run_case "exec format error" "$BADBIN_FILE"
run_case "not a directory" "cd $NOEXEC_FILE"
run_case "cd missing directory" "cd $WORK_DIR/missing_dir"
run_case "cd too many args" "cd / /"
run_case "pushd empty stack" "pushd"
run_case "popd empty stack" "popd"
run_case "dirs too many args" "dirs a b"
run_case "alias missing" "alias missing_alias"
run_case "unalias missing args" "unalias"
run_case "unalias missing alias" "unalias missing_alias"
run_case "abbr missing" "abbr missing_abbr"
run_case "abbr invalid name" "abbr 'bad name=foo'"
run_case "unabbr missing args" "unabbr"
run_case "unabbr missing" "unabbr missing_abbr"
run_case "local outside function" "local foo=1"
run_case "source missing args" "source"
run_case "source directory" "source $WORK_DIR"
run_case "shift too far" "set -- a b; shift 5"
run_case "return outside function" "return 1"
run_case "break outside loop" "break"
run_case "continue outside loop" "continue"
run_case "exit invalid numeric" "exit nope"
run_case "readonly invalid name" "readonly 1abc=3"
run_case "unset invalid name" "unset 1abc"
run_case "export invalid name" "export 1abc=3"
run_case "export special parameter" "export ?=1"
run_case "unset special parameter" "unset ?"
run_case "set invalid option" "set -o nope"
run_case "invalid option (export)" "export -z"
run_case "invalid option (pwd)" "pwd -z"
run_case "invalid option (type)" "type -z"
run_case "invalid option (read)" "read -z var"
run_case "invalid option (umask)" "umask -z"
run_case "invalid mode (umask)" "umask zzzz"
run_case "invalid option (ulimit)" "ulimit -Z"
run_case "invalid option (trap)" "trap -z"
run_case "invalid signal (trap)" "trap 'echo hi' 99999"
run_case "test syntax error" "[ 1 -eq ]"
run_case "command missing args" "command"
run_case "command invalid option" "command -z"
run_case "command verbose not found" "command -V does_not_exist"
run_case "builtin missing args" "builtin"
run_case "builtin recursive" "builtin builtin echo hi"
run_case "builtin not found" "builtin does_not_exist"
run_case "hash invalid option" "hash -z"
run_case "history invalid option" "history -z"
run_case "fc invalid option" "fc -z"
run_case "exec not found" "exec does_not_exist"
run_case "if missing args" "if"
run_case "if bad syntax" "if true; fi"
run_case "jobs invalid option" "jobs -z"
run_case "jobs positional arg" "jobs 1"
run_case "fg no jobs" "fg"
run_case "bg no jobs" "bg"
run_case "wait invalid job" "wait %999"
run_case "wait invalid pid" "wait abc"
run_case "kill invalid option" "kill -z 1"
run_case "kill invalid pid" "kill abc"
run_case "disown invalid option" "disown -z"
run_case "disown invalid job" "disown %999"
run_case "jobname missing args" "jobname"
run_case "jobname invalid job" "jobname %999 newname"
run_case "getopts missing args" "getopts"
run_case "getopts illegal option" "getopts a opt -z"
run_case "generate-completions invalid option" "generate-completions --jobs nope"
run_case "hook missing command" "hook"
run_case "hook unknown command" "hook bogus"
run_case "hook invalid type" "hook list bogus"
run_case "hook add missing args" "hook add"
run_case "hook remove missing args" "hook remove"
run_case "cjsh-widget missing args" "cjsh-widget"
run_case "cjsh-widget unknown subcommand" "cjsh-widget bogus"
run_case "cjsh-widget no session" "cjsh-widget get-buffer"
run_case "cjshopt missing subcommand" "cjshopt"
run_case "cjshopt unknown subcommand" "cjshopt bogus"
run_case "redirect permission denied" "echo hi > $NOPERM_DIR/out"
run_case "redirect to directory" "echo hi > $WORK_DIR"
run_case "redirect missing input" "cat < $WORK_DIR/missing.txt"
run_case "noclobber redirect" "set -o noclobber; echo hi > $NOCLOBBER_FILE"
run_case "bad fd duplication" "echo hi 1>&999999"
