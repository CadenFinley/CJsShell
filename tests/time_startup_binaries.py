#!/usr/bin/env python3
"""Time cjsh binaries using the --startup-test flag."""

from __future__ import annotations

import argparse
import os
import re
import statistics
import subprocess
import sys
import shutil
from pathlib import Path
from typing import Iterable, List, Sequence, Set

DEFAULT_RUNS = 25
STARTUP_ARGS: Sequence[str] = ["--startup-test", "--show-startup-time", "--no-titleline", "--login"]


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def find_cjsh_binaries(build_dir: Path) -> List[Path]:
    binaries: List[Path] = []
    seen: Set[Path] = set()

    path_binary = shutil.which("cjsh")
    if path_binary:
        path_candidate = Path(path_binary)
        if os.access(path_candidate, os.X_OK):
            resolved = path_candidate.resolve()
            binaries.append(path_candidate)
            seen.add(resolved)

    if build_dir.exists():
        for entry in sorted(build_dir.iterdir()):
            if not entry.is_file():
                continue
            if not entry.name.startswith("cjsh"):
                continue
            if not os.access(entry, os.X_OK):
                continue
            resolved = entry.resolve()
            if resolved in seen:
                continue
            binaries.append(entry)
            seen.add(resolved)

    return binaries


def run_startup_test(binary: Path) -> float:
    master_fd, slave_fd = os.openpty()
    env = os.environ.copy()
    env.setdefault("TERM", "xterm-256color")

    process = subprocess.Popen(
        [str(binary), *STARTUP_ARGS],
        stdin=slave_fd,
        stdout=slave_fd,
        stderr=slave_fd,
        env=env,
        close_fds=True,
    )

    os.close(slave_fd)

    output = bytearray()
    pending = b""

    try:
        while True:
            chunk = os.read(master_fd, 1024)
            if not chunk:
                break

            output.extend(chunk)
            combined = pending + chunk
            request_count = combined.count(b"\x1b[6n")
            for _ in range(request_count):
                os.write(master_fd, b"\x1b[1;1R")

            pending = combined[-3:]
    finally:
        process.wait()
        os.close(master_fd)

    if process.returncode != 0:
        text_out = output.decode("utf-8", errors="replace")
        raise subprocess.CalledProcessError(process.returncode, process.args, text_out)

    text = output.decode("utf-8", errors="replace")

    for line in text.splitlines():
        parsed = parse_startup_line(line)
        if parsed is not None:
            return parsed

    raise ValueError(f"Could not parse startup time from output: {text!r}")


def parse_startup_line(line: str) -> float | None:
    cleaned = strip_control_sequences(line)
    lowered = cleaned.lower()
    start_idx = lowered.find("started in ")
    if start_idx == -1:
        return None

    value = cleaned[start_idx + len("started in "):].strip()
    return parse_duration_to_ms(value)


def parse_duration_to_ms(text: str) -> float:
    if text.endswith("\u03bcs"):
        return float(text[:-2]) / 1000.0
    if text.endswith("ms"):
        return float(text[:-2])
    if text.endswith("s"):
        return float(text[:-1]) * 1000.0
    raise ValueError(f"Unrecognized duration format: {text}")


CONTROL_SEQ_OSC = re.compile(r"\x1b\][^\x1b\x07]*(?:\x07|\x1b\\)")
CONTROL_SEQ_CSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


def strip_control_sequences(text: str) -> str:
    without_osc = CONTROL_SEQ_OSC.sub("", text)
    without_csi = CONTROL_SEQ_CSI.sub("", without_osc)
    return without_csi.replace("\x1b", "").strip()


def time_binary(binary: Path, runs: int, warmup: int) -> List[float]:
    durations: List[float] = []

    for _ in range(max(0, warmup)):
        run_startup_test(binary)

    for _ in range(runs):
        durations.append(run_startup_test(binary))

    return durations


def format_row(values: Iterable[str]) -> str:
    columns = list(values)
    widths = [24, 12, 12, 12, 12]
    padded = [col.ljust(widths[i]) if i == 0 else col.rjust(widths[i]) for i, col in enumerate(columns)]
    return " ".join(padded)


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Time cjsh binaries using the --startup-test flag.")
    parser.add_argument("--runs", type=int, default=DEFAULT_RUNS, help="Number of measured runs per binary (default: %(default)s)")
    parser.add_argument("--warmup", type=int, default=5, help="Number of warmup runs per binary (default: %(default)s)")
    args = parser.parse_args(argv)

    if args.runs <= 0:
        print("--runs must be greater than 0", file=sys.stderr)
        return 2
    if args.warmup < 0:
        print("--warmup must be non-negative", file=sys.stderr)
        return 2

    root = repo_root()
    os.chdir(root)
    binaries = find_cjsh_binaries(root / "build")

    if not binaries:
        print("No executable cjsh binaries found under build/. Run the build first.", file=sys.stderr)
        return 1

    header = format_row(["Binary", "Runs", "Average", "Std Dev", "Range"])
    print(header)
    print("-" * len(header))

    for binary in binaries:
        try:
            durations = time_binary(binary, args.runs, args.warmup)
        except subprocess.CalledProcessError as exc:
            print(f"Failed to run {binary.name}: {exc}", file=sys.stderr)
            continue
        except ValueError as exc:
            print(f"Failed to parse output from {binary.name}: {exc}", file=sys.stderr)
            continue

        average = statistics.mean(durations)
        stddev = statistics.pstdev(durations) if len(durations) > 1 else 0.0
        run_range = max(durations) - min(durations) if durations else 0.0

        print(
            format_row(
                [
                    binary.name,
                    str(args.runs),
                    f"{average:0.3f} ms",
                    f"{stddev:0.3f} ms",
                    f"{run_range:0.3f} ms",
                ]
            )
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
