#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([trik-media-sensors], [0.0.2-trunk])
AM_INIT_AUTOMAKE([foreign color-tests -Wall -Werror])
AC_CONFIG_SRCDIR([object_sensor/src
		  line_sensor/src])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# We want more functions
AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CXX
CXXFLAGS+=" -std=c++0x"

# Checks for libraries.
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG


# Checks for header files.
AC_CHECK_HEADERS([unistd.h assert.h errno.h getopt.h inttypes.h sysexits.h time.h],,[AC_MSG_ERROR([Mandatory system headers are missing])])
AC_CHECK_HEADERS([sys/ioctl.h sys/mman.h sys/select.h sys/types.h sys/stat.h],,[AC_MSG_ERROR([Mandatory system headers are missing])])
AC_CHECK_HEADERS([sys/socket.h netdb.h],,[AC_MSG_ERROR([Mandatory network headers are missing])])
AC_CHECK_HEADERS([fcntl.h termios.h],,[AC_MSG_ERROR([Mandatory terminal headers are missing])])
AC_CHECK_HEADERS([linux/videodev2.h linux/fb.h],,[AC_MSG_ERROR([Mandatory framebuffer headers are missing])])
AC_CHECK_HEADERS([linux/i2c-dev.h linux/input.h],,[AC_MSG_ERROR([Mandatory linux I/O headers are missing])])


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_ASSERT
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_LIB([pthread], [pthread_create],,[AC_MSG_ERROR([libpthread is mandatory])])
AC_CHECK_LIB([v4l2], [v4l2_open],,[AC_MSG_ERROR([libv4l2 is mandatory])])

# Check for C++0x support features
AC_LANG(C++)


# Check for pkgconfig dependencies and setup flags
PKGCONFIG_REQUIRES="libcodecengine-client"
AC_SUBST([PKGCONFIG_REQUIRES])

PKG_CHECK_MODULES([PKGCONFIG], [${PKGCONFIG_REQUIRES}])
CPPFLAGS+=" ${PKGCONFIG_CFLAGS}"
LIBS+=" ${PKGCONFIG_LIBS}"


AC_CONFIG_FILES([Makefile
		 object_sensor/Makefile
		 object_sensor/build/Makefile
		 line_sensor/Makefile
                 line_sensor/build/Makefile])

AC_OUTPUT

