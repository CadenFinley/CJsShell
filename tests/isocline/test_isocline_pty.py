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
import pty
import re
import signal
import sys
import time
import fcntl


RESULT_RE = re.compile(r"\[IC_RESULT_BEGIN\](.*?)\[IC_RESULT_END\]", re.S)
ANSI_CSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
ANSI_OSC_RE = re.compile(r"\x1b\].*?(?:\x07|\x1b\\)", re.S)
PROMPT_GUARD_RE = re.compile(r"[%#][ ]+pty> ")
PTY_CASE_COUNT = 0


def normalize_terminal_output(text: str) -> str:
    normalized = text.replace("\r", "")
    normalized = ANSI_OSC_RE.sub("", normalized)
    normalized = ANSI_CSI_RE.sub("", normalized)
    return normalized


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
) -> None:
    actual = run_case_timed(
        binary,
        scenario,
        chunks,
        initial_delay_s=initial_delay_s,
        step_delay_s=step_delay_s,
        poll_interval_s=poll_interval_s,
    )
    if actual != expected:
        raise AssertionError(f"{label} expected {expected!r}, got {actual!r}")


def assert_prompt_guard_case(binary: str, scenario: str, expect_marker: bool) -> None:
    result, output_text = run_case(binary, scenario, b"ok\r", capture_output=True)
    if result != "ok":
        raise AssertionError(f"{scenario} expected 'ok', got {result!r}")
    assert_prompt_guard_marker(scenario, output_text, expect_marker)


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
            try:
                chunk = os.read(fd, 4096)
                if chunk:
                    output.extend(chunk)
            except BlockingIOError:
                pass
            except OSError:
                pass

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

    try:
        while time.monotonic() < deadline:
            now = time.monotonic()
            if prompt_seen and send_index < len(chunks) and now >= next_send_at:
                os.write(fd, chunks[send_index])
                send_index += 1
                next_send_at = now + step_delay_s

            try:
                chunk = os.read(fd, 4096)
                if chunk:
                    output.extend(chunk)
                    if not prompt_seen and b"pty> " in output:
                        prompt_seen = True
            except BlockingIOError:
                pass
            except OSError:
                pass

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

    undo_single = run_case(binary, "undo_single_change", b"c\x1a\r")
    if undo_single != "ab":
        raise AssertionError(f"undo_single_change expected 'ab', got {undo_single!r}")

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

    ctrl_c = run_case(binary, "ctrl_c", b"\x03")
    if ctrl_c != "<CTRL+C>":
        raise AssertionError(f"ctrl_c expected '<CTRL+C>', got {ctrl_c!r}")

    ctrl_d = run_case(binary, "ctrl_d_empty", b"\x04")
    if ctrl_d != "<CTRL+D>":
        raise AssertionError(f"ctrl_d_empty expected '<CTRL+D>', got {ctrl_d!r}")

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

    mouse_wheel_down = b"\x1b[<65;1;1M"
    mouse_wheel_down_shift = b"\x1b[<69;1;1M"
    mouse_release = b"\x1b[<3;1;1m"

    hist_scroll = run_case(
        binary, "history_search_scroll", b"\x12" + mouse_wheel_down + b"\r"
    )
    if hist_scroll != "history beta":
        raise AssertionError(
            f"history_search_scroll expected 'history beta', got {hist_scroll!r}"
        )

    comp_single = run_case(binary, "completion_single_tab", b"hel\t\r")
    if comp_single != "hello":
        raise AssertionError(
            f"completion_single_tab expected 'hello', got {comp_single!r}"
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

    comp_nomatch = run_case(binary, "completion_no_match", b"xyz\t\r")
    if comp_nomatch != "xyz":
        raise AssertionError(
            f"completion_no_match expected 'xyz', got {comp_nomatch!r}"
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

    comp_scroll = run_case(
        binary,
        "completion_many_menu",
        b"s\t\x0a" + mouse_wheel_down + b"\r\r",
    )
    if comp_scroll != "s02":
        raise AssertionError(
            f"completion_many_menu expected 's02', got {comp_scroll!r}"
        )

    comp_scroll_release = run_case(
        binary,
        "completion_many_menu",
        b"s\t\x0a" + mouse_wheel_down + mouse_release + b"\r\r",
    )
    if comp_scroll_release != "s02":
        raise AssertionError(
            "completion_many_menu with release expected 's02', got "
            f"{comp_scroll_release!r}"
        )

    comp_scroll_shift = run_case(
        binary,
        "completion_many_menu",
        b"s\t\x0a" + mouse_wheel_down_shift + b"\r\r",
    )
    if comp_scroll_shift != "s02":
        raise AssertionError(
            f"completion_many_menu shift-wheel expected 's02', got {comp_scroll_shift!r}"
        )

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
