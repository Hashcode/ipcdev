#
# Copyright (c) 2013, Texas Instruments Incorporated
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# *  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# *  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# *  Neither the name of Texas Instruments Incorporated nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
include ../../common.mk

CCFLAGS += -O3

$(PROJECT_ROOT)/arm/o.le.v7/mmrpc_test: $(IPC_REPO)/qnx/src/mm/arm/a.le.v7/libmmrpc.a
$(PROJECT_ROOT)/arm/o.le.v7/mmrpc_test: $(IPC_REPO)/qnx/src/ipc3x_dev/sharedmemallocator/usr/arm/so.le.v7/libsharedmemallocatorS.a
$(PROJECT_ROOT)/arm/o.le.v7/mmrpc_test: $(IPC_REPO)/qnx/src/api/arm/a.le.v7/libipc.a
$(PROJECT_ROOT)/arm/o.le.v7/mmrpc_test: $(IPC_REPO)/qnx/src/utils/arm/a.le.v7/libutils.a
$(PROJECT_ROOT)/arm/o.le.v7/mmrpc_test: $(IPC_REPO)/qnx/src/ipc3x_dev/ti/syslink/build/Qnx/lib/arm/so.le.v7/libipc_clientS.a

# link with syslink static library, dynamic for others
LIBPREF_mmrpc = -Bstatic
LIBPOST_mmrpc = -Bdynamic
LIBPREF_sharedmemallocatorS = -Bstatic
LIBPOST_sharedmemallocatorS = -Bdynamic
LIBPREF_ipc = -Bstatic
LIBPOST_ipc = -Bdynamic
LIBPREF_utils = -Bstatic
LIBPOST_utils = -Bdynamic
LIBPREF_ipc_client = -Bstatic
LIBPOST_ipc_client = -Bdynamic

# add libpath and libs
EXTRA_LIBVPATH += \
        $(IPC_REPO)/qnx/src/ipc3x_dev/ti/syslink/build/Qnx/lib/arm/so.le.v7 \
        $(IPC_REPO)/qnx/src/api/arm/a.le.v7 \
        $(IPC_REPO)/qnx/src/utils/arm/a.le.v7 \
        $(IPC_REPO)/qnx/src/mm/arm/a.le.v7 \
        $(IPC_REPO)/qnx/src/ipc3x_dev/sharedmemallocator/usr/arm/so.le.v7

LIBS += mmrpc
LIBS += sharedmemallocatorS
LIBS += ipc
LIBS += utils
LIBS += ipc_clientS
