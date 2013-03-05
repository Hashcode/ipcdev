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

vpath %.idl	$(VPATH.idl)

MAKEFILE = idl.make

IDLFLAGS += --codegen-qcm --relative-paths
IDLFLAGS.skel += --qcm-skel
IDLFLAGS.impl += --qcm-impl
IDLFLAGS.common +=

IDLINCLUDES = $(INCLUDES.idl)

IDL = idl

IDL.skel := $(IDL) $(IDLFLAGS) $(IDLFLAGS.skel) $(IDLINCLUDES)
IDL.impl := $(IDL) $(IDLFLAGS) $(IDLFLAGS.impl) $(IDLINCLUDES)
IDL.common := $(IDL) $(IDLFLAGS) $(IDLFLAGS.common) $(IDLINCLUDES)

%.h: %.idl
	$(IDL.common) $<

%.c: %.idl
	$(IDL.common) $<

%_impl.c: %.idl
	$(IDL.impl) $<

%_impl.h: %.idl
	$(IDL.impl) --no-poa-ties --no-poa-stubs $<
	@-rm $*_impl.c

%_skel.c: %.idl
	$(IDL.skel) $<
