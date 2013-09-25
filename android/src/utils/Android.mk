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

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

IPC_ROOT := ../../..

LOCAL_C_INCLUDES +=  $(LOCAL_PATH)/$(IPC_ROOT)/linux/include \
                     $(LOCAL_PATH)/$(IPC_ROOT)/packages \
                     $(LOCAL_PATH)/$(IPC_ROOT)/hlos_common/include

LOCAL_CFLAGS += -DIPC_BUILDOS_ANDROID
LOCAL_MODULE_TAGS:= optional

LOCAL_SRC_FILES:= $(IPC_ROOT)/linux/src/utils/LAD_client.c \
                  $(IPC_ROOT)/linux/src/utils/SocketFxns.c \
                  $(IPC_ROOT)/linux/src/utils/MultiProc_app.c \
                  $(IPC_ROOT)/hlos_common/src/utils/MultiProc.c

LOCAL_SHARED_LIBRARIES := \
    liblog

LOCAL_MODULE:= libtiipcutils
include $(BUILD_SHARED_LIBRARY)

##### libtiipcutils_lad #####
include $(CLEAR_VARS)

IPC_ROOT := ../../..

LOCAL_C_INCLUDES +=  $(LOCAL_PATH)/$(IPC_ROOT)/linux/include \
                     $(LOCAL_PATH)/$(IPC_ROOT)/packages \
                     $(LOCAL_PATH)/$(IPC_ROOT)/hlos_common/include

LOCAL_CFLAGS += -DIPC_BUILDOS_ANDROID
LOCAL_MODULE_TAGS:= optional

LOCAL_SRC_FILES:= $(IPC_ROOT)/linux/src/utils/LAD_client.c \
                  $(IPC_ROOT)/linux/src/utils/SocketFxns.c \
                  $(IPC_ROOT)/hlos_common/src/utils/MultiProc.c

LOCAL_SHARED_LIBRARIES := \
    liblog

LOCAL_ALLOW_UNDEFINED_SYMBOLS:= true

LOCAL_MODULE:= libtiipcutils_lad
include $(BUILD_SHARED_LIBRARY)
