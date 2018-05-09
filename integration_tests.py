import unittest
from subprocess import call
import os
import tempfile
import shutil
import difflib
import filecmp
import sys

class IntegrationTests(unittest.TestCase):
  def run_one_directory(self, input_path, expected_output_path):
    cwd = os.getcwd()
    pcb2gcode = os.path.join(cwd, "pcb2gcode")
    os.chdir(input_path)
    actual_output_path = tempfile.mkdtemp()
    call([pcb2gcode, "--output-dir", actual_output_path])
    diff = filecmp.dircmp(actual_output_path, expected_output_path)
    # Now compare all the differing files.
    for f in diff.left_only:
      actual_file = os.path.join(actual_output_path, f)
      with open(actual_file, 'r') as myfile:
        data=myfile.readlines()
        sys.stdout.writelines(difflib.unified_diff(data, "", f, "/dev/null"))

    # Now compare all the differing files.
    all_diffs = []
    for f in diff.left_only:
      actual_file = os.path.join(actual_output_path, f)
      with open(actual_file, 'r') as myfile:
        data=myfile.readlines()
        all_diffs += difflib.unified_diff(data, [], f, "/dev/null")
    for f in diff.right_only:
      expected_file = os.path.join(expected_output_path, f)
      with open(expected_file, 'r') as myfile:
        data=myfile.readlines()
        all_diffs += difflib.unified_diff([], data, "/dev/null", f)
    for f in diff.diff_files:
      actual_file = os.path.join(actual_output_path, f)
      expected_file = os.path.join(expected_output_path, f)
      with open(actual_file, 'r') as myfile0, open(expected_file, 'r') as myfile1:
        data0=myfile0.readlines()
        data1=myfile1.readlines()
        all_diffs += difflib.unified_diff(data0, data1, f, f)

    shutil.rmtree(actual_output_path)
    os.chdir(cwd)
    self.assertFalse(diff.left_only + diff.right_only + diff.diff_files,
                     'Files don\'t match\n' + ''.join(all_diffs))

  def test_all(self):
    cwd = os.getcwd()
    examples_path = os.path.join(cwd, "testing/gerbv_example")
    test_cases = ["multivibrator"]
    for test_case in test_cases:
      input_path = os.path.join(examples_path, test_case)
      expected_output_path = os.path.join(input_path, "expected")
      self.run_one_directory(input_path, expected_output_path)

if __name__ == '__main__':
  call(["make"])
  unittest.main()
