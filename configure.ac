#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([pcb2gcode], [1.1.0], [pbirnzain@users.sourceforge.net])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([m4])

AM_INIT_AUTOMAKE([pcb2gcode],  [1.1.0])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

export LC_NUMERIC="POSIX"

# Checks for libraries.
BOOST_REQUIRE([1.39.0])
BOOST_PROGRAM_OPTIONS
BOOST_SMART_PTR
BOOST_FOREACH
BOOST_TUPLE

PKG_CHECK_MODULES([glibmm], [glibmm-2.4 >= 2.8])
PKG_CHECK_MODULES([gdkmm], [gdkmm-2.4 >= 2.8])

PKG_CHECK_MODULES([gerbv], [libgerbv >= 2.1.0])

AC_SUBST(gerbv_LIBS)
AC_SUBST(gerbv_CFLAGS)


# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT(man/Makefile)
