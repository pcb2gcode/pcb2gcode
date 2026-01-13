#!/usr/bin/python3

"""Run integration tests on pcb2gcode."""

from __future__ import print_function
import argparse
import collections
import difflib
import filecmp
import multiprocessing
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree
try:
  import colour_runner.runner
  colour_runner_available = True
except:
  colour_runner_available = False
import in_place
import termcolor
import unittest

try:
  from concurrencytest import ConcurrentTestSuite, fork_for_tests
  concurrencytest_available = True
except:
  concurrencytest_available = False

TestCase = collections.namedtuple("TestCase", ["name", "input_path", "args", "exit_code"])

EXAMPLES_PATH = "testing/gerbv_example"
BROKEN_EXAMPLES_PATH = "testing/broken_examples"
TEST_CASES = ([TestCase(x, os.path.join(EXAMPLES_PATH, x), [], 0)
              for x in [
                  "am-test",
                  "am-test-counterclockwise",
                  "am-test-extended",
                  "am-test-millinfeed",
                  "am-test-voronoi",
                  "am-test-voronoi-extra-passes",
                  "am-test-voronoi-front",
                  "am-test-voronoi-wide-extra-passes",
                  "backtrack",
                  "backtrack_0",
                  "D1MiniGSR",
                  "Easy-SDR_HF_Upconverter_SMD_Gerbers",
                  "edge-cuts-broken-loop",
                  "edge-cuts-inside-cuts",
                  "example_board_new_default",
                  "example_board_new_mirror_x",
                  "example_board_new_mirror_y",
                  "example_board_new_mirror_y_drill_back",
                  "example_board_new_zero_start",
                  "example_board_al_custom",
                  "example_board_al_custom_tiled",
                  "example_board_al_linuxcnc",
                  "example_board_al_linuxcnc_tiled",
                  "example_board_al_mach3",
                  "example_board_al_mach3_tiled",
                  "example_board_al_mach4",
                  "example_board_al_mach4_tiled",
                  "invert_gerbers",
                  "invert_gerbers_fill",
                  "KeyboardControllerM102",
                  "KNoT-Gateway Mini Starter Board",
                  "KNoT_Thing_Starter_Board",
                  "lift-mill",
                  "mill_masking",
                  "mill_masking_voronoi",
                  "milldrilldiatest",
                  "milldrilldiatest_units",
                  "multivibrator",
                  "multivibrator-basename",
                  "multivibrator-clockwise",
                  "multivibrator-contentions",
                  "multivibrator-extra-passes",
                  "multivibrator-extra-passes-big",
                  "multivibrator-extra-passes-two-isolators",
                  "multivibrator-extra-passes-two-isolators-tiles",
                  "multivibrator-extra-passes-two-isolators-tiles-al",
                  "multivibrator-extra-passes-voronoi",
                  "multivibrator-identical-isolators",
                  "multivibrator-no-tsp-2opt",
                  "multivibrator-no-zbridges",
                  "multivibrator-two-isolators",
                  "multivibrator_backtrack",
                  "multivibrator_milldrill_nom6",
                  "multivibrator_no_export",
                  "multivibrator_no_export_milldrill",
                  "multivibrator_no_optimise",
                  "multivibrator_no_zero_start",
                  "multivibrator_nom6",
                  "multivibrator_pre_post_milling_gcode",
                  "multivibrator_xy_offset",
                  "multivibrator_xy_offset_zero_start",
                  "multivibrator-zchange-absolute",
                  "multi_outline",
                  "null_drill",
                  "overlapping_edge_cuts",
                  "project-controller",
                  "Rotary-Encoder-Breakout",
                  "round_pcb_3",
                  "round_pcb_4",
                  "round_pcb_5",
                  "shaped_pcb",
                  "sharp_corner",
                  "sharp_corner_2",
                  "sharp_corner_2_offset",
                  "sharp_corner_big_isolation_width",
                  "silk",
                  "silk-lines",
                  "slots-milldrill",
                  "slots-milldrill-metric",
                  "slots-with-drill",
                  "slots-with-drill-and-milldrill",
                  "slots-with-drill-metric",
                  "slots-with-drills-available",
              ]] +
              [TestCase("split config csv", os.path.join(BROKEN_EXAMPLES_PATH, "split_config"),
                        ["--config=millproject,millproject2"], 0)] +
              [TestCase("bad output dir " + x, os.path.join(EXAMPLES_PATH, x),
                        ["--output-dir=/tmp/nonexistantpath"], 1)
               for x in ("multivibrator", "slots-with-drill", "slots-milldrill")] +
              [TestCase("split config", os.path.join(BROKEN_EXAMPLES_PATH, "split_config"),
                        ["--config=millproject", "--config=millproject2"], 0)] +
              [TestCase("split config", os.path.join(BROKEN_EXAMPLES_PATH, "split_config"),
                        [], 14)] +
              [TestCase("multivibrator_bad_" + x,
                        os.path.join(EXAMPLES_PATH, "multivibrator"),
                        ["--" + x + "=non_existant_file"], 100)
               for x in ("front", "back", "outline", "drill")] +
              [TestCase("broken_" + x,
                        os.path.join(BROKEN_EXAMPLES_PATH, x),
                        [], 100)
               for x in ("invalid-config",
                         )
              ] +
              [TestCase("version",
                        os.path.join(EXAMPLES_PATH),
                        ["--version"],
                        0)] +
              [TestCase("help",
                        os.path.join(EXAMPLES_PATH),
                        ["--help"],
                        0)] +
              [TestCase("tsp_2opt_with_millfeedirection",
                        os.path.join(EXAMPLES_PATH, "am-test"),
                        ["--tsp-2opt", "--mill-feed-direction=climb"],
                        100)] +
              [TestCase("g64_and_tolerance",
                        os.path.join(EXAMPLES_PATH, "am-test"),
                        ["--g64=5", "--tolerance=123"],
                        49)] +
              [TestCase("negative_spinup",
                        os.path.join(EXAMPLES_PATH, "am-test"),
                        ["--spinup-time=-5"],
                        52)] +
              [TestCase("zero_millinfeed",
                        os.path.join(EXAMPLES_PATH, "am-test"),
                        ["--mill-infeed=0"],
                        55)] +
              [TestCase("ignore warnings",
                        os.path.join(BROKEN_EXAMPLES_PATH, "invalid-config"),
                        ["--ignore-warnings"],
                        0)] +
              [TestCase("provided config",
                        os.path.join(EXAMPLES_PATH, "am-test"),
                        ["--config=millproject"],
                        0)] +
              [TestCase("missing config",
                        os.path.join(EXAMPLES_PATH, "am-test"),
                        ["--config=millproject_file_does_not_exist"],
                        100)] +
              [TestCase("invalid_millfeedirection",
                        os.path.join(EXAMPLES_PATH),
                        ["--mill-feed-direction=invalid_value"],
                        101)] +
              [TestCase("zchange_below_zdrill",
                        os.path.join(EXAMPLES_PATH, "multivibrator-zchange-absolute"),
                        ["--zchange-absolute=false"],
                        19)]
)

