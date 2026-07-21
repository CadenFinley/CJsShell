#!/usr/bin/env python3

# test_isocline_pty.py
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import os
import platform
import pty
import re
import signal
import struct
import sys
import termios
import time
import fcntl


RESULT_RE = re.compile(r"\[IC_RESULT_BEGIN\](.*?)\[IC_RESULT_END\]", re.S)
ANSI_CSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
ANSI_OSC_RE = re.compile(r"\x1b\].*?(?:\x07|\x1b\\)", re.S)
PROMPT_GUARD_RE = re.compile(r"[%#][ ]+pty> ")
PROMPT_LINE_RE = re.compile(r"(?m)^pty> ")
PTY_CASE_COUNT = 0
IS_DARWIN = platform.system() == "Darwin"
READLINE_STEP_MARKER = b"[IC_READLINE_STEP_DONE]"

LEFT = b"\x1b[D"
RIGHT = b"\x1b[C"
UP = b"\x1b[A"
DOWN = b"\x1b[B"
HOME = b"\x1b[H"
END = b"\x1b[F"
PAGEUP = b"\x1b[5~"
PAGEDOWN = b"\x1b[6~"
CTRL_HOME = b"\x1b[1;5H"
CTRL_END = b"\x1b[1;5F"
SHIFT_HOME = b"\x1b[1;2H"
SHIFT_END = b"\x1b[1;2F"
SHIFT_UP = b"\x1b[1;2A"
SHIFT_TAB = b"\x1b[Z"
F1 = b"\x1bOP"
F2 = b"\x1bOQ"
F3 = b"\x1bOR"
ALT_LT = b"\x1b<"
ALT_GT = b"\x1b>"
ALT_S = b"\x1bs"
ALT_DELETE = b"\x1b[3;3~"
CTRL_ENTER = b"\x1b[13;5u"
WORD_PREV = b"\x1b[1;2D" if IS_DARWIN else b"\x1b[1;5D"
WORD_NEXT = b"\x1b[1;2C" if IS_DARWIN else b"\x1b[1;5C"


def mouse_left_click(column: int, row: int) -> bytes:
    press = f"\x1b[<0;{column};{row}M".encode("ascii")
    release = f"\x1b[<0;{column};{row}m".encode("ascii")
    return press + release


def normalize_terminal_output(text: str) -> str:
    normalized = text.replace("\r", "")
    normalized = ANSI_OSC_RE.sub("", normalized)
    normalized = ANSI_CSI_RE.sub("", normalized)
    return normalized


def count_prompt_lines(output_text: str) -> int:
    return len(PROMPT_LINE_RE.findall(normalize_terminal_output(output_text)))


def read_pending_output(fd: int, output: bytearray) -> bool:
    received = False
    while True:
        try:
            chunk = os.read(fd, 4096)
            if not chunk:
                break
            output.extend(chunk)
            received = True
        except BlockingIOError:
            break
        except OSError:
            break
    return received


def drain_remaining_output(
    fd: int,
    output: bytearray,
    idle_timeout_s: float = 0.1,
    poll_interval_s: float = 0.01,
) -> None:
    # PTYs can still have unread output queued after the child has exited.
    idle_deadline = time.monotonic() + idle_timeout_s
    while time.monotonic() < idle_deadline:
        if read_pending_output(fd, output):
            idle_deadline = time.monotonic() + idle_timeout_s
            continue
        time.sleep(poll_interval_s)


def assert_prompt_guard_marker(
    scenario: str, output_text: str, expect_marker: bool
) -> None:
    normalized = normalize_terminal_output(output_text)
    marker_count = len(PROMPT_GUARD_RE.findall(normalized))
    expected_count = 1 if expect_marker else 0
    if marker_count != expected_count:
        raise AssertionError(
            f"{scenario} expected marker_count={expected_count}, got {marker_count}, "
            f"normalized_output={normalized!r}"
        )


def assert_case(
    binary: str, label: str, scenario: str, key_bytes: bytes, expected: str
) -> None:
    actual = run_case(binary, scenario, key_bytes)
    if actual != expected:
        raise AssertionError(f"{label} expected {expected!r}, got {actual!r}")


def assert_timed_case(
    binary: str,
    label: str,
    scenario: str,
    chunks: list[bytes],
    expected: str,
    initial_delay_s: float = 0.08,
    step_delay_s: float = 0.06,
    poll_interval_s: float = 0.01,
    wait_for_reprompt: bool = False,
) -> None:
    actual = run_case_timed(
        binary,
        scenario,
        chunks,
        initial_delay_s=initial_delay_s,
        step_delay_s=step_delay_s,
        poll_interval_s=poll_interval_s,
        wait_for_reprompt=wait_for_reprompt,
    )
    if actual != expected:
        raise AssertionError(f"{label} expected {expected!r}, got {actual!r}")


def assert_resize_case(
    binary: str,
    label: str,
    scenario: str,
    actions: list[tuple[str, object]],
    expected: str,
    initial_rows: int = 24,
    initial_cols: int = 40,
    timeout_s: float = 8.0,
    poll_interval_s: float = 0.01,
) -> None:
    actual = run_resize_case(
        binary,
        scenario,
        actions,
        initial_rows=initial_rows,
        initial_cols=initial_cols,
        timeout_s=timeout_s,
        poll_interval_s=poll_interval_s,
    )
    if actual != expected:
        raise AssertionError(f"{label} expected {expected!r}, got {actual!r}")


def assert_prompt_guard_case(binary: str, scenario: str, expect_marker: bool) -> None:
    result, output_text = run_case(binary, scenario, b"ok\r", capture_output=True)
    if result != "ok":
        raise AssertionError(f"{scenario} expected 'ok', got {result!r}")
    assert_prompt_guard_marker(scenario, output_text, expect_marker)


def assert_last_prompt_suffix(
    scenario: str, output_text: str, expected_suffix: str
) -> None:
    pre_result = output_text.split("[IC_RESULT_BEGIN]", 1)[0]
    normalized = normalize_terminal_output(pre_result).rstrip("\n")
    prompt = "pty> "
    prompt_index = normalized.rfind(prompt)
    if prompt_index < 0:
        raise AssertionError(
            f"{scenario} missing final prompt in output: normalized_output={normalized!r}"
        )
    actual_suffix = normalized[prompt_index + len(prompt) :]
    if actual_suffix != expected_suffix:
        raise AssertionError(
            f"{scenario} expected final prompt suffix {expected_suffix!r}, got "
            f"{actual_suffix!r}, normalized_output={normalized!r}"
        )


def parse_readline_status_payload(payload: str) -> tuple[str, str, bool, bool]:
    parts = payload.split("|")
    if len(parts) != 4:
        raise AssertionError(f"invalid status payload format: {payload!r}")

    disposition, line, tty_part, lost_part = parts
    if not tty_part.startswith("tty="):
        raise AssertionError(f"invalid tty marker in payload: {payload!r}")
    if not lost_part.startswith("lost="):
        raise AssertionError(f"invalid lost marker in payload: {payload!r}")

    tty_active = tty_part.split("=", 1)[1] == "1"
    tty_lost = lost_part.split("=", 1)[1] == "1"
    return disposition, line, tty_active, tty_lost


def run_case(
    binary: str,
    scenario: str,
    key_bytes: bytes,
    timeout_s: float = 5.0,
    capture_output: bool = False,
) -> str | tuple[str, str]:
    global PTY_CASE_COUNT
    PTY_CASE_COUNT += 1

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
            read_pending_output(fd, output)

            if not sent and b"pty> " in output:
                if key_bytes:
                    os.write(fd, key_bytes)
                sent = True

            waited_pid, status = os.waitpid(pid, os.WNOHANG)
            if waited_pid == pid:
                if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
                    text = output.decode("utf-8", errors="replace")
                    raise AssertionError(
                        f"case {scenario} failed: exit={status}, output={text!r}"
                    )
                drain_remaining_output(fd, output)
                text = output.decode("utf-8", errors="replace")
                match = RESULT_RE.search(text)
                if match is None:
                    raise AssertionError(
                        f"case {scenario} missing result marker: {text!r}"
                    )
                result = match.group(1).replace("\r", "")
                if capture_output:
                    return result, text
                return result

            time.sleep(0.01)
    finally:
        try:
            os.close(fd)
        except OSError:
            pass
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


