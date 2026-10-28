#!/usr/bin/env python3
"""Normalize generated pcb2gcode output before comparing it to expected/.

Walks a directory and, for each file that looks like SVG, enlarges width/height
attributes by factors of 10 until they are at least 1000 in each dimension.
Preserves the original line in a comment. Replaces the project version in
G-code headers so a release bump does not change the comparison.
"""

from __future__ import print_function
import argparse
import os
import re
import sys


# G-code headers include the project version. Expected files keep a stable
# token so a release bump does not fail the byte comparison.
_GCODE_VERSION = re.compile(r"\( pcb2gcode [0-9]+\.[0-9]+\.[0-9]+ \)")


def fix_up_expected(path):
    """Fix up generated output under path in place.

    Enlarges all SVG width/height by factors of 10 until at least 1000 each.
    Replaces the pcb2gcode version in G-code headers with VERSION.
    """
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
                line = _GCODE_VERSION.sub("( pcb2gcode VERSION )", line)
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


def main():
    parser = argparse.ArgumentParser(
        description='Enlarge SVG dimensions under a directory for comparison/viewing.'
    )
    parser.add_argument('path', help='Directory to process')
    args = parser.parse_args()
    if not os.path.isdir(args.path):
        print('Not a directory: %s' % args.path, file=sys.stderr)
        sys.exit(1)
    fix_up_expected(args.path)


if __name__ == '__main__':
    main()
