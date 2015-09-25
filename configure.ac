AC_PREREQ([2.67])
AC_INIT([daliserver], [0.2], [onitake@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([src/daliserver.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Include autoconf macros

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR

# Test and apply compiler flags
AX_CFLAGS_WARN_ALL
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" = "no"; then
	AC_MSG_WARN([ISO C99 support not available, compilation may fail])
fi

# Custom features
AM_MAINTAINER_MODE([enable])
AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug], [enable debug messages (default is no)])], [
	AC_DEFINE(DEBUG, 1, [Enable debug messages])
])
AC_ARG_ENABLE([threads], [AS_HELP_STRING([--enable-threads], [enable thread support (unused, default is no)])], [
	AC_DEFINE(THREADS, 1, [Enable thread support])
])

# Checks for POSIX thread support
if test "$enable_threads" = "yes"; then
	ACX_PTHREAD
	CC="$PTHREAD_CC"
	CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
	LIBS="$PTHREAD_LIBS $LIBS"
fi

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h stdint.h stdlib.h string.h sys/param.h sys/socket.h unistd.h syslog.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset socket strerror getopt strtol fdopen lockf umask], [], [
	AC_MSG_FAILURE([$ac_func is required])
])
# Optional
AC_CHECK_FUNCS([localtime_r vsyslog])

# Checks for libusb
PKG_CHECK_MODULES([LIBUSB10], [libusb-1.0 >= 1.0.8], [], [
	AC_MSG_FAILURE([libusb-1.0 is required])
])

AC_CONFIG_FILES([Makefile src/Makefile lib/Makefile doc/Makefile test/Makefile perl/Makefile])
AC_OUTPUT