def run_case_timed(
    binary: str,
    scenario: str,
    chunks: list[bytes],
    timeout_s: float = 8.0,
    initial_delay_s: float = 0.08,
    step_delay_s: float = 0.25,
    poll_interval_s: float = 0.01,
    wait_for_reprompt: bool = False,
) -> str:
    global PTY_CASE_COUNT
    PTY_CASE_COUNT += 1

    pid, fd = pty.fork()
    if pid == 0:
        os.execv(binary, [binary, scenario])

    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    output = bytearray()
    deadline = time.monotonic() + timeout_s
    next_send_at = time.monotonic() + initial_delay_s
    send_index = 0
    prompt_seen = False
    last_output_at = time.monotonic()
    reprompt_output_start: int | None = None
    reprompt_idle_s = max(poll_interval_s * 3, 0.05)

    try:
        while time.monotonic() < deadline:
            now = time.monotonic()

            if read_pending_output(fd, output):
                last_output_at = now
            if not prompt_seen and b"pty> " in output:
                prompt_seen = True

            ready_to_send = prompt_seen and send_index < len(chunks) and now >= next_send_at
            if ready_to_send and reprompt_output_start is not None:
                new_output = output[reprompt_output_start:]
                marker_idx = new_output.find(READLINE_STEP_MARKER)
                prompt_after_marker = (
                    marker_idx >= 0
                    and new_output.find(b"pty> ", marker_idx + len(READLINE_STEP_MARKER)) >= 0
                )
                ready_to_send = prompt_after_marker and (now - last_output_at) >= reprompt_idle_s
            if ready_to_send:
                chunk_to_send = chunks[send_index]
                os.write(fd, chunk_to_send)
                send_index += 1
                if (
                    wait_for_reprompt
                    and send_index < len(chunks)
                    and chunk_to_send.endswith((b"\r", b"\n"))
                ):
                    # History triplet cases emit a marker after each hidden readline.
                    # Wait for the marker and the following fresh prompt.
                    reprompt_output_start = len(output)
                else:
                    reprompt_output_start = None
                next_send_at = now + step_delay_s

            waited_pid, status = os.waitpid(pid, os.WNOHANG)
            if waited_pid == pid:
                if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
                    text = output.decode("utf-8", errors="replace")
                    raise AssertionError(
                        f"case {scenario} failed: exit={status}, output={text!r}"
                    )
                drain_remaining_output(fd, output, poll_interval_s=poll_interval_s)
                text = output.decode("utf-8", errors="replace")
                match = RESULT_RE.search(text)
                if match is None:
                    raise AssertionError(
                        f"case {scenario} missing result marker: {text!r}"
                    )
                return match.group(1).replace("\r", "")

            time.sleep(poll_interval_s)
    finally:
        try:
            os.close(fd)
        except OSError:
            pass
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


def set_pty_window_size(fd: int, rows: int, cols: int) -> None:
    winsize = struct.pack("HHHH", rows, cols, 0, 0)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, winsize)


def run_resize_case(
    binary: str,
    scenario: str,
    actions: list[tuple[str, object]],
    initial_rows: int = 24,
    initial_cols: int = 40,
    timeout_s: float = 8.0,
    poll_interval_s: float = 0.01,
    return_after_actions: bool = False,
) -> str:
    global PTY_CASE_COUNT
    PTY_CASE_COUNT += 1

    pid, fd = pty.fork()
    if pid == 0:
        os.execv(binary, [binary, scenario])

    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)
    set_pty_window_size(fd, initial_rows, initial_cols)

    output = bytearray()
    deadline = time.monotonic() + timeout_s
    prompt_seen = False
    action_index = 0
    current_rows = initial_rows
    last_output_at = time.monotonic()

    def normalized_output() -> str:
        text = output.decode("utf-8", errors="replace")
        return normalize_terminal_output(text)

    try:
        while time.monotonic() < deadline:
            if read_pending_output(fd, output):
                last_output_at = time.monotonic()
                if not prompt_seen and b"pty> " in output:
                    prompt_seen = True

            while prompt_seen and action_index < len(actions):
                action, value = actions[action_index]
                if action == "wait":
                    if value not in normalized_output():
                        break
                elif action == "idle":
                    if time.monotonic() - last_output_at < float(value):
                        break
                elif action == "send":
                    os.write(fd, value)
                elif action == "resize":
                    if isinstance(value, tuple):
                        next_rows, next_cols = value
                    else:
                        next_rows, next_cols = current_rows, value
                    set_pty_window_size(fd, next_rows, next_cols)
                    try:
                        os.kill(pid, signal.SIGWINCH)
                    except OSError:
                        pass
                    current_rows = next_rows
                else:
                    raise AssertionError(
                        f"case {scenario} has unknown resize action {action!r}"
                    )
                action_index += 1

            if return_after_actions and prompt_seen and action_index == len(actions):
                return output.decode("utf-8", errors="replace")

            waited_pid, status = os.waitpid(pid, os.WNOHANG)
            if waited_pid == pid:
                drain_remaining_output(fd, output, poll_interval_s=poll_interval_s)
                text = output.decode("utf-8", errors="replace")
                normalized = normalize_terminal_output(text)
                if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
                    raise AssertionError(
                        f"case {scenario} failed: exit={status}, output={text!r}"
                    )
                if action_index != len(actions):
                    action, value = actions[action_index]
                    raise AssertionError(
                        f"case {scenario} exited before completing {action} {value!r}, "
                        f"normalized_output={normalized!r}"
                    )
                match = RESULT_RE.search(text)
                if match is None:
                    raise AssertionError(
                        f"case {scenario} missing result marker: {text!r}"
                    )
                return match.group(1).replace("\r", "")

            time.sleep(poll_interval_s)
    finally:
        try:
            os.close(fd)
        except OSError:
            pass
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass
        try:
            os.waitpid(pid, 0)
        except OSError:
            pass

    normalized = normalized_output()
    if not prompt_seen:
        missing = "initial prompt"
    elif action_index < len(actions):
        action, value = actions[action_index]
        if action == "wait":
            missing = f"fragment {value!r}"
        elif action == "idle":
            missing = f"idle {value!r}"
        elif action == "send":
            missing = f"send {value!r}"
        else:
            missing = f"resize {value!r}"
    else:
        missing = "case completion"
    raise AssertionError(
        f"case {scenario} timed out while waiting for {missing}, "
        f"normalized_output={normalized!r}"
    )


