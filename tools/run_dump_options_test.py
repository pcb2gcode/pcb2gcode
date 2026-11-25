#!/usr/bin/env python3
"""End-to-end test for `pcb2gcode --dump-options`.

Runs pcb2gcode the way a GUI would and checks that it prints JSON in the
documented format, that the JSON lists the same options and groups as --help,
and that a millproject file in the working directory doesn't change it.

Usage:
  tools/run_dump_options_test.py <pcb2gcode_binary>
"""

from __future__ import print_function

import json
import os
import re
import subprocess
import sys
import tempfile


def _run(binary, args, cwd):
    result = subprocess.run([binary] + args, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, universal_newlines=True, timeout=60)
    assert result.returncode == 0, "{} {} exited with {}\nstderr:\n{}".format(
        binary, " ".join(args), result.returncode, result.stderr)
    return result.stdout


def _help_options(help_text):
    """The long option names in --help, listed like "  --zsafe arg" or "  -? [ --help ]"."""
    return set(re.findall(r"^  (?:-\S \[ )?--([\w-]+)", help_text, re.MULTILINE))


def main(argv):
    if len(argv) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    binary = os.path.abspath(argv[0])
    with tempfile.TemporaryDirectory() as cwd:
        dump_text = _run(binary, ["--dump-options", "--noconfigfile"], cwd)
        dump = json.loads(dump_text)
        assert dump["format_version"] == 1, dump["format_version"]
        names = [option["name"] for option in dump["options"]]
        assert len(names) == len(set(names)), "options listed twice"

        help_text = _run(binary, ["--help"], cwd)
        help_names = _help_options(help_text)
        assert set(names) == help_names, "only in the dump: {}, only in --help: {}".format(
            sorted(set(names) - help_names), sorted(help_names - set(names)))
        help_lines = help_text.splitlines()
        for group in dump["groups"]:
            assert group["title"] + ":" in help_lines, "no group {} in --help".format(
                group["title"])

        with open(os.path.join(cwd, "millproject"), "w") as millproject:
            millproject.write("metric=true\nzsafe=3\nmill-feed-direction=climb\n")
        assert _run(binary, ["--dump-options"], cwd) == dump_text, \
            "a millproject changed the dump"
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
