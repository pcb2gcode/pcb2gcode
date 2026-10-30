find_package(Boost 1.60 CONFIG REQUIRED COMPONENTS unit_test_framework)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
pkg_check_modules(GLIBMM REQUIRED IMPORTED_TARGET glibmm-2.4>=2.8)
pkg_check_modules(GDKMM REQUIRED IMPORTED_TARGET gdkmm-3.0)
pkg_check_modules(RSVG REQUIRED IMPORTED_TARGET librsvg-2.0>=2.0)
if(RSVG_VERSION VERSION_GREATER_EQUAL "2.46")
  set(HAVE_RSVG_HANDLE_RENDER_DOCUMENT 1 PARENT_SCOPE)
endif()
if(RSVG_VERSION VERSION_GREATER_EQUAL "2.52")
  set(HAVE_RSVG_HANDLE_GET_INTRINSIC_SIZE_IN_PIXELS 1 PARENT_SCOPE)
endif()

# Integration tests pin an exact (Boost, GEOS) pair so their expected/ outputs
# stay reproducible across CI cells and developer machines. When upgrading to a
# newer known-good GEOS release, update both PCB2GCODE_INTEGRATION_GEOS_VERSION
# below AND the geos matrix in .github/workflows/ci.yml so a matrix cell exists
# that satisfies the pin.
# g++ 15 changes floating-point results enough that those golden bytes no longer
# match. clang++ and other g++ majors still match, so the pin stays and only
# g++ 15 is excluded. Drop the exclusion once GCC 15 output is stable or
# expected/ is regenerated for it.
# Integration script tests live in tests/data/CMakeLists.txt (and gerbv_example/).
# They stay registered but are DISABLED unless PCB2GCODE_RUN_INTEGRATION_TESTS is ON.
# CTest labels: integration_expect_success vs integration_expect_failure.
# Regenerate expected/ with tools/run_gerbv_example_test.py --regenerate-expected.
set(PCB2GCODE_INTEGRATION_GEOS_VERSION "3.15.0")

