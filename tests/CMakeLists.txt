find_package(Boost 1.60 CONFIG REQUIRED COMPONENTS unit_test_framework)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
pkg_check_modules(GLIBMM REQUIRED IMPORTED_TARGET glibmm-2.4>=2.8)
pkg_check_modules(GDKMM REQUIRED IMPORTED_TARGET gdkmm-2.4>=2.8)
pkg_check_modules(RSVG REQUIRED IMPORTED_TARGET librsvg-2.0>=2.0)
if(RSVG_VERSION VERSION_GREATER_EQUAL "2.46")
  set(HAVE_RSVG_HANDLE_RENDER_DOCUMENT 1 PARENT_SCOPE)
endif()
if(RSVG_VERSION VERSION_GREATER_EQUAL "2.52")
  set(HAVE_RSVG_HANDLE_GET_INTRINSIC_SIZE_IN_PIXELS 1 PARENT_SCOPE)
endif()

# Integration tests require Boost 1.83 and GEOS 3.13.1 for reproducible results.
set(PCB2GCODE_RUN_INTEGRATION_TESTS OFF)
if(PCB2GCODE_HAS_GEOS
   AND PCB2GCODE_GEOS_VERSION VERSION_EQUAL "3.13.1"
   AND Boost_VERSION VERSION_EQUAL "1.83")
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

# Integration script tests live in tests/data/CMakeLists.txt (and gerbv_example/).
# They stay registered but are DISABLED unless PCB2GCODE_RUN_INTEGRATION_TESTS is ON
# (see above). CTest labels: integration_expect_success vs integration_expect_failure.
# Regenerate expected/ with tools/run_gerbv_example_test.py --overwrite-expected.
if(NOT PCB2GCODE_RUN_INTEGRATION_TESTS)
  set(_geos_display "${PCB2GCODE_GEOS_VERSION}")
  if(NOT _geos_display)
    set(_geos_display "none")
  endif()
  message(
    STATUS
    "Skipping integration tests (require Boost 1.83 and GEOS 3.13.1, have Boost ${Boost_VERSION} and GEOS ${_geos_display})")
endif()

if(PCB2GCODE_ENABLE_CODE_COVERAGE)
  include(ProcessorCount)
  ProcessorCount(PCB2GCODE_CTEST_JOBS_VALUE)
  if(PCB2GCODE_CTEST_JOBS_VALUE EQUAL 0)
    set(PCB2GCODE_CTEST_JOBS_VALUE 1)
  else()
    math(EXPR PCB2GCODE_CTEST_JOBS_VALUE "${PCB2GCODE_CTEST_JOBS_VALUE} * 4")
  endif()
  get_property(PCB2GCODE_TEST_TARGETS GLOBAL PROPERTY PCB2GCODE_TEST_TARGETS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    setup_target_for_coverage_lcov(
      NAME lcov
      EXECUTABLE "${CMAKE_CTEST_COMMAND}"
      EXECUTABLE_ARGS --output-on-failure -j ${PCB2GCODE_CTEST_JOBS_VALUE}
      DEPENDENCIES pcb2gcode ${PCB2GCODE_TEST_TARGETS}
      INCLUDE "src/*")
  else()
    setup_target_for_coverage_lcov(
      NAME lcov
      EXECUTABLE "${CMAKE_CTEST_COMMAND}"
      EXECUTABLE_ARGS --output-on-failure -j ${PCB2GCODE_CTEST_JOBS_VALUE}
      DEPENDENCIES pcb2gcode ${PCB2GCODE_TEST_TARGETS}
      LCOV_ARGS --ignore-errors mismatch,inconsistent --rc branch_coverage=1 --rc function_coverage=1 --no-external
      EXCLUDE "tests/*")
  endif()
endif()
