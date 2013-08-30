#
#   Copyright (c) 2012-2013, Texas Instruments Incorporated
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
#  ======== products.mak ========
#

# Note that these variables can be explicitly set here or on the command line.
# The ?= assignment used through gives the command line precedence over
# settings in this file.

# Optional: recommended to install all dependent components in one folder.
#
DEPOT ?= _your_depot_folder_

# Optional: platform to build
#   Supported platforms (choose one):
#       omapl138, omap54xx_smp, dra7xx, tci6636, tci6638
#
# Note, this is used for both Linux and BIOS builds
#
PLATFORM ?=


#################### IPC Linux ####################

# Set up required cross compiler path for IPC Linux configuration and build
#
TOOLCHAIN_LONGNAME ?= arm-none-linux-gnueabi
TOOLCHAIN_INSTALL_DIR ?= $(DEPOT)/_your_arm_code_gen_install_
TOOLCHAIN_PREFIX ?= $(TOOLCHAIN_INSTALL_DIR)/bin/$(TOOLCHAIN_LONGNAME)-

# Optional: Path to Linux Kernel - needed to build the MmRpc user libraries
# (for devices that support it)
#
KERNEL_INSTALL_DIR ?=

# Optional: Path to DRM Library
#
DRM_PREFIX ?=

# Optional: Path to TI Linux Utils product
#
CMEM_INSTALL_DIR ?=


#################### IPC QNX ####################

# Path to QNX tools installation
#
QNX_INSTALL_DIR ?=

# Destination for target binaries
#
DESTDIR ?=

# List of supported devices (choose one): omap5432, vayu, simvayu
#
DEVICE ?= _device_

#################### IPC Bios ####################

# Path to required dependencies for IPC BIOS builds
#
XDC_INSTALL_DIR ?= $(DEPOT)/_your_xdctools_install_
BIOS_INSTALL_DIR ?= $(DEPOT)/_your_bios_install_

# Path to various cgtools
#
ti.targets.C28_large ?=
ti.targets.C28_float ?=
ti.targets.C64P ?=
ti.targets.C64P_big_endian ?=
ti.targets.C674 ?=

ti.targets.elf.C64P ?=
ti.targets.elf.C64P_big_endian ?=
ti.targets.elf.C64T ?=
ti.targets.elf.C66 ?=
ti.targets.elf.C66_big_endian ?=
ti.targets.elf.C674 ?=

ti.targets.arm.elf.Arm9 ?=
ti.targets.arm.elf.A8F ?=
ti.targets.arm.elf.A8Fnv ?=
ti.targets.arm.elf.M3 ?=
ti.targets.arm.elf.M4 ?=
ti.targets.arm.elf.M4F ?=

ti.targets.arp32.elf.ARP32 ?=
ti.targets.arp32.elf.ARP32_far ?=

gnu.targets.arm.A8F ?=
gnu.targets.arm.A15F ?=
