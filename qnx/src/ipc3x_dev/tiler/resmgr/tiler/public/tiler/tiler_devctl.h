/*
 * Copyright (c) 2010, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */
/*
 * tiler_devctl.h
 *
 * TILER driver support functions for TI OMAP processors.
 */

#ifndef TILER_DEVCTL_H
#define TILER_DEVCTL_H

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <atomic.h>
#include <stdbool.h>
#include <devctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <hw/inout.h>
#include <sys/dispatch.h>
#include <sys/procmgr.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <sys/ioctl.h>
#include <tiler/tiler_devctl_cmds.h>

//#include "proto.h"

typedef signed int s32;
typedef unsigned int u32;
typedef signed short s16;
typedef unsigned short u16;
typedef signed char s8;
typedef unsigned char u8;
typedef signed long long s64;
typedef unsigned long long u64;

#define TILER_MAX_NUM_BLOCKS 16

enum tiler_fmt {
	TILFMT_MIN     = -1,
	TILFMT_INVALID = -1,
	TILFMT_NONE    = 0,
	TILFMT_8BIT    = 1,
	TILFMT_16BIT   = 2,
	TILFMT_32BIT   = 3,
	TILFMT_PAGE    = 4,
	TILFMT_MAX     = 4
};

struct area {
	u16 width;
	u16 height;
};

struct tiler_block_info {
	enum tiler_fmt fmt;
	union {
		struct area area;
		u32 len;
	} dim;
	u32 stride;
	void *ptr;
	u32 ssptr;
};

struct tiler_buf_info {
	s32 num_blocks;
	struct tiler_block_info blocks[TILER_MAX_NUM_BLOCKS];
	s32 offset;
};

struct tiler_view_orient {
	u8 rotate_90;
	u8 x_invert;
	u8 y_invert;
};

struct tiler_mapx_info {
	enum tiler_fmt fmt;
	uint32_t width;
	uint32_t height;
	uint32_t gid;
	pid_t pid;
	uint32_t sys_addr;
	uint32_t usr_addr;
};

struct tiler_allocx_info {
	enum tiler_fmt fmt;
	uint32_t width;
	uint32_t height;
	uint32_t align;
	uint32_t offs;
	uint32_t gid;
	pid_t pid;
	uint32_t sys_addr;
};

struct tiler_alloc_block_area_info {
	enum tiler_fmt fmt;
	uint32_t width;
	uint32_t height;
	uint32_t gid;
	uint32_t sys_addr;
	uint32_t num_pages;
};

struct tiler_map_block_info {
	uint32_t sys_addr;
	uint32_t num_pages;
};

struct tiler_reservex_info {
	uint32_t n;
	struct tiler_buf_info b;
	pid_t pid;
};

struct tiler_allocp_info {
	int32_t count;
	enum tiler_fmt fmt;
	uint32_t width;
	uint32_t height;
	int32_t aligned;
};

struct tiler_allocpnv12_info {
	int32_t count;
	uint32_t width;
	uint32_t height;
	int32_t aligned;
};

struct tiler_reorient_info {
	uint32_t tsptr;
	struct tiler_view_orient orient;
};

struct tiler_reorient_tl_info {
	uint32_t tsptr;
	struct tiler_view_orient orient;
	uint32_t width;
	uint32_t height;
};

struct tiler_rotate_view_info {
	struct tiler_view_orient orient;
	uint32_t rotation;
};

struct tiler_regnotify_info {
	uint32_t cmd;
	char *name;
	uint32_t name_len;
};

struct tiler_regnotify_response {
	uint32_t event;
	pid_t pid;
};

#endif

