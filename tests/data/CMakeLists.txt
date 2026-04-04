# Test fixtures (gerbers, expected outputs, broken examples, etc.).

# Always register tests; ctest skips them when versions do not match (see tests/CMakeLists.txt).
set(_pcb2gcode_integration_disabled TRUE)
if(PCB2GCODE_RUN_INTEGRATION_TESTS)
  set(_pcb2gcode_integration_disabled FALSE)
endif()

# run_gerbv_example_test.py harness. Ctest label reflects expected pcb2gcode exit code.
# Also registers ${_name}_regenerate_expected. Build target integration_regenerate_all_expected
# depends on every such target (one command refreshes all).
function(pcb2gcode_add_integration_script_test _name _input_dir_relpath)
  cmake_parse_arguments(
    _arg
    ""
    "EXPECTED_EXIT"
    "PCB2GCODE_ARGS"
    ${ARGN})
  if(NOT DEFINED _arg_EXPECTED_EXIT)
    set(_arg_EXPECTED_EXIT 0)
  endif()
  if(_arg_EXPECTED_EXIT EQUAL 0)
    set(_integration_label integration_expect_success)
  else()
    set(_integration_label integration_expect_failure)
  endif()
  set(_base_cmd
      "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/tools/run_gerbv_example_test.py"
      "${CMAKE_SOURCE_DIR}/${_input_dir_relpath}"
      "$<TARGET_FILE:pcb2gcode>")
  set(_extra_args "")
  if(NOT _arg_EXPECTED_EXIT EQUAL 0)
    list(APPEND _extra_args "--expected-exit-code" "${_arg_EXPECTED_EXIT}")
  endif()
  foreach(_a IN LISTS _arg_PCB2GCODE_ARGS)
    list(APPEND _extra_args "--pcb2gcode-arg=${_a}")
  endforeach()
  set(_test_cmd ${_base_cmd})
  list(APPEND _test_cmd ${_extra_args})
  add_test(NAME "${_name}" COMMAND ${_test_cmd} WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  set_tests_properties(
    "${_name}"
    PROPERTIES LABELS "${_integration_label}" DISABLED "${_pcb2gcode_integration_disabled}")
  set(_regenerate_cmd ${_base_cmd})
  list(APPEND _regenerate_cmd "--regenerate-expected")
  list(APPEND _regenerate_cmd ${_extra_args})
  add_custom_target(
    ${_name}_regenerate_expected
    COMMAND ${_regenerate_cmd}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS pcb2gcode
    COMMENT "Regenerate expected/ for ${_name}")
  set_property(
    GLOBAL
    APPEND
    PROPERTY PCB2GCODE_INTEGRATION_REGENERATE_TARGETS "${_name}_regenerate_expected")
endfunction()

set_property(GLOBAL PROPERTY PCB2GCODE_INTEGRATION_REGENERATE_TARGETS "")
add_subdirectory(gerbv_example)

pcb2gcode_add_integration_script_test(
  integration_split_config_csv
  "tests/data/broken_examples/split_config"
  PCB2GCODE_ARGS "--config=millproject,millproject2")

foreach(_bad_out_subdir IN ITEMS multivibrator slots-with-drill slots-milldrill)
  string(MAKE_C_IDENTIFIER "${_bad_out_subdir}" _bad_out_id)
  pcb2gcode_add_integration_script_test(
    "integration_bad_output_dir_${_bad_out_id}"
    "tests/data/gerbv_example/${_bad_out_subdir}"
    EXPECTED_EXIT 1
    PCB2GCODE_ARGS "--output-dir=/tmp/nonexistantpath")
endforeach()

pcb2gcode_add_integration_script_test(
  integration_split_config_two_flags
  "tests/data/broken_examples/split_config"
  PCB2GCODE_ARGS "--config=millproject" "--config=millproject2")

pcb2gcode_add_integration_script_test(
  integration_split_config_no_args_exit_14
  "tests/data/broken_examples/split_config"
  EXPECTED_EXIT 14)

foreach(_bad_layer IN ITEMS front back outline drill)
  pcb2gcode_add_integration_script_test(
    "integration_multivibrator_bad_${_bad_layer}"
    "tests/data/gerbv_example/multivibrator"
    EXPECTED_EXIT 100
    PCB2GCODE_ARGS "--${_bad_layer}=non_existant_file")
endforeach()

pcb2gcode_add_integration_script_test(
  integration_broken_invalid_config
  "tests/data/broken_examples/invalid-config"
  EXPECTED_EXIT 100)

pcb2gcode_add_integration_script_test(
  integration_cli_version
  "tests/data/gerbv_example"
  PCB2GCODE_ARGS "--version")

pcb2gcode_add_integration_script_test(
  integration_cli_help
  "tests/data/gerbv_example"
  PCB2GCODE_ARGS "--help")

pcb2gcode_add_integration_script_test(
  integration_tsp_2opt_with_mill_feed_direction
  "tests/data/gerbv_example/am-test"
  EXPECTED_EXIT 100
  PCB2GCODE_ARGS "--tsp-2opt" "--mill-feed-direction=climb")

pcb2gcode_add_integration_script_test(
  integration_g64_and_tolerance
  "tests/data/gerbv_example/am-test"
  EXPECTED_EXIT 49
  PCB2GCODE_ARGS "--g64=5" "--tolerance=123")

pcb2gcode_add_integration_script_test(
  integration_negative_spinup
  "tests/data/gerbv_example/am-test"
  EXPECTED_EXIT 52
  PCB2GCODE_ARGS "--spinup-time=-5")

pcb2gcode_add_integration_script_test(
  integration_zero_mill_infeed
  "tests/data/gerbv_example/am-test"
  EXPECTED_EXIT 55
  PCB2GCODE_ARGS "--mill-infeed=0")

pcb2gcode_add_integration_script_test(
  integration_ignore_warnings_invalid_config
  "tests/data/broken_examples/invalid-config"
  PCB2GCODE_ARGS "--ignore-warnings")

pcb2gcode_add_integration_script_test(
  integration_provided_config_am_test
  "tests/data/gerbv_example/am-test"
  PCB2GCODE_ARGS "--config=millproject")

pcb2gcode_add_integration_script_test(
  integration_missing_config_am_test
  "tests/data/gerbv_example/am-test"
  EXPECTED_EXIT 100
  PCB2GCODE_ARGS "--config=millproject_file_does_not_exist")

pcb2gcode_add_integration_script_test(
  integration_invalid_mill_feed_direction
  "tests/data/gerbv_example"
  EXPECTED_EXIT 101
  PCB2GCODE_ARGS "--mill-feed-direction=invalid_value")

pcb2gcode_add_integration_script_test(
  integration_zchange_below_zdrill
  "tests/data/gerbv_example/multivibrator-zchange-absolute"
  EXPECTED_EXIT 19
  PCB2GCODE_ARGS "--zchange-absolute=false")

get_property(
  _pcb2gcode_integration_regenerate_targets
  GLOBAL
  PROPERTY PCB2GCODE_INTEGRATION_REGENERATE_TARGETS)
if(_pcb2gcode_integration_regenerate_targets)
  add_custom_target(
    integration_regenerate_all_expected
    DEPENDS ${_pcb2gcode_integration_regenerate_targets}
    COMMENT "Regenerate expected/ for all integration script tests")
endif()
