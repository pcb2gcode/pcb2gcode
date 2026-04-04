# For each subdirectory with an expected/ tree, run pcb2gcode and compare to expected/.
# Requires the same Boost/GEOS versions as tools/integration_tests.py for stable outputs.

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
  pcb2gcode_add_integration_script_test("gerbv_example_${_test_id}" "${_name}")
endforeach()
