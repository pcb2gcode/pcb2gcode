#!/usr/bin/env python3
"""Run pcb2gcode on one gerbv_example case and compare output to expected/.

Usage:
  tools/run_gerbv_example_test.py <input_dir> <pcb2gcode_binary>
    [--regenerate-expected] [--expected-exit-code N] [--pcb2gcode-arg ARG ...]

By default, pcb2gcode writes to a temporary directory that is removed after the test,
and the script compares that output to the existing expected/ tree. If expected/ is
missing, exit code 0 is accepted only when pcb2gcode produces no output files (same
idea as tools/integration_tests.py for --version / --help).

With --regenerate-expected, the script removes expected/ under the input directory
(if present), runs pcb2gcode with --output-dir set to that path, runs fix-up on the
new tree, and skips comparison (use this to refresh golden files).

Use --expected-exit-code when pcb2gcode is supposed to fail; the default is 0.
If the expected exit code is not zero, the script does not compare output to expected/.

Extra pcb2gcode arguments (--pcb2gcode-arg) are passed through as given; if they do
not already include --output-dir (or --output-dir=…), this script appends
--output-dir with its managed path. When an argument starts with - (e.g. --foo=bar),
use one token: --pcb2gcode-arg=--foo=bar (argparse otherwise treats the value as a
new option).

Working directory should be the repository root (same as integration_tests.py).
"""

from __future__ import print_function

import argparse
import filecmp
import os
import re
import shutil
import subprocess
import sys
import tempfile

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)
from fix_up_expected import fix_up_expected  # noqa: E402


def _sanitize_for_path(s):
    return re.sub(r"[^A-Za-z0-9_\-]", "_", s)


def _pcb2gcode_args_include_output_dir(extra_args):
    for arg in extra_args:
        if arg == "--output-dir" or arg.startswith("--output-dir="):
            return True
    return False


def _pcb2gcode_output_dir_value(extra_args):
    for i, arg in enumerate(extra_args):
        if arg == "--output-dir":
            if i + 1 < len(extra_args):
                return extra_args[i + 1]
            return None
        if arg.startswith("--output-dir="):
            return arg.split("=", 1)[1]
    return None


def _directory_tree_has_files(path):
    if not os.path.isdir(path):
        return False
    for _root, _dirs, files in os.walk(path):
        if files:
            return True
    return False


def compare_directories(left, right):
    """Return True if both trees exist, have the same relative files, and contents match."""
    if not os.path.isdir(left) or not os.path.isdir(right):
        return False
    d = filecmp.dircmp(left, right)
    if d.left_only or d.right_only or d.funny_files:
        return False
    for name in d.common_files:
        p1 = os.path.join(left, name)
        p2 = os.path.join(right, name)
        if not filecmp.cmp(p1, p2, shallow=False):
            return False
    for name in d.common_dirs:
        if not compare_directories(os.path.join(left, name), os.path.join(right, name)):
            return False
    return True


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Run pcb2gcode on one gerbv_example case: compare to expected/, "
            "or overwrite expected/ with fresh output (--regenerate-expected)."
        )
    )
    parser.add_argument("input_dir", help="Example directory (contains millproject, expected/, …)")
    parser.add_argument("pcb2gcode_binary", help="Path to pcb2gcode executable")
    parser.add_argument(
        "--regenerate-expected",
        action="store_true",
        help="Remove input_dir/expected/, regenerate it with pcb2gcode, and skip comparison",
    )
    parser.add_argument(
        "--expected-exit-code",
        type=int,
        default=0,
        metavar="N",
        help="Exit code pcb2gcode should return (default: 0). If not 0, skip comparison to expected/.",
    )
    parser.add_argument(
        "--pcb2gcode-arg",
        action="append",
        default=[],
        metavar="ARG",
        help=(
            "Extra argument for pcb2gcode (repeat per arg). "
            "Use --pcb2gcode-arg=--switch if the value starts with '-'."
        ),
    )
    args = parser.parse_args()

    input_path = os.path.abspath(args.input_dir)
    binary = os.path.abspath(args.pcb2gcode_binary)
    expected_path = os.path.join(input_path, "expected")
    # Like tools/integration_tests.py: expected/ may be missing when only the exit
    # code matters, or when pcb2gcode writes no files (--version / --help).
    missing_expected = not os.path.isdir(expected_path)

    sanitized = _sanitize_for_path(input_path)
    out = tempfile.mkdtemp(prefix="pcb2gcode-gerbv-", suffix="-" + sanitized)

    extra = list(args.pcb2gcode_arg)
    try:
        if _pcb2gcode_args_include_output_dir(extra):
            od_val = _pcb2gcode_output_dir_value(extra)
            if not od_val:
                print(
                    "pcb2gcode-arg includes --output-dir but no path was found",
                    file=sys.stderr,
                )
                return 2
            effective_out = os.path.abspath(
                od_val if os.path.isabs(od_val) else os.path.join(input_path, od_val)
            )
            cmd = [binary] + extra
        else:
            effective_out = out
            cmd = [binary] + extra + ["--output-dir", out]

        print("Running {}".format(" ".join("'{}'".format(x) for x in cmd)), file=sys.stderr)
        proc = subprocess.run(
            cmd,
            cwd=input_path,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if proc.stdout:
            print(proc.stdout, file=sys.stderr)
        if proc.returncode != args.expected_exit_code:
            print(
                "pcb2gcode exited with code %s (expected %s)"
                % (proc.returncode, args.expected_exit_code),
                file=sys.stderr,
            )
            return 1

        if args.expected_exit_code != 0:
            return 0

        fix_up_expected(effective_out)

        if missing_expected:
            if _directory_tree_has_files(effective_out):
                print(
                    "Missing expected/ under "
                    + input_path
                    + " but pcb2gcode produced output in "
                    + effective_out,
                    file=sys.stderr,
                )
                return 1
            return 0
        if not compare_directories(expected_path, effective_out):
            if args.regenerate_expected:
                # Delete expected_path and copy effective_out to it.
                shutil.rmtree(expected_path, ignore_errors=True)
                shutil.copytree(effective_out, expected_path)
                return 0
            return 1
        return 0
    finally:
        shutil.rmtree(out, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
