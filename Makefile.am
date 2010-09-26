
bin_PROGRAMS = pcb2gcode

pcb2gcode_SOURCES = \
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
	config.h \
	main.cpp

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(BOOST_CPPFLAGS) $(gtkmm2_CFLAGS) $(gerbv_CFLAGS) $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_PROGRAM_OPTIONS_LDFLAGS)
LIBS = $(gtkmm2_LIBS) $(gerbv_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS)
