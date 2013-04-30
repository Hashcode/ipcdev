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
## ======== src/daemon/Makefile.am ========
##

# additional include paths necessary to compile the program
AM_CFLAGS = -I$(top_srcdir)/linux/include -I$(top_srcdir)/packages \
        -D_GNU_SOURCE -Wall @AM_CFLAGS@

###############################################################################
# THE PROGRAMS TO BUILD
###############################################################################

# the program to build (the names of the final binaries)
bin_PROGRAMS =

if OMAP54XX_SMP
  bin_PROGRAMS += lad_omap54xx_smp
else
if OMAPL138
  bin_PROGRAMS += lad_omapl138
else
if TCI6614
  bin_PROGRAMS += lad_tci6614
else
if TCI6638
  bin_PROGRAMS += lad_tci6638
else
  bin_PROGRAMS += lad_omap54xx_smp lad_omapl138 lad_tci6614 lad_tci6638
endif
endif
endif
endif

common_sources = \
                lad.c \
                MessageQ_daemon.c \
                MultiProc_daemon.c \
                NameServer_daemon.c \
                $(top_srcdir)/linux/include/ti/ipc/Std.h \
                $(top_srcdir)/linux/include/_NameServerRemoteRpmsg.h \
                $(top_srcdir)/linux/include/_MessageQ.h \
                $(top_srcdir)/linux/include/_NameServer.h \
                $(top_srcdir)/linux/include/_MultiProc.h \
                $(top_srcdir)/linux/include/_lad.h \
                $(top_srcdir)/linux/include/SocketFxns.h \
                $(top_srcdir)/linux/include/ladclient.h \
                $(top_srcdir)/linux/include/net/rpmsg.h \
                $(top_srcdir)/packages/ti/ipc/MessageQ.h \
                $(top_srcdir)/packages/ti/ipc/NameServer.h \
                $(top_srcdir)/packages/ti/ipc/MultiProc.h

# list of sources for the 'lad' binary and to add to the source distribution
lad_omap54xx_smp_SOURCES = $(common_sources) MultiProcCfg_omap54xx_smp.c
lad_omapl138_SOURCES = $(common_sources) MultiProcCfg_omapl138.c
lad_tci6614_SOURCES = $(common_sources) MultiProcCfg_tci6614.c
lad_tci6638_SOURCES = $(common_sources) MultiProcCfg_tci6638.c

common_libraries = -lpthread $(top_builddir)/linux/src/api/libtiipc.la \
                $(top_builddir)/linux/src/utils/libtiipcutils.la

# the additional libraries needed to link program
lad_omap54xx_smp_LDADD = $(common_libraries) \
                $(AM_LDFLAGS)
lad_omapl138_LDADD = $(common_libraries) \
                $(AM_LDFLAGS)
lad_tci6614_LDADD = $(common_libraries) \
                $(AM_LDFLAGS)
lad_tci6638_LDADD = $(common_libraries) \
                $(AM_LDFLAGS)

###############################################################################
