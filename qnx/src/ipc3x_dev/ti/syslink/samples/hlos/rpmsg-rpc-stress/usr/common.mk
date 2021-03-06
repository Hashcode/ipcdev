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
PINFO DESCRIPTION=rpmsg-rpc Stress Test
endef

NAME = rpmsg-rpc-stress
INSTALLDIR = bin

#default SYSLINK Product root path and can be overridden from commandline
SYSLINK_ROOT = $(PROJECT_ROOT)/../../../../../..
SYSLINK_BUILDOS = Qnx

#Add extra include path
EXTRA_INCVPATH += $(SYSLINK_ROOT) \
        $(SYSLINK_ROOT)/ti/syslink/inc \
        $(SYSLINK_ROOT)/ti/syslink/inc/usr/$(SYSLINK_BUILDOS) \
        $(SYSLINK_ROOT)/ti/syslink/inc/usr \
        $(SYSLINK_ROOT)/ti/syslink/inc/$(SYSLINK_BUILDOS) \
        $(IPC_REPO)/qnx/src/ipc3x_dev/sharedmemallocator/usr/public \
        $(IPC_REPO)/qnx/src/ipc3x_dev/sharedmemallocator/resmgr/public

EXTRA_SRCVPATH += \
        $(PROJECT_ROOT)/$(SYSLINK_BUILDOS) \
        $(PROJECT_ROOT)/..

CCOPTS += -g -O0 -DSYSLINK_BUILDOS_QNX

include $(MKFILES_ROOT)/qtargets.mk
