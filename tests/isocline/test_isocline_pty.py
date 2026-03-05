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
                return match.group(1).strip()

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

    ctrl_c = run_case(binary, "ctrl_c", b"\x03")
    if ctrl_c != "<CTRL+C>":
        raise AssertionError(f"ctrl_c expected '<CTRL+C>', got {ctrl_c!r}")

    print("All PTY isocline integration tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
