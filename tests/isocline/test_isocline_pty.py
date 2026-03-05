#!/usr/bin/env python3
import os
import pty
import re
import signal
import sys
import time
import fcntl


RESULT_RE = re.compile(r"\[IC_RESULT\](.*)")


def run_case(
    binary: str, scenario: str, key_bytes: bytes, timeout_s: float = 5.0
) -> str:
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(binary, [binary, scenario])

    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    output = bytearray()
    deadline = time.monotonic() + timeout_s
    sent = False

    try:
        while time.monotonic() < deadline:
            try:
                chunk = os.read(fd, 4096)
                if chunk:
                    output.extend(chunk)
            except BlockingIOError:
                pass
            except OSError:
                break

            if not sent and b"pty> " in output:
                os.write(fd, key_bytes)
                sent = True

            waited_pid, status = os.waitpid(pid, os.WNOHANG)
            if waited_pid == pid:
                if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
                    text = output.decode("utf-8", errors="replace")
                    raise AssertionError(
                        f"case {scenario} failed: exit={status}, output={text!r}"
                    )
                text = output.decode("utf-8", errors="replace")
                match = RESULT_RE.search(text)
                if match is None:
                    raise AssertionError(
                        f"case {scenario} missing result marker: {text!r}"
                    )
                return match.group(1).replace("\r", "")

            time.sleep(0.01)
    finally:
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass
        try:
            os.waitpid(pid, 0)
        except OSError:
            pass

    text = output.decode("utf-8", errors="replace")
    raise AssertionError(f"case {scenario} timed out, output={text!r}")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <isocline_pty_driver>", file=sys.stderr)
        return 2

    binary = os.path.abspath(sys.argv[1])
    if not os.path.exists(binary):
        print(f"driver binary not found: {binary}", file=sys.stderr)
        return 2

    insert = run_case(binary, "insert_backspace", b"ab\x7fcd\r")
    if insert != "acd":
        raise AssertionError(f"insert_backspace expected 'acd', got {insert!r}")

    moved = run_case(binary, "cursor_move_insert", b"\x02Z\r")
    if moved != "aZb":
        raise AssertionError(f"cursor_move_insert expected 'aZb', got {moved!r}")

    home_insert = run_case(binary, "home_insert", b"\x01a\r")
    if home_insert != "abc":
        raise AssertionError(f"home_insert expected 'abc', got {home_insert!r}")

    end_insert = run_case(binary, "end_insert", b"\x05c\r")
    if end_insert != "abc":
        raise AssertionError(f"end_insert expected 'abc', got {end_insert!r}")

    backspace_start = run_case(binary, "backspace_at_start_noop", b"\x01\x7f\r")
    if backspace_start != "ab":
        raise AssertionError(
            f"backspace_at_start_noop expected 'ab', got {backspace_start!r}"
        )

    delete_end = run_case(binary, "delete_at_end_noop", b"\x05\x04\r")
    if delete_end != "ab":
        raise AssertionError(f"delete_at_end_noop expected 'ab', got {delete_end!r}")

    kill_end_noop = run_case(binary, "kill_to_end_at_end_noop", b"\x05\x0b\r")
    if kill_end_noop != "ab":
        raise AssertionError(
            f"kill_to_end_at_end_noop expected 'ab', got {kill_end_noop!r}"
        )

    kill_start_noop = run_case(binary, "kill_to_start_at_start_noop", b"\x01\x15\r")
    if kill_start_noop != "ab":
        raise AssertionError(
            f"kill_to_start_at_start_noop expected 'ab', got {kill_start_noop!r}"
        )

    left_boundary = run_case(binary, "left_boundary_insert", b"\x01\x02X\r")
    if left_boundary != "Xab":
        raise AssertionError(
            f"left_boundary_insert expected 'Xab', got {left_boundary!r}"
        )

    right_boundary = run_case(binary, "right_boundary_insert", b"\x05\x06X\r")
    if right_boundary != "abX":
        raise AssertionError(
            f"right_boundary_insert expected 'abX', got {right_boundary!r}"
        )

    append_initial = run_case(binary, "append_to_initial_input", b"c\r")
    if append_initial != "abc":
        raise AssertionError(
            f"append_to_initial_input expected 'abc', got {append_initial!r}"
        )

    redraw_keeps_buffer = run_case(binary, "ctrl_l_redraw_keeps_buffer", b"\x0c\r")
    if redraw_keeps_buffer != "ab":
        raise AssertionError(
            f"ctrl_l_redraw_keeps_buffer expected 'ab', got {redraw_keeps_buffer!r}"
        )

    kill_to_end = run_case(binary, "ctrl_k_delete_to_end", b"\x02\x02\x0b\r")
    if kill_to_end != "abcd":
        raise AssertionError(
            f"ctrl_k_delete_to_end expected 'abcd', got {kill_to_end!r}"
        )

    kill_to_start = run_case(binary, "ctrl_u_delete_to_start", b"\x02\x02\x15\r")
    if kill_to_start != "ef":
        raise AssertionError(
            f"ctrl_u_delete_to_start expected 'ef', got {kill_to_start!r}"
        )

    delete_word = run_case(binary, "ctrl_w_delete_word", b"alpha beta\x17\r")
    if delete_word != "alpha ":
        raise AssertionError(
            f"ctrl_w_delete_word expected 'alpha ', got {delete_word!r}"
        )

    delete_single_word = run_case(binary, "ctrl_w_single_word", b"\x17\r")
    if delete_single_word != "":
        raise AssertionError(
            f"ctrl_w_single_word expected empty string, got {delete_single_word!r}"
        )

    delete_mid = run_case(binary, "ctrl_d_delete_mid", b"\x02\x04\r")
    if delete_mid != "ab":
        raise AssertionError(f"ctrl_d_delete_mid expected 'ab', got {delete_mid!r}")

    ctrl_c = run_case(binary, "ctrl_c", b"\x03")
    if ctrl_c != "<CTRL+C>":
        raise AssertionError(f"ctrl_c expected '<CTRL+C>', got {ctrl_c!r}")

    ctrl_d = run_case(binary, "ctrl_d_empty", b"\x04")
    if ctrl_d != "<CTRL+D>":
        raise AssertionError(f"ctrl_d_empty expected '<CTRL+D>', got {ctrl_d!r}")

    print("All PTY isocline integration tests passed (19 cases)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
