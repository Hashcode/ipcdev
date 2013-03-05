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
 * tiler.h
 *
 * TILER driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 */

#ifndef TILER_H
#define TILER_H

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
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/procmgr.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <sys/ioctl.h>

#include "proto.h"

#include "tiler/tiler_devctl.h"

#define PAGE_SIZE 0x1000
#define TILER_PAGE 0x1000
#define TILER_WIDTH    256
#define TILER_HEIGHT   128
#define TILER_BLOCK_WIDTH  64
#define TILER_BLOCK_HEIGHT 64
#define TILER_LENGTH (TILER_WIDTH * TILER_HEIGHT * TILER_PAGE)


/* utility functions */
static inline u32 tilfmt_bpp(enum tiler_fmt fmt)
{
	return  fmt == TILFMT_8BIT ? 1 :
		fmt == TILFMT_16BIT ? 2 :
		fmt == TILFMT_32BIT ? 4 : 0;
}

/**
 * Prototype for notifier callback.
 *
 * @param event_type	type of event that happened
 *
 * @param arg			user private data
 *
 * @return error status
 */
typedef s32 (*tiler_notifier_cb) (int event_type, void *data, void *arg);

/**
 * Registers a notifier block with TILER driver.
 *
 * @param nb		notifier_block
 *
 * @return error status
 */
int tiler_reg_notifier(tiler_notifier_cb cb_ptr, void * arg);

/**
 * Un-registers a notifier block with TILER driver.
 *
 * @param nb		notifier_block
 *
 * @return error status
 */
int tiler_unreg_notifier(tiler_notifier_cb cb_ptr, void * arg);

/**
 * Reserves a 1D or 2D TILER block area and memory for the
 * current process with group ID 0.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 *
 * @return error status
 */
s32 tiler_alloc(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr);

/**
 * Reserves a 1D or 2D TILER block area and memory with extended
 * arguments.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param align 	block alignment (default: PAGE_SIZE)
 * @param offs 		block offset
 * @param gid 		group ID
 * @param pi 		process information
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 *
 * @return error status
 */
s32 tiler_allocx(enum tiler_fmt fmt, u32 width, u32 height,
			u32 align, u32 offs, u32 gid, void *pi, u32 *sys_addr);

/**
 * Maps an existing buffer to a 1D or 2D TILER area for the
 * current process with group ID 0.
 *
 * Currently, only 1D area mapping is supported.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 * @param usr_addr	user space address of existing buffer.
 *
 * @return error status
 */
s32 tiler_map(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr,
								u32 usr_addr);

/**
 * Maps an existing buffer to a 1D or 2D TILER area with
 * extended arguments.
 *
 * Currently, only 1D area mapping is supported.
 *
 * NOTE: alignment is always PAGE_SIZE and offset is 0
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param gid 		group ID
 * @param pid 		process information
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 * @param usr_addr	user space address of existing buffer.
 *
 * @return error status
 */
s32 tiler_mapx(enum tiler_fmt fmt, u32 width, u32 height,
			u32 gid, void *pi, u32 *sys_addr, u32 usr_addr);

/**
 * Free TILER memory.
 *
 * @param sys_addr system space (L3) address.
 *
 * @return an error status.
 */
s32 tiler_free(u32 sys_addr);

/**
 * Reserves tiler area for n identical set of blocks (buffer)
 * for the current process.  Use this method to get optimal
 * placement of multiple related tiler blocks; however, it may
 * not reserve area if tiler_alloc is equally efficient.
 *
 * @param n 	number of identical set of blocks
 * @param b	information on the set of blocks (ptr, ssptr and
 *  		stride fields are ignored)
 *
 * @return error status
 */
s32 tiler_reserve(u32 n, struct tiler_buf_info *b);

/**
 * Reserves tiler area for n identical set of blocks (buffer) fo
 * a given process. Use this method to get optimal placement of
 * multiple related tiler blocks; however, it may not reserve
 * area if tiler_alloc is equally efficient.
 *
 * @param n 	number of identical set of blocks
 * @param b	information on the set of blocks (ptr, ssptr and
 *  		stride fields are ignored)
 * @param pid   process ID
 *
 * @return error status
 */
s32 tiler_reservex(u32 n, struct tiler_buf_info *b, pid_t pid);

u32 tiler_reorient_addr(u32 tsptr, struct tiler_view_orient orient);

u32 tiler_get_natural_addr(void *sys_ptr);

u32 tiler_reorient_topleft(u32 tsptr, struct tiler_view_orient orient,
				u32 width, u32 height);

u32 tiler_stride(u32 tsptr);

void tiler_rotate_view(struct tiler_view_orient *orient, u32 rotation);

void tiler_alloc_packed(s32 *count, enum tiler_fmt fmt, u32 width, u32 height,
			void **sysptr, void **allocptr, s32 aligned, void *pi);

void tiler_alloc_packed_nv12(s32 *count, u32 width, u32 height, void **y_sysptr,
				void **uv_sysptr, void **y_allocptr,
				void **uv_allocptr, s32 aligned, void *pi );

/**
 * Reserves a 1D or 2D TILER block area with extended
 * arguments.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param gid 		group ID
 * @param pi 		process information
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 * @param num_pages	pointer where the number of pages required to
 *  			program this area will be stored.
 *
 * @return error status
 */
s32 tiler_alloc_block_area(enum tiler_fmt fmt, u32 width, u32 height, u32 gid,
				void *pi, u32 *sys_addr, u32 *num_pages);

/**
 * Maps an array of physical pages to a 1D or 2D TILER area.
 *
 * @param sys_addr 	The system space (L3) address, previously
 * 				allocated with tiler_alloc_block_area, to which
 * 				the array of pages will be mapped.
 * @param num_pages	number of pages in the pages array
 * @param pages		array of pages to be mapped to sys_addr
 *
 * @return error status
 */
s32 tiler_map_block(u32 sys_addr, u32 num_pages, u32 *pages);

/**
 * Unmaps a block previously mapped with tiler_map_block.
 *
 * @param sys_addr 	The system space (L3) address, previously
 * 				mapped with tiler_map_block, that is to be unmapped.
 *
 * @return error status
 */
s32 tiler_unmap_block(u32 sys_addr);

#endif