def observe_resize_case(
    binary: str,
    scenario: str,
    actions: list[tuple[str, object]],
    initial_rows: int = 24,
    initial_cols: int = 40,
    timeout_s: float = 8.0,
    poll_interval_s: float = 0.01,
) -> str:
    return run_resize_case(
        binary,
        scenario,
        actions,
        initial_rows=initial_rows,
        initial_cols=initial_cols,
        timeout_s=timeout_s,
        poll_interval_s=poll_interval_s,
        return_after_actions=True,
    )


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

    alias_navigation_cases = [
        ("left_arrow_insert", "insert_backspace", b"ab" + LEFT + b"Z\r", "aZb"),
        (
            "right_arrow_insert",
            "insert_backspace",
            b"ab" + LEFT + RIGHT + b"Z\r",
            "abZ",
        ),
        ("home_key_insert", "insert_backspace", b"bc" + HOME + b"a\r", "abc"),
        (
            "end_key_insert",
            "insert_backspace",
            b"ab" + HOME + END + b"c\r",
            "abc",
        ),
        ("ctrl_h_backspace", "insert_backspace", b"ab\x08cd\r", "acd"),
        (
            "ctrl_f_cursor_right_midline",
            "insert_backspace",
            b"ab\x01\x06X\r",
            "aXb",
        ),
        (
            "delete_key_midline",
            "insert_backspace",
            b"abc" + LEFT + b"\x1b[3~\r",
            "ab",
        ),
        ("ctrl_h_at_start", "backspace_at_start_noop", b"\x01\x08\r", "ab"),
        ("delete_key_at_end", "delete_at_end_noop", b"\x05\x1b[3~\r", "ab"),
    ]
    for label, scenario, key_bytes, expected in alias_navigation_cases:
        assert_case(binary, label, scenario, key_bytes, expected)

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

    roundtrip_nav = run_case(binary, "ctrl_a_ctrl_e_append", b"\x01\x05c\r")
    if roundtrip_nav != "abc":
        raise AssertionError(
            f"ctrl_a_ctrl_e_append expected 'abc', got {roundtrip_nav!r}"
        )

    delete_end_noop_2 = run_case(binary, "ctrl_d_at_end_noop", b"\x05\x04\r")
    if delete_end_noop_2 != "ab":
        raise AssertionError(
            f"ctrl_d_at_end_noop expected 'ab', got {delete_end_noop_2!r}"
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

    kill_then_type = run_case(binary, "ctrl_k_then_type", b"\x02\x02\x0bXY\r")
    if kill_then_type != "abcdXY":
        raise AssertionError(
            f"ctrl_k_then_type expected 'abcdXY', got {kill_then_type!r}"
        )

    kill_start_then_type = run_case(binary, "ctrl_u_then_type", b"\x02\x02\x15XY\r")
    if kill_start_then_type != "XYef":
        raise AssertionError(
            f"ctrl_u_then_type expected 'XYef', got {kill_start_then_type!r}"
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

    delete_mid_2 = run_case(binary, "delete_mid_twice", b"\x02\x02\x04\x04\r")
    if delete_mid_2 != "ab":
        raise AssertionError(f"delete_mid_twice expected 'ab', got {delete_mid_2!r}")

    backspace_twice = run_case(binary, "backspace_twice_typed", b"abcd\x7f\x7f\r")
    if backspace_twice != "ab":
        raise AssertionError(
            f"backspace_twice_typed expected 'ab', got {backspace_twice!r}"
        )

    ctrl_w_then_type = run_case(binary, "ctrl_w_then_type", b"alpha beta\x17gamma\r")
    if ctrl_w_then_type != "alpha gamma":
        raise AssertionError(
            f"ctrl_w_then_type expected 'alpha gamma', got {ctrl_w_then_type!r}"
        )

    word_and_edit_alias_cases = [
        (
            "alt_backspace_delete_word_start",
            "insert_backspace",
            b"alpha beta\x1b\x7f\r",
            "alpha",
        ),
        (
            "alt_delete_delete_word_start",
            "insert_backspace",
            b"alpha beta" + ALT_DELETE + b"\r",
            "alpha",
        ),
        (
            "alt_d_delete_word_end",
            "insert_backspace",
            b"alpha beta" + LEFT * 4 + b"\x1bd\r",
            "alpha ",
        ),
        (
            "alt_b_prev_word",
            "insert_backspace",
            b"alpha beta\x1bbX\r",
            "alphaX beta",
        ),
        (
            "alt_f_next_word",
            "insert_backspace",
            b"alpha beta\x01\x1bfX\r",
            "alpha Xbeta",
        ),
        (
            "platform_word_prev_alias",
            "insert_backspace",
            b"alpha beta" + WORD_PREV + b"X\r",
            "alphaX beta",
        ),
        (
            "platform_word_next_alias",
            "insert_backspace",
            b"alpha beta\x01" + WORD_NEXT + b"X\r",
            "alpha Xbeta",
        ),
        ("match_brace_alt_m", "insert_backspace", b"(ab)\x1bmX\r", "(Xab)"),
        ("transpose_ctrl_t", "insert_backspace", b"ab" + LEFT + b"\x14\r", "ba"),
    ]
    for label, scenario, key_bytes, expected in word_and_edit_alias_cases:
        assert_case(binary, label, scenario, key_bytes, expected)

    undo_single = run_case(binary, "undo_single_change", b"c\x1a\r")
    if undo_single != "ab":
        raise AssertionError(f"undo_single_change expected 'ab', got {undo_single!r}")

    undo_alias = run_case(binary, "undo_single_change", b"c\x1f\r")
    if undo_alias != "ab":
        raise AssertionError(f"undo_single_change ctrl+_ expected 'ab', got {undo_alias!r}")

    undo_redo = run_case(binary, "undo_redo_roundtrip", b"c\x1a\x19\r")
    if undo_redo != "abc":
        raise AssertionError(f"undo_redo_roundtrip expected 'abc', got {undo_redo!r}")

    undo_kill = run_case(binary, "undo_after_kill_to_end", b"\x02\x02\x0b\x1a\r")
    if undo_kill != "abcdef":
        raise AssertionError(
            f"undo_after_kill_to_end expected 'abcdef', got {undo_kill!r}"
        )

    redo_cleared = run_case(binary, "redo_cleared_by_new_edit", b"c\x1aX\x19\r")
    if redo_cleared != "abX":
        raise AssertionError(
            f"redo_cleared_by_new_edit expected 'abX', got {redo_cleared!r}"
        )

    multiline_ctrl_j = run_case(binary, "multiline_ctrl_j_insert_newline", b"a\x0ab\r")
    if multiline_ctrl_j != "a\nb":
        raise AssertionError(
            f"multiline_ctrl_j_insert_newline expected 'a\\nb', got {multiline_ctrl_j!r}"
        )

    multiline_backslash = run_case(
        binary, "multiline_backslash_continuation", b"echo \\\rhi\r"
    )
    if multiline_backslash != "echo \nhi":
        raise AssertionError(
            f"multiline_backslash_continuation expected 'echo \\nhi', got {multiline_backslash!r}"
        )

    multiline_initial = run_case(binary, "multiline_initial_ctrl_j", b"\x0acd\r")
    if multiline_initial != "ab\ncd":
        raise AssertionError(
            f"multiline_initial_ctrl_j expected 'ab\\ncd', got {multiline_initial!r}"
        )

    multiline_ctrl_a_stays_on_line = run_case(
        binary, "multiline_ctrl_a_stays_on_line", b"\x01\x01X\r"
    )
    if multiline_ctrl_a_stays_on_line != "ab\ncd\nXef":
        raise AssertionError(
            "multiline_ctrl_a_stays_on_line expected 'ab\\ncd\\nXef', got "
            f"{multiline_ctrl_a_stays_on_line!r}"
        )

    multiline_ctrl_e_stays_on_line = run_case(
        binary,
        "multiline_ctrl_e_stays_on_line",
        b"\x01\x02\x01\x02\x05\x05\x05X\r",
    )
    if multiline_ctrl_e_stays_on_line != "abX\ncd\nef":
        raise AssertionError(
            "multiline_ctrl_e_stays_on_line expected 'abX\\ncd\\nef', got "
            f"{multiline_ctrl_e_stays_on_line!r}"
        )

    multiline_navigation_cases = [
        (
            "pageup_input_start",
            b"ab\x0acd\x0aef" + PAGEUP + b"X\r",
            "Xab\ncd\nef",
        ),
        (
            "ctrl_home_input_start",
            b"ab\x0acd\x0aef" + CTRL_HOME + b"X\r",
            "Xab\ncd\nef",
        ),
        (
            "shift_home_input_start",
            b"ab\x0acd\x0aef" + SHIFT_HOME + b"X\r",
            "Xab\ncd\nef",
        ),
        (
            "alt_lt_input_start",
            b"ab\x0acd\x0aef" + ALT_LT + b"X\r",
            "Xab\ncd\nef",
        ),
        (
            "pagedown_input_end",
            b"ab\x0acd\x0aef" + PAGEUP + PAGEDOWN + b"X\r",
            "ab\ncd\nefX",
        ),
        (
            "ctrl_end_input_end",
            b"ab\x0acd\x0aef" + PAGEUP + CTRL_END + b"X\r",
            "ab\ncd\nefX",
        ),
        (
            "shift_end_input_end",
            b"ab\x0acd\x0aef" + PAGEUP + SHIFT_END + b"X\r",
            "ab\ncd\nefX",
        ),
        (
            "alt_gt_input_end",
            b"ab\x0acd\x0aef" + PAGEUP + ALT_GT + b"X\r",
            "ab\ncd\nefX",
        ),
    ]
    for label, key_bytes, expected in multiline_navigation_cases:
        assert_case(
            binary,
            label,
            "multiline_ctrl_j_insert_newline",
            key_bytes,
            expected,
        )

    multiline_row_navigation_cases = [
        ("up_row_navigation", b"ab\x0acd\x0aef" + UP + b"X\r", "ab\ncdX\nef"),
        (
            "shift_up_row_navigation",
            b"ab\x0acd\x0aef" + SHIFT_UP + b"X\r",
            "ab\ncdX\nef",
        ),
        (
            "down_row_navigation",
            b"ab\x0acd\x0aef" + PAGEUP + DOWN + b"X\r",
            "ab\nXcd\nef",
        ),
    ]
    for label, key_bytes, expected in multiline_row_navigation_cases:
        assert_case(
            binary,
            label,
            "multiline_ctrl_j_insert_newline",
            key_bytes,
            expected,
        )

    if IS_DARWIN:
        shift_tab_newline = run_case(
            binary, "multiline_ctrl_j_insert_newline", b"a" + SHIFT_TAB + b"b\r"
        )
        if shift_tab_newline != "a\nb":
            raise AssertionError(
                f"multiline shift+tab expected 'a\\nb', got {shift_tab_newline!r}"
            )
    else:
        ctrl_enter_newline = run_case(
            binary, "multiline_ctrl_j_insert_newline", b"a" + CTRL_ENTER + b"b\r"
        )
        if ctrl_enter_newline != "a\nb":
            raise AssertionError(
                f"multiline ctrl+enter expected 'a\\nb', got {ctrl_enter_newline!r}"
            )

    ctrl_c = run_case(binary, "ctrl_c", b"\x03")
    if ctrl_c != "<CTRL+C>":
        raise AssertionError(f"ctrl_c expected '<CTRL+C>', got {ctrl_c!r}")

    ctrl_d = run_case(binary, "ctrl_d_empty", b"\x04")
    if ctrl_d != "<CTRL+D>":
        raise AssertionError(f"ctrl_d_empty expected '<CTRL+D>', got {ctrl_d!r}")

    status_submit = run_case(binary, "status_text", b"ok\r")
    disposition, line, tty_active, tty_lost = parse_readline_status_payload(status_submit)
    if disposition != "submit" or line != "ok":
        raise AssertionError(
            "status_text expected submit with line 'ok', "
            f"got disposition={disposition!r}, line={line!r}"
        )
    if not tty_active or tty_lost:
        raise AssertionError(
            f"status_text expected tty_active=1/lost=0, got tty_active={tty_active}, "
            f"tty_lost={tty_lost}"
        )

    status_interrupt = run_case(binary, "status_ctrl_c", b"\x03")
    disposition, line, tty_active, tty_lost = parse_readline_status_payload(status_interrupt)
    if disposition != "interrupt" or line != "<CTRL+C>":
        raise AssertionError(
            "status_ctrl_c expected interrupt with <CTRL+C>, "
            f"got disposition={disposition!r}, line={line!r}"
        )
    if not tty_active or tty_lost:
        raise AssertionError(
            f"status_ctrl_c expected tty_active=1/lost=0, got tty_active={tty_active}, "
            f"tty_lost={tty_lost}"
        )

    status_eof = run_case(binary, "status_ctrl_d", b"\x04")
    disposition, line, tty_active, tty_lost = parse_readline_status_payload(status_eof)
    if disposition != "eof" or line != "<CTRL+D>":
        raise AssertionError(
            "status_ctrl_d expected eof with <CTRL+D>, "
            f"got disposition={disposition!r}, line={line!r}"
        )
    if not tty_active or tty_lost:
        raise AssertionError(
            f"status_ctrl_d expected tty_active=1/lost=0, got tty_active={tty_active}, "
            f"tty_lost={tty_lost}"
        )

    status_stop = run_case(binary, "status_stop_event", b"")
    disposition, line, tty_active, tty_lost = parse_readline_status_payload(status_stop)
    if disposition != "stop" or line != "<CTRL+C>":
        raise AssertionError(
            "status_stop_event expected stop with compatibility token <CTRL+C>, "
            f"got disposition={disposition!r}, line={line!r}"
        )
    if not tty_active or tty_lost:
        raise AssertionError(
            "status_stop_event expected tty_active=1/lost=0 for synthetic stop event, "
            f"got tty_active={tty_active}, tty_lost={tty_lost}"
        )

    ctrl_o_submit = run_case(binary, "insert_backspace", b"abc\x0f")
    if ctrl_o_submit != "abc":
        raise AssertionError(f"ctrl_o_submit expected 'abc', got {ctrl_o_submit!r}")

    ctrl_g_cancel = run_case(binary, "insert_backspace", b"abc\x07")
    if ctrl_g_cancel != "":
        raise AssertionError(f"ctrl_g_cancel expected empty string, got {ctrl_g_cancel!r}")

    assert_timed_case(
        binary,
        "esc_clear_then_enter",
        "insert_backspace",
        [b"abc", b"\x1b", b"\r"],
        "",
        initial_delay_s=0.12,
        step_delay_s=0.5,
    )
    assert_timed_case(
        binary,
        "esc_empty_continues",
        "insert_backspace",
        [b"\x1b", b"ok\r"],
        "ok",
        initial_delay_s=0.12,
        step_delay_s=0.5,
    )

    timed_history_cases = [
        ("history_prev_ctrl_p", "history_prev", [b"one\r", b"two\r", b"\x10\r"], "two"),
        ("history_prev_up", "history_prev", [b"one\r", b"two\r", UP + b"\r"], "two"),
        (
            "history_prev_shift_up",
            "history_prev",
            [b"one\r", b"two\r", SHIFT_UP + b"\r"],
            "two",
        ),
        (
            "history_prev_prev_ctrl_p",
            "history_prev_prev",
            [b"one\r", b"two\r", b"\x10\x10\r"],
            "one",
        ),
        (
            "history_next_ctrl_n",
            "history_next_empty",
            [b"one\r", b"two\r", b"\x10\x0e\r"],
            "",
        ),
        (
            "history_next_down",
            "history_next_empty",
            [b"one\r", b"two\r", b"\x10" + DOWN + b"\r"],
            "",
        ),
        (
            "history_prev_prefix_filter",
            "history_prev_edit",
            [b"alpha\r", b"alpine\r", b"al\x10\r"],
            "alpine",
        ),
    ]
    for label, scenario, chunks, expected in timed_history_cases:
        assert_timed_case(
            binary,
            label,
            scenario,
            chunks,
            expected,
            initial_delay_s=0.12,
            step_delay_s=0.5,
            wait_for_reprompt=True,
        )

    hist_latest = run_case(binary, "history_probe_latest", b"")
    if hist_latest != "echo two":
        raise AssertionError(
            f"history_probe_latest expected 'echo two', got {hist_latest!r}"
        )

    hist_prev = run_case(binary, "history_probe_previous", b"")
    if hist_prev != "echo one":
        raise AssertionError(
            f"history_probe_previous expected 'echo one', got {hist_prev!r}"
        )

    hist_removed = run_case(binary, "history_probe_remove_last", b"")
    if hist_removed != "echo one":
        raise AssertionError(
            f"history_probe_remove_last expected 'echo one', got {hist_removed!r}"
        )

    hist_count = run_case(binary, "history_probe_count", b"")
    if hist_count != "2":
        raise AssertionError(f"history_probe_count expected '2', got {hist_count!r}")

    yank_meta_dot = run_case(binary, "yank_last_arg_meta_dot", b"\x1b.\r")
    if yank_meta_dot != 'mv "dest dir"':
        raise AssertionError(
            f"yank_last_arg_meta_dot expected 'mv \"dest dir\"', got {yank_meta_dot!r}"
        )

    yank_meta_underscore = run_case(binary, "yank_last_arg_meta_underscore", b"\x1b_\r")
    if yank_meta_underscore != 'mv "dest dir"':
        raise AssertionError(
            "yank_last_arg_meta_underscore expected 'mv \"dest dir\"', got "
            f"{yank_meta_underscore!r}"
        )

    yank_repeat = run_case(binary, "yank_last_arg_repeat", b"\x1b.\x1b.\r")
    if yank_repeat != "open beta.txt":
        raise AssertionError(
            f"yank_last_arg_repeat expected 'open beta.txt', got {yank_repeat!r}"
        )

    yank_repeat_underscore = run_case(binary, "yank_last_arg_repeat", b"\x1b_\x1b_\r")
    if yank_repeat_underscore != "open beta.txt":
        raise AssertionError(
            "yank_last_arg_repeat alt+_ expected 'open beta.txt', got "
            f"{yank_repeat_underscore!r}"
        )

    yank_repeat_mixed = run_case(binary, "yank_last_arg_repeat", b"\x1b.\x1b_\r")
    if yank_repeat_mixed != "open beta.txt":
        raise AssertionError(
            "yank_last_arg_repeat mixed meta-. / meta-_ expected 'open beta.txt', got "
            f"{yank_repeat_mixed!r}"
        )

    yank_repeat_mixed_reverse = run_case(
        binary, "yank_last_arg_repeat", b"\x1b_\x1b.\r"
    )
    if yank_repeat_mixed_reverse != "open beta.txt":
        raise AssertionError(
            "yank_last_arg_repeat mixed meta-_ / meta-. expected 'open beta.txt', got "
            f"{yank_repeat_mixed_reverse!r}"
        )

    mouse_wheel_down = b"\x1b[<65;1;1M"
    mouse_wheel_down_shift = b"\x1b[<69;1;1M"
    mouse_release = b"\x1b[<3;1;1m"
    mouse_click_history_second = mouse_left_click(6, 4)
    mouse_click_completion_expanded_second = mouse_left_click(6, 4)
    mouse_click_completion_collapsed_second = mouse_left_click(6, 3)
    mouse_click_inline_hint = mouse_left_click(10, 1)

    mouse_status_result, mouse_status_output = run_case(
        binary, "insert_backspace", F2 + b"x\x7f\r", capture_output=True
    )
    if mouse_status_result != "":
        raise AssertionError(
            f"mouse status toggle case expected empty result, got {mouse_status_result!r}"
        )
    normalized_mouse_status_output = normalize_terminal_output(mouse_status_output)
    if "Mouse clicking is enabled" not in normalized_mouse_status_output:
        raise AssertionError(
            "status line should show mouse indicator after toggle, got "
            f"normalized_output={normalized_mouse_status_output!r}"
        )
    if "complete:" not in normalized_mouse_status_output:
        raise AssertionError(
            "mouse indicator should not replace default status hints, got "
            f"normalized_output={normalized_mouse_status_output!r}"
        )
    mouse_top_index = normalized_mouse_status_output.rfind("Mouse clicking is enabled")
    hint_index = normalized_mouse_status_output.rfind("complete:")
    if hint_index >= 0 and mouse_top_index > hint_index:
        raise AssertionError(
            "mouse indicator should render above default status hints, got "
            f"normalized_output={normalized_mouse_status_output!r}"
        )

    mouse_nonempty_result, mouse_nonempty_output = run_case(
        binary, "mouse_status_nonempty_buffer", b"\r", capture_output=True
    )
    if mouse_nonempty_result != "x":
        raise AssertionError(
            "mouse non-empty buffer case expected 'x', got "
            f"{mouse_nonempty_result!r}"
        )
    normalized_mouse_nonempty_output = normalize_terminal_output(mouse_nonempty_output)
    if "Mouse clicking is enabled" not in normalized_mouse_nonempty_output:
        raise AssertionError(
            "non-empty mouse status case should show mouse indicator, got "
            f"normalized_output={normalized_mouse_nonempty_output!r}"
        )
    if "complete:" in normalized_mouse_nonempty_output:
        raise AssertionError(
            "default status hints should stay hidden when input buffer has content, got "
            f"normalized_output={normalized_mouse_nonempty_output!r}"
        )

    mouse_default_click = run_case(
        binary,
        "completion_many_menu_mouse_default_on",
        b"s\t" + mouse_click_completion_collapsed_second + b"\r",
    )
    if mouse_default_click != "s02":
        raise AssertionError(
            "completion_many_menu with default mouse enabled expected 's02', got "
            f"{mouse_default_click!r}"
        )

    mouse_hidden_result, mouse_hidden_output = run_case(
        binary,
        "insert_backspace_mouse_default_on_hidden_status",
        b"x\x7f\r",
        capture_output=True,
    )
    if mouse_hidden_result != "":
        raise AssertionError(
            "mouse hidden-status case expected empty result, got "
            f"{mouse_hidden_result!r}"
        )
    normalized_mouse_hidden_output = normalize_terminal_output(mouse_hidden_output)
    if "Mouse clicking is enabled" in normalized_mouse_hidden_output:
        raise AssertionError(
            "hidden mouse status toggle should suppress indicator text, got "
            f"normalized_output={normalized_mouse_hidden_output!r}"
        )
    if "complete:" not in normalized_mouse_hidden_output:
        raise AssertionError(
            "hidden mouse status toggle should keep default status hints visible, got "
            f"normalized_output={normalized_mouse_hidden_output!r}"
        )

    hist_scroll, hist_scroll_output = run_case(
        binary,
        "history_search_scroll",
        b"\x12" + mouse_wheel_down + b"\r",
        capture_output=True,
    )
    if hist_scroll != "history alpha":
        raise AssertionError(
            f"history_search_scroll expected 'history alpha', got {hist_scroll!r}"
        )
    normalized_hist_scroll_output = normalize_terminal_output(hist_scroll_output)
    if "Mouse clicking is enabled" not in normalized_hist_scroll_output:
        raise AssertionError(
            "history search menu should show mouse indicator when click support is active, got "
            f"normalized_output={normalized_hist_scroll_output!r}"
        )

    hist_scroll_toggle = run_case(
        binary, "history_search_scroll", F2 + b"\x12" + mouse_wheel_down + b"\r"
    )
    if hist_scroll_toggle != "history beta":
        raise AssertionError(
            "history_search_scroll with mouse toggle expected 'history beta', got "
            f"{hist_scroll_toggle!r}"
        )

    hist_click = run_case(
        binary,
        "history_search_scroll",
        b"\x12" + mouse_click_history_second + b"!\r",
    )
    if hist_click != "history alpha!":
        raise AssertionError(
            f"history_search_click expected 'history alpha!', got {hist_click!r}"
        )

    hist_search_ctrl_s = run_case(binary, "history_search_scroll", b"\x13\r")
    if hist_search_ctrl_s != "history beta":
        raise AssertionError(
            f"history_search_scroll ctrl+s expected 'history beta', got {hist_search_ctrl_s!r}"
        )

    hist_sort_alt_s = run_case(
        binary,
        "history_search_sort_alt_s",
        b"\x12" + ALT_S + b"\r",
    )
    if hist_sort_alt_s != "apple":
        raise AssertionError(
            f"history_search_sort_alt_s expected 'apple', got {hist_sort_alt_s!r}"
        )

    hist_sort_metadata = run_case(
        binary,
        "history_search_sort_default_metadata",
        b"\x12rank\r",
    )
    if hist_sort_metadata != "rank one":
        raise AssertionError(
            "history_search_sort_default_metadata expected 'rank one', got "
            f"{hist_sort_metadata!r}"
        )

    hist_sort_cycle_nonpersistent = run_case_timed(
        binary,
        "history_search_sort_cycle_nonpersistent",
        [b"\x12a" + ALT_S + b"\r", b"\x12a\r"],
        wait_for_reprompt=True,
    )
    if hist_sort_cycle_nonpersistent != "carrot|apple":
        raise AssertionError(
            "history_search_sort_cycle_nonpersistent expected 'carrot|apple', got "
            f"{hist_sort_cycle_nonpersistent!r}"
        )

    hist_multiline_preview, hist_multiline_output = run_case(
        binary,
        "history_search_multiline",
        b"\x12\r",
        capture_output=True,
    )
    if hist_multiline_preview != "mlhist first line\nmlhist second line":
        raise AssertionError(
            "history_search_multiline expected full multiline command, got "
            f"{hist_multiline_preview!r}"
        )
    normalized_hist_multiline_output = normalize_terminal_output(hist_multiline_output)
    if "mlhist second line" not in normalized_hist_multiline_output:
        raise AssertionError(
            "history search menu should show full selected multiline command text, got "
            f"normalized_output={normalized_hist_multiline_output!r}"
        )
    if "mlhist first line..." in normalized_hist_multiline_output:
        raise AssertionError(
            "history search menu should render selected multiline command inline instead of "
            "truncating it, got "
            f"normalized_output={normalized_hist_multiline_output!r}"
        )

    comp_single = run_case(binary, "completion_single_tab", b"hel\t\r")
    if comp_single != "hello":
        raise AssertionError(
            f"completion_single_tab expected 'hello', got {comp_single!r}"
        )

    comp_single_hint_click = run_case(
        binary,
        "hint_clears_on_empty_line",
        F2 + b"hel" + mouse_click_inline_hint + b"\r",
    )
    if comp_single_hint_click != "hello":
        raise AssertionError(
            "mouse click on inline hint expected 'hello', got "
            f"{comp_single_hint_click!r}"
        )

    comp_single_type = run_case(binary, "completion_single_then_type", b"hel\t!\r")
    if comp_single_type != "hello!":
        raise AssertionError(
            f"completion_single_then_type expected 'hello!', got {comp_single_type!r}"
        )

    comp_midline = run_case(binary, "completion_midline_single", b"\t\r")
    if comp_midline != "say hello":
        raise AssertionError(
            f"completion_midline_single expected 'say hello', got {comp_midline!r}"
        )

    completion_alias_cases = [
        ("completion_ctrl_f_at_eol", b"hel\x06\r", "hello"),
        ("completion_right_arrow_at_eol", b"hel" + RIGHT + b"\r", "hello"),
        ("completion_word_next_at_eol", b"hel" + WORD_NEXT + b"\r", "hello"),
        ("completion_alt_f_at_eol", b"hel\x1bf\r", "hello"),
    ]
    for label, key_bytes, expected in completion_alias_cases:
        assert_case(binary, label, "completion_single_tab", key_bytes, expected)

    assert_timed_case(
        binary,
        "completion_alt_question",
        "completion_single_tab",
        [b"hel", b"\x1b?", b"\r"],
        "hello",
        initial_delay_s=0.12,
        step_delay_s=0.2,
    )

    empty_hint_result, empty_hint_output = run_case(
        binary, "hint_clears_on_empty_line", b" \x7f\r", capture_output=True
    )
    if empty_hint_result != "":
        raise AssertionError(
            "hint_clears_on_empty_line expected empty string, got "
            f"{empty_hint_result!r}"
        )
    assert_last_prompt_suffix("hint_clears_on_empty_line", empty_hint_output, "")

    comp_nomatch = run_case(binary, "completion_no_match", b"xyz\t\r")
    if comp_nomatch != "xyz":
        raise AssertionError(
            f"completion_no_match expected 'xyz', got {comp_nomatch!r}"
        )

    spell_submit_disabled = run_case(binary, "enter_spell_single_disabled", b"hlelo\r")
    if spell_submit_disabled != "hlelo":
        raise AssertionError(
            "enter_spell_single_disabled expected 'hlelo', got "
            f"{spell_submit_disabled!r}"
        )

    spell_submit_enabled = run_case(binary, "enter_spell_single_enabled", b"hlelo\r")
    if spell_submit_enabled != "hello":
        raise AssertionError(
            "enter_spell_single_enabled expected 'hello', got "
            f"{spell_submit_enabled!r}"
        )

    comp_common = run_case(binary, "completion_dual_common_prefix", b"pla\t\r\r")
    if comp_common != "planet":
        raise AssertionError(
            f"completion_dual_common_prefix expected 'planet', got {comp_common!r}"
        )

    comp_scroll_collapsed = run_case(
        binary,
        "completion_many_menu",
        b"s\t" + mouse_wheel_down + b"\r\r",
    )
    if comp_scroll_collapsed != "s01":
        raise AssertionError(
            "completion_many_menu collapsed expected 's01', got "
            f"{comp_scroll_collapsed!r}"
        )

    comp_click_collapsed_default = run_case(
        binary,
        "completion_many_menu",
        b"s\t" + mouse_click_completion_collapsed_second + b"\r\r",
    )
    if comp_click_collapsed_default != "s01":
        raise AssertionError(
            "completion_many_menu collapsed click without toggle expected 's01', got "
            f"{comp_click_collapsed_default!r}"
        )

    comp_click_collapsed_toggle = run_case(
        binary,
        "completion_many_menu",
        F2 + b"s\t" + mouse_click_completion_collapsed_second + b"\r",
    )
    if comp_click_collapsed_toggle != "s02":
        raise AssertionError(
            "completion_many_menu collapsed click with toggle expected 's02', got "
            f"{comp_click_collapsed_toggle!r}"
        )

    comp_toggle_collapsed_inside_menu = run_case(
        binary,
        "completion_many_menu",
        b"s\t" + F2 + b"\r\r",
    )
    if comp_toggle_collapsed_inside_menu != "s01":
        raise AssertionError(
            "completion_many_menu toggle inside collapsed menu expected 's01', got "
            f"{comp_toggle_collapsed_inside_menu!r}"
        )

    comp_click_collapsed_toggle_inside_menu = run_case(
        binary,
        "completion_many_menu",
        b"s\t" + F2 + mouse_click_completion_collapsed_second + b"\r",
    )
    if comp_click_collapsed_toggle_inside_menu != "s02":
        raise AssertionError(
            "completion_many_menu collapsed click after in-menu toggle expected 's02', got "
            f"{comp_click_collapsed_toggle_inside_menu!r}"
        )

    comp_click_collapsed_custom_toggle_binding = run_case(
        binary,
        "completion_many_menu_custom_mouse_toggle",
        b"s\t" + F3 + mouse_click_completion_collapsed_second + b"\r",
    )
    if comp_click_collapsed_custom_toggle_binding != "s02":
        raise AssertionError(
            "completion_many_menu custom mouse-toggle key expected 's02', got "
            f"{comp_click_collapsed_custom_toggle_binding!r}"
        )

    comp_click_expanded, comp_click_expanded_output = run_case(
        binary,
        "completion_many_menu_mouse_default_on",
        b"s\t\x0a" + mouse_click_completion_expanded_second + b"\r",
        capture_output=True,
    )
    if comp_click_expanded != "s02":
        raise AssertionError(
            f"completion_many_menu expanded click expected 's02', got {comp_click_expanded!r}"
        )
    normalized_comp_click_expanded_output = normalize_terminal_output(
        comp_click_expanded_output
    )
    if "Mouse clicking is enabled" not in normalized_comp_click_expanded_output:
        raise AssertionError(
            "completion menu should show mouse indicator when click support is active, got "
            f"normalized_output={normalized_comp_click_expanded_output!r}"
        )

    comp_scroll = run_case(
        binary,
        "completion_many_menu_mouse_default_on",
        b"s\t\x0a" + mouse_wheel_down + b"\r\r",
    )
    if comp_scroll != "s02":
        raise AssertionError(
            f"completion_many_menu expected 's02', got {comp_scroll!r}"
        )

    comp_scroll_release = run_case(
        binary,
        "completion_many_menu_mouse_default_on",
        b"s\t\x0a" + mouse_wheel_down + mouse_release + b"\r\r",
    )
    if comp_scroll_release != "s02":
        raise AssertionError(
            "completion_many_menu with release expected 's02', got "
            f"{comp_scroll_release!r}"
        )

    comp_scroll_shift = run_case(
        binary,
        "completion_many_menu_mouse_default_on",
        b"s\t\x0a" + mouse_wheel_down_shift + b"\r\r",
    )
    if comp_scroll_shift != "s02":
        raise AssertionError(
            f"completion_many_menu shift-wheel expected 's02', got {comp_scroll_shift!r}"
        )

    comp_multiline_preview, comp_multiline_preview_output = run_case(
        binary,
        "completion_many_menu_multiline",
        b"m\t\x0a" + DOWN + b"\r\r",
        capture_output=True,
    )
    if comp_multiline_preview != "m02":
        raise AssertionError(
            "completion_many_menu_multiline expected 'm02', got "
            f"{comp_multiline_preview!r}"
        )
    normalized_comp_multiline_preview_output = normalize_terminal_output(
        comp_multiline_preview_output
    )
    if (
        "→ m02 first line..." not in normalized_comp_multiline_preview_output
        and "> m02 first line..." not in normalized_comp_multiline_preview_output
    ):
        raise AssertionError(
            "expanded completion menu should keep the selected multiline candidate collapsed, got "
            f"normalized_output={normalized_comp_multiline_preview_output!r}"
        )
    if "m02 second line" in normalized_comp_multiline_preview_output:
        raise AssertionError(
            "expanded completion menu should not expand selected multiline candidate text, got "
            f"normalized_output={normalized_comp_multiline_preview_output!r}"
        )

    help_result, help_output = run_case(
        binary, "insert_backspace", F2 + b"ab" + F1 + b"c\r", capture_output=True
    )
    if help_result != "abc":
        raise AssertionError(f"show_help expected 'abc', got {help_result!r}")
    normalized_help_output = normalize_terminal_output(help_output)
    if "Navigation:" not in normalized_help_output or "Editing:" not in normalized_help_output:
        raise AssertionError(
            "show_help expected rendered help headings, got "
            f"normalized_output={normalized_help_output!r}"
        )
    if (
        "toggle mouse reporting for this prompt (Mouse clicking is enabled; press "
        not in normalized_help_output
        or " to disable)" not in normalized_help_output
    ):
        raise AssertionError(
            "show_help should mark mouse toggle binding as enabled after F2, got "
            f"normalized_output={normalized_help_output!r}"
        )

    vim_cases = [
        ("vim_alt_h_left", "vim_insert_backspace", b"ab\x1bhX\r", "aXb"),
        ("vim_alt_l_midline", "vim_insert_backspace", b"ab\x01\x1blX\r", "aXb"),
        ("vim_alt_l_complete", "vim_completion_single_tab", b"hel\x1bl\r", "hello"),
        (
            "vim_alt_w_word_next",
            "vim_insert_backspace",
            b"alpha beta\x01\x1bwX\r",
            "alpha Xbeta",
        ),
        ("vim_alt_w_complete", "vim_completion_single_tab", b"hel\x1bw\r", "hello"),
    ]
    for label, scenario, key_bytes, expected in vim_cases:
        assert_case(binary, label, scenario, key_bytes, expected)

    vim_timed_cases = [
        ("vim_alt_k_history_prev", "vim_history_prev", [b"one\r", b"two\r", b"\x1bk\r"], "two"),
        (
            "vim_alt_j_history_next",
            "vim_history_next_empty",
            [b"one\r", b"two\r", b"\x1bk\x1bj\r"],
            "",
        ),
    ]
    for label, scenario, chunks, expected in vim_timed_cases:
        assert_timed_case(
            binary,
            label,
            scenario,
            chunks,
            expected,
            initial_delay_s=0.12,
            step_delay_s=0.5,
            wait_for_reprompt=True,
        )

    vim_multiline_cases = [
        (
            "vim_alt_k_row_up",
            "vim_multiline_ctrl_j_insert_newline",
            b"ab\x0acd\x0aef\x1bkX\r",
            "ab\ncdX\nef",
        ),
        (
            "vim_alt_j_row_down",
            "vim_multiline_ctrl_j_insert_newline",
            b"ab\x0acd\x0aef" + PAGEUP + b"\x1bjX\r",
            "ab\nXcd\nef",
        ),
    ]
    for label, scenario, key_bytes, expected in vim_multiline_cases:
        assert_case(binary, label, scenario, key_bytes, expected)

    bp_start = b"\x1b[200~"
    bp_end = b"\x1b[201~"

    assert_case(
        binary,
        "bracketed_paste_plain",
        "insert_backspace",
        bp_start + b"hello world" + bp_end + b"\r",
        "hello world",
    )
    assert_case(
        binary,
        "bracketed_paste_empty",
        "insert_backspace",
        bp_start + bp_end + b"\r",
        "",
    )
    assert_case(
        binary,
        "bracketed_paste_wrapped",
        "insert_backspace",
        b"pre-" + bp_start + b"MID" + bp_end + b"-post\r",
        "pre-MID-post",
    )
    assert_case(
        binary,
        "bracketed_paste_initial_append",
        "append_to_initial_input",
        bp_start + b"cd" + bp_end + b"\r",
        "abcd",
    )
    assert_case(
        binary,
        "bracketed_paste_midline",
        "cursor_move_insert",
        b"\x02" + bp_start + b"Z" + bp_end + b"\r",
        "aZb",
    )
    assert_case(
        binary,
        "bracketed_paste_two_blocks",
        "insert_backspace",
        bp_start + b"alpha" + bp_end + bp_start + b"beta" + bp_end + b"\r",
        "alphabeta",
    )
    assert_case(
        binary,
        "bracketed_paste_end_without_start",
        "insert_backspace",
        bp_end + b"tail\r",
        "tail",
    )
    assert_case(
        binary,
        "bracketed_paste_singleline_cr",
        "insert_backspace",
        bp_start + b"one\rtwo" + bp_end + b"\r",
        "onetwo",
    )
    assert_case(
        binary,
        "bracketed_paste_multiline_cr",
        "multiline_ctrl_j_insert_newline",
        bp_start + b"one\rtwo" + bp_end + b"\r",
        "one\ntwo",
    )
    assert_case(
        binary,
        "bracketed_paste_multiline_multi_cr",
        "multiline_ctrl_j_insert_newline",
        bp_start + b"a\rb\rc" + bp_end + b"\r",
        "a\nb\nc",
    )

    assert_timed_case(
        binary,
        "bracketed_paste_chunked",
        "insert_backspace",
        [bp_start, b"chunked", bp_end, b"\r"],
        "chunked",
    )
    assert_timed_case(
        binary,
        "bracketed_paste_multiline_chunked",
        "multiline_ctrl_j_insert_newline",
        [bp_start, b"row1\r", b"row2", bp_end, b"\r"],
        "row1\nrow2",
    )
    assert_timed_case(
        binary,
        "bracketed_paste_chunked_two_blocks",
        "insert_backspace",
        [bp_start, b"alpha", bp_end, bp_start, b"beta", bp_end, b"\r"],
        "alphabeta",
    )

    assert_case(
        binary,
        "bracketed_paste_repeated_start_single_end",
        "insert_backspace",
        bp_start + bp_start + b"nested" + bp_end + b"\r",
        "nested",
    )
    assert_case(
        binary,
        "bracketed_paste_repeated_start_double_end",
        "insert_backspace",
        bp_start + bp_start + b"nested" + bp_end + bp_end + b"\r",
        "nested",
    )
    assert_case(
        binary,
        "bracketed_paste_end_then_start",
        "insert_backspace",
        bp_end + bp_start + b"abc" + bp_end + b"\r",
        "abc",
    )
    assert_case(
        binary,
        "bracketed_paste_double_end_no_start",
        "insert_backspace",
        bp_end + bp_end + b"tail\r",
        "tail",
    )
    assert_case(
        binary,
        "bracketed_paste_empty_then_payload",
        "insert_backspace",
        bp_start + bp_end + bp_start + b"payload" + bp_end + b"\r",
        "payload",
    )
    assert_case(
        binary,
        "bracketed_paste_malformed_start_final",
        "insert_backspace",
        b"\x1b[200Xabc\r",
        "abc",
    )
    assert_case(
        binary,
        "bracketed_paste_malformed_end_final",
        "insert_backspace",
        b"\x1b[201Xtail\r",
        "tail",
    )
    assert_case(
        binary,
        "bracketed_paste_unknown_vt_code_outside_paste",
        "insert_backspace",
        b"\x1b[202~z\r",
        "z",
    )

    assert_timed_case(
        binary,
        "bracketed_paste_split_start_marker",
        "insert_backspace",
        [b"\x1b[200", b"~split", bp_end, b"\r"],
        "split",
        step_delay_s=0.003,
        poll_interval_s=0.001,
    )
    assert_timed_case(
        binary,
        "bracketed_paste_split_end_marker",
        "insert_backspace",
        [bp_start, b"splitend", b"\x1b[201", b"~", b"\r"],
        "splitend",
        step_delay_s=0.003,
        poll_interval_s=0.001,
    )
    assert_timed_case(
        binary,
        "bracketed_paste_split_both_markers",
        "insert_backspace",
        [b"\x1b[200", b"~ab", b"\x1b[201", b"~", b"\r"],
        "ab",
        step_delay_s=0.003,
        poll_interval_s=0.001,
    )

    # TODO: Re-enable PTY resize/reflow coverage once terminal resize
    # propagation is stable across local and CI environments.
    # reflow_single_line = "pty> abcdefghij"
    # reflow_cursor_boundary = "pty> abc\n   > "
    # shell_prompt_boundary = (
    #     "pty> CJsShell git:(master) x abc\n                           > "
    # )
    # reflow_wrapped = "pty> ab↵\n   > cd↵\n   > ef↵\n   > gh↵\n   > ij"
    # reflow_wrapped_tail = "↵\n   > ij"
    #
    # assert_resize_case(
    #     binary,
    #     "typed_wrap_cursor_boundary",
    #     "insert_backspace",
    #     [
    #         ("resize", 8),
    #         ("send", b"abc"),
    #         ("wait", reflow_cursor_boundary),
    #         ("send", b"\r"),
    #     ],
    #     "abc",
    # )
    # assert_resize_case(
    #     binary,
    #     "shell_prompt_wrap_boundary",
    #     "shell_prompt_wrap_boundary",
    #     [
    #         ("resize", 32),
    #         ("send", b"abc"),
    #         ("wait", shell_prompt_boundary),
    #         ("send", b"\r"),
    #     ],
    #     "abc",
    # )
    #
    # These regression checks verify resize-driven reflow, including idle redraws
    # while the editor is blocked waiting for input.
    # assert_resize_case(
    #     binary,
    #     "resize_reflow_while_waiting_for_input",
    #     "resize_reflow_initial_input",
    #     [
    #         ("wait", reflow_single_line),
    #         ("idle", 0.05),
    #         ("resize", 8),
    #         ("wait", reflow_wrapped),
    #         ("send", b"\r"),
    #     ],
    #     "abcdefghij",
    #     poll_interval_s=0.001,
    # )
    #
    # resize_observation = observe_resize_case(
    #     binary,
    #     "resize_reflow_initial_input",
    #     [
    #         ("wait", reflow_single_line),
    #         ("resize", 8),
    #         ("wait", reflow_wrapped),
    #     ],
    #     poll_interval_s=0.001,
    # )
    # if re.search(r"\x1b\[[1-9][0-9]*A", resize_observation):
    #     raise AssertionError(
    #         "resize_reflow_initial_input unexpectedly moved the cursor above the "
    #         f"existing prompt origin: output={resize_observation!r}"
    #     )
    #
    # height_resize_observation = observe_resize_case(
    #     binary,
    #     "resize_reflow_initial_input",
    #     [
    #         ("wait", reflow_wrapped),
    #         ("resize", (3, 8)),
    #         ("idle", 0.05),
    #     ],
    #     initial_rows=6,
    #     initial_cols=8,
    #     poll_interval_s=0.001,
    # )
    # height_resize_ups = re.findall(r"\x1b\[(\d+)A", height_resize_observation)
    # if not height_resize_ups or int(height_resize_ups[-1]) != 4:
    #     raise AssertionError(
    #         "height-only resize should walk back using the previous visible cursor row "
    #         f"before redrawing, output={height_resize_observation!r}"
    #     )
    #
    # assert_resize_case(
    #     binary,
    #     "resize_reflow_initial_input",
    #     "resize_reflow_initial_input",
    #     [
    #         ("wait", reflow_single_line),
    #         ("resize", 8),
    #         ("wait", reflow_wrapped),
    #         ("send", b"\r"),
    #     ],
    #     "abcdefghij",
    # )
    # assert_resize_case(
    #     binary,
    #     "resize_reflow_typed_input_expand",
    #     "resize_reflow_typed_input",
    #     [
    #         ("resize", 8),
    #         ("send", b"abcdefghij"),
    #         ("wait", reflow_wrapped_tail),
    #         ("resize", 40),
    #         ("wait", reflow_single_line),
    #         ("send", b"\r"),
    #     ],
    #     "abcdefghij",
    # )

    prompt_guard_expectations = [
        ("prompt_guard_visible_text", True),
        ("prompt_guard_tab_only", True),
        ("prompt_guard_escape_only", False),
        ("prompt_guard_osc_only", False),
        ("prompt_guard_newline_reset", False),
        ("prompt_guard_escape_then_visible", True),
        ("prompt_guard_spaces_only", True),
        ("prompt_guard_controls_only", False),
        ("prompt_guard_carriage_return_only", False),
        ("prompt_guard_visible_then_carriage_return", False),
        ("prompt_guard_visible_then_carriage_return_clear", False),
        ("prompt_guard_forced_visible_line_start", False),
        ("prompt_guard_visible_then_newline", False),
        ("prompt_guard_newline_then_visible", True),
        ("prompt_guard_double_newline_reset", False),
        ("prompt_guard_escape_then_space", True),
        ("prompt_guard_escape_then_newline_then_visible", True),
        ("prompt_guard_visible_then_newline_then_escape", False),
        ("prompt_guard_bracketed_toggle_only", False),
        ("prompt_guard_bracketed_toggle_then_tab", True),
        ("prompt_guard_utf8_visible", True),
        ("prompt_guard_osc_then_space", True),
    ]
    for scenario, expect_marker in prompt_guard_expectations:
        assert_prompt_guard_case(binary, scenario, expect_marker)

    print(f"All {PTY_CASE_COUNT} PTY isocline integration tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
