##
##  Copyright (c) 2013, Texas Instruments Incorporated
##
##  Redistribution and use in source and binary forms, with or without
##  modification, are permitted provided that the following conditions
##  are met:
##
##  *  Redistributions of source code must retain the above copyright
##     notice, this list of conditions and the following disclaimer.
##
##  *  Redistributions in binary form must reproduce the above copyright
##     notice, this list of conditions and the following disclaimer in the
##     documentation and/or other materials provided with the distribution.
##
##  *  Neither the name of Texas Instruments Incorporated nor the names of
##     its contributors may be used to endorse or promote products derived
##     from this software without specific prior written permission.
##
##  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
##  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
##  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
##  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
##  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
##  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
##  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
##  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
##  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
##  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
##  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
## ======== src/family/Makefile.am ========
##

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
