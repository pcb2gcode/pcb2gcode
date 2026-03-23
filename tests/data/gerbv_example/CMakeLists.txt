# For each subdirectory with an expected/ tree, run pcb2gcode and compare to expected/.
# Requires the same Boost/GEOS versions as tools/integration_tests.py for stable outputs.

if(NOT PCB2GCODE_RUN_INTEGRATION_TESTS)
  return()
endif()

file(
  GLOB _gerbv_entries
  RELATIVE "${CMAKE_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/*")

foreach(_name ${_gerbv_entries})
  set(_dir "${CMAKE_SOURCE_DIR}/${_name}")
  if(NOT IS_DIRECTORY "${_dir}")
    continue()
  endif()
  if(NOT IS_DIRECTORY "${_dir}/expected")
    continue()
  endif()
  string(MAKE_C_IDENTIFIER "${_name}" _test_id)
  add_test(
    NAME "gerbv_example_${_test_id}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/run_gerbv_example_test.py" "${_dir}"
            "$<TARGET_FILE:pcb2gcode>"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  set_tests_properties("gerbv_example_${_test_id}" PROPERTIES LABELS "gerbv_example")
endforeach()
