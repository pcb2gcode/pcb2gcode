#!/usr/bin/env python3
"""Feed WKT to wkt_to_svg and check the SVG it writes.

wkt_to_svg is a standalone executable (src/wkt_to_svg.cpp), so the only way
to cover it is to run that binary. Each case below hits a different branch
of its stdin loop: one geometry type, a line it ignores, an empty stream,
or a blank line that stops reading.

Usage:
  tools/run_wkt_to_svg_test.py <wkt_to_svg_binary> [case|all]
"""

from __future__ import print_function

import argparse
import subprocess
import sys

# Hardcoded in wkt_to_svg.cpp: envelope from (0, 0) to (14, 14), times
# SVG_DOTS_PER_IN (2000). Independent of the WKT that follows.
_VIEWBOX = 'viewBox="0 0 28000 28000"'

_LINE_STYLE = (
    "stroke:rgb(0,0,0);stroke-width:10;fill:none;"
    "stroke-opacity:0.3;stroke-linecap:round;stroke-linejoin:round;"
)
_POLY_STYLE = (
    "stroke:rgb(0,0,0);stroke-width:10;fill:red;"
    "stroke-opacity:1;stroke-linecap:round;stroke-linejoin:round;"
)

# Unit square. Mapped alone, the corners land on the viewBox edges and the
# SVG y axis is flipped, so (0, 0) is the bottom-left.
_LINESTRING = "LINESTRING(0 0, 1 1)\n"
_LINESTRING_POINTS = 'points="0,28000 28000,0"'
_MULTILINESTRING = "MULTILINESTRING((0 0, 1 1),(2 2, 3 3))\n"
_POLYGON = "POLYGON((0 0, 1 0, 1 1, 0 0))\n"
_POLYGON_PATH = "M 0,28000 L 28000,28000 L 28000,0 L 0,28000 z"
# Two polygons, so this is not interchangeable with the single POLYGON case.
_MULTIPOLYGON = (
    "MULTIPOLYGON(((0 0, 1 0, 1 1, 0 0)),((2 0, 3 0, 3 1, 2 0)))\n"
)


def _run(binary, text):
    # Bytes, not text mode: text mode would turn \n into \r\n on Windows and
    # the '\r' would be part of the WKT line.
    proc = subprocess.run(
        [binary],
        input=text.encode("ascii"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout = proc.stdout.decode("ascii", errors="replace")
    stderr = proc.stderr.decode("ascii", errors="replace")
    if proc.returncode != 0:
        raise AssertionError(
            "wkt_to_svg exited with {}\nstdout:\n{}\nstderr:\n{}".format(
                proc.returncode, stdout, stderr))
    if _VIEWBOX not in stdout or "</svg>" not in stdout:
        raise AssertionError(
            "expected an SVG document with {}\nstdout:\n{}".format(
                _VIEWBOX, stdout))
    return stdout


def _expect(stdout, polylines, paths, line_style, poly_style, needles=()):
    got_polylines = stdout.count("<polyline ")
    got_paths = stdout.count("<path ")
    if got_polylines != polylines or got_paths != paths:
        raise AssertionError(
            "expected {} polylines and {} paths, got {} and {}\nstdout:\n{}".format(
                polylines, paths, got_polylines, got_paths, stdout))
    if line_style != (_LINE_STYLE in stdout):
        raise AssertionError(
            "line style present={}, expected {}\nstdout:\n{}".format(
                _LINE_STYLE in stdout, line_style, stdout))
    if poly_style != (_POLY_STYLE in stdout):
        raise AssertionError(
            "polygon style present={}, expected {}\nstdout:\n{}".format(
                _POLY_STYLE in stdout, poly_style, stdout))
    for needle in needles:
        if needle not in stdout:
            raise AssertionError(
                "missing {!r}\nstdout:\n{}".format(needle, stdout))


def case_empty(binary):
    stdout = _run(binary, "")
    _expect(stdout, polylines=0, paths=0, line_style=False, poly_style=False)


def case_linestring(binary):
    stdout = _run(binary, _LINESTRING)
    _expect(stdout, polylines=1, paths=0, line_style=True, poly_style=False,
            needles=(_LINESTRING_POINTS,))


def case_multilinestring(binary):
    stdout = _run(binary, _MULTILINESTRING)
    _expect(stdout, polylines=2, paths=0, line_style=True, poly_style=False)


def case_polygon(binary):
    stdout = _run(binary, _POLYGON)
    _expect(stdout, polylines=0, paths=1, line_style=False, poly_style=True,
            needles=(_POLYGON_PATH,))


def case_multipolygon(binary):
    stdout = _run(binary, _MULTIPOLYGON)
    _expect(stdout, polylines=0, paths=2, line_style=False, poly_style=True)


def case_ignored(binary):
    # Prefixes that are not an exact type name, plus a short line. None of
    # these match the strncmp checks, so they must not throw and must not
    # draw anything.
    stdout = _run(binary, "NOT_WKT\nPOLY\nLINE\n \n")
    _expect(stdout, polylines=0, paths=0, line_style=False, poly_style=False)


def case_blank_line_stops(binary):
    # A blank line sets the read buffer to empty and ends the loop, so the
    # polygon after it is never read.
    stdout = _run(binary, _LINESTRING + "\n" + _POLYGON)
    _expect(stdout, polylines=1, paths=0, line_style=True, poly_style=False,
            needles=(_LINESTRING_POINTS,))


def case_combined(binary):
    # One process walks every branch of the if/else chain, then stops.
    text = "".join([
        _LINESTRING,
        _MULTILINESTRING,
        _POLYGON,
        _MULTIPOLYGON,
        "NOT_WKT\n",
    ])
    stdout = _run(binary, text)
    _expect(stdout, polylines=3, paths=3, line_style=True, poly_style=True,
            needles=(_LINESTRING_POINTS,))


_CASES = (
    ("empty", case_empty),
    ("linestring", case_linestring),
    ("multilinestring", case_multilinestring),
    ("polygon", case_polygon),
    ("multipolygon", case_multipolygon),
    ("ignored", case_ignored),
    ("blank_line_stops", case_blank_line_stops),
    ("combined", case_combined),
)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wkt_to_svg_binary", help="Path to the wkt_to_svg executable")
    parser.add_argument(
        "case",
        nargs="?",
        default="all",
        choices=["all"] + [name for name, _fn in _CASES],
        help="Which case to run (default: all)",
    )
    args = parser.parse_args(argv)
    selected = _CASES if args.case == "all" else [
        item for item in _CASES if item[0] == args.case]
    for name, fn in selected:
        try:
            fn(args.wkt_to_svg_binary)
        except AssertionError as exc:
            print("wkt_to_svg case {!r} failed:\n{}".format(name, exc), file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
