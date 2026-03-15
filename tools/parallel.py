#!/usr/bin/env python3
import os
import shlex
import sys
import argparse
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Any


def build_cmd(command_template: list[str], argument: str) -> list[str] | None:
    """Build the command list for the given argument. Returns None if argument is empty."""
    argument = argument.strip()
    if not argument:
        return None
    if any("{}" in part for part in command_template):
        return [part.replace("{}", argument) for part in command_template]
    return command_template + [argument]


def run_command(command_template: list[str], argument: str, index: Any) -> tuple[dict[str, str] | None, Any]:
    """Executes a single command with the given argument."""
    cmd = build_cmd(command_template, argument)
    if cmd is None:
        return (None, index)
    try:
        # Capture output so stdout/stderr from different threads don't interleave
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        return {
            "stdout": result.stdout,
            "stderr": result.stderr,
            "returncode": result.returncode
        }, index
    except Exception as e:
        return {
            "stdout": "",
            "stderr": f"Error executing '{' '.join(cmd)}': {e}\n",
            "returncode": -1
        }, index

def main() -> None:
    parser = argparse.ArgumentParser(
        description="A lightweight Python clone of GNU Parallel."
    )
    parser.add_argument(
        "-j", "--jobs",
        type=int,
        default=0,
        help="Number of concurrent jobs. Default is based on CPU count."
    )
    parser.add_argument(
        "-k", "--keep-order",
        action="store_true",
        help="Keep same order. If set, print outputs in input order; if not, print in completion order."
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the command that would be run for each input instead of running it."
    )
    parser.add_argument(
        "--halt",
        choices=["now", "soon", "never"],
        default="never",
        help="On failure: 'now' = stop all and exit; 'soon' = stop starting new jobs, let running finish; 'never' = run all (default)."
    )
    parser.add_argument(
        "command",
        nargs=argparse.REMAINDER,
        help="The command to run. Use {} as a placeholder."
    )

    args = parser.parse_args()

    # argparse includes the "--" delimiter in REMAINDER; strip it when passing to the command
    if args.command and args.command[0] == "--":
        args.command = args.command[1:]

    if not args.command:
        parser.error("You must provide a command to execute.")

    # If 0 or not provided, use same default as ThreadPoolExecutor (needed for --halt soon)
    if args.jobs > 0:
        max_workers = args.jobs
    else:
        max_workers = min(32, (os.cpu_count() or 1) + 4)

    if sys.stdin.isatty():
        print("Warning: Waiting for input from standard input (Ctrl+D to end)...", file=sys.stderr)

    # Read all inputs from stdin
    inputs = sys.stdin.read().splitlines()

    if args.dry_run:
        for line in inputs:
            cmd = build_cmd(args.command, line)
            if cmd is not None:
                print(shlex.join(cmd))
        return

    def print_result(res: dict[str, str] | None) -> None:
        if res:
            if res["stdout"]:
                sys.stdout.write(res["stdout"])
                sys.stdout.flush()
            if res["stderr"]:
                sys.stderr.write(res["stderr"])
                sys.stderr.flush()

    # Filter to inputs that produce a command (skip blank lines)
    work_items = [line for line in inputs if build_cmd(args.command, line) is not None]
    if not work_items:
        return

    # We use ThreadPoolExecutor because subprocesses inherently bypass the Global Interpreter Lock (GIL)
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        stop_submitting = False # Whether to stop submitting new jobs.
        pending = set() # A set of futures to wait on.
        exit_code = 0
        next_to_print = 0 # The index of the next result to print.
        to_print = {} # A dictionary of indices to results to print.
        work_iter = iter(enumerate(work_items))
        while True:
            # Submit more work while we have capacity and no failure yet
            while not stop_submitting and len(pending) < max_workers:
                try:
                    index, arg = next(work_iter)
                except StopIteration:
                    break
                pending.add(executor.submit(run_command, args.command, arg, index))

            if not pending:
                break
            # Wait for any future to complete
            future = next(iter(as_completed(pending)))
            res, index = future.result()
            pending.discard(future)
            if res and res.get("returncode", 0) != 0:
                exit_code = res["returncode"]
                if args.halt == "now":
                    # Cancel all pending futures, print the failure output, and exit.
                    for f in pending:
                        f.cancel()
                    for available_to_print in sorted(to_print.keys()):
                        print_result(to_print[available_to_print])
                        del to_print[available_to_print]
                    break
                if args.halt == "soon":
                    # Stop submitting new jobs.
                    stop_submitting = True
            if args.keep_order:
                # Put them into the to_print dictionary and only print them in order.
                to_print[index] = res
                # Print as many as possible such that they are in the correct order.
                while next_to_print in to_print:
                    res = to_print[next_to_print]
                    print_result(res)
                    del to_print[next_to_print]
                    next_to_print += 1
            else:
                # Print the result immediately.
                print_result(res)
        sys.exit(exit_code)

if __name__ == "__main__":
    main()
