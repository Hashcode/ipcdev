# src/utils/Makefile.am

# additional include paths necessary to compile the library
AM_CFLAGS = -I$(top_srcdir)/linux/include -I$(top_srcdir)/packages -D_GNU_SOURCE -Wall @AM_CFLAGS@

###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################

# the library names to build (note we are building shared libs)
lib_LTLIBRARIES = libtiipcutils.la

# where to install the headers on the system
libtiipcutils_ladir = $(includedir)

# the list of header files that belong to the library (to be installed later)
libtiipcutils_la_HEADERS =    \
                        $(top_srcdir)/linux/include/Std.h

# the sources to add to the library and to add to the source distribution
libtiipcutils_la_SOURCES =    \
                        $(libtiipcutils_la_HEADERS) \
                        $(top_srcdir)/linux/include/ladclient.h \
                        $(top_srcdir)/linux/include/_lad.h \
                        $(top_srcdir)/linux/include/_MultiProc.h \
                        $(top_srcdir)/linux/include/net/rpmsg.h \
                        $(top_srcdir)/packages/ti/ipc/MultiProc.h \
                        LAD_client.c \
                        MultiProc.c \
                        SocketFxns.c

# Add version info to the shared library
libtiipcutils_la_LDFLAGS = -version-info 1:0:0

###############################################################################
