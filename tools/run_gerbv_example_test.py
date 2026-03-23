#!/usr/bin/env python3
"""Run pcb2gcode on one gerbv_example case and compare output to expected/.

Usage:
  tools/run_gerbv_example_test.py <input_dir> <pcb2gcode_binary> [--output-dir DIR]

If --output-dir is omitted, output goes to a temporary directory that is removed
after the test. If --output-dir is set, that directory is used and left in place.

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
        description="Run pcb2gcode on one gerbv_example case and compare to expected/."
    )
    parser.add_argument("input_dir", help="Example directory (contains millproject, expected/, …)")
    parser.add_argument("pcb2gcode_binary", help="Path to pcb2gcode executable")
    parser.add_argument(
        "--output-dir",
        metavar="DIR",
        default=None,
        help="Write output here; directory is not deleted after the test (default: temporary dir)",
    )
    args = parser.parse_args()

    input_path = os.path.abspath(args.input_dir)
    binary = os.path.abspath(args.pcb2gcode_binary)
    expected_path = os.path.join(input_path, "expected")
    if not os.path.isdir(expected_path):
        print("Missing expected/ under " + input_path, file=sys.stderr)
        return 2

    if args.output_dir is not None:
        out = os.path.abspath(args.output_dir)
        remove_out_after = False
    else:
        sanitized = _sanitize_for_path(input_path)
        out = tempfile.mkdtemp(prefix="pcb2gcode-gerbv-", suffix="-" + sanitized)
        remove_out_after = True

    try:
        cmd = [binary, "--output-dir", out]
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
        if proc.returncode != 0:
            print("pcb2gcode exited with code %s" % proc.returncode, file=sys.stderr)
            return 1

        fix_up_expected(out)

        if not compare_directories(expected_path, out):
            return 1
        return 0
    finally:
        if remove_out_after:
            shutil.rmtree(out, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
