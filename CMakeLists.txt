cmake_minimum_required(VERSION 3.16)

project(
  pcb2gcode
  VERSION 3.0.4
  DESCRIPTION "Isolation routing and drilling G-code generator for PCBs"
  HOMEPAGE_URL "https://github.com/pcb2gcode/pcb2gcode"
  LANGUAGES C CXX)

# compile_commands.json for clangd and other tools that consume the compile database.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(PCB2GCODE_WITH_GEOS "Enable GEOS support when available" ON)
option(PCB2GCODE_ENABLE_CODE_COVERAGE "Enable code coverage instrumentation" OFF)
option(PCB2GCODE_USE_CCACHE "Use ccache for compilation when available" ON)
option(
  PCB2GCODE_COMPILE_WARNING_AS_ERROR
  "Treat compiler warnings as errors (CMake COMPILE_WARNING_AS_ERROR / -Werror where supported)"
  ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
if(PCB2GCODE_USE_CCACHE)
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
endif()

if(PCB2GCODE_ENABLE_CODE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    include(ClangCodeCoverage)
  else()
    include(CodeCoverage)
  endif()
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
    mcp_server.cpp
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

if(PCB2GCODE_HAS_GEOS)
  separate_arguments(PCB2GCODE_GEOS_CFLAGS_LIST NATIVE_COMMAND "${PCB2GCODE_GEOS_CFLAGS}")
  separate_arguments(PCB2GCODE_GEOS_LDFLAGS_LIST NATIVE_COMMAND "${PCB2GCODE_GEOS_LDFLAGS}")
endif()

function(pcb2gcode_apply_common target_name)
  if(PCB2GCODE_COMPILE_WARNING_AS_ERROR)
    set_property(TARGET "${target_name}" PROPERTY COMPILE_WARNING_AS_ERROR ON)
  endif()
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
            "GERBV_VERSION=\"${PCB2GCODE_GERBV_VERSION}\"")
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

# Shared object library so pcb2gcode and tests use the same compiled objects
set(PCB2GCODE_CORE_SOURCES ${PCB2GCODE_MAIN_SOURCES})
list(FILTER PCB2GCODE_CORE_SOURCES EXCLUDE REGEX "main\\.cpp$")
pcb2gcode_resolve_sources(PCB2GCODE_CORE_SOURCES_RESOLVED ${PCB2GCODE_CORE_SOURCES})
add_library(pcb2gcode_objects OBJECT ${PCB2GCODE_CORE_SOURCES_RESOLVED})
pcb2gcode_apply_common(pcb2gcode_objects)
if(PCB2GCODE_ENABLE_CODE_COVERAGE)
  # std::launch::async makes LLVM source-based / grcov coverage vary run-to-run.
  target_compile_definitions(
    pcb2gcode_objects
    PRIVATE PCB2GCODE_COVERAGE_DISABLE_INTERNAL_MT=1)
endif()

pcb2gcode_resolve_sources(PCB2GCODE_MAIN_SOURCES_RESOLVED main.cpp)
add_executable(pcb2gcode ${PCB2GCODE_MAIN_SOURCES_RESOLVED})
target_link_libraries(pcb2gcode PRIVATE pcb2gcode_objects)
pcb2gcode_apply_common(pcb2gcode)

pcb2gcode_resolve_sources(PCB2GCODE_WKT_TO_SVG_SOURCES wkt_to_svg.cpp)
add_executable(wkt_to_svg ${PCB2GCODE_WKT_TO_SVG_SOURCES})
pcb2gcode_apply_common(wkt_to_svg)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

configure_file(
  "${CMAKE_SOURCE_DIR}/src/config.h.cmake.in"
  "${CMAKE_BINARY_DIR}/config.h"
  @ONLY)

install(TARGETS pcb2gcode RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_SOURCE_DIR}/man/pcb2gcode.1"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
