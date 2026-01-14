SUBDIRS = man

bin_PROGRAMS = pcb2gcode wkt_to_svg

pcb2gcode_SOURCES = \
    autoleveller.hpp \
    autoleveller.cpp \
    available_drills.hpp \
    backtrack.hpp \
    backtrack.cpp \
    board.hpp \
    board.cpp \
    bg_helpers.hpp \
    bg_helpers.cpp \
    bg_operators.hpp \
    bg_operators.cpp \
    common.hpp \
    common.cpp \
    consistent_rand.hpp \
    consistent_rand.cpp \
    drill.hpp \
    drill.cpp \
    eulerian_paths.hpp \
    eulerian_paths.cpp \
    flatten.hpp \
    geos_helpers.hpp \
    geos_helpers.cpp \
    geometry.hpp \
    geometry_int.hpp \
    gerberimporter.hpp \
    gerberimporter.cpp \
    importer.hpp \
    layer.hpp \
    layer.cpp \
    merge_near_points.hpp \
    merge_near_points.cpp \
    mill.hpp \
    ngc_exporter.hpp \
    ngc_exporter.cpp \
    path_finding.hpp \
    path_finding.cpp \
    segment_tree.hpp \
    segment_tree.cpp \
    segmentize.hpp \
    segmentize.cpp \
    surface_vectorial.hpp \
    surface_vectorial.cpp \
    tile.hpp \
    tile.cpp \
    trim_paths.hpp \
    trim_paths.cpp \
    tsp_solver.hpp \
    options.hpp \
    options.cpp \
    outline_bridges.hpp \
    outline_bridges.cpp \
    svg_writer.hpp \
    svg_writer.cpp \
    units.hpp \
    unique_codes.hpp \
    voronoi.hpp \
    voronoi.cpp \
    voronoi_visual_utils.hpp \
    config.h \
    main.cpp

wkt_to_svg_SOURCES = \
    wkt_to_svg.cpp

ACLOCAL_AMFLAGS = -I m4

@CODE_COVERAGE_RULES@

GIT_VERSION = `git describe --dirty --always --tags`
GERBV_VERSION = `pkg-config --modversion libgerbv`