# Whole 15.x line, not later majors. CMAKE_CXX_COMPILER_VERSION is "15.2.0".
set(_pcb2gcode_gxx15 OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
  set(_pcb2gcode_gxx15 ON)
endif()

set(PCB2GCODE_RUN_INTEGRATION_TESTS OFF)
if(NOT PCB2GCODE_HAS_GEOS
    OR NOT Boost_VERSION VERSION_EQUAL "1.83"
    OR NOT PCB2GCODE_GEOS_VERSION VERSION_EQUAL "${PCB2GCODE_INTEGRATION_GEOS_VERSION}")
  set(_geos_display "${PCB2GCODE_GEOS_VERSION}")
  if(NOT _geos_display)
    set(_geos_display "none")
  endif()
  message(
    WARNING
    "Integration tests disabled (require Boost 1.83 and GEOS ${PCB2GCODE_INTEGRATION_GEOS_VERSION}, have Boost ${Boost_VERSION} and GEOS ${_geos_display})")
elseif(_pcb2gcode_gxx15)
  message(
    WARNING
    "Integration tests disabled (g++ ${CMAKE_CXX_COMPILER_VERSION}; golden G-code differs on g++ 15)")
else()
  set(PCB2GCODE_RUN_INTEGRATION_TESTS ON)
endif()

function(pcb2gcode_add_test_executable target_name)
  cmake_parse_arguments(_arg "" "" "SOURCES;TEST_ARGS" ${ARGN})
  add_executable("${target_name}" ${_arg_SOURCES} ${_arg_UNPARSED_ARGUMENTS})
  target_link_libraries("${target_name}" PRIVATE pcb2gcode_objects)
  pcb2gcode_apply_common("${target_name}")
  target_link_libraries("${target_name}" PRIVATE Boost::unit_test_framework)
  target_include_directories("${target_name}" SYSTEM PRIVATE ${GLIBMM_INCLUDE_DIRS} ${GDKMM_INCLUDE_DIRS} ${RSVG_INCLUDE_DIRS})
  target_compile_options("${target_name}" PRIVATE ${GLIBMM_CFLAGS_OTHER} ${GDKMM_CFLAGS_OTHER} ${RSVG_CFLAGS_OTHER})
  target_link_libraries("${target_name}" PRIVATE PkgConfig::GLIBMM PkgConfig::GDKMM PkgConfig::RSVG)
  add_test(
    NAME "${target_name}"
    COMMAND "${target_name}" ${_arg_TEST_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  set_property(GLOBAL APPEND PROPERTY PCB2GCODE_TEST_TARGETS "${target_name}")
  set_tests_properties("${target_name}" PROPERTIES LABELS "unit_test")
endfunction()

add_subdirectory(data)

pcb2gcode_add_test_executable(
  voronoi_tests
  voronoi_tests.cpp)

pcb2gcode_add_test_executable(
  eulerian_paths_tests
  eulerian_paths_tests.cpp)

pcb2gcode_add_test_executable(
  segmentize_tests
  segmentize_tests.cpp)

pcb2gcode_add_test_executable(
  path_finding_tests
  path_finding_tests.cpp)

pcb2gcode_add_test_executable(
  tsp_solver_tests
  tsp_solver_tests.cpp)

pcb2gcode_add_test_executable(
  units_tests
  units_tests.cpp)

pcb2gcode_add_test_executable(
  available_drills_tests
  available_drills_tests.cpp)

pcb2gcode_add_test_executable(
  gerberimporter_tests
  SOURCES gerberimporter_tests.cpp
  TEST_ARGS -- --gerber-root tests/data/gerberimporter)

pcb2gcode_add_test_executable(
  options_tests
  options_tests.cpp)

pcb2gcode_add_test_executable(
  autoleveller_tests
  autoleveller_tests.cpp)

pcb2gcode_add_test_executable(
  common_tests
  common_tests.cpp)

pcb2gcode_add_test_executable(
  backtrack_tests
  backtrack_tests.cpp)

pcb2gcode_add_test_executable(
  trim_paths_tests
  trim_paths_tests.cpp)

pcb2gcode_add_test_executable(
  outline_bridges_tests
  outline_bridges_tests.cpp)

pcb2gcode_add_test_executable(
  geos_helpers_tests
  geos_helpers_tests.cpp)

pcb2gcode_add_test_executable(
  disjoint_set_tests
  disjoint_set_tests.cpp)

pcb2gcode_add_test_executable(
  segment_tree_tests
  segment_tree_tests.cpp)

pcb2gcode_add_test_executable(
  mcp_server_tests
  mcp_server_tests.cpp)
# The tools/call handler fork/execs the pcb2gcode binary; hand the tests the
# built path so they can exercise every branch (version/help/run/unknown).
target_compile_definitions(mcp_server_tests PRIVATE
  "PCB2GCODE_TEST_BINARY=\"$<TARGET_FILE:pcb2gcode>\"")
add_dependencies(mcp_server_tests pcb2gcode)

# End-to-end test: spawn the real pcb2gcode --mcp subprocess, exchange one
# initialize + tools/list request, close stdin, and require a clean exit.
add_test(
  NAME mcp_stdio_shutdown_test
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tools/run_mcp_stdin_test.py"
    "$<TARGET_FILE:pcb2gcode>"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_tests_properties(mcp_stdio_shutdown_test PROPERTIES
  LABELS "integration_expect_success"
  TIMEOUT 60)

# src/wkt_to_svg.cpp is its own executable, so ctest has to run that binary
# to cover it. One test per stdin shape (see tools/run_wkt_to_svg_test.py).
foreach(_wkt_case IN ITEMS
    empty linestring multilinestring polygon multipolygon
    ignored blank_line_stops combined)
  add_test(
    NAME "wkt_to_svg_${_wkt_case}"
    COMMAND
      "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/tools/run_wkt_to_svg_test.py"
      "$<TARGET_FILE:wkt_to_svg>"
      "${_wkt_case}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  set_tests_properties("wkt_to_svg_${_wkt_case}" PROPERTIES LABELS "unit_test")
endforeach()

if(PCB2GCODE_ENABLE_CODE_COVERAGE)
  include(ProcessorCount)
  ProcessorCount(PCB2GCODE_CTEST_JOBS_VALUE)
  if(PCB2GCODE_CTEST_JOBS_VALUE EQUAL 0)
    set(PCB2GCODE_CTEST_JOBS_VALUE 1)
  else()
    math(EXPR PCB2GCODE_CTEST_JOBS_VALUE "${PCB2GCODE_CTEST_JOBS_VALUE} * 4")
  endif()
  get_property(PCB2GCODE_TEST_TARGETS GLOBAL PROPERTY PCB2GCODE_TEST_TARGETS)
  # FetchContent unpacks nlohmann_json under the build tree. lcov --no-external
  # still keeps it, because --directory is the build dir, so those headers count
  # as internal. Drop that tree. grcov globs do not let * cross a slash, so the
  # clang pattern uses **.
  set(_pcb2gcode_coverage_excludes "tests/*")
  set(_nlohmann_cov_exclude "")
  if(nlohmann_json_SOURCE_DIR)
    list(APPEND _pcb2gcode_coverage_excludes "${nlohmann_json_SOURCE_DIR}/*")
    set(_nlohmann_cov_exclude EXCLUDE "${nlohmann_json_SOURCE_DIR}/**")
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    setup_target_for_coverage_lcov(
      NAME lcov
      EXECUTABLE "${CMAKE_CTEST_COMMAND}"
      EXECUTABLE_ARGS --output-on-failure -j ${PCB2GCODE_CTEST_JOBS_VALUE}
      DEPENDENCIES pcb2gcode wkt_to_svg ${PCB2GCODE_TEST_TARGETS}
      INCLUDE "src/*"
      ${_nlohmann_cov_exclude})
  else()
    setup_target_for_coverage_lcov(
      NAME lcov
      EXECUTABLE "${CMAKE_CTEST_COMMAND}"
      EXECUTABLE_ARGS --output-on-failure -j ${PCB2GCODE_CTEST_JOBS_VALUE}
      DEPENDENCIES pcb2gcode wkt_to_svg ${PCB2GCODE_TEST_TARGETS}
      LCOV_ARGS --ignore-errors mismatch,inconsistent --rc branch_coverage=1 --rc function_coverage=1 --no-external
      EXCLUDE ${_pcb2gcode_coverage_excludes})
  endif()
endif()
