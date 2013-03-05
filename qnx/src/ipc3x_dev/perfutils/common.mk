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
PINFO DESCRIPTION=SysLink IPC PerfUtils Library
endef

NAME = ipc_perfutils
INSTALLDIR = usr/lib

ifeq ("$(SYSLINK_PLATFORM)", "")
CCOPTS += -DSYSLINK_PLATFORM_OMAP4430
endif # ifeq ("$(SYSLINK_PLATFORM)", "")

ifeq ("$(SYSLINK_PLATFORM)", "omap4430")
CCOPTS += -DSYSLINK_PLATFORM_OMAP4430
endif # ifeq ("$(SYSLINK_PLATFORM)", "omap4430")

ifeq ("$(SYSLINK_PLATFORM)", "omap5430")
CCOPTS += -DSYSLINK_PLATFORM_OMAP5430
endif # ifeq ("$(SYSLINK_PLATFORM)", "omap5430")

include $(MKFILES_ROOT)/qtargets.mk

CCOPTS += -DUSE_GPTIMER
#CCOPTS += -g -O0
CCFLAGS += $(COMPILE_FLAGS)
CCFLAGS += -fPIC


