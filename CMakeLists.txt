cmake_minimum_required(VERSION 3.16)

project(
  pcb2gcode
  VERSION 3.0.2
  DESCRIPTION "Isolation routing and drilling G-code generator for PCBs"
  HOMEPAGE_URL "https://github.com/pcb2gcode/pcb2gcode"
  LANGUAGES C CXX)

# Default single-config generators to RelWithDebInfo unless explicitly set.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

include(CTest)
include(GNUInstallDirs)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(PCB2GCODE_WITH_GEOS "Enable GEOS support when available" ON)
option(PCB2GCODE_ENABLE_CODE_COVERAGE "Enable code coverage instrumentation" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_program(PCB2GCODE_CCACHE_PROGRAM ccache)
if(PCB2GCODE_CCACHE_PROGRAM)
  if(NOT CMAKE_C_COMPILER_LAUNCHER)
    set(CMAKE_C_COMPILER_LAUNCHER "${PCB2GCODE_CCACHE_PROGRAM}")
  endif()
  if(NOT CMAKE_CXX_COMPILER_LAUNCHER)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${PCB2GCODE_CCACHE_PROGRAM}")
  endif()
  if(NOT CMAKE_CXX_LINKER_LAUNCHER)
    # The linker results can't be ccached but we still want to obey --cache_skip.
    set(CMAKE_CXX_LINKER_LAUNCHER "${PCB2GCODE_CCACHE_PROGRAM}")
  endif()
  message(STATUS "Using ccache: ${PCB2GCODE_CCACHE_PROGRAM}")
endif()

if(PCB2GCODE_ENABLE_CODE_COVERAGE)
  include(CodeCoverage)
  append_coverage_compiler_flags()
endif()

find_package(PkgConfig REQUIRED)
find_package(Boost 1.60 CONFIG REQUIRED COMPONENTS program_options)
pkg_check_modules(GERBV REQUIRED IMPORTED_TARGET libgerbv>=2.1.0)
find_package(Git QUIET)

set(PCB2GCODE_GIT_VERSION "unknown")
if(GIT_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" describe --dirty --always --tags
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE PCB2GCODE_GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()

set(PCB2GCODE_GERBV_VERSION "${GERBV_VERSION}")
if(PCB2GCODE_GERBV_VERSION STREQUAL "")
  set(PCB2GCODE_GERBV_VERSION "unknown")
endif()

set(PCB2GCODE_HAS_GEOS OFF)
set(PCB2GCODE_GEOS_VERSION "")
set(PCB2GCODE_GEOS_CFLAGS "")
set(PCB2GCODE_GEOS_LDFLAGS "")
if(PCB2GCODE_WITH_GEOS)
  find_program(PCB2GCODE_GEOS_CONFIG NAMES geos-config)
  if(PCB2GCODE_GEOS_CONFIG)
    set(PCB2GCODE_GEOS_CONFIG_CMD bash "${PCB2GCODE_GEOS_CONFIG}")
    execute_process(
      COMMAND ${PCB2GCODE_GEOS_CONFIG_CMD} --version
      OUTPUT_VARIABLE PCB2GCODE_GEOS_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    execute_process(
      COMMAND ${PCB2GCODE_GEOS_CONFIG_CMD} --cflags
      OUTPUT_VARIABLE PCB2GCODE_GEOS_CFLAGS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    execute_process(
      COMMAND ${PCB2GCODE_GEOS_CONFIG_CMD} --ldflags
      OUTPUT_VARIABLE PCB2GCODE_GEOS_LDFLAGS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    set(PCB2GCODE_HAS_GEOS ON)
    message(STATUS "GEOS enabled (${PCB2GCODE_GEOS_VERSION})")
  else()
    message(STATUS "GEOS not found; building without GEOS support")
  endif()
endif()

set(PCB2GCODE_PACKAGE "pcb2gcode")
set(PCB2GCODE_PACKAGE_BUGREPORT "https://github.com/pcb2gcode/pcb2gcode/issues/new")
set(PCB2GCODE_PACKAGE_NAME "pcb2gcode")
set(PCB2GCODE_PACKAGE_STRING "pcb2gcode ${PROJECT_VERSION}")
set(PCB2GCODE_PACKAGE_TARNAME "pcb2gcode")
set(PCB2GCODE_PACKAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(PCB2GCODE_PACKAGE_VERSION "${PROJECT_VERSION}")
set(PCB2GCODE_VERSION "${PROJECT_VERSION}")
set(HAVE_RSVG_HANDLE_RENDER_DOCUMENT 0)
set(HAVE_RSVG_HANDLE_GET_INTRINSIC_SIZE_IN_PIXELS 0)

if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  pkg_check_modules(GLIBMM REQUIRED IMPORTED_TARGET glibmm-2.4>=2.8)
  pkg_check_modules(GDKMM REQUIRED IMPORTED_TARGET gdkmm-2.4>=2.8)
  pkg_check_modules(RSVG REQUIRED IMPORTED_TARGET librsvg-2.0>=2.0)
  if(RSVG_VERSION VERSION_GREATER_EQUAL "2.46")
    set(HAVE_RSVG_HANDLE_RENDER_DOCUMENT 1)
  endif()
  if(RSVG_VERSION VERSION_GREATER_EQUAL "2.52")
    set(HAVE_RSVG_HANDLE_GET_INTRINSIC_SIZE_IN_PIXELS 1)
  endif()
endif()

configure_file(
  "${CMAKE_SOURCE_DIR}/src/pcb2gcode_version_generated.hpp.in"
  "${CMAKE_BINARY_DIR}/pcb2gcode_version_generated.hpp"
  @ONLY)
configure_file(
  "${CMAKE_SOURCE_DIR}/src/config.h.cmake.in"
  "${CMAKE_BINARY_DIR}/config.h"
  @ONLY)

set(PCB2GCODE_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(PCB2GCODE_TESTS_DIR "${CMAKE_SOURCE_DIR}/tests")

function(pcb2gcode_resolve_sources out_var)
  set(resolved_sources)
  foreach(source IN LISTS ARGN)
    if(IS_ABSOLUTE "${source}")
      list(APPEND resolved_sources "${source}")
    elseif(EXISTS "${PCB2GCODE_SRC_DIR}/${source}")
      list(APPEND resolved_sources "${PCB2GCODE_SRC_DIR}/${source}")
    elseif(EXISTS "${PCB2GCODE_TESTS_DIR}/${source}")
      list(APPEND resolved_sources "${PCB2GCODE_TESTS_DIR}/${source}")
    else()
      list(APPEND resolved_sources "${CMAKE_SOURCE_DIR}/${source}")
    endif()
  endforeach()
  set("${out_var}" "${resolved_sources}" PARENT_SCOPE)
endfunction()

set(PCB2GCODE_MAIN_SOURCES
    autoleveller.cpp
    backtrack.cpp
    board.cpp
    bg_helpers.cpp
    bg_operators.cpp
    common.cpp
    consistent_rand.cpp
    drill.cpp
    eulerian_paths.cpp
    geos_helpers.cpp
    gerberimporter.cpp
    layer.cpp
    main.cpp
    merge_near_points.cpp
    ngc_exporter.cpp
    options.cpp
    outline_bridges.cpp
    path_finding.cpp
    segment_tree.cpp
    segmentize.cpp
    surface_vectorial.cpp
    svg_writer.cpp
    tile.cpp
    trim_paths.cpp
    voronoi.cpp)

pcb2gcode_resolve_sources(PCB2GCODE_MAIN_SOURCES_RESOLVED ${PCB2GCODE_MAIN_SOURCES})
add_executable(pcb2gcode ${PCB2GCODE_MAIN_SOURCES_RESOLVED})
if(PCB2GCODE_HAS_GEOS)
  separate_arguments(PCB2GCODE_GEOS_CFLAGS_LIST NATIVE_COMMAND "${PCB2GCODE_GEOS_CFLAGS}")
  separate_arguments(PCB2GCODE_GEOS_LDFLAGS_LIST NATIVE_COMMAND "${PCB2GCODE_GEOS_LDFLAGS}")
endif()

function(pcb2gcode_apply_common target_name)
  #set_property(TARGET "${target_name}" PROPERTY COMPILE_WARNING_AS_ERROR ON) # TODO: Uncomment this.
  target_compile_options(
    "${target_name}"
    PRIVATE
      $<$<COMPILE_LANGUAGE:C>:-Wall>
      $<$<COMPILE_LANGUAGE:CXX>:-Wall>)
  target_include_directories(
    "${target_name}" PRIVATE "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}" "${PCB2GCODE_SRC_DIR}")
  target_link_libraries("${target_name}" PRIVATE Boost::program_options PkgConfig::GERBV)
  target_include_directories("${target_name}" SYSTEM PRIVATE ${GERBV_INCLUDE_DIRS})
  target_compile_options("${target_name}" PRIVATE ${GERBV_CFLAGS_OTHER})
  target_compile_definitions(
    "${target_name}"
    PRIVATE "GIT_VERSION=\"${PCB2GCODE_GIT_VERSION}\""
            "GERBV_VERSION=\"${PCB2GCODE_GERBV_VERSION}\""
            "PACKAGE=\"${PCB2GCODE_PACKAGE}\""
            "PACKAGE_BUGREPORT=\"${PCB2GCODE_PACKAGE_BUGREPORT}\""
            "PACKAGE_NAME=\"${PCB2GCODE_PACKAGE_NAME}\""
            "PACKAGE_STRING=\"${PCB2GCODE_PACKAGE_STRING}\""
            "PACKAGE_TARNAME=\"${PCB2GCODE_PACKAGE_TARNAME}\""
            "PACKAGE_URL=\"${PCB2GCODE_PACKAGE_URL}\""
            "PACKAGE_VERSION=\"${PCB2GCODE_PACKAGE_VERSION}\""
            "VERSION=\"${PCB2GCODE_VERSION}\"")
  if(PCB2GCODE_HAS_GEOS)
    target_compile_options("${target_name}" PRIVATE ${PCB2GCODE_GEOS_CFLAGS_LIST})
    target_link_options("${target_name}" PRIVATE ${PCB2GCODE_GEOS_LDFLAGS_LIST})
    target_link_libraries("${target_name}" PRIVATE geos)
    target_compile_definitions(
      "${target_name}"
      PRIVATE USE_UNSTABLE_GEOS_CPP_API
              "GEOS_VERSION=\"${PCB2GCODE_GEOS_VERSION}\"")
  endif()
endfunction()

pcb2gcode_apply_common(pcb2gcode)

pcb2gcode_resolve_sources(PCB2GCODE_WKT_TO_SVG_SOURCES wkt_to_svg.cpp)
add_executable(wkt_to_svg ${PCB2GCODE_WKT_TO_SVG_SOURCES})
pcb2gcode_apply_common(wkt_to_svg)

if(BUILD_TESTING)
  function(pcb2gcode_disable_test_source_coverage target_name)
    if(NOT PCB2GCODE_ENABLE_CODE_COVERAGE)
      return()
    endif()

    get_target_property(target_sources "${target_name}" SOURCES)
    foreach(source IN LISTS target_sources)
      if(source MATCHES "(/|^)([^/]+_tests\\.cpp|boost_unit_test\\.cpp)$")
        set_property(
          SOURCE "${source}"
          APPEND
          PROPERTY COMPILE_OPTIONS -fno-profile-arcs -fno-test-coverage)
      endif()
    endforeach()
  endfunction()

  function(pcb2gcode_add_test_executable target_name)
    pcb2gcode_resolve_sources(PCB2GCODE_TEST_SOURCES ${ARGN})
    add_executable("${target_name}" ${PCB2GCODE_TEST_SOURCES})
    pcb2gcode_disable_test_source_coverage("${target_name}")
    pcb2gcode_apply_common("${target_name}")
    add_test(NAME "${target_name}" COMMAND "${target_name}")
    set_property(GLOBAL APPEND PROPERTY PCB2GCODE_TEST_TARGETS "${target_name}")
  endfunction()

  pcb2gcode_add_test_executable(
    voronoi_tests
    voronoi.cpp
    voronoi_tests.cpp
    boost_unit_test.cpp
    consistent_rand.cpp)

  pcb2gcode_add_test_executable(
    eulerian_paths_tests
    eulerian_paths_tests.cpp
    boost_unit_test.cpp
    bg_operators.cpp
    bg_helpers.cpp
    eulerian_paths.cpp
    segmentize.cpp
    merge_near_points.cpp
    geos_helpers.cpp
    options.cpp)

  pcb2gcode_add_test_executable(
    segmentize_tests
    segmentize_tests.cpp
    segmentize.cpp
    merge_near_points.cpp
    boost_unit_test.cpp
    bg_helpers.cpp
    eulerian_paths.cpp
    bg_operators.cpp
    geos_helpers.cpp
    options.cpp)

  pcb2gcode_add_test_executable(
    path_finding_tests
    path_finding_tests.cpp
    path_finding.cpp
    boost_unit_test.cpp
    bg_helpers.cpp
    eulerian_paths.cpp
    segmentize.cpp
    merge_near_points.cpp
    bg_operators.cpp
    geos_helpers.cpp
    options.cpp
    segment_tree.cpp)

  pcb2gcode_add_test_executable(
    tsp_solver_tests
    tsp_solver_tests.cpp
    boost_unit_test.cpp)

  pcb2gcode_add_test_executable(
    units_tests
    units_tests.cpp
    boost_unit_test.cpp)

  pcb2gcode_add_test_executable(
    available_drills_tests
    available_drills_tests.cpp
    boost_unit_test.cpp)

  pcb2gcode_resolve_sources(
    PCB2GCODE_GERBERIMPORTER_TEST_SOURCES
    gerberimporter.cpp
    gerberimporter_tests.cpp
    merge_near_points.cpp
    eulerian_paths.cpp
    segmentize.cpp
    boost_unit_test.cpp
    bg_helpers.cpp
    bg_operators.cpp
    geos_helpers.cpp
    options.cpp)
  add_executable(gerberimporter_tests ${PCB2GCODE_GERBERIMPORTER_TEST_SOURCES})
  pcb2gcode_disable_test_source_coverage(gerberimporter_tests)
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
    PRIVATE PkgConfig::GLIBMM PkgConfig::GDKMM PkgConfig::RSVG)

  pcb2gcode_add_test_executable(
    options_tests
    options_tests.cpp
    options.cpp
    boost_unit_test.cpp)

  pcb2gcode_add_test_executable(
    autoleveller_tests
    autoleveller_tests.cpp
    autoleveller.cpp
    options.cpp
    boost_unit_test.cpp
    bg_operators.cpp
    bg_helpers.cpp
    eulerian_paths.cpp
    segmentize.cpp
    merge_near_points.cpp
    geos_helpers.cpp)

  pcb2gcode_add_test_executable(
    common_tests
    common.cpp
    common_tests.cpp
    boost_unit_test.cpp)

  pcb2gcode_add_test_executable(
    backtrack_tests
    backtrack.cpp
    backtrack_tests.cpp
    boost_unit_test.cpp
    bg_helpers.cpp
    eulerian_paths.cpp
    segmentize.cpp
    merge_near_points.cpp
    bg_operators.cpp
    geos_helpers.cpp
    options.cpp)

  pcb2gcode_add_test_executable(
    trim_paths_tests
    trim_paths.cpp
    trim_paths_tests.cpp
    boost_unit_test.cpp
    bg_helpers.cpp
    eulerian_paths.cpp
    segmentize.cpp
    merge_near_points.cpp
    bg_operators.cpp
    geos_helpers.cpp
    options.cpp)

  pcb2gcode_add_test_executable(
    outline_bridges_tests
    outline_bridges_tests.cpp
    outline_bridges.cpp
    bg_operators.cpp
    bg_helpers.cpp
    eulerian_paths.cpp
    segmentize.cpp
    boost_unit_test.cpp
    merge_near_points.cpp
    geos_helpers.cpp
    options.cpp)

  pcb2gcode_add_test_executable(
    geos_helpers_tests
    geos_helpers_tests.cpp
    geos_helpers.cpp
    boost_unit_test.cpp
    bg_operators.cpp
    bg_helpers.cpp
    eulerian_paths.cpp
    segmentize.cpp
    merge_near_points.cpp
    options.cpp)

  pcb2gcode_add_test_executable(
    disjoint_set_tests
    disjoint_set_tests.cpp
    boost_unit_test.cpp)

  pcb2gcode_add_test_executable(
    segment_tree_tests
    segment_tree_tests.cpp
    segment_tree.cpp
    boost_unit_test.cpp)

  add_test(
    NAME integration_tests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/integration/integration_tests.py"
            --pcb2gcode-binary "$<TARGET_FILE:pcb2gcode>"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  if(PCB2GCODE_ENABLE_CODE_COVERAGE)
    get_property(PCB2GCODE_TEST_TARGETS GLOBAL PROPERTY PCB2GCODE_TEST_TARGETS)
    setup_target_for_coverage_lcov(
      NAME pcb2gcode-lcov
      EXECUTABLE "${CMAKE_CTEST_COMMAND}"
      EXECUTABLE_ARGS --output-on-failure
      DEPENDENCIES pcb2gcode ${PCB2GCODE_TEST_TARGETS}
      LCOV_ARGS --ignore-errors mismatch,inconsistent --rc branch_coverage=1 --rc function_coverage=1 --no-external
      EXCLUDE "tests/*")
  endif()
endif()

install(TARGETS pcb2gcode RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_SOURCE_DIR}/man/pcb2gcode.1"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
