# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)

# Initilize autoconf
AC_INIT(TI IPC, 3.0.0, ) 

# Initilize automake
AM_INIT_AUTOMAKE

# Initalize libtool but disable fortran check
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
AC_PROG_LIBTOOL(gcc)

# Check common src files to ensure integrity of the package
AC_CONFIG_SRCDIR([packages/ti/ipc/MessageQ.h])

# Checks for programs.
AC_PROG_CC(gcc)
AC_PROG_INSTALL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h unistd.h])

# Export CFLAGS and LDFLAGS to Makefile(s)
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

# Add config variables/options and check them
AC_ARG_VAR(PLATFORM, Platform to build. Options are: 'omapl138' 'omap4330' 'tci6614' and 'tci6638'. If not defined all platforms will be built.)
AC_ARG_VAR(CMEM_INSTALL_DIR, Installation path directory to the CMEM libraries)

# Test platform variable for setting
AS_IF([test "x$PLATFORM" = "xomapl138"],
  [AC_MSG_NOTICE([PLATFORM is set to ${PLATFORM}])],
  [AS_IF([test "x$PLATFORM" = "xomap4430"],
  [AC_MSG_NOTICE([PLATFORM is set to ${PLATFORM}])],
  [AS_IF([test "x$PLATFORM" = "xtci6614"],
  [AC_MSG_NOTICE([PLATFORM is set to ${PLATFORM}])],
  [AS_IF([test "x$PLATFORM" = "xtci6638"],
  [AC_MSG_NOTICE([PLATFORM is set to ${PLATFORM}])],
  [AS_IF([test "x$PLATFORM" = "x"],
  [AC_MSG_NOTICE([PLATFORM is not set. All supported platforms will be built ....])],
  [AC_MSG_ERROR([PLATFORM is set to "${PLATFORM}": run ./configure --help for available PLATFORM options])])])])])])

# Export optional user variable to Makefile(s)
AC_SUBST([PLATFORM])
AC_SUBST([CMEM_INSTALL_DIR])

# Define specific variables to be used in Makefile.am
AM_CONDITIONAL([OMAPL138], [test "x$PLATFORM" = "xomapl138"]) 
AM_CONDITIONAL([OMAP4430], [test "x$PLATFORM" = "xomap4430"]) 
AM_CONDITIONAL([TCI6614], [test "x$PLATFORM" = "xtci6614"]) 
AM_CONDITIONAL([TCI6638], [test "x$PLATFORM" = "xtci6638"]) 

AM_CONDITIONAL([CMEM], [test "x$CMEM_INSTALL_DIR" != "x"]) 

# Generate Makefile(s)
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([linux/etc/Makefile])
AC_CONFIG_FILES([linux/src/utils/Makefile])
AC_CONFIG_FILES([linux/src/api/Makefile])
AC_CONFIG_FILES([linux/src/family/Makefile])
AC_CONFIG_FILES([linux/src/daemon/Makefile])
AC_CONFIG_FILES([linux/src/tests/Makefile])
AC_OUTPUT

echo \
"-----------------------------------------------------------

Thank you for installing the Linux ${PACKAGE_NAME} package

Installation directory prefix: '${prefix}'.
Compilation command: '${CC} ${CFLAGS} ${CPPFLAGS}'

You have configured to build for the '${PLATFORM}' platform

Your CMEM installation is at:
    '${CMEM_INSTALL_DIR}'

Now type 'make; sudo make install' to generate the program
and install it to your system.

------------------------------------------------------------"
