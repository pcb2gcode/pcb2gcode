#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
package, version, [bug-report], [tarname], [url])
AC_INIT([pcb2gcode], [2.0.0],
        [https://github.com/pcb2gcode/pcb2gcode/issues/new],
        [pcb2gcode],
        [https://github.com/pcb2gcode/pcb2gcode])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([m4])

AM_SILENT_RULES([yes])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

# Check for C++11
AX_CXX_COMPILE_STDCXX_11

# As reported here https://svn.boost.org/trac/boost/ticket/9240,
# Boost requires -fext-numeric-literals when built with C++11
AX_CHECK_COMPILE_FLAG([-fext-numeric-literals],
                      [CPPFLAGS="$CPPFLAGS -fext-numeric-literals"])

# Enable warnings
AX_CXXFLAGS_WARN_ALL

# Useful for measuring the coverage of the unit tests
AX_CODE_COVERAGE

export LC_NUMERIC="POSIX"

# Checks for libraries.
BOOST_REQUIRE([1.60])
BOOST_PROGRAM_OPTIONS
BOOST_GEOMETRY
BOOST_STRING_ALGO
BOOST_FORMAT
BOOST_OPTIONAL
BOOST_VARIANT

AC_SUBST([BOOST_CPPFLAGS_SYSTEM], ['$(subst -I,-isystem ,$(BOOST_CPPFLAGS))'])

PKG_CHECK_MODULES([glibmm], [glibmm-2.4 >= 2.8])
PKG_CHECK_MODULES([gdkmm], [gdkmm-2.4 >= 2.8])
PKG_CHECK_MODULES([gerbv], [libgerbv >= 2.1.0])
PKG_CHECK_MODULES([rsvg], [librsvg-2.0 >= 2.0])

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
