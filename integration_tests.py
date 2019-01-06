#!/usr/bin/python

from __future__ import print_function
import unittest2
import subprocess
import os
import tempfile
import shutil
import difflib
import filecmp
import sys
import argparse
import re
import collections
import termcolor
import colour_runner.runner

from concurrencytest import ConcurrentTestSuite, fork_for_tests

TestCase = collections.namedtuple("TestCase", ["name", "input_path", "args", "exit_code"])

# Sanitize a string to be a python identifier
clean = lambda varStr: re.sub('\W|^(?=\d)','_', varStr)

EXAMPLES_PATH = "testing/gerbv_example"
BROKEN_EXAMPLES_PATH = "testing/broken_examples"
TEST_CASES = ([TestCase(clean(x), os.path.join(EXAMPLES_PATH, x), [], 0)
              for x in [
                  "am-test",
                  "am-test-counterclockwise",
                  "am-test-extended",
                  "am-test-voronoi",
                  "am-test-voronoi-front",
                  "edge-cuts-inside-cuts",
                  "edge-cuts-broken-loop",
                  "example_board_al_custom",
                  "example_board_al_linuxcnc",
                  "KNoT-Gateway Mini Starter Board",
                  "KNoT_Thing_Starter_Board",
                  "mill_masking",
                  "mill_masking_voronoi",
                  "milldrilldiatest",
                  "multivibrator",
                  "multivibrator-basename",
                  "multivibrator-clockwise",
                  "multivibrator-contentions",
                  "multivibrator-extra-passes",
                  "multivibrator-extra-passes-big",
                  "multivibrator-extra-passes-voronoi",
                  "multivibrator-no-tsp-2opt",
                  "multivibrator-no-zbridges",
                  "multivibrator_no_export",
                  "multivibrator_no_export_milldrill",
                  "multivibrator_pre_post_milling_gcode",
                  "multivibrator_xy_offset",
                  "multivibrator_xy_offset_zero_start",
                  "slots-milldrill",
                  "slots-with-drill",
                  "slots-with-drill-and-milldrill",
                  "slots-with-drill-metric",
                  "slots-with-drills-available",
              ]] +
              [TestCase(clean("multivibrator_bad_" + x), os.path.join(EXAMPLES_PATH, "multivibrator"), ["--" + x + "=non_existant_file"], 1)
               for x in ("front", "back", "outline", "drill")] +
              [TestCase(clean("broken_" + x),
                        os.path.join(BROKEN_EXAMPLES_PATH, x),
                        [], 1)
               for x in ("invalid-config",)
              ] +
              [TestCase(clean("version"),
                        os.path.join(EXAMPLES_PATH),
                        ["--version"],
                        0)] +
              [TestCase(clean("tsp_2opt_with_millfeedirection"),
                        os.path.join(EXAMPLES_PATH, "am-test"),
                        ["--tsp-2opt", "--mill-feed-direction=climb"],
                        100)] +
              [TestCase(clean("ignore warnings"),
                        os.path.join(BROKEN_EXAMPLES_PATH, "invalid-config"),
                        ["--ignore-warnings"],
                        0)] +
              [TestCase(clean("invalid_millfeedirection"),
                        os.path.join(EXAMPLES_PATH),
                        ["--mill-feed-direction=invalid_value"],
                        101)]
)

def colored(text, **color):
  if hasattr(sys.stderr, "isatty") and sys.stderr.isatty():
    return termcolor.colored(text, **color)
  else:
    return text

