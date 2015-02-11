SUBDIRS = man

bin_PROGRAMS = pcb2gcode

pcb2gcode_SOURCES = \
	autoleveller.hpp \
	autoleveller.cpp \
	svg_exporter.hpp \
	svg_exporter.cpp \
	board.hpp \
	board.cpp \
	coord.hpp \
	drill.hpp \
	drill.cpp \
	exporter.hpp \
	Fixed.hpp \
	gerberimporter.hpp \
	gerberimporter.cpp \
	importer.hpp \
	layer.hpp \
	layer.cpp \
	mill.hpp \
	mill.cpp \
	ngc_exporter.hpp \
	ngc_exporter.cpp \
	surface.hpp \
	surface.cpp \
	options.hpp \
	options.cpp \
    outline_bridges.hpp \
    outline_bridges.cpp \
	config.h \
	main.cpp

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(BOOST_CPPFLAGS) $(glibmm_CFLAGS) $(gdkmm_CFLAGS) $(gerbv_CFLAGS)
AM_LDFLAGS = $(BOOST_PROGRAM_OPTIONS_LDFLAGS)
LIBS = $(glibmm_LIBS) $(gdkmm_LIBS) $(gerbv_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_REGEX_LIBS)

EXTRA_DIST = millproject