AM_CPPFLAGS = $(BOOST_CPPFLAGS) $(gerbv_CFLAGS_SYSTEM) $(CODE_COVERAGE_CPPFLAGS) -DGIT_VERSION=\"$(GIT_VERSION)\" -Wall -Wpedantic -Wextra $(pcb2gcode_CPPFLAGS_EXTRA) $(GEOS_CFLAGS_SYSTEM) $(GEOS_EXTRA)
AM_CXXFLAGS = $(CODE_COVERAGE_CXXFLAGS) -DGIT_VERSION=\"$(GIT_VERSION)\" -DGERBV_VERSION=\"$(GERBV_VERSION)\"
AM_LDFLAGS = $(BOOST_PROGRAM_OPTIONS_LDFLAGS) $(pcb2gcode_LDFLAGS_EXTRA)
LIBS = $(gerbv_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(CODE_COVERAGE_LIBS) $(GEOS_CC_LIBS)

EXTRA_DIST = millproject

check_PROGRAMS = voronoi_tests eulerian_paths_tests segmentize_tests tsp_solver_tests units_tests \
                 available_drills_tests gerberimporter_tests options_tests path_finding_tests \
                 autoleveller_tests common_tests backtrack_tests trim_paths_tests outline_bridges_tests \
                 geos_helpers_tests disjoint_set_tests segment_tree_tests


voronoi_tests_SOURCES = voronoi.hpp voronoi.cpp voronoi_tests.cpp boost_unit_test.cpp consistent_rand.hpp consistent_rand.cpp
eulerian_paths_tests_SOURCES = eulerian_paths_tests.cpp eulerian_paths.hpp geometry_int.hpp boost_unit_test.cpp  bg_operators.hpp bg_operators.cpp bg_helpers.hpp bg_helpers.cpp eulerian_paths.cpp segmentize.cpp merge_near_points.cpp geos_helpers.hpp geos_helpers.cpp
segmentize_tests_SOURCES = segmentize_tests.cpp segmentize.cpp segmentize.hpp merge_near_points.cpp merge_near_points.hpp boost_unit_test.cpp bg_helpers.hpp bg_helpers.cpp eulerian_paths.cpp bg_operators.hpp bg_operators.cpp geos_helpers.hpp geos_helpers.cpp
path_finding_tests_SOURCES = path_finding_tests.cpp path_finding.cpp path_finding.hpp boost_unit_test.cpp bg_helpers.cpp bg_helpers.hpp eulerian_paths.cpp eulerian_paths.hpp segmentize.hpp segmentize.cpp merge_near_points.cpp merge_near_points.hpp bg_operators.hpp bg_operators.cpp geos_helpers.hpp geos_helpers.cpp options.hpp options.cpp segment_tree.cpp segment_tree.hpp
tsp_solver_tests_SOURCES = tsp_solver_tests.cpp tsp_solver.hpp boost_unit_test.cpp
units_tests_SOURCES = units_tests.cpp units.hpp boost_unit_test.cpp
available_drills_tests_SOURCES = available_drills_tests.cpp available_drills.hpp boost_unit_test.cpp
gerberimporter_tests_SOURCES = gerberimporter.hpp gerberimporter.cpp gerberimporter_tests.cpp merge_near_points.hpp merge_near_points.cpp eulerian_paths.cpp eulerian_paths.hpp segmentize.cpp segmentize.hpp boost_unit_test.cpp bg_helpers.cpp bg_helpers.hpp bg_operators.hpp bg_operators.cpp geos_helpers.hpp geos_helpers.cpp
gerberimporter_tests_LDFLAGS = $(glibmm_LIBS) $(gdkmm_LIBS) $(rsvg_LIBS) $(BOOST_PROGRAM_OPTIONS_LDFLAGS)
gerberimporter_tests_CPPFLAGS = $(AM_CPPFLAGS) $(glibmm_CFLAGS) $(gdkmm_CFLAGS) $(rsvg_CFLAGS)
options_tests_SOURCES = options_tests.cpp options.hpp options.cpp boost_unit_test.cpp
autoleveller_tests_SOURCES = autoleveller_tests.cpp autoleveller.hpp autoleveller.cpp options.cpp options.hpp boost_unit_test.cpp bg_operators.hpp bg_operators.cpp bg_helpers.hpp bg_helpers.cpp eulerian_paths.hpp eulerian_paths.cpp segmentize.hpp segmentize.cpp merge_near_points.hpp merge_near_points.cpp geos_helpers.hpp geos_helpers.cpp
common_tests_SOURCES = common.hpp common.cpp common_tests.cpp boost_unit_test.cpp
backtrack_tests_SOURCES = backtrack.hpp backtrack.cpp backtrack_tests.cpp boost_unit_test.cpp bg_helpers.hpp bg_helpers.cpp eulerian_paths.hpp eulerian_paths.cpp segmentize.hpp segmentize.cpp merge_near_points.hpp merge_near_points.cpp bg_operators.hpp bg_operators.cpp geos_helpers.hpp geos_helpers.cpp
trim_paths_tests_SOURCES = trim_paths.hpp trim_paths.cpp trim_paths_tests.cpp boost_unit_test.cpp bg_helpers.hpp bg_helpers.cpp eulerian_paths.hpp eulerian_paths.cpp segmentize.hpp segmentize.cpp merge_near_points.hpp merge_near_points.cpp bg_operators.hpp bg_operators.cpp geos_helpers.hpp geos_helpers.cpp
outline_bridges_tests_SOURCES = outline_bridges_tests.cpp outline_bridges.hpp outline_bridges.cpp bg_operators.hpp bg_operators.cpp bg_helpers.hpp bg_helpers.cpp eulerian_paths.hpp eulerian_paths.cpp segmentize.hpp segmentize.cpp boost_unit_test.cpp merge_near_points.hpp merge_near_points.cpp geos_helpers.hpp geos_helpers.cpp
geos_helpers_tests_SOURCES = geos_helpers_tests.cpp geos_helpers.cpp geos_helpers.hpp boost_unit_test.cpp bg_operators.cpp bg_helpers.cpp eulerian_paths.cpp segmentize.cpp merge_near_points.cpp
disjoint_set_tests_SOURCES = disjoint_set_tests.cpp disjoint_set.hpp boost_unit_test.cpp
segment_tree_tests_SOURCES = segment_tree_tests.cpp segment_tree.cpp boost_unit_test.cpp

TESTS = $(check_PROGRAMS)

@VALGRIND_CHECK_RULES@
VALGRIND_SUPPRESSIONS_FILES = gerberimporter_tests.supp
VALGRIND_FLAGS = --error-exitcode=127 --errors-for-leak-kinds=definite --show-leak-kinds=definite --leak-check=full -s --exit-on-first-error=yes --expensive-definedness-checks=yes

check-syntax:
	timeout 10 $(COMPILE) -o /dev/null -S ${CHK_SOURCES} || true