def colored(text, **color):
  """Colorize text if supported."""
  if hasattr(sys.stderr, "isatty") and sys.stderr.isatty():
    return termcolor.colored(text, **color)
  return text

class IntegrationTests(unittest.TestCase):
  """Run integration tests."""
  def fix_up_expected(self, path):
    """Fix up any files made in the output directory

    This will enlarge all SVG by a factor of 10 in each direction until they are
    at least 1000 in each dimension.  This makes them easier to view on github.
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
        with in_place.InPlace(os.path.join(root, current_file)) as svg_file:
          for line in svg_file:
            if line.startswith("<svg"):
              svg_file.write("<!-- original:\n" +
                             line +
                             "-->\n" +
                             re.sub('width="(?P<width>[^"]*)" height="(?P<height>[^"]*)" ',
                                    bigger,
                                    line))
            else:
              svg_file.write(line)

  def pcb2gcode_one_directory(self, input_path, pcb2gcode_binary, args=None, exit_code=0):
    """Run pcb2gcode once in one directory.

    Current working directory remains unchanged at the end.

    input_path: Where to run pcb2gcode
    Returns the path to the output files created.
    """
    cwd = os.getcwd()  # Save this for later restoring the current working directory
    actual_output_path = tempfile.mkdtemp()
    os.chdir(input_path)
    try:
      cmd = [pcb2gcode_binary]
      if not any("output-dir" in x for x in args):
        cmd += ["--output-dir", actual_output_path]
      cmd += args or []
      print("Running {}".format(" ".join("'{}'".format(x) for x in cmd)))
      proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
      result = proc.communicate()
      self.assertEqual(proc.returncode, exit_code)
      self.fix_up_expected(actual_output_path)
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

  def run_one_directory(self, input_path, pcb2gcode_binary, expected_output_path, test_prefix, args=[], exit_code=0):
    """Run pcb2gcode on a directory and return the diff as a string.

    Returns an empty string if there is no mismatch.
    Returns the diff if there is a mismatch.
    input_path: Path to inputs
    expected_output_path: Path to expected outputs
    test_prefix: Strin to prepend to all filenamess
    """
    actual_output_path = self.pcb2gcode_one_directory(input_path, pcb2gcode_binary, args, exit_code)
    if exit_code:
      return ""
    diff_text = self.compare_directories(expected_output_path, actual_output_path,
                                         os.path.join("expected", test_prefix),
                                         os.path.join("actual", test_prefix))
    shutil.rmtree(actual_output_path)
    return diff_text

  def do_test_one(self, test_case, pcb2gcode_binary):
    test_prefix = os.path.join(test_case.input_path, "expected")
    cwd = os.path.dirname(pcb2gcode_binary)
    input_path = os.path.join(cwd, test_case.input_path)
    expected_output_path = os.path.join(cwd, test_case.input_path, "expected")
    print(colored("\nRunning test case:\n" + "\n".join("    %s=%s" % (k,v) for k,v in test_case._asdict().items()), attrs=["bold"]), file=sys.stderr)
    diff_text = self.run_one_directory(input_path, pcb2gcode_binary, expected_output_path, test_prefix, test_case.args, test_case.exit_code)
    self.assertFalse(bool(diff_text), 'Files don\'t match\n' + diff_text)

def cmp(x,y):
  return (x>y) - (x<y)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Integration test of pcb2gcode.')
  parser.add_argument('--fix', action='store_true', dest='fix',
                      help='Update expected outputs automatically')
  parser.add_argument('--no-fix', action='store_false', dest='fix',
                      help='Don\'t update expected outputs automatically')
  parser.add_argument('--add', action='store_true', default=False,
                      help='git add new expected outputs automatically')
  parser.add_argument('-j', '--jobs', type=int,
                      default=multiprocessing.cpu_count(),
                      help='number of threads for running tests concurrently')
  parser.add_argument('--tests', type=str, default="",
                      help='regex of tests to run')
  parser.add_argument('--pcb2gcode-binary', type=str, default="",
                      help='path to pcb2gcode binary to run.  The tests are expected to be in subdirectories of the directory containing the binary.')
  args = parser.parse_args()
  if args.tests:
    TEST_CASES = [t for t in TEST_CASES if re.search(args.tests, t.name)]
  pcb2gcode_binary = os.path.join(os.getcwd(), "pcb2gcode") if not args.pcb2gcode_binary else args.pcb2gcode_binary
  def add_test_case(t):
    def test_method(self):
      self.do_test_one(t, pcb2gcode_binary)
    setattr(IntegrationTests, 'test_' + t.name, test_method)
    test_method.__name__ = 'test_' + t.name
    test_method.__doc__ = str(test_case)
  for test_case in TEST_CASES:
    add_test_case(test_case)
  if args.fix:
    print("Generating expected outputs...")
    output = None
    try:
      subprocess.check_output(sys.argv + ['--no-fix'], stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
      output = e.output
    if not output:
      print("No diffs, nothing to do.")
      exit(0)
    p = subprocess.Popen(["patch", "-p1"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    result = p.communicate(input=output)
    files_patched = []
    for l in result[0].split(b'\n'):
      if l.startswith(b"patching file '"):
        files_patched.append(l[len("patching file '"):-1].decode())
      elif l.startswith(b"patching file "):
        files_patched.append(l[len("patching file "):].decode())
    print(result[0].decode())
    if args.add:
      subprocess.call(["git", "add"] + files_patched)
      print("Done.\nAdded to git:\n" +
            '\n'.join(files_patched))
    else:
      print("Done.\nYou now need to run:\n" +
            '\n'.join('git add ' + x for x in files_patched))
  else:
    test_loader = unittest.TestLoader()
    all_test_names = ["test_" + t.name for t in TEST_CASES]
    test_loader.sortTestMethodsUsing = lambda x,y: cmp(all_test_names.index(x), all_test_names.index(y))
    suite = test_loader.loadTestsFromTestCase(IntegrationTests)
    if args.jobs > 1:
      if concurrencytest_available:
        suite = ConcurrentTestSuite(suite, fork_for_tests(args.jobs))
      else:
        print("WARNING: Module 'concurrencytest' not available. Running tests sequentially.", file=sys.stderr)
    if colour_runner_available and hasattr(sys.stderr, "isatty") and sys.stderr.isatty():
      test_result = colour_runner.runner.ColourTextTestRunner(verbosity=2).run(suite)
    else:
      test_result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not test_result.wasSuccessful():
      print('\n***\nRun one of these:\n' +
            './integration_tests.py --fix\n' +
            './integration_tests.py --fix --add\n' +
            '***\n')
      exit(1)
    else:
      exit(0)
