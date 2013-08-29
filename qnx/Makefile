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

ifeq ("$(PLATFORM)","omap5432")
    SYSLINK_PLATFORM=omap5430
else ifeq ("$(PLATFORM)","vayu")
    SYSLINK_PLATFORM=$(PLATFORM)
endif

all:  utils ipc3x_dev libs tests

libs: ipc3x_dev
	@cd src/api; make
	@cd src/mm; make

utils:
	@cd src/utils; make

tests: libs
	@cd src/tests; make

ipc3x_dev: utils
	@cd src/ipc3x_dev; \
            make SYSLINK_PLATFORM=$(SYSLINK_PLATFORM) SMP=1 VIRTIO=$(BUILD_FOR_VIRTIO)

install:
	@cd src/ipc3x_dev; make SYSLINK_PLATFORM=$(SYSLINK_PLATFORM) \
		SMP=1 VIRTIO=$(BUILD_FOR_VIRTIO) USE_INSTALL_ROOT=1 \
		INSTALL_ROOT_nto=$(DESTDIR) install
	@cd src/mm; make USE_INSTALL_ROOT=1 DESTDIR=$(DESTDIR) \
		INSTALL_ROOT_nto=$(DESTDIR) install
	@cd src/tests; make USE_INSTALL_ROOT=1 DESTDIR=$(DESTDIR) \
		SYSLINK_PLATFORM=$(SYSLINK_PLATFORM) \
		INSTALL_ROOT_nto=$(DESTDIR) install

clean:
	@cd src/api; make clean
	@cd src/mm; make clean
	@cd src/utils; make clean
	@cd src/tests; make clean
	@cd src/ipc3x_dev; \
	make clean SYSLINK_PLATFORM=$(SYSLINK_PLATFORM) SMP=1 VIRTIO=$(BUILD_FOR_VIRTIO)
