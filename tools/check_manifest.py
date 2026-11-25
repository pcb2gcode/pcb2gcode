#!/usr/bin/env python3
"""Check that a pcb2gcode --manifest matches the files next to it.

The manifest must list every file in its directory except itself, and nothing
else, and each entry must agree with the name that pcb2gcode gave the file.
"""

from __future__ import print_function
import argparse
import json
import os
import re
import sys

_LAYERS = {"front", "back", "outline", "drill", "milldrill"}
_TOOLPATH_STAGES = {"processed", "traced", "contentions"}


def _expected_name(entry):
    """A regex for the file name of an SVG entry."""
    stage = entry["stage"]
    layer = re.escape(entry["layer"])
    if stage in ("original", "masked"):
        if entry["layer"] in ("drill", "milldrill"):
            return stage + "_" + layer + r"\.svg"
        return r"outp[0-9]+_" + stage + "_" + layer + r"\.svg"
    final = "_final" if entry["final"] else ""
    # Only several mill-diameters get a tool suffix.  Gerber lines get their width.
    tool = "(_{}|_lines_[0-9.]+)?".format(entry["tool_index"])
    return stage + "_" + layer + final + tool + r"\.svg"


def _entry_problems(entry):
    if entry.get("type") not in ("gcode", "svg"):
        return ["unknown type"]
    if entry.get("layer") not in _LAYERS:
        return ["unknown layer"]
    name = os.path.basename(entry["path"])
    if entry["type"] == "gcode":
        tools = entry.get("tools")
        if not isinstance(tools, list) or not all(
                isinstance(t.get("number"), int) and
                isinstance(t.get("diameter_mm"), (int, float)) and t["diameter_mm"] >= 0
                for t in tools):
            return ["tools must be a list of {number, diameter_mm}"]
        if name.endswith(".svg"):
            return ["G-code with an .svg name"]
        return []
    stage = entry.get("stage")
    if stage in _TOOLPATH_STAGES:
        if not isinstance(entry.get("final"), bool) or not isinstance(entry.get("tool_index"), int):
            return ["missing final or tool_index"]
    elif stage != "original" and stage != "masked":
        return ["unknown stage"]
    if not re.fullmatch(_expected_name(entry), name):
        return ["the name doesn't match the layer, stage, final and tool_index"]
    return []


def check_manifest(directory, manifest_name="manifest.json"):
    """Return a list of problems with directory/manifest_name, if it exists."""
    manifest_path = os.path.join(directory, manifest_name)
    if not os.path.isfile(manifest_path):
        return []
    with open(manifest_path, encoding="utf-8") as f:
        manifest = json.load(f)
    if manifest.get("format_version") != 1:
        return [manifest_path + ": format_version is not 1"]
    problems = []
    listed = []
    for entry in manifest["files"]:
        listed.append(entry["path"])
        problems.extend("{}: {}: {}".format(manifest_path, entry["path"], p)
                        for p in _entry_problems(entry))
    if len(listed) != len(set(listed)):
        problems.append(manifest_path + ": a path is listed more than once")
    on_disk = set()
    for root, _, files in os.walk(directory):
        for name in files:
            path = os.path.relpath(os.path.join(root, name), directory).replace(os.sep, "/")
            if path != manifest_name:
                on_disk.add(path)
    for path in sorted(on_disk - set(listed)):
        problems.append("{}: missing {}".format(manifest_path, path))
    for path in sorted(set(listed) - on_disk):
        problems.append("{}: lists {}, which wasn't written".format(manifest_path, path))
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", help="Directory with the manifest and the files")
    parser.add_argument("--manifest", default="manifest.json", help="Name of the manifest")
    args = parser.parse_args()
    if not os.path.isfile(os.path.join(args.directory, args.manifest)):
        print("No manifest in " + args.directory, file=sys.stderr)
        sys.exit(1)
    problems = check_manifest(args.directory, args.manifest)
    for problem in problems:
        print(problem, file=sys.stderr)
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
