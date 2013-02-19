# src/family/Makefile.am

# additional include paths necessary to compile the library
AM_CFLAGS = -I$(top_srcdir)/linux/include -D_GNU_SOURCE -Wall @AM_CFLAGS@

###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################

# the library names to build (note we are building shared libs)
lib_LTLIBRARIES =

if OMAP4430
  lib_LTLIBRARIES += libtiipccfg_omap4430.la
else 
if OMAPL138
  lib_LTLIBRARIES += libtiipccfg_omapl138.la
else 
if TCI6614
  lib_LTLIBRARIES += libtiipccfg_tci6614.la
else 
if TCI6638
  lib_LTLIBRARIES += libtiipccfg_tci6638.la
else 
  lib_LTLIBRARIES += \
                libtiipccfg_omap4430.la libtiipccfg_omapl138.la \
                libtiipccfg_tci6614.la libtiipccfg_tci6638.la
endif
endif
endif
endif

common_install_headers = $(top_srcdir)/linux/include/Std.h

common_sources = $(top_srcdir)/linux/include/_MultiProc.h

# where to install the headers on the system
libtiipccfg_omap4430_ladir = $(includedir)
libtiipccfg_omapl138_ladir = $(includedir)
libtiipccfg_tci6614_ladir = $(includedir)
libtiipccfg_tci6638_ladir = $(includedir)

# the list of header files that belong to the library (to be installed later)
libtiipccfg_omap4430_la_HEADERS =	\
                        $(common_install_headers)

libtiipccfg_omapl138_la_HEADERS =	\
                        $(common_install_headers)

libtiipccfg_tci6614_la_HEADERS =	\
                        $(common_install_headers)

libtiipccfg_tci6638_la_HEADERS =	\
                        $(common_install_headers)

# the sources to add to the library and to add to the source distribution
libtiipccfg_omap4430_la_SOURCES =	\
                        $(libtiipccfg_omap4430_la_HEADERS) \
                        $(common_sources) \
                        SystemCfg_omap4430.c

libtiipccfg_omapl138_la_SOURCES =	\
                        $(libtiipccfg_omapl138_la_HEADERS) \
                        $(common_sources) \
                        SystemCfg_omapl138.c

libtiipccfg_tci6614_la_SOURCES =	\
                        $(libtiipccfg_tci6614_la_HEADERS) \
                        $(common_sources) \
                        SystemCfg_tci6614.c

libtiipccfg_tci6638_la_SOURCES =	\
                        $(libtiipccfg_tci6638_la_HEADERS) \
                        $(common_sources) \
                        SystemCfg_tci6638.c

# Add version info to the shared library
libtiipccfg_omap4430_la_LDFLAGS = -version-info 1:0:0
libtiipccfg_omapl138_la_LDFLAGS = -version-info 1:0:0
libtiipccfg_tci6614_la_LDFLAGS = -version-info 1:0:0
libtiipccfg_tci6638_la_LDFLAGS = -version-info 1:0:0

###############################################################################
