# src/api/Makefile.am

# additional include paths necessary to compile the library
AM_CFLAGS = -I$(top_srcdir)/linux/include -I$(top_srcdir)/packages -D_GNU_SOURCE -Wall @AM_CFLAGS@

###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################

# the library names to build (note we are building shared libs)
lib_LTLIBRARIES = libtiipc.la

# where to install the headers on the system
libtiipc_ladir = $(includedir)

# the list of header files that belong to the library (to be installed later)
libtiipc_la_HEADERS =    \
                        $(top_srcdir)/linux/include/Std.h \
                        $(top_srcdir)/linux/include/SysLink.h

# the sources to add to the library and to add to the source distribution
libtiipc_la_SOURCES =    \
                        $(libtiipc_la_HEADERS) \
                        $(top_srcdir)/linux/include/_MultiProc.h \
                        $(top_srcdir)/linux/include/_MessageQ.h \
                        $(top_srcdir)/linux/include/_NameServer.h \
                        $(top_srcdir)/linux/include/ladclient.h \
                        $(top_srcdir)/linux/include/_lad.h \
                        $(top_srcdir)/linux/include/SocketFxns.h \
                        $(top_srcdir)/linux/include/net/rpmsg.h \
                        $(top_srcdir)/packages/ti/ipc/NameServer.h \
                        $(top_srcdir)/packages/ti/ipc/MultiProc.h \
                        $(top_srcdir)/packages/ti/ipc/MessageQ.h \
                        MessageQ.c \
                        NameServer.c \
                        SysLink.c

# Add version info to the shared library
libtiipc_la_LDFLAGS = -version-info 1:0:0
						
###############################################################################
