#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([pcb2gcode], [2.4.0],
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
AX_CXX_COMPILE_STDCXX_14

# As reported here https://svn.boost.org/trac/boost/ticket/9240,
# Boost requires -fext-numeric-literals when built with C++11
AX_CHECK_COMPILE_FLAG([-fext-numeric-literals],
                      [CPPFLAGS="$CPPFLAGS -fext-numeric-literals"])

# Enable warnings
AX_CXXFLAGS_WARN_ALL

# Useful for measuring the coverage of the unit tests
AX_CODE_COVERAGE

AX_VALGRIND_DFLT([sgcheck], [off])
AX_VALGRIND_DFLT([helgrind], [off])
AX_VALGRIND_DFLT([drd], [off])
AX_VALGRIND_CHECK

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

# 2.46 for rsvg_handle_render_document
PKG_CHECK_MODULES(RSVG_HANDLE_RENDER_DOCUMENT, librsvg-2.0 >= 2.46,
  [AC_DEFINE(HAVE_RSVG_HANDLE_RENDER_DOCUMENT,1,[Define to 1 if your librsvg has rsvg_handle_render_document().])],
  [:])

# 2.52 for rsvg_handle_get_intrinsic_size_in_pixels
PKG_CHECK_MODULES(RSVG_HANDLE_GET_INTRINSIC_SIZE_IN_PIXELS, librsvg-2.0 >= 2.52,
  [AC_DEFINE(HAVE_RSVG_HANDLE_GET_INTRINSIC_SIZE_IN_PIXELS,1,[Define to 1 if your librsvg has rsvg_handle_get_intrinsic_size_in_pixels().])],
  [:])

# Optional GEOS, as a faster and more reliable replacement for Boost geometry.
GEOS_INIT([3.8.1])

AS_IF([test "x$HAVE_GEOS" = "xyes"],
      [AC_MSG_NOTICE([Found geos, is it geos++?])],
      [AC_MSG_NOTICE([Didn't find geos.])])

AC_LANG_CPLUSPLUS
if test "x$HAVE_GEOS" = "xyes"
then
  saved_ldflags="${LDFLAGS}"
  saved_cppflags="${CPPFLAGS}"
  LDFLAGS="${LDFLAGS} ${GEOS_CC_LIBS}"
  CPPFLAGS="${CPPFLAGS} ${GEOS_CFLAGS}"
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <geos/geom/Geometry.h>], [geos::geom::geosversion])],
    [HAVE_GEOS="yes"],
    [HAVE_GEOS="no"] [GEOS_CC_LIBS=""] [GEOS_CFLAGS=""])
  LDFLAGS="${saved_ldflags}"
  CPPFLAGS="${saved_cppflags}"
fi

AS_IF([test "x$HAVE_GEOS" = "xyes"],
      [AC_MSG_NOTICE([Found geos++, we'll use it instead of some of the boost geometry for greater accuracy and speed.])],
      [AC_MSG_NOTICE([Didn't find geos++, we'll use boost geometry for all geometry functions.  If you see inaccuracies or you want faster run times, try geos.])])
AS_IF([test "x$HAVE_GEOS" = "xyes"],
      [AC_SUBST(GEOS_EXTRA, ["-DUSE_UNSTABLE_GEOS_CPP_API -DGEOS_VERSION=\\\"\$(GEOS_VERSION)\\\""])])
AS_IF([test "x$HAVE_GEOS" = "xyes"],
      [AC_SUBST(GEOS_CFLAGS_SYSTEM, ['$(subst -I,-I,$(GEOS_CFLAGS))'])])


AC_SUBST([gdkmm_CFLAGS_SYSTEM], ['$(subst -I,-I,$(gdkmm_CFLAGS))'])

AC_SUBST(gerbv_LIBS)
AC_SUBST(gerbv_CFLAGS)
AC_ARG_VAR([pcb2gcode_CPPFLAGS_EXTRA], [Additional flags when compiling pcb2gcode])
AC_ARG_VAR([pcb2gcode_LDFLAGS_EXTRA], [Additional flags when linking pcb2gcode])

# GERBV_INTERPOLATION_LINEARx10 isn't available in older gerbv.h's, but
# we want to use the new symbol
CPPFLAGS+=" ${gerbv_CFLAGS}"
AX_C_COMPILE_VALUE(GERBV_INTERPOLATION_LINEARx10,NEW_LINEARX10,[#include "gerbv.h"])

# Ignore GTK 2.0 warnings for linux and macos and windows
AC_SUBST([gerbv_CFLAGS_SYSTEM1], ['$(subst -I/home/runner/.local/include,-isystem /home/runner/.local/include,$(gerbv_CFLAGS))'])
AC_SUBST([gerbv_CFLAGS_SYSTEM2], ['$(subst -I/usr/local/Cellar/gtk+/2.24.33/include,-I/usr/local/Cellar/gtk+/2.24.33/include,$(gerbv_CFLAGS_SYSTEM1))'])
AC_SUBST([gerbv_CFLAGS_SYSTEM3], ['$(subst -I/usr/include/gtk-2.0,-isystem /usr/include/gtk-2.0,$(gerbv_CFLAGS_SYSTEM2))'])
AC_SUBST([gerbv_CFLAGS_SYSTEM], ['$(subst -ID:/a/pcb2gcode/.local/include,-isystem D:/a/pcb2gcode/.local/include,$(gerbv_CFLAGS_SYSTEM3))'])

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT(man/Makefile)
