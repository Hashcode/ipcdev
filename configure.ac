#
#   Copyright (c) 2013, Texas Instruments Incorporated
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#   *  Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#
#   *  Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#
#   *  Neither the name of Texas Instruments Incorporated nor the names of
#      its contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
#   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
#   EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ======== configure.ac ========
#

# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)

# Initilize autoconf
AC_INIT(TI IPC, 3.0.0, ) 

# Check common src files to ensure integrity of the package
AC_CONFIG_SRCDIR([packages/ti/ipc/MessageQ.h])

# Set auxiliary build tools directory
AC_CONFIG_AUX_DIR([linux/build-aux])

# Initilize automake
AM_INIT_AUTOMAKE([foreign])

# Disabling maintainer mode to prevent rebuild of configure script
AM_MAINTAINER_MODE([disable])

# Initalize libtool but disable fortran check
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
AC_PROG_LIBTOOL(gcc)

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
AC_ARG_VAR(PLATFORM, Platform to build. Options are: 'omapl138' 'omap54xx_smp' 'tci6614' and 'tci6638'. If not defined all platforms will be built.)
AC_ARG_VAR(CMEM_INSTALL_DIR, Installation path directory to the CMEM libraries)
AC_ARG_VAR(KERNEL_INSTALL_DIR, Installation path to the Linux kernel.)

# Test platform variable for setting
AS_IF([test "x$PLATFORM" = "xomapl138"],
  [AC_MSG_NOTICE([PLATFORM is set to ${PLATFORM}])],
  [AS_IF([test "x$PLATFORM" = "xtci6614"],
  [AC_MSG_NOTICE([PLATFORM is set to ${PLATFORM}])],
  [AS_IF([test "x$PLATFORM" = "xtci6638"],
  [AC_MSG_NOTICE([PLATFORM is set to ${PLATFORM}])],
  [AS_IF([test "x$PLATFORM" = "xomap54xx_smp"],
  [AC_MSG_NOTICE([PLATFORM is set to ${PLATFORM}])],
  [AS_IF([test "x$PLATFORM" = "x"],
  [AC_MSG_NOTICE([PLATFORM is not set. All supported platforms will be built ....])],
  [AC_MSG_ERROR([PLATFORM is set to "${PLATFORM}": run ./configure --help for available PLATFORM options])])])])])])

# Test platform for omap5 variable and require a KERNEL path
AS_IF([test "x$PLATFORM" = "xomap54xx_smp"],
   [AS_IF([test "x$KERNEL_INSTALL_DIR" = "x"],
   [AC_MSG_ERROR([Kernel path for ${PLATFORM} is not set])])])

# Ignore KERNEL path when no platform is set
AS_IF([test "x$PLATFORM" = "x"], [KERNEL_INSTALL_DIR=]) 

# Export optional user variable to Makefile(s)
AC_SUBST([PLATFORM])
AC_SUBST([CMEM_INSTALL_DIR])
AC_SUBST([KERNEL_INSTALL_DIR])

# Define specific variables to be used in Makefile.am
AM_CONDITIONAL([OMAPL138], [test "x$PLATFORM" = "xomapl138"]) 
AM_CONDITIONAL([TCI6614], [test "x$PLATFORM" = "xtci6614"]) 
AM_CONDITIONAL([TCI6638], [test "x$PLATFORM" = "xtci6638"]) 
AM_CONDITIONAL([OMAP54XX_SMP], [test "x$PLATFORM" = "xomap54xx_smp"])
AM_CONDITIONAL([CMEM], [test "x$CMEM_INSTALL_DIR" != "x"]) 
AM_CONDITIONAL([KDIR], [test "x$KERNEL_INSTALL_DIR" != "x"]) 

# Generate Makefile(s)
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([linux/etc/Makefile])
AC_CONFIG_FILES([linux/src/utils/Makefile])
AC_CONFIG_FILES([linux/src/api/Makefile])
AC_CONFIG_FILES([linux/src/family/Makefile])
AC_CONFIG_FILES([linux/src/mm/Makefile])
AC_CONFIG_FILES([linux/src/daemon/Makefile])
AC_CONFIG_FILES([linux/src/tests/Makefile])
AC_OUTPUT

echo \
"-----------------------------------------------------------

Thank you for installing the Linux ${PACKAGE_NAME} package

Installation directory prefix: '${prefix}'.
Compilation command: '${CC} ${CFLAGS} ${CPPFLAGS}'

You have configured to build for the '${PLATFORM}' platform

Your KERNEL installation directory is set to:
    '${KERNEL_INSTALL_DIR}'

Your CMEM installation directory is set to:
    '${CMEM_INSTALL_DIR}'

Now type 'make; sudo make install' to generate the program
and install it to your system.

------------------------------------------------------------"
