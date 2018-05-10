#!/usr/bin/python

import unittest
from subprocess import call
import os
import tempfile
import shutil
import difflib
import filecmp
import sys

class IntegrationTests(unittest.TestCase):

  def pcb2gcode_one_directory(self, input_path):
    """Run pcb2gcode once in one directory.

    Current working directory remains unchanged at the end.

    input_path: Where to run pcb2gcode
    Returns the path to the output files created.
    """
    cwd = os.getcwd()
    pcb2gcode = os.path.join(cwd, "pcb2gcode")
    os.chdir(input_path)
    actual_output_path = tempfile.mkdtemp()
    call([pcb2gcode, "--output-dir", actual_output_path])
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
    diff = filecmp.dircmp(left, right)
    # Now compare all the differing files.
    all_diffs = []
    for f in diff.left_only:
      all_diffs += "Found %s but not %s.\n" % (os.path.join(left_prefix, f), os.path.join(right_prefix, f))
      left_file = os.path.join(left, f)
      with open(left_file, 'r') as myfile:
        data=myfile.readlines()
        all_diffs += difflib.unified_diff(data, [], os.path.join(left_prefix, f), "/dev/null")
    for f in diff.right_only:
      all_diffs += "Found %s but not %s.\n" % (os.path.join(right_prefix, f), os.path.join(left_prefix, f))
      right_file = os.path.join(right, f)
      with open(right_file, 'r') as myfile:
        data=myfile.readlines()
        all_diffs += difflib.unified_diff([], data, "/dev/null", os.path.join(right_prefix, f))
    for f in diff.diff_files:
      left_file = os.path.join(left, f)
      right_file = os.path.join(right, f)
      with open(left_file, 'r') as myfile0, open(right_file, 'r') as myfile1:
        data0=myfile0.readlines()
        data1=myfile1.readlines()
        all_diffs += difflib.unified_diff(data0, data1, os.path.join(left_prefix, f), os.path.join(right_prefix, f))
    return ''.join(all_diffs)

  def run_one_directory(self, input_path, expected_output_path, test_prefix):
    actual_output_path = self.pcb2gcode_one_directory(input_path)
    diff_text = self.compare_directories(expected_output_path, actual_output_path,
                                         os.path.join(test_prefix, "expected"),
                                         os.path.join(test_prefix, "actual"))
    shutil.rmtree(actual_output_path)
    self.assertFalse(diff_text,
                     'Files don\'t match\n' + diff_text)

  def test_all(self):
    cwd = os.getcwd()
    examples_path = "testing/gerbv_example"
    test_cases = ["multivibrator"]
    for test_case in test_cases:
      test_prefix = os.path.join(examples_path, test_case)
      input_path = os.path.join(cwd, test_prefix)
      expected_output_path = os.path.join(input_path, "expected")
      self.run_one_directory(input_path, expected_output_path, test_prefix)

if __name__ == '__main__':
  call(["make"])
  unittest.main()
