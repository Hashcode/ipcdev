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

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION= TI OMAP4 Tiler PAT lib
endef


ifeq ("$(TILER_DEBUG)", "1")
#enable debug build
CCOPTS += -g -O0
endif # ifeq ("$(TILER_DEBUG)", "")

ifeq ("$(TILER_PLATFORM)", "")
#default value
TILER_PLATFORM=omap4430
endif # ifeq ("$(TILER_PLATFORM)", "")

ifeq ("$(TILER_PLATFORM)", "omap4430")
CCOPTS += -DTILER_PLATFORM_OMAP4
endif # ifeq ("$(TILER_PLATFORM)", "omap4430")
ifeq ("$(TILER_PLATFORM)", "omap5430")
CCOPTS += -DTILER_PLATFORM_OMAP5
endif # ifeq ("$(TILER_PLATFORM)", "omap5430")

USEFILE =
NAME = tiler_pat
CCFLAGS +=
LDFLAGS +=
EXTRA_INCVPATH += ../../resmgr/tiler	\
				$(PROJECT_ROOT)/../resmgr/tiler/public

include $(MKFILES_ROOT)/qmacros.mk
include $(MKFILES_ROOT)/qtargets.mk
