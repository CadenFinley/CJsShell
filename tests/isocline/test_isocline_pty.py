#!/usr/bin/env python3
import os
import pty
import re
import signal
import sys
import time
import fcntl


RESULT_RE = re.compile(r"\[IC_RESULT_BEGIN\](.*?)\[IC_RESULT_END\]", re.S)


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


def run_case_timed(
    binary: str,
    scenario: str,
    chunks: list[bytes],
    timeout_s: float = 8.0,
    initial_delay_s: float = 0.08,
    step_delay_s: float = 0.25,
) -> str:
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(binary, [binary, scenario])

    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    output = bytearray()
    deadline = time.monotonic() + timeout_s
    next_send_at = time.monotonic() + initial_delay_s
    send_index = 0

    try:
        while time.monotonic() < deadline:
            now = time.monotonic()
            if send_index < len(chunks) and now >= next_send_at:
                os.write(fd, chunks[send_index])
                send_index += 1
                next_send_at = now + step_delay_s

            try:
                chunk = os.read(fd, 4096)
                if chunk:
                    output.extend(chunk)
            except BlockingIOError:
                pass
            except OSError:
                break

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

    print("All PTY isocline integration tests passed (43 cases)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
