#!/usr/bin/env python3
"""Set CMakeLists.txt VERSION from the draft release's semantic version.

The draft release is still published by hand. This script only chooses the
next version and writes it into the project:

  * major  — a merged pull request title is a breaking conventional commit
             (feat!:, fix!:, ...) or carries the "major" label
  * minor  — a feature, and nothing breaking
  * patch  — any other change since the last published tag (a bugfix)

Run from a checkout of master:

  tools/bump_release_version.py --part auto \\
      --resolved-version 3.0.2 --last-tag v3.0.1 --write

--part major|minor|patch bumps the current project version by that part,
ignoring the draft. auto is idempotent: running it again, with no new
increment, leaves the file alone.

The version is applied to the project version from before any
"chore: set version to ..." commits, so a second run does not bump twice
while the same draft is still open.
"""

from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

_CMAKE_VERSION = re.compile(
    r"^(?P<prefix>  VERSION )(?P<version>[0-9]+\.[0-9]+\.[0-9]+)\s*$",
    re.MULTILINE,
)
_BOT_PREFIX = "chore: set version to "


def parse_version(text):
    """Return (major, minor, patch) from '3.0.4' or 'v3.0.4'."""
    value = text.strip()
    if value.startswith("v") or value.startswith("V"):
        value = value[1:]
    parts = value.split(".")
    if len(parts) != 3:
        raise ValueError("not a major.minor.patch version: %s" % text)
    try:
        numbers = tuple(int(part) for part in parts)
    except ValueError:
        raise ValueError("not a major.minor.patch version: %s" % text)
    if any(number < 0 for number in numbers):
        raise ValueError("not a major.minor.patch version: %s" % text)
    return numbers


def format_version(numbers):
    return "%d.%d.%d" % numbers


def read_project_version(cmake_text):
    match = _CMAKE_VERSION.search(cmake_text)
    if not match:
        raise ValueError("CMakeLists.txt has no '  VERSION x.y.z' line")
    return match.group("version")


def write_project_version(cmake_text, version):
    parse_version(version)
    if not _CMAKE_VERSION.search(cmake_text):
        raise ValueError("CMakeLists.txt has no '  VERSION x.y.z' line")

    def replace(match):
        return match.group("prefix") + version

    return _CMAKE_VERSION.sub(replace, cmake_text, count=1)


def bump(version, part):
    major, minor, patch = parse_version(version)
    if part == "major":
        return format_version((major + 1, 0, 0))
    if part == "minor":
        return format_version((major, minor + 1, 0))
    if part == "patch":
        return format_version((major, minor, patch + 1))
    raise ValueError("part must be major, minor, or patch, not %s" % part)


def rank_since_last_tag(last_tag, resolved_version):
    """Return major, minor, patch, or None when the draft did not move."""
    last = parse_version(last_tag)
    resolved = parse_version(resolved_version)
    if resolved[0] > last[0]:
        return "major"
    if resolved[0] == last[0] and resolved[1] > last[1]:
        return "minor"
    if resolved > last:
        return "patch"
    return None


def next_version(current, baseline, part, resolved_version, last_tag):
    """Return the version that should be written.

    current is the version in the working tree. baseline is the version
    before automated bumps in this unpublished cycle. An explicit part bumps
    current. auto applies the draft's increment to baseline and never
    decreases current.
    """
    if part != "auto":
        return bump(current, part)
    rank = rank_since_last_tag(last_tag, resolved_version)
    if rank is None:
        return current
    target = bump(baseline, rank)
    if parse_version(target) < parse_version(current):
        return current
    return target


def _git(repo, *args):
    output = subprocess.check_output(
        ["git", "-C", repo] + list(args),
        stderr=subprocess.STDOUT,
    )
    return output.decode("utf-8").strip()


def baseline_version(repo):
    """Project version before consecutive automated version commits."""
    rev = "HEAD"
    while True:
        subject = _git(repo, "log", "-1", "--format=%s", rev)
        if not subject.startswith(_BOT_PREFIX):
            break
        rev = rev + "^"
    cmake_text = _git(repo, "show", "%s:CMakeLists.txt" % rev)
    return read_project_version(cmake_text)


def _write_github_output(name, value):
    path = os.environ.get("GITHUB_OUTPUT")
    if not path:
        return
    with open(path, "a", encoding="utf-8") as handle:
        handle.write("%s=%s\n" % (name, value))


def apply_version(repo, version):
    path = os.path.join(repo, "CMakeLists.txt")
    with open(path, "r", encoding="utf-8") as handle:
        original = handle.read()
    updated = write_project_version(original, version)
    if updated == original:
        return False
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(updated)
    return True


