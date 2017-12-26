SUBDIRS = man

bin_PROGRAMS = pcb2gcode

pcb2gcode_SOURCES = \
    autoleveller.hpp \
    autoleveller.cpp \
    board.hpp \
    board.cpp \
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
    unique_codes.hpp \
    voronoi.hpp \
    voronoi.cpp \
    voronoi_visual_utils.hpp \
    config.h \
    main.cpp

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(BOOST_CPPFLAGS) $(glibmm_CFLAGS) $(gdkmm_CFLAGS) $(gerbv_CFLAGS)
AM_LDFLAGS = $(BOOST_PROGRAM_OPTIONS_LDFLAGS)
LIBS = $(glibmm_LIBS) $(gdkmm_LIBS) $(gerbv_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS)

EXTRA_DIST = millproject

check_PROGRAMS = eulerian_paths_tests segmentize_tests
eulerian_paths_tests_SOURCES = eulerian_paths_tests.cpp
segmentize_tests_SOURCES = segmentize_tests.cpp
TESTS = eulerian_paths_tests segmentize_tests
