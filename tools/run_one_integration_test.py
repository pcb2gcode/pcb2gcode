#!/usr/bin/env python3
"""Run a single integration test case. Used by ctest; no dependency on integration_tests.py."""

from __future__ import print_function
import argparse
import difflib
import filecmp
import os
import re
import shutil
import subprocess
import sys
import tempfile


def fix_up_expected(path):
    """Enlarge SVG dimensions to at least 1000 for consistent comparison (matches integration_tests.py)."""
    def bigger(matchobj):
        width = float(matchobj.group('width'))
        height = float(matchobj.group('height'))
        while width < 1000 or height < 1000:
            width *= 10
            height *= 10
        return 'width="{:.12g}" height="{:.12g}" '.format(width, height)

    for root, _, files in os.walk(path):
        for current_file in files:
            filepath = os.path.join(root, current_file)
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    lines = f.readlines()
            except (OSError, UnicodeDecodeError):
                continue
            new_lines = []
            for line in lines:
                if line.startswith("<svg"):
                    new_lines.append(
                        "<!-- original:\n" + line + "-->\n" +
                        re.sub(
                            r'width="(?P<width>[^"]*)" height="(?P<height>[^"]*)" ',
                            bigger,
                            line
                        )
                    )
                else:
                    new_lines.append(line)
            with open(filepath, 'w', encoding='utf-8') as f:
                f.writelines(new_lines)


def compare_directories(left, right, left_prefix="", right_prefix=""):
    """Return diff string if directories differ, else empty string."""
    if not os.path.exists(right):
        all_diffs = []
        for f in os.listdir(left):
            all_diffs.append(
                "Found %s but not %s.\n" % (
                    os.path.join(left_prefix, f), os.path.join(right_prefix, f)
                )
            )
            left_file = os.path.join(left, f)
            with open(left_file, 'r', encoding='utf-8', errors='replace') as myfile:
                data = myfile.readlines()
                all_diffs += difflib.unified_diff(
                    data, [], '"' + os.path.join(left_prefix, f) + '"', "/dev/null"
                )
        return ''.join(all_diffs)

    if not os.path.exists(left):
        all_diffs = []
        for f in os.listdir(right):
            all_diffs.append(
                "Found %s but not %s.\n" % (
                    os.path.join(right_prefix, f), os.path.join(left_prefix, f)
                )
            )
            right_file = os.path.join(right, f)
            with open(right_file, 'r', encoding='utf-8', errors='replace') as myfile:
                data = myfile.readlines()
                all_diffs += difflib.unified_diff(
                    [], data, "/dev/null",
                    '"' + os.path.join(right_prefix, f) + '"'
                )
        return ''.join(all_diffs)

    diff = filecmp.dircmp(left, right)
    all_diffs = []
    for f in diff.left_only:
        all_diffs.append(
            "Found %s but not %s.\n" % (
                os.path.join(left_prefix, f), os.path.join(right_prefix, f)
            )
        )
        left_file = os.path.join(left, f)
        with open(left_file, 'r', encoding='utf-8', errors='replace') as myfile:
            data = myfile.readlines()
            all_diffs += difflib.unified_diff(
                data, [], '"' + os.path.join(left_prefix, f) + '"', "/dev/null"
            )
    for f in diff.right_only:
        all_diffs.append(
            "Found %s but not %s.\n" % (
                os.path.join(right_prefix, f), os.path.join(left_prefix, f)
            )
        )
        right_file = os.path.join(right, f)
        with open(right_file, 'r', encoding='utf-8', errors='replace') as myfile:
            data = myfile.readlines()
            all_diffs += difflib.unified_diff(
                [], data, "/dev/null",
                '"' + os.path.join(right_prefix, f) + '"'
            )
    for f in diff.diff_files:
        left_file = os.path.join(left, f)
        right_file = os.path.join(right, f)
        with open(left_file, 'r', encoding='utf-8', errors='replace') as myfile0, \
             open(right_file, 'r', encoding='utf-8', errors='replace') as myfile1:
            data0 = myfile0.readlines()
            data1 = myfile1.readlines()
            all_diffs += difflib.unified_diff(
                data0, data1,
                '"' + os.path.join(left_prefix, f) + '"',
                '"' + os.path.join(right_prefix, f) + '"'
            )
    return ''.join(all_diffs)


def main():
    parser = argparse.ArgumentParser(description='Run one pcb2gcode integration test.')
    parser.add_argument('--pcb2gcode', required=True, help='Path to pcb2gcode binary')
    parser.add_argument('--input-dir', required=True, help='Test input directory (cwd for pcb2gcode)')
    parser.add_argument('--expected-dir', default=None,
                        help='Expected output directory (default: input-dir/expected)')
    parser.add_argument('--exit-code', type=int, required=True,
                        help='Expected process exit code')
    parser.add_argument('--arg', action='append', default=[], dest='args',
                        help='Extra argument for pcb2gcode (repeat for multiple)')
    args = parser.parse_args()

    input_dir = os.path.abspath(args.input_dir)
    expected_dir = args.expected_dir
    if expected_dir is None:
        expected_dir = os.path.join(input_dir, 'expected')
    else:
        expected_dir = os.path.abspath(expected_dir)

    if not os.path.isdir(input_dir):
        print('Input directory does not exist: %s' % input_dir, file=sys.stderr)
        return 1

    extra_args = args.args or []
    has_output_dir = any('output-dir' in x for x in extra_args)

    actual_output_path = None
    if not has_output_dir:
        actual_output_path = tempfile.mkdtemp(prefix='pcb2gcode-integration-')

    try:
        cmd = [os.path.abspath(args.pcb2gcode)]
        if not has_output_dir:
            cmd += ['--output-dir', actual_output_path]
        cmd += extra_args

        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=input_dir
        )
        out, _ = proc.communicate()
        if out:
            sys.stderr.buffer.write(out)
            sys.stderr.write('\n')

        if proc.returncode != args.exit_code:
            print(
                'Exit code mismatch: got %d, expected %d' % (proc.returncode, args.exit_code),
                file=sys.stderr
            )
            return 1

        if args.exit_code != 0:
            return 0

        if not os.path.isdir(expected_dir):
            return 0

        fix_up_expected(actual_output_path)
        diff_text = compare_directories(
            expected_dir, actual_output_path,
            'expected', 'actual'
        )
        if diff_text:
            print('Output differs from expected:\n' + diff_text, file=sys.stderr)
            return 1
        return 0
    finally:
        if actual_output_path and os.path.exists(actual_output_path):
            shutil.rmtree(actual_output_path)


if __name__ == '__main__':
    sys.exit(main())
