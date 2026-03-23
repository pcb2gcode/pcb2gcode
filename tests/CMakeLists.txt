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

function(pcb2gcode_add_test_executable target_name)
  pcb2gcode_resolve_sources(PCB2GCODE_TEST_SOURCES ${ARGN})
  add_executable("${target_name}" ${PCB2GCODE_TEST_SOURCES})
  target_link_libraries("${target_name}" PRIVATE pcb2gcode_objects)
  pcb2gcode_apply_common("${target_name}")
  target_link_libraries("${target_name}" PRIVATE Boost::unit_test_framework)
  add_test(NAME "${target_name}" COMMAND "${target_name}")
  set_property(GLOBAL APPEND PROPERTY PCB2GCODE_TEST_TARGETS "${target_name}")
endfunction()

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

pcb2gcode_resolve_sources(
  PCB2GCODE_GERBERIMPORTER_TEST_SOURCES
  gerberimporter_tests.cpp)
add_executable(gerberimporter_tests ${PCB2GCODE_GERBERIMPORTER_TEST_SOURCES})
target_link_libraries(gerberimporter_tests PRIVATE pcb2gcode_objects)
pcb2gcode_apply_common(gerberimporter_tests)
add_test(
  NAME gerberimporter_tests
  COMMAND gerberimporter_tests -- --gerber-root tests/data/gerberimporter
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(GLOBAL APPEND PROPERTY PCB2GCODE_TEST_TARGETS "gerberimporter_tests")
target_include_directories(
  gerberimporter_tests
  SYSTEM PRIVATE ${GLIBMM_INCLUDE_DIRS} ${GDKMM_INCLUDE_DIRS} ${RSVG_INCLUDE_DIRS})
target_compile_options(
  gerberimporter_tests
  PRIVATE ${GLIBMM_CFLAGS_OTHER} ${GDKMM_CFLAGS_OTHER} ${RSVG_CFLAGS_OTHER})
target_link_libraries(
  gerberimporter_tests
  PRIVATE Boost::unit_test_framework PkgConfig::GLIBMM PkgConfig::GDKMM PkgConfig::RSVG)

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

# Integration tests require Boost 1.83 and GEOS 3.13.1 for reproducible results.
set(PCB2GCODE_RUN_INTEGRATION_TESTS OFF)
if(PCB2GCODE_HAS_GEOS
   AND PCB2GCODE_GEOS_VERSION VERSION_EQUAL "3.13.1"
   AND Boost_VERSION VERSION_EQUAL "1.83")
  set(PCB2GCODE_RUN_INTEGRATION_TESTS ON)
endif()

if(PCB2GCODE_RUN_INTEGRATION_TESTS)
  # Require integration_tests.py Python modules (e.g. termcolor); CMake fails if any are missing.
  set(PCB2GCODE_INTEGRATION_TEST_DEPS "argparse;collections;difflib;filecmp;multiprocessing;os;re;shutil;subprocess;sys;tempfile;xml.etree.ElementTree;termcolor;unittest")
  foreach(_mod IN LISTS PCB2GCODE_INTEGRATION_TEST_DEPS)
    execute_process(
      COMMAND "${Python3_EXECUTABLE}" -c "import ${_mod}"
      RESULT_VARIABLE _import_result
      OUTPUT_QUIET
      ERROR_QUIET
    )
    if(NOT _import_result EQUAL 0)
      message(FATAL_ERROR "integration_tests require Python module '${_mod}', which is not available. Install it (e.g. pip install ${_mod}).")
    endif()
  endforeach()

  add_test(
    NAME integration_tests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/integration_tests.py"
            --pcb2gcode-binary "$<TARGET_FILE:pcb2gcode>"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
else()
  set(_geos_display "${PCB2GCODE_GEOS_VERSION}")
  if(NOT _geos_display)
    set(_geos_display "none")
  endif()
  message(STATUS "Skipping integration_tests (require Boost 1.83 and GEOS 3.13.1, have Boost ${Boost_VERSION} and GEOS ${_geos_display})")
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
