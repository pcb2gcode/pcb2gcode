#!/usr/bin/env python3
"""Run pcb2gcode on an example directory, optionally under valgrind."""

import argparse
import os
import sys
import time
import shutil
import subprocess


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run pcb2gcode on an example directory, optionally under valgrind."
    )
    parser.add_argument(
        "--code-coverage",
        action="store_true",
        help="Code coverage is enabled (skip valgrind as it makes pcb2gcode too slow)",
    )
    parser.add_argument(
        "--pcb2gcode-binary",
        required=True,
        help="Path to pcb2gcode binary to run.",
    )
    parser.add_argument(
        "example_dir",
        help="Example directory to run (e.g. a subdir of tests/data/gerbv_example)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    example_dir = args.example_dir
    code_coverage_enabled = args.code_coverage
    pcb2gcode_binary = args.pcb2gcode_binary
    pcb2gcode_binary = os.path.abspath(pcb2gcode_binary)

    have_valgrind = shutil.which("valgrind") is not None
    return_code = 0
    try:
        if os.path.isfile(os.path.join(example_dir, "no-valgrind")):
            with open(os.path.join(example_dir, "no-valgrind")) as f:
                valgrind_disabled_reason = f.read()
        else:
            valgrind_disabled_reason = None
        run_with_valgrind = (not valgrind_disabled_reason and
                             have_valgrind and
                             not code_coverage_enabled)
        print(f'::group::Running {"with valgrind" if run_with_valgrind else "without valgrind"} on {example_dir}')
        if valgrind_disabled_reason:
            print(f"valgrind is disabled: {valgrind_disabled_reason}")
        elif not have_valgrind:
            print("valgrind is not installed; running pcb2gcode without valgrind")
        elif code_coverage_enabled:
            print("code coverage is enabled and that makes pcb2gcode too slow; running pcb2gcode without valgrind")

        if not os.path.isfile(pcb2gcode_binary):
            sys.exit(f"pcb2gcode not found: {pcb2gcode_binary}")

        if run_with_valgrind:
            cmd = [
                "valgrind",
                "--error-exitcode=127",
                "--errors-for-leak-kinds=definite",
                "--leak-check=full",
                "-s",
                "--exit-on-first-error=yes",
                "--expensive-definedness-checks=yes",
                "--",
                pcb2gcode_binary,
            ]
        else:
            cmd = [pcb2gcode_binary]

        start = time.perf_counter()
        result = subprocess.run(cmd, cwd=example_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        print(result.stdout, end="")
        return_code = result.returncode
        elapsed = time.perf_counter() - start
        print(f"real {elapsed:.3f} seconds")
        if return_code != 0:
            print(f"::error file={example_dir}::pcb2gcode failed with return code {return_code}")
    finally:
        print("::endgroup::")
    sys.exit(return_code)


if __name__ == "__main__":
    main()