class IntegrationTests(unittest2.TestCase):
  def pcb2gcode_one_directory(self, input_path, args=[], exit_code=0):
    """Run pcb2gcode once in one directory.

    Current working directory remains unchanged at the end.

    input_path: Where to run pcb2gcode
    Returns the path to the output files created.
    """
    cwd = os.getcwd()
    pcb2gcode = os.path.join(cwd, "pcb2gcode")
    actual_output_path = tempfile.mkdtemp()
    os.chdir(input_path)
    try:
      cmd = [pcb2gcode, "--output-dir", actual_output_path] + args
      print("Running {}".format(" ".join("'{}'".format(x) for x in cmd)))
      p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
      result = p.communicate()
      self.assertEqual(p.returncode, exit_code)
    finally:
      print(result[0], file=sys.stderr)
      os.chdir(cwd)
    return actual_output_path

  def compare_directories(self, left, right, left_prefix="", right_prefix=""):
    """Compares two directories.

    Returns a string representing the diff between them if there is
    any difference between them.  If there is no difference between
    them, returns an empty string.
    left: Path to left side of diff
    right: Path to right side of diff
    left_prefix: String to prepend to all left-side files
    right_prefix: String to prepend to all right-side files
    """

    # Right side might not exist.
    if not os.path.exists(right):
      all_diffs = []
      for f in os.listdir(left):
        all_diffs += "Found %s but not %s.\n" % (os.path.join(left_prefix, f), os.path.join(right_prefix, f))
        left_file = os.path.join(left, f)
        with open(left_file, 'r') as myfile:
          data=myfile.readlines()
          all_diffs += difflib.unified_diff(data, [], '"' + os.path.join(left_prefix, f) + '"', "/dev/null")
      return ''.join(all_diffs)

    # Left side might not exist.
    if not os.path.exists(left):
      all_diffs = []
      for f in os.listdir(right):
        all_diffs += "Found %s but not %s.\n" % (os.path.join(right_prefix, f), os.path.join(left_prefix, f))
        right_file = os.path.join(right, f)
        with open(right_file, 'r') as myfile:
          data=myfile.readlines()
          all_diffs += difflib.unified_diff([], data, "/dev/null", '"' + os.path.join(right_prefix, f) + '"')
      return ''.join(all_diffs)


    diff = filecmp.dircmp(left, right)
    # Now compare all the differing files.
    all_diffs = []
    for f in diff.left_only:
      all_diffs += "Found %s but not %s.\n" % (os.path.join(left_prefix, f), os.path.join(right_prefix, f))
      left_file = os.path.join(left, f)
      with open(left_file, 'r') as myfile:
        data=myfile.readlines()
        all_diffs += difflib.unified_diff(data, [], '"' + os.path.join(left_prefix, f) + '"', "/dev/null")
    for f in diff.right_only:
      all_diffs += "Found %s but not %s.\n" % (os.path.join(right_prefix, f), os.path.join(left_prefix, f))
      right_file = os.path.join(right, f)
      with open(right_file, 'r') as myfile:
        data=myfile.readlines()
        all_diffs += difflib.unified_diff([], data, "/dev/null", '"' + os.path.join(right_prefix, f) + '"')
    for f in diff.diff_files:
      left_file = os.path.join(left, f)
      right_file = os.path.join(right, f)
      with open(left_file, 'r') as myfile0, open(right_file, 'r') as myfile1:
        data0=myfile0.readlines()
        data1=myfile1.readlines()
        all_diffs += difflib.unified_diff(data0, data1, '"' + os.path.join(left_prefix, f) + '"', '"' + os.path.join(right_prefix, f) + '"')
    return ''.join(all_diffs)

  def run_one_directory(self, input_path, expected_output_path, test_prefix, args=[], exit_code=0):
    """Run pcb2gcode on a directory and return the diff as a string.

    Returns an empty string if there is no mismatch.
    Returns the diff if there is a mismatch.
    input_path: Path to inputs
    expected_output_path: Path to expected outputs
    test_prefix: Strin to prepend to all filenamess
    """
    actual_output_path = self.pcb2gcode_one_directory(input_path, args, exit_code)
    if exit_code:
      return ""
    diff_text = self.compare_directories(expected_output_path, actual_output_path,
                                         os.path.join("expected", test_prefix),
                                         os.path.join("actual", test_prefix))
    shutil.rmtree(actual_output_path)
    return diff_text

  def do_test_one(self, test_case):
    cwd = os.getcwd()
    test_prefix = os.path.join(test_case.input_path, "expected")
    input_path = os.path.join(cwd, test_case.input_path)
    expected_output_path = os.path.join(cwd, test_case.input_path, "expected")
    print(colored("\nRunning test case:\n" + "\n".join("    %s=%s" % (k,v) for k,v in test_case._asdict().items()), attrs=["bold"]), file=sys.stderr)
    diff_text = self.run_one_directory(input_path, expected_output_path, test_prefix, test_case.args, test_case.exit_code)
    self.assertFalse(bool(diff_text), 'Files don\'t match\n' + diff_text)

if __name__ == '__main__':
  def add_test_case(t):
    def test_method(self):
      self.do_test_one(t)
    setattr(IntegrationTests, 'test_' + t.name, test_method)
    test_method.__name__ = 'test_' + t.name
    test_method.__doc__ = str(test_case)
  for test_case in TEST_CASES:
    add_test_case(test_case)
  parser = argparse.ArgumentParser(description='Integration test of pcb2gcode.')
  parser.add_argument('--fix', action='store_true', default=False,
                      help='Generate expected outputs automatically')
  parser.add_argument('--add', action='store_true', default=False,
                      help='git add new expected outputs automatically')
  args = parser.parse_args()
  if args.fix:
    print("Generating expected outputs...")
    output = None
    try:
      subprocess.check_output([sys.argv[0]], stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError, e:
      output = str(e.output)
    if not output:
      print("No diffs, nothing to do.")
      exit(0)
    p = subprocess.Popen(["patch", "-p1"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    result = p.communicate(input=output)
    files_patched = []
    for l in result[0].split('\n'):
      if l.startswith("patching file '"):
        files_patched.append(l[len("patching file '"):-1])
      elif l.startswith("patching file "):
        files_patched.append(l[len("patching file "):])
    print(result[0])
    if args.add:
      subprocess.call(["git", "add"] + files_patched)
      print("Done.\nAdded to git:\n" +
            '\n'.join(files_patched))
    else:
      print("Done.\nYou now need to run:\n" +
            '\n'.join('git add ' + x for x in files_patched))
  else:
    test_loader = unittest2.TestLoader()
    all_test_names = ["test_" + t.name for t in TEST_CASES]
    test_loader.sortTestMethodsUsing = lambda x,y: cmp(all_test_names.index(x), all_test_names.index(y))
    suite = test_loader.loadTestsFromTestCase(IntegrationTests)
    concurrent_suite = ConcurrentTestSuite(suite, fork_for_tests(3))
    if hasattr(sys.stderr, "isatty") and sys.stderr.isatty():
      test_result = colour_runner.runner.ColourTextTestRunner(verbosity=2).run(concurrent_suite)
    else:
      test_result = unittest2.TextTestRunner(verbosity=2).run(concurrent_suite)
    if not test_result.wasSuccessful():
      print('\n***\nRun one of these:\n' +
            './integration_tests.py --fix\n' +
            './integration_tests.py --fix --add\n' +
            '***\n')
      exit(1)
    else:
      exit(0)
