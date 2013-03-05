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
## ======== src/test/Makefile.am ========
##

# additional include paths necessary to compile the program
AM_CFLAGS = -I$(top_srcdir)/linux/include -I$(top_srcdir)/packages  \
        -I$(CMEM_INSTALL_DIR)/packages -D_GNU_SOURCE -Wall @AM_CFLAGS@

###############################################################################
# THE PROGRAMS TO BUILD
###############################################################################

# the program to build (the names of the final binaries)
bin_PROGRAMS = ping_rpmsg

if OMAP4430
  bin_PROGRAMS += MessageQApp_omap4430 MessageQBench_omap4430 \
                MessageQMulti_omap4430 NameServerApp_omap4430
else
if OMAPL138
  bin_PROGRAMS += MessageQApp_omapl138 MessageQBench_omapl138 \
                MessageQMulti_omapl138 NameServerApp_omapl138
if CMEM
  bin_PROGRAMS += nano_test_omapl138
endif
else
if TCI6614
  bin_PROGRAMS += MessageQApp_tci6614 MessageQBench_tci6614 \
                MessageQMulti_tci6614 NameServerApp_tci6614
else
if TCI6638
  bin_PROGRAMS += MessageQApp_tci6638 MessageQBench_tci6638 \
                MessageQMulti_tci6638 NameServerApp_tci6638
else
  bin_PROGRAMS += MessageQApp_omap4430 MessageQBench_omap4430 \
                MessageQMulti_omap4430 NameServerApp_omap4430 \
                MessageQApp_omapl138 MessageQBench_omapl138 \
                MessageQMulti_omapl138 NameServerApp_omapl138 \
                MessageQApp_tci6614 MessageQBench_tci6614 \
                MessageQMulti_tci6614 NameServerApp_tci6614 \
                MessageQApp_tci6638 MessageQBench_tci6638 \
                MessageQMulti_tci6638 NameServerApp_tci6638
if CMEM
  bin_PROGRAMS += nano_test_omapl138
endif
endif
endif
endif
endif

common_sources = \
                $(top_srcdir)/linux/include/Std.h \
                $(top_srcdir)/linux/include/SysLink.h \
                $(top_srcdir)/packages/ti/ipc/MessageQ.h

nameServer_common_sources = \
                $(top_srcdir)/linux/include/Std.h\
                $(top_srcdir)/linux/include/ladclient.h \
                $(top_srcdir)/linux/include/_NameServer.h \
                $(top_srcdir)/packages/ti/ipc/NameServer.h \
                NameServerApp.c

# list of sources for the 'ping_rpmsg' binary
ping_rpmsg_SOURCES = ping_rpmsg.c

# list of sources for the 'MessageQApp' binary
MessageQApp_omap4430_SOURCES = $(common_sources) MessageQApp.c
MessageQApp_omapl138_SOURCES = $(common_sources) MessageQApp.c
MessageQApp_tci6614_SOURCES = $(common_sources) MessageQApp.c
MessageQApp_tci6638_SOURCES = $(common_sources) MessageQApp.c

# list of sources for the 'MessageQBench' binary
MessageQBench_omap4430_SOURCES = $(common_sources) MessageQBench.c
MessageQBench_omapl138_SOURCES = $(common_sources) MessageQBench.c
MessageQBench_tci6614_SOURCES  = $(common_sources) MessageQBench.c
MessageQBench_tci6638_SOURCES  = $(common_sources) MessageQBench.c

# list of sources for the 'MessageQMulti' binary
MessageQMulti_omap4430_SOURCES = $(common_sources) MessageQMulti.c
MessageQMulti_omapl138_SOURCES = $(common_sources) MessageQMulti.c
MessageQMulti_tci6614_SOURCES  = $(common_sources) MessageQMulti.c
MessageQMulti_tci6638_SOURCES  = $(common_sources) MessageQMulti.c

# list of sources for the 'NameServerApp' binary
NameServerApp_omap4430_SOURCES = $(nameServer_common_sources)
NameServerApp_omapl138_SOURCES = $(nameServer_common_sources)
NameServerApp_tci6614_SOURCES  = $(nameServer_common_sources)
NameServerApp_tci6638_SOURCES  = $(nameServer_common_sources)

# list of sources for the 'nano_test' binary
nano_test_omapl138_SOURCES = $(common_sources) nano_test.c

common_libraries = -lpthread $(top_builddir)/linux/src/api/libtiipc.la \
                $(top_builddir)/linux/src/utils/libtiipcutils.la

# the additional libraries to link ping_rpmsg
ping_rpmsg_LDADD = -lrt

# the additional libraries needed to link MessageQApp
MessageQApp_omap4430_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omap4430.la \
                $(AM_LDFLAGS)
MessageQApp_omapl138_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omapl138.la \
                $(AM_LDFLAGS)
MessageQApp_tci6614_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_tci6614.la \
                $(AM_LDFLAGS)
MessageQApp_tci6638_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_tci6638.la \
                $(AM_LDFLAGS)

# the additional libraries needed to link MessageQBench
MessageQBench_omap4430_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omap4430.la -lrt \
                $(AM_LDFLAGS)
MessageQBench_omapl138_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omapl138.la -lrt \
                $(AM_LDFLAGS)
MessageQBench_tci6614_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_tci6614.la -lrt \
                $(AM_LDFLAGS)
MessageQBench_tci6638_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_tci6638.la -lrt \
                $(AM_LDFLAGS)

# the additional libraries needed to link MessageQMulti
MessageQMulti_omap4430_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omap4430.la \
                $(AM_LDFLAGS)
MessageQMulti_omapl138_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omapl138.la \
                $(AM_LDFLAGS)
MessageQMulti_tci6614_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_tci6614.la \
                $(AM_LDFLAGS)
MessageQMulti_tci6638_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_tci6638.la \
                $(AM_LDFLAGS)

# the additional libraries needed to link NameServerApp
NameServerApp_omap4430_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omap4430.la \
                $(AM_LDFLAGS)
NameServerApp_omapl138_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omapl138.la \
                $(AM_LDFLAGS)
NameServerApp_tci6614_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_tci6614.la \
                $(AM_LDFLAGS)
NameServerApp_tci6638_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_tci6638.la \
                $(AM_LDFLAGS)

# the additional libraries needed to link nano_test
nano_test_omapl138_LDADD = $(common_libraries) \
                $(top_builddir)/linux/src/family/libtiipccfg_omapl138.la \
                $(CMEM_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV \
                $(AM_LDFLAGS)

###############################################################################