def _self_test_git_baseline():
    import tempfile
    root = tempfile.mkdtemp(prefix="bump-version-")
    cmake = "project(\n  pcb2gcode\n  VERSION 3.0.4\n)\n"
    subprocess.check_call(["git", "init", "-q"], cwd=root)
    with open(os.path.join(root, "CMakeLists.txt"), "w", encoding="utf-8") as handle:
        handle.write(cmake)
    subprocess.check_call(["git", "add", "CMakeLists.txt"], cwd=root)
    subprocess.check_call(
        [
            "git",
            "-c", "user.email=bump-test@example.com",
            "-c", "user.name=bump-test",
            "commit", "-q", "-m", "initial",
        ],
        cwd=root,
    )
    assert baseline_version(root) == "3.0.4"
    apply_version(root, "3.0.5")
    subprocess.check_call(["git", "add", "CMakeLists.txt"], cwd=root)
    subprocess.check_call(
        [
            "git",
            "-c", "user.email=bump-test@example.com",
            "-c", "user.name=bump-test",
            "commit", "-q", "-m", "chore: set version to 3.0.5",
        ],
        cwd=root,
    )
    # The open draft still accounts for the same changes, so the baseline
    # stays 3.0.4 and a second auto run does not become 3.0.6.
    assert baseline_version(root) == "3.0.4"
    assert next_version("3.0.5", baseline_version(root), "auto", "3.0.2", "v3.0.1") == "3.0.5"


def _self_test():
    assert parse_version("v3.0.4") == (3, 0, 4)
    assert bump("3.0.4", "patch") == "3.0.5"
    assert bump("3.0.4", "minor") == "3.1.0"
    assert bump("3.0.4", "major") == "4.0.0"
    assert rank_since_last_tag("v3.0.1", "3.0.2") == "patch"
    assert rank_since_last_tag("v3.0.1", "3.1.0") == "minor"
    assert rank_since_last_tag("v3.0.1", "4.0.0") == "major"
    assert rank_since_last_tag("v3.0.1", "3.0.1") is None
    # Draft says the next patch after v3.0.1, but the tree is already 3.0.4.
    # The bump is one patch on that baseline, not a downgrade to 3.0.2.
    assert next_version("3.0.4", "3.0.4", "auto", "3.0.2", "v3.0.1") == "3.0.5"
    assert next_version("3.0.4", "3.0.4", "auto", "3.1.0", "v3.0.1") == "3.1.0"
    assert next_version("3.0.4", "3.0.4", "auto", "4.2.0", "v3.0.1") == "4.0.0"
    # A second look at the same draft, after 3.0.4 was already moved to 3.0.5.
    assert next_version("3.0.5", "3.0.4", "auto", "3.0.2", "v3.0.1") == "3.0.5"
    # A later breaking change raises the same baseline to the next major.
    assert next_version("3.0.5", "3.0.4", "auto", "4.0.0", "v3.0.1") == "4.0.0"
    assert next_version("3.0.4", "3.0.4", "auto", "3.0.1", "v3.0.1") == "3.0.4"
    assert next_version("3.0.4", "3.0.4", "patch", "3.0.2", "v3.0.1") == "3.0.5"
    sample = "project(\n  pcb2gcode\n  VERSION 3.0.4\n  DESCRIPTION \"x\"\n)\n"
    assert read_project_version(sample) == "3.0.4"
    rewritten = write_project_version(sample, "3.1.0")
    assert "VERSION 3.1.0" in rewritten
    assert read_project_version(rewritten) == "3.1.0"
    _self_test_git_baseline()
    print("bump_release_version self-test passed")


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--part",
        choices=["auto", "major", "minor", "patch"],
        default="auto",
        help="auto follows the draft release; the others force that component",
    )
    parser.add_argument("--resolved-version", help="Version chosen by Release Drafter")
    parser.add_argument("--last-tag", help="Newest published tag, such as v3.0.1")
    parser.add_argument("--repo", default=".", help="Checkout to read and update")
    parser.add_argument("--write", action="store_true", help="Rewrite CMakeLists.txt")
    parser.add_argument("--print-current", action="store_true", help="Print the version in CMakeLists.txt and exit")
    parser.add_argument("--self-test", action="store_true", help="Run the built-in checks and exit")
    args = parser.parse_args(argv)

    if args.self_test:
        _self_test()
        return 0

    repo = os.path.abspath(args.repo)
    cmake_path = os.path.join(repo, "CMakeLists.txt")
    with open(cmake_path, "r", encoding="utf-8") as handle:
        cmake_text = handle.read()
    current = read_project_version(cmake_text)

    if args.print_current:
        print("version=%s" % current)
        _write_github_output("version", current)
        return 0

    if args.part == "auto":
        if not args.resolved_version or not args.last_tag:
            print(
                "auto needs --resolved-version and --last-tag",
                file=sys.stderr,
            )
            return 2
        # Release Drafter still proposes a patch when the draft notes are
        # "* No changes". A pull request reference means there is something
        # to release. Local runs that omit RELEASE_BODY trust the version.
        release_body = os.environ.get("RELEASE_BODY")
        if release_body is not None and "(#" not in release_body:
            chosen = current
        else:
            baseline = baseline_version(repo)
            chosen = next_version(
                current, baseline, args.part, args.resolved_version, args.last_tag)
    else:
        chosen = next_version(current, current, args.part, "0.0.0", "v0.0.0")

    changed = chosen != current
    if args.write and changed:
        apply_version(repo, chosen)
    print("version=%s" % chosen)
    print("changed=%s" % ("true" if changed else "false"))
    _write_github_output("version", chosen)
    _write_github_output("changed", "true" if changed else "false")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
