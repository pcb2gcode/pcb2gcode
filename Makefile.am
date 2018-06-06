SUBDIRS = man

bin_PROGRAMS = pcb2gcode

pcb2gcode_SOURCES = \
    autoleveller.hpp \
    autoleveller.cpp \
    available_drills.hpp \
    board.hpp \
    board.cpp \
    bg_helpers.hpp \
    common.hpp \
    common.cpp \
    core.hpp \
    core.cpp \
    drill.hpp \
    drill.cpp \
    eulerian_paths.hpp \
    exporter.hpp \
    Fixed.hpp \
    geometry.hpp \
    gerberimporter.hpp \
    gerberimporter.cpp \
    importer.hpp \
    layer.hpp \
    layer.cpp \
    mill.hpp \
    ngc_exporter.hpp \
    ngc_exporter.cpp \
    segmentize.hpp \
    surface.hpp \
    surface.cpp \
    surface_vectorial.hpp \
    surface_vectorial.cpp \
    tile.hpp \
    tile.cpp \
    tsp_solver.hpp \
    options.hpp \
    options.cpp \
    outline_bridges.hpp \
    outline_bridges.cpp \
    units.hpp \
    unique_codes.hpp \
    voronoi.hpp \
    voronoi.cpp \
    voronoi_visual_utils.hpp \
    config.h \
    main.cpp

ACLOCAL_AMFLAGS = -I m4

@CODE_COVERAGE_RULES@

AM_CPPFLAGS = $(BOOST_CPPFLAGS) $(glibmm_CFLAGS) $(gdkmm_CFLAGS) $(gerbv_CFLAGS) $(CODE_COVERAGE_CPPFLAGS)
AM_CXXFLAGS = $(CODE_COVERAGE_CXXFLAGS)
AM_LDFLAGS = $(BOOST_PROGRAM_OPTIONS_LDFLAGS)
LIBS = $(glibmm_LIBS) $(gdkmm_LIBS) $(gerbv_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(CODE_COVERAGE_LIBS)

EXTRA_DIST = millproject

check_PROGRAMS = voronoi_tests eulerian_paths_tests segmentize_tests tsp_solver_tests units_tests \
                 available_drills_tests
voronoi_tests_SOURCES = voronoi.hpp voronoi.cpp voronoi_tests.cpp
eulerian_paths_tests_SOURCES = eulerian_paths_tests.cpp
segmentize_tests_SOURCES = segmentize_tests.cpp
tsp_solver_tests_SOURCES = tsp_solver_tests.cpp tsp_solver.hpp
units_tests_SOURCES = units_tests.cpp units.hpp
available_drills_tests_SOURCES = units_tests.cpp units.hpp

TESTS = $(check_PROGRAMS)
